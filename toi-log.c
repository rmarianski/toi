#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <futile.h>
#include "hash.h"
#include "util.h"

typedef struct {
    uint64_t coord_int;
    unsigned int n;
} tile_log_entry_s;

typedef struct tile_log_chunk_s {
    tile_log_entry_s *entries;
    unsigned int n_entries;
    struct tile_log_chunk_s *next;
} tile_log_chunk_s;

typedef struct {
    tile_log_chunk_s *first;
    unsigned int chunk_size;
} tile_log_chunks_s;

typedef struct {
    // 0 -> z11 - counts for z11-20
    unsigned int n_dropped_by_zoom[10];
} prune_stat_s;

void add_log_entry(tile_log_chunks_s *chunks, tile_log_entry_s *entry) {
    tile_log_chunk_s *chunk = chunks->first;
    if (!chunk || chunk->n_entries == chunks->chunk_size) {
        chunk = malloc(sizeof(tile_log_chunk_s) + chunks->chunk_size * sizeof(tile_log_entry_s));
        chunk->entries = (tile_log_entry_s *)(chunk + 1);
        chunk->n_entries = 0;
        chunk->next = chunks->first;
        chunks->first = chunk;
    }
    chunk->entries[chunk->n_entries++] = *entry;
}

void free_tile_log_chunks(tile_log_chunks_s *chunks) {
    tile_log_chunk_s *next, *cur;
    for (cur = chunks->first; cur; cur = next) {
        next = cur->next;
        free(cur);
    }
}

coord_hash_table_s create_log_entry_hash(tile_log_chunks_s *chunks) {
    unsigned int n_log_entries = 0;
    for (tile_log_chunk_s *chunk = chunks->first; chunk; chunk = chunk->next) {
        n_log_entries += chunk->n_entries;
    }

    unsigned int hash_size = find_nearest_power_2_lower(n_log_entries);
    unsigned int size_hash_entries = sizeof(coord_hash_entry_s) * n_log_entries;
    unsigned int size_buckets = sizeof(coord_hash_entry_s *) * hash_size;
    unsigned int total_mem_size = size_hash_entries + size_buckets;
    void *memory = malloc(total_mem_size);
    memset(memory, 0, total_mem_size);

    coord_hash_entry_s **buckets = (coord_hash_entry_s **)memory;
    coord_hash_entry_s *hash_entries = (coord_hash_entry_s *)(buckets + hash_size);

    unsigned int hash_entry_index = 0;
    for (tile_log_chunk_s *chunk = chunks->first; chunk; chunk = chunk->next) {
        for (unsigned int log_index = 0; log_index < chunk->n_entries; log_index++) {
            tile_log_entry_s *log_entry = chunk->entries + log_index;
            coord_hash_entry_s *hash_entry = hash_entries + hash_entry_index++;
            hash_entry->coord_int = log_entry->coord_int;

            unsigned int hashcode = calc_coord_int_hash(log_entry->coord_int);
            unsigned int bucket = hashcode & (hash_size - 1);

            coord_hash_entry_s *location = buckets[bucket];
            hash_entry->next = location;
            buckets[bucket] = hash_entry;
        }
    }
    coord_hash_table_s result = {
        .buckets = buckets,
        .size = hash_size,
    };
    return result;
}

