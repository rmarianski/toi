#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "util.h"

void free_coord_ints(coord_ints_s *coord_ints) {
    free(coord_ints->coord_ints);
    coord_ints->coord_ints = NULL;
}

coord_ints_s read_coord_ints(char *filename) {
    FILE *fh = fopen(filename, "rb");
    perr_die_if(!fh, "fopen");
    perr_die_if(fseek(fh, 0, SEEK_END) != 0, "fseek");
    long size = ftell(fh);
    perr_die_if(size < 0, "ftell");
    perr_die_if(fseek(fh, 0, SEEK_SET) != 0, "fseek");
    long n_coords = size / sizeof(uint64_t);
    uint64_t *coord_ints = malloc(size);
    perr_die_if(fread(coord_ints, sizeof(uint64_t), n_coords, fh) != n_coords, "fread");
    perr_die_if(fclose(fh), "fclose");
    coord_ints_s result = {
        .coord_ints = coord_ints,
        .n = n_coords,
    };
    return result;
}

void add_coord_int(coord_chunks_s *chunks, uint64_t coord_int) {
    coord_chunk_s *chunk = chunks->first;
    if (!chunk || chunk->coord_ints.n == chunks->chunk_size) {
        coord_chunk_s *new_chunk = malloc(
                sizeof(coord_chunk_s) + sizeof(uint64_t) * chunks->chunk_size);
        new_chunk->coord_ints.coord_ints = (uint64_t *)(new_chunk + 1);
        new_chunk->coord_ints.n = 0;
        new_chunk->next = chunk;
        chunks->first = chunk = new_chunk;
    }
    chunk->coord_ints.coord_ints[chunk->coord_ints.n++] = coord_int;
}

void free_coord_chunks(coord_chunks_s *chunks) {
    coord_chunk_s *next = 0;
    for (coord_chunk_s *chunk = chunks->first;
         chunk;
         chunk = next) {
        next = chunk->next;
        free(chunk);
    }
}
