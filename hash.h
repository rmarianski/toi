#ifndef HASH_H
#define HASH_H

#include "util.h"

typedef struct coord_hash_entry_s {
    uint64_t coord_int;
    struct coord_hash_entry_s *next;
} coord_hash_entry_s;

typedef struct {
    coord_hash_entry_s **buckets;
    size_t size;
} coord_hash_table_s;

unsigned int find_nearest_power_2_lower(unsigned int x);
unsigned int find_nearest_power_2_higher(unsigned int x);

unsigned int calc_coord_int_hash(uint64_t coord_int);

coord_hash_table_s create_coord_hash(coord_ints_s *coord_ints);

bool table_contains_coord(coord_hash_table_s *table, uint64_t coord_int);

void free_coord_table(coord_hash_table_s *table);

#endif
