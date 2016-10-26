#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <inttypes.h>
#include <hiredis/hiredis.h>
#include <futile.h>

typedef struct {
    uint64_t *coord_ints;
    size_t n;
} toi_s;

toi_s read_toi_redis(char *redis_host) {
    redisContext *context = redisConnect(redis_host, 6379);
    if (context != NULL && context->err) {
        printf("Error: %s\n", context->errstr);
        exit(EXIT_FAILURE);
    }
    redisReply *reply = redisCommand(context, "SMEMBERS tilequeue.tiles-of-interest");
    if (reply == NULL) {
        printf("Error: %s\n", context->errstr);
        exit(EXIT_FAILURE);
    }
    uint64_t *coord_ints = malloc(sizeof(uint64_t) * reply->elements);
    size_t n = 0;
    for (size_t i = 0; i < reply->elements; i++) {
        redisReply *element = reply->element[i];
        char *coord_str = element->str;
        uint64_t coord_int;
        int scanned = sscanf(coord_str, "%" SCNu64, &coord_int);
        if (scanned != 1) {
            fprintf(stderr, "Could not convert %s to uint64\n", coord_str);
            continue;
        }
        coord_ints[n++] = coord_int;
    }
    freeReplyObject(reply);
    redisFree(context);

    toi_s toi = {.coord_ints = coord_ints, .n = n};
    return toi;
}

toi_s read_toi_bin(char *filename) {
    struct stat stat_buf;
    if (stat(filename, &stat_buf) != 0) {
        perror("stat");
        exit(EXIT_FAILURE);
    }
    FILE *fin = fopen(filename, "r");
    if (!fin) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    off_t bin_size = stat_buf.st_size;
    unsigned int n_coord_ints = bin_size / sizeof(uint64_t);
    uint64_t *coord_ints = malloc(bin_size);
    size_t n_read = fread(coord_ints, sizeof(uint64_t), n_coord_ints, fin);
    if (n_read != n_coord_ints) {
        perror("fread");
        exit(EXIT_FAILURE);
    }

    int rc = fclose(fin);
    if (rc) {
        perror("fclose");
        exit(EXIT_FAILURE);
    }
    toi_s result = {.coord_ints = coord_ints, .n=n_coord_ints};
    return result;
}

void free_toi(toi_s toi) {
    free(toi.coord_ints);
}

void print_zoom_counts(toi_s toi) {
    unsigned int zoom_counts[21] = {0};
    unsigned int total = 0;

    futile_coord_s coord;
    for (size_t toi_index = 0; toi_index < toi.n; toi_index++) {
        uint64_t coord_int = toi.coord_ints[toi_index];
        futile_coord_unmarshall_int(coord_int, &coord);
        if (coord.z > 20) {
            continue;
        }
        zoom_counts[coord.z] += 1;
        total++;
    }

    for (int zoom_index = 0; zoom_index <= 20; zoom_index++) {
        unsigned int zoom_count = zoom_counts[zoom_index];
        printf("%2d: %u\n", zoom_index, zoom_count);
    }
    printf("Total: %u\n", total);
}

typedef struct toi_hash_entry_s {
    uint64_t coord_int;
    struct toi_hash_entry_s *next;
} toi_hash_entry_s;

typedef struct {
    toi_hash_entry_s **table;
    size_t size;
} toi_hash_table_s;

unsigned int calc_hash(uint64_t coord_int) {
    const int prime = 12289;
    futile_coord_s coord;
    futile_coord_unmarshall_int(coord_int, &coord);
    unsigned int result;
    result = coord.z;
    result = result * prime + coord.x;
    result = result * prime + coord.y;
    return result;
}

void print_hash_stats(toi_hash_table_s *table) {
    const size_t lengths_size = 10000;
    unsigned int lengths[lengths_size];
    memset(lengths, 0, sizeof(lengths));
    for (size_t entry_index = 0; entry_index < table->size; entry_index++) {
        toi_hash_entry_s *entry = table->table[entry_index];
        if (entry) {
            unsigned int length = 0;
            while (entry) {
                length++;
                entry = entry->next;
            }
            if (length > lengths_size) {
                length = lengths_size;
            }
            lengths[length-1]++;
        }
    }
    for (size_t length_index = 0; length_index < lengths_size; length_index++) {
        unsigned int length = lengths[length_index];
        if (length > 0) {
            printf("%2zu: %u\n", length_index + 1, length);
        }
    }
}

