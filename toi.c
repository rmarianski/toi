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
#include <futile/coord.h>

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
    for (size_t i = 0; i < n_coord_ints; i++) {
        int n_read = fread(coord_ints + i, sizeof(uint64_t), 1, fin);
        if (n_read != 1) {
            perror("fread");
            exit(EXIT_FAILURE);
        }
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

    futile_coord_s coord;
    for (size_t toi_index = 0; toi_index < toi.n; toi_index++) {
        uint64_t coord_int = toi.coord_ints[toi_index];
        futile_coord_unmarshall_int(coord_int, &coord);
        if (coord.z > 20) {
            continue;
        }
        zoom_counts[coord.z] += 1;
    }

    for (int zoom_index = 0; zoom_index <= 20; zoom_index++) {
        unsigned int zoom_count = zoom_counts[zoom_index];
        printf("%2d: %u\n", zoom_index, zoom_count);
    }
}

typedef struct toi_hash_entry_s {
    uint64_t coord_int;
    struct toi_hash_entry_s *next;
} toi_hash_entry_s;

unsigned int calc_hash(uint64_t coord_int) {
    // TODO experiment here
    // return coord_int;
    // return 0;
    futile_coord_s coord;
    futile_coord_unmarshall_int(coord_int, &coord);
    return coord.z * 7 + coord.x * 5 + coord.y * 3;
}

void print_hash_stats(toi_hash_entry_s **table, size_t hash_size) {
    const size_t lengths_size = 10000;
    unsigned int lengths[lengths_size];
    memset(lengths, 0, sizeof(lengths));
    for (size_t entry_index = 0; entry_index < hash_size; entry_index++) {
        toi_hash_entry_s *entry = table[entry_index];
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
            printf("%zu: %u\n", length_index + 1, length);
        }
    }
}

void hash(toi_s toi) {
    toi_hash_entry_s *entries = malloc(sizeof(toi_hash_entry_s) * toi.n);

    size_t hash_size = pow(2, 24);
    toi_hash_entry_s **table = malloc(sizeof(*(toi_hash_entry_s *)0) * hash_size);
    memset(table, 0, sizeof(*(toi_hash_entry_s *)0) * hash_size);

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

    print_hash_stats(table, hash_size);
    free(table);
    free(entries);
}

void dump(toi_s toi, char *filename) {
    FILE *fout = fopen(filename, "w");
    if (!fout) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    for (size_t toi_index = 0; toi_index < toi.n; toi_index++) {
        int n_written = fwrite(toi.coord_ints + toi_index, sizeof(uint64_t), 1, fout);
        if (n_written != 1) {
            perror("fwrite");
            exit(EXIT_FAILURE);
        }
        assert(n_written == 1);
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

int main(int argc, char *argv[]) {

    input_source_s input_source = {};
    int command = 0; // print->1, hash->2, dump->3
    int opt;
    char *dump_filename = NULL;

    while ((opt = getopt(argc, argv, "r:b:c:d:")) != -1) {
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
            } else {
                usage(argv[0]);
            }
            break;
        case 'd':
            dump_filename = strdup(optarg);
            break;
        default:
            usage(argv[0]);
        }
    }

    if (!input_source.type) {
        fprintf(stderr, "Must specify either -r (redis host) or -b (binary file)\n");
        exit(EXIT_FAILURE);
    }

    if (!command) {
        fprintf(stderr, "Must specify -c command <print|hash|dump>\n");
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
    default:
        assert(!"Add command");
    }

    free_toi(toi);
    free(dump_filename);
    free(input_source.value);

    return 0;
}
