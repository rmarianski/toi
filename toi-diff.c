#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <getopt.h>
#include <inttypes.h>
#include <futile.h>
#include "util.h"
#include "hash.h"

void die_with_usage(char *prog) {
    fprintf(stderr, "%s -f filename [minx,miny,maxx,maxy:z0-zn]\n", prog);
    exit(EXIT_FAILURE);
}

typedef struct {
    unsigned int zoom_start, zoom_until;
    int minx, miny, maxx, maxy;
} coord_range_s;

#define MAX_COORD_RANGES 16
typedef struct {
    coord_range_s ranges[MAX_COORD_RANGES];
    unsigned int n;
} coord_ranges_s;

typedef struct {
    coord_hash_table_s *table;
    unsigned int missing_coords[21];
} for_coord_data_s;

void for_coord_diff(futile_coord_s *coord, void *data_) {
    for_coord_data_s *data = data_;
    assert(coord->z >= 0 && coord->z <= 20);
    uint64_t coord_int = futile_coord_marshall_int(coord);
    if (!table_contains_coord(data->table, coord_int)) {
        data->missing_coords[coord->z]++;
    }
}

void command_diff(coord_ints_s *coord_ints, coord_ranges_s *ranges) {
    coord_hash_table_s table = create_coord_hash(coord_ints);

    for_coord_data_s for_coord_data = {
        .table = &table,
    };
    memset(for_coord_data.missing_coords, 0, sizeof(for_coord_data.missing_coords));

    for (unsigned int range_index = 0;
         range_index < ranges->n;
         range_index++) {
        if (range_index > 0) {
            puts("");
        }
        coord_range_s *range = ranges->ranges + range_index;
        futile_for_coord_zoom_range(
            range->minx, range->miny, range->maxx, range->maxy,
            range->zoom_start, range->zoom_until,
            for_coord_diff, &for_coord_data);
        for (unsigned int zoom_index = 0; zoom_index <= 20; zoom_index++) {
            if (zoom_index >= range->zoom_start && zoom_index <= range->zoom_until) {
                printf("%2u: %u\n", zoom_index, for_coord_data.missing_coords[zoom_index]);
            }
        }
        memset(for_coord_data.missing_coords, 0, sizeof(for_coord_data.missing_coords));
    }
}

int main(int argc, char *argv[]) {
    char filename[256];
    memset(filename, 0, sizeof(filename));

    int opt;
    while ((opt = getopt(argc, argv, "f:")) != -1) {
        switch (opt) {
            case 'f':
                strncpy(filename, optarg, sizeof(filename)-1);
                break;
            default:
                die_with_usage(argv[0]);
        }
    }

    if (!*filename || optind >= argc) {
        die_with_usage(argv[0]);
    }

    coord_ranges_s ranges = {};
    assert(argc - optind < arraycount(ranges.ranges));
    while (optind < argc) {
        char *range_str = argv[optind++];
        coord_range_s *r = ranges.ranges + ranges.n++;
        unsigned int n_scanned = sscanf(range_str, "%d,%d,%d,%d:%u-%u",
            &r->minx, &r->miny, &r->maxx, &r->maxy, &r->zoom_start, &r->zoom_until);
        if (n_scanned != 6) {
            die_with_usage(argv[0]);
        }
    }

    coord_ints_s coord_ints = read_coord_ints(filename);
    command_diff(&coord_ints, &ranges);
    free_coord_ints(&coord_ints);

    return 0;
}
