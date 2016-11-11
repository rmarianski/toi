#ifndef UTIL_H
#define UTIL_H

#define perr_die_if(cond, perr_str) if (cond) { perror(perr_str); exit(EXIT_FAILURE); }
#define die_if(cond, err_str, ...) if (cond) { fprintf(stderr, err_str, ##__VA_ARGS__); exit(EXIT_FAILURE); }

#define INVALID_CODE_PATH assert(!"Invalid code path");
#define arraycount(x) (sizeof(x) / sizeof(x[0]))

typedef struct {
    uint64_t *coord_ints;
    size_t n;
} coord_ints_s;

coord_ints_s read_coord_ints(char *filename);
void free_coord_ints(coord_ints_s *coord_ints);

typedef struct coord_chunk_s {
    coord_ints_s coord_ints;
    struct coord_chunk_s *next;
} coord_chunk_s;

typedef struct {
    coord_chunk_s *first;
    unsigned int chunk_size;
} coord_chunks_s;

void add_coord_int(coord_chunks_s *chunks, uint64_t coord_int);
void free_coord_chunks(coord_chunks_s *chunks);

#endif