toi_hash_table_s create_hash(toi_s toi, size_t hash_size) {
    unsigned int size_entries = sizeof(toi_hash_entry_s) * toi.n;
    unsigned int size_buckets = sizeof(toi_hash_entry_s *) * hash_size;
    unsigned int total_memory = size_entries + size_buckets;
    void *memory = malloc(total_memory);
    memset(memory, 0, total_memory);

    toi_hash_entry_s **table = (toi_hash_entry_s **)memory;
    toi_hash_entry_s *entries = (toi_hash_entry_s *)(table + hash_size);
    for (size_t toi_index = 0; toi_index < toi.n; toi_index++) {
        toi_hash_entry_s *entry = entries + toi_index;
        entry->coord_int = toi.coord_ints[toi_index];

        unsigned int hashcode = calc_hash(entry->coord_int);
        size_t bucket = hashcode & (hash_size - 1);

        toi_hash_entry_s *location = table[bucket];
        if (location) {
            entry->next = location;
        } else {
            entry->next = NULL;
        }
        table[bucket] = entry;
    }
    toi_hash_table_s result = {
        .table = table,
        .size = hash_size,
    };

    return result;
}

void hash(toi_s toi) {
    size_t hash_size = pow(2, 24);
    toi_hash_table_s table = create_hash(toi, hash_size);
    print_hash_stats(&table);
    free(table.table);
}

void dump(toi_s toi, char *filename) {
    FILE *fout = fopen(filename, "w");
    if (!fout) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    size_t n_written = fwrite(toi.coord_ints, sizeof(uint64_t), toi.n, fout);
    if (n_written != toi.n) {
        perror("fwrite");
        exit(EXIT_FAILURE);
    }

    int rc = fclose(fout);
    if (rc) {
        perror("fclose");
        exit(EXIT_FAILURE);
    }
}

void usage(char *progname) {
    fprintf(stderr, "Usage: %s [-r redis-host] [-b binary-file] [-c <print|hash|dump>] [-d dumpfile]\n",
            progname);
    exit(EXIT_FAILURE);
}

typedef struct {
    int type; // 1 for redis, 2 for bin file
    char *value; // redis host or filename
} input_source_s;

typedef struct {
    futile_coord_s start, end;
    unsigned int end_zoom;
} coord_range_s;

typedef struct {
    uint64_t *missing_coord_ints;
    unsigned int n_missing_coord_ints;
    unsigned int max_missing_coord_ints;
    toi_hash_table_s *table;
} command_diff_baton_s;

bool table_contains_coord(toi_hash_table_s *table, uint64_t coord_int) {
    bool result = false;
    unsigned int hashcode = calc_hash(coord_int);
    size_t bucket = hashcode & (table->size - 1);
    for (toi_hash_entry_s *entry = table->table[bucket]; entry; entry = entry->next) {
        if (entry->coord_int == coord_int) {
            result = true;
            break;
        }
    }
    return result;
}

void command_diff_coord_to_check(futile_coord_s *coord, void *userdata_) {
    command_diff_baton_s *userdata = (command_diff_baton_s *)userdata_;
    uint64_t coord_int = futile_coord_marshall_int(coord);
    if (!table_contains_coord(userdata->table, coord_int)) {
        assert(userdata->n_missing_coord_ints < userdata->max_missing_coord_ints);
        userdata->missing_coord_ints[userdata->n_missing_coord_ints++] = coord_int;
    }
}

