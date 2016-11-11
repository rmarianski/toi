#include <stdlib.h>
#include <memory.h>
#include <futile.h>
#include "util.h"
#include "hash.h"

unsigned int find_nearest_power_2_lower(unsigned int x) {
    int power = 1;
    while (x >>= 1) power <<= 1;
    return power;
}

unsigned int find_nearest_power_2_higher(unsigned int x) {
    int power = 2;
    while (x >>= 1) power <<= 1;
    return power;
}

unsigned int calc_coord_int_hash(uint64_t coord_int) {
    const int prime = 12289;
    futile_coord_s coord;
    futile_coord_unmarshall_int(coord_int, &coord);
    unsigned int result;
    result = coord.z;
    result = result * prime + coord.x;
    result = result * prime + coord.y;
    return result;
}

coord_hash_table_s create_coord_hash(coord_ints_s *coord_ints) {
    unsigned int hash_size = find_nearest_power_2_lower(coord_ints->n);

    unsigned int size_entries = sizeof(coord_hash_entry_s) * coord_ints->n;
    unsigned int size_buckets = sizeof(coord_hash_entry_s *) * hash_size;
    unsigned int total_memory = size_entries + size_buckets;
    void *memory = malloc(total_memory);
    memset(memory, 0, total_memory);

    coord_hash_entry_s **buckets = (coord_hash_entry_s **)memory;
    coord_hash_entry_s *entries = (coord_hash_entry_s *)(buckets + hash_size);
    for (size_t coord_ints_index = 0;
        coord_ints_index < coord_ints->n;
        coord_ints_index++) {
        coord_hash_entry_s *entry = entries + coord_ints_index;
        entry->coord_int = coord_ints->coord_ints[coord_ints_index];

        unsigned int hashcode = calc_coord_int_hash(entry->coord_int);
        size_t bucket_index = hashcode & (hash_size - 1);

        coord_hash_entry_s *location = buckets[bucket_index];
        entry->next = location;
        buckets[bucket_index] = entry;
    }
    coord_hash_table_s result = {
        .buckets = buckets,
        .size = hash_size,
    };

    return result;
}

void print_hash_stats(coord_hash_table_s *table) {
    const size_t lengths_size = 10000;
    unsigned int lengths[lengths_size];
    memset(lengths, 0, sizeof(lengths));
    for (size_t bucket_index = 0;
        bucket_index < table->size;
        bucket_index++) {
        coord_hash_entry_s *entry = table->buckets[bucket_index];
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

bool table_contains_coord(coord_hash_table_s *table, uint64_t coord_int) {
    bool result = false;
    unsigned int hashcode = calc_coord_int_hash(coord_int);
    size_t bucket = hashcode & (table->size - 1);
    for (coord_hash_entry_s *entry = table->buckets[bucket]; entry; entry = entry->next) {
        if (entry->coord_int == coord_int) {
            result = true;
            break;
        }
    }
    return result;
}

void free_coord_table(coord_hash_table_s *table) {
    free(table->buckets);
}