void command_prune_stats(coord_ints_s *toi, char *tile_logs_str) {
    // for arrays, the 0th element will start off at z11
    unsigned int base = 11;

    unsigned int toi_counts_by_zoom[10] = {};

    FILE *in = fopen(tile_logs_str, "rb");
    perr_die_if(!in, "fopen");
    perr_die_if(fseek(in, 0, SEEK_END) != 0, "fseek");
    long size = ftell(in);
    perr_die_if(size < 0, "ftell");
    perr_die_if(fseek(in, 0, SEEK_SET) != 0, "fseek");

    unsigned int n_log_entries = size / sizeof(tile_log_entry_s);
    coord_hash_table_s toi_table = create_coord_hash(toi);
    tile_log_entry_s *log_entries = malloc(sizeof(tile_log_entry_s) * n_log_entries);
    size_t n_read = fread(log_entries, sizeof(tile_log_entry_s), n_log_entries, in);
    assert(n_read == n_log_entries);

    unsigned int n_log_buckets = find_nearest_power_2_lower(n_log_entries);
    coord_hash_entry_s *log_hash_entries = malloc(sizeof(coord_hash_entry_s) * n_log_entries);
    coord_hash_entry_s **log_hash_buckets = malloc(sizeof(coord_hash_entry_s *) * n_log_entries);
    unsigned int log_hash_entry_index = 0;
    for (unsigned int log_entry_index = 0;
         log_entry_index < n_log_entries;
         log_entry_index++) {
        tile_log_entry_s *log_entry = log_entries + log_entry_index;
        unsigned int hash_code = calc_coord_int_hash(log_entry->coord_int);
        unsigned int bucket_index = hash_code & (n_log_buckets - 1);
        coord_hash_entry_s *new_entry = log_hash_entries + log_hash_entry_index++;
        new_entry->coord_int = log_entry->coord_int;
        new_entry->next = log_hash_buckets[bucket_index];
        log_hash_buckets[bucket_index] = new_entry;
    }
    coord_hash_table_s log_table = {
        .buckets = log_hash_buckets,
        .size = n_log_buckets,
    };

    // for 0, all toi that are not in entries list
    // for > 0, all entries that are not in toi, with results starting at the entry counts

    // prune stats contains counts for 0-10
    prune_stat_s prune_stats[11] = {};
    // probably not necessary, but just in case
    memset(prune_stats, 0, sizeof(prune_stats));

    // NOTE: iterate through toi first, and all coords that don't exist in logs
    // are 0 requests
    for (unsigned int toi_index = 0; toi_index < toi_table.size; toi_index++) {

        for (coord_hash_entry_s *entry = toi_table.buckets[toi_index]; entry; entry = entry->next) {

            futile_coord_s coord;
            futile_coord_unmarshall_int(entry->coord_int, &coord);

            if (coord.z < 11 || coord.z > 20) continue;

            toi_counts_by_zoom[coord.z - base]++;

            if (!table_contains_coord(&log_table, entry->coord_int)) {
                // assert(coord.z >= 11 && coord.z <= 20);

                unsigned int drop_index = coord.z - base;
                assert(drop_index >= 0 && drop_index < 10);
                for (unsigned int prune_index = 0;
                     prune_index < sizeof(prune_stats) / sizeof(prune_stats[0]);
                     prune_index++) {
                    prune_stat_s *prune_stat = prune_stats + prune_index;
                    prune_stat->n_dropped_by_zoom[drop_index]++;
                }
            }
        }
    }
    free_coord_table(&log_table);

    // NOTE: for each entry, count where dropped appropriately
    for (unsigned int log_entry_index = 0;
         log_entry_index < n_log_entries;
         log_entry_index++) {
        tile_log_entry_s *entry = log_entries + log_entry_index;
        if (entry->n > 10) continue;
        futile_coord_s coord;
        futile_coord_unmarshall_int(entry->coord_int, &coord);
        unsigned int z = coord.z;
        if (z < 11 || z > 20) continue;
        unsigned int drop_index = z - base;
        assert(drop_index >= 0 && drop_index < 10);
        if (table_contains_coord(&toi_table, entry->coord_int)) {
            for (unsigned int prune_index = entry->n;
                 prune_index < sizeof(prune_stats) / sizeof(prune_stats[0]);
                 prune_index++) {
                assert(prune_index > 0);
                prune_stat_s *prune_stat = prune_stats + prune_index;
                prune_stat->n_dropped_by_zoom[drop_index]++;
            }
        }
    }

    printf("Original toi:\n");
    for (unsigned int toi_index = 0; toi_index < sizeof(toi_counts_by_zoom) / sizeof(toi_counts_by_zoom[0]); toi_index++) {
        unsigned int toi_count = toi_counts_by_zoom[toi_index];
        printf("%2d: %d\n", toi_index + base, toi_count);
    }
    puts("\n");

    for (unsigned int prune_index = 0; prune_index < sizeof(prune_stats) / sizeof(prune_stats[0]); prune_index++) {
        prune_stat_s *prune_stat = prune_stats + prune_index;
        printf("Pruned for request counts <= %u\n", prune_index);
        for (unsigned int zoom_index = 0; zoom_index < sizeof(prune_stat->n_dropped_by_zoom) / sizeof(prune_stat->n_dropped_by_zoom[0]); zoom_index++) {
            unsigned int z = zoom_index + base;
            unsigned int toi_count = toi_counts_by_zoom[zoom_index];
            unsigned int prune_count = prune_stat->n_dropped_by_zoom[zoom_index];
            assert(toi_count >= prune_count);
            unsigned int new_count = toi_count - prune_count;
            printf("%2u: %d\n", z, new_count);
        }
        puts("\n");
    }

    free_coord_table(&toi_table);
}

void parse_log_entries(tile_log_chunks_s *chunks, char *filename) {
    FILE *fh = fopen(filename, "r");
    perr_die_if(!fh, "fopen");

    char buffer[64];
    while (fgets(buffer, sizeof(buffer)-1, fh) != NULL) {
        tile_log_entry_s entry;
        int z, x, y, n;
        int entries_read = sscanf(buffer, "%2d | %10d | %10d | %2d", &z, &x, &y, &n);
        assert(entries_read == 4);
        if (z < 11 || z > 20 ||
            x < 0 || y < 0 ||
            x >= pow(2, z) || y >= pow(2, z)) {
            continue;
        }
        assert(n > 0);
        futile_coord_s coord = {
            .z = z,
            .x = x,
            .y = y,
        };
        uint64_t coord_int = futile_coord_marshall_int(&coord);
        entry.coord_int = coord_int;
        entry.n = n;

        add_log_entry(chunks, &entry);
    }

    perr_die_if(fclose(fh), "fclose");
}

void write_log_entries(tile_log_chunks_s *chunks, char *filename) {
    FILE *fh = fopen(filename, "wb");
    perr_die_if(!fh, "fopen");
    for (tile_log_chunk_s *chunk = chunks->first;
         chunk;
         chunk = chunk->next) {
        perr_die_if(
            fwrite(chunk->entries, sizeof(tile_log_entry_s), chunk->n_entries, fh) !=
                chunk->n_entries, "fwrite");
    }
    perr_die_if(fclose(fh), "fclose");
}

int main(int argc, char *argv[]) {
#define LOG_CHUNK_SIZE 4096 * 1000
#if 1
    // create a binary file of log entries given the text sql results
    die_if(argc != 2, "Specify sql results text file\n");
    tile_log_chunks_s chunks = {
        .chunk_size = LOG_CHUNK_SIZE,
    };
    for (unsigned int file_index = 1; file_index < argc; file_index++) {
        char *file_path = argv[file_index];
        parse_log_entries(&chunks, file_path);
    }
    write_log_entries(&chunks, "log_entries.bin");
    free_tile_log_chunks(&chunks);
#else
    // read in the toi, the log entries, and print out the prune stats
    coord_ints_s toi = read_coord_ints("toi.bin");
    command_prune_stats(&toi, "log_entries.bin");
    free_coord_ints(&toi);
#endif
}