void diff(toi_s toi, coord_range_s *coord_range) {
    assert(coord_range->end_zoom >= coord_range->start.z);
    size_t hash_size = pow(2, 24);
    toi_hash_table_s table = create_hash(toi, hash_size);
    command_diff_baton_s userdata = {};
    userdata.table = &table;
    unsigned int max_missing_coord_ints = toi.n * 10;
    userdata.missing_coord_ints = malloc(sizeof(uint64_t) * max_missing_coord_ints);
    userdata.max_missing_coord_ints = max_missing_coord_ints;

    futile_for_coord_zoom_range(
        coord_range->start.x, coord_range->start.y,
        coord_range->end.x, coord_range->end.y,
        coord_range->start.z, coord_range->end_zoom,
        command_diff_coord_to_check, &userdata);

    unsigned int zoom_range = coord_range->end_zoom - coord_range->start.z + 1;
    unsigned int size_missing_coords_per_zoom = sizeof(unsigned int) * zoom_range;
    unsigned int *missing_coords_per_zoom = malloc(size_missing_coords_per_zoom);
    memset(missing_coords_per_zoom, 0, size_missing_coords_per_zoom);
    unsigned int base = coord_range->start.z;

    for (unsigned int i = 0; i < userdata.n_missing_coord_ints; i++) {
        uint64_t coord_int = userdata.missing_coord_ints[i];
        futile_coord_s coord;
        futile_coord_unmarshall_int(coord_int, &coord);
        assert(coord.z >= base && coord.z <= coord_range->end_zoom);
        unsigned int missing_coords_per_zoom_idx = coord.z - base;
        missing_coords_per_zoom[missing_coords_per_zoom_idx]++;
    }
    for (unsigned int i = 0; i < zoom_range; i++) {
        unsigned int z = i + base;
        printf("%u: %d\n", z, missing_coords_per_zoom[i]);
    }

    free(userdata.missing_coord_ints);
    free(missing_coords_per_zoom);
}

int main(int argc, char *argv[]) {

    input_source_s input_source = {};
    int command = 0; // print->1, hash->2, dump->3, diff->4
    int opt;
    char *dump_filename = NULL;
    char *coord_range_str = NULL;
    coord_range_s coord_range = {};

    while ((opt = getopt(argc, argv, "r:b:c:d:z:")) != -1) {
        switch (opt) {
        case 'r':
            input_source.type = 1;
            input_source.value = strdup(optarg);
            break;
        case 'b':
            input_source.type = 2;
            input_source.value = strdup(optarg);
            break;
        case 'c':
            if (strcmp(optarg, "print") == 0) {
                command = 1;
            } else if (strcmp(optarg, "hash") == 0) {
                command = 2;
            } else if (strcmp(optarg, "dump") == 0) {
                command = 3;
            } else if (strcmp(optarg, "diff") == 0) {
                command = 4;
            } else {
                usage(argv[0]);
            }
            break;
        case 'd':
            dump_filename = strdup(optarg);
            break;
        case 'z': {
            coord_range_str = strdup(optarg);
            int n_scanned = sscanf(coord_range_str, "%10d/%10d/%10d-%10d/%10d/%10d-%2d",
                                   &coord_range.start.z, &coord_range.start.x, &coord_range.start.y,
                                   &coord_range.end.z, &coord_range.end.x, &coord_range.end.y,
                                   &coord_range.end_zoom);
            if (n_scanned != 7) {
                fprintf(stderr, "Invalid coord range: %s\n", coord_range_str);
                exit(EXIT_FAILURE);
            }
            assert(
                coord_range.start.z == coord_range.end.z &&
                coord_range.start.x <= coord_range.end.x &&
                coord_range.start.y <= coord_range.end.y &&
                coord_range.end_zoom >= coord_range.start.z
                );
        } break;
        default:
            usage(argv[0]);
        }
    }

    if (!input_source.type) {
        fprintf(stderr, "Must specify either -r (redis host) or -b (binary file)\n");
        exit(EXIT_FAILURE);
    }

    if (!command) {
        fprintf(stderr, "Must specify -c command <print|hash|dump|diff>\n");
        exit(EXIT_FAILURE);
    }

    toi_s toi;
    if (input_source.type == 1) {
        toi = read_toi_redis(input_source.value);
    } else if (input_source.type == 2) {
        toi = read_toi_bin(input_source.value);
    } else {
        assert(!"Add input source type");
    }

    switch (command) {
    case 1:
        print_zoom_counts(toi);
        break;
    case 2:
        hash(toi);
        break;
    case 3:
        if (!dump_filename) {
            fprintf(stderr, "Must specify -d with dump command\n");
            exit(EXIT_FAILURE);
        }
        dump(toi, dump_filename);
        break;
    case 4:
        if (!coord_range_str) {
            fprintf(stderr, "Must specify -z with diff command\n");
            exit(EXIT_FAILURE);
        }
        diff(toi, &coord_range);
        break;
    default:
        assert(!"Add command");
    }

    free_toi(toi);
    free(dump_filename);
    free(input_source.value);
    free(coord_range_str);

    return 0;
}
