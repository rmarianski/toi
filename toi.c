#include <assert.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <futile.h>
#include <hiredis/hiredis.h>
#include "util.h"

coord_ints_s read_toi(char *redis_host) {
    redisContext *context = redisConnect(redis_host, 6379);
    die_if(context != NULL && context->err, "Redis connect error: %s\n", context->errstr);
    redisReply *reply = redisCommand(context, "SMEMBERS tilequeue.tiles-of-interest");
    die_if(reply == NULL, "Redis reply error: %s\n", context->errstr);
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

    coord_ints_s result = {.coord_ints = coord_ints, .n = n};
    return result;
}

void write_coord_ints(coord_ints_s *coord_ints, char *filename) {
    FILE *fh = fopen(filename, "wb");
    perr_die_if(!fh, "fopen");
    perr_die_if(fwrite(coord_ints->coord_ints, sizeof(uint64_t), coord_ints->n, fh) != coord_ints->n, "fwrite");
    perr_die_if(fclose(fh), "fclose");
}

void command_print(char *filename) {
    unsigned int zoom_counts[21] = {0};
    unsigned int total = 0;
    coord_ints_s coord_ints = read_coord_ints(filename);

    futile_coord_s coord;
    for (size_t coord_int_index = 0; coord_int_index < coord_ints.n; coord_int_index++) {
        uint64_t coord_int = coord_ints.coord_ints[coord_int_index];
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

    free_coord_ints(&coord_ints);
}

void command_save(char *host, char *filename) {
    coord_ints_s coord_ints = read_toi(host);
    write_coord_ints(&coord_ints, filename);
    free_coord_ints(&coord_ints);
}

typedef enum {
    CMD_NONE,
    CMD_PRINT,
    CMD_SAVE,
} CMD;

void die_with_usage(char *prog) {
    fprintf(stderr, "%s print|save -f filename\n", prog);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {

    char filename[256];
    char host[256];
    CMD cmd = CMD_NONE;

    if (argc < 2) {
        die_with_usage(argv[0]);
    }

    char *command = argv[1];
    if (strcmp(command, "print") == 0) {
        cmd = CMD_PRINT;
    } else if (strcmp(command, "save") == 0) {
        cmd = CMD_SAVE;
    } else {
        die_with_usage(argv[0]);
    }

    memset(filename, 0, sizeof(filename));
    memset(host, 0, sizeof(host));

    int opt;
    while ((opt = getopt(argc - 1, argv + 1, "f:h:")) != -1) {
        switch (opt) {
            case 'f':
                strncpy(filename, optarg, sizeof(filename)-1);
                break;
            case 'h':
                strncpy(host, optarg, sizeof(host)-1);
                break;
            default:
                die_with_usage(argv[0]);
        }
    }

    switch (cmd) {
        case CMD_PRINT:
            die_if(*filename == '\0', "Missing filename\n");
            command_print(filename);
            break;
        case CMD_SAVE:
            die_if(*filename == '\0', "Missing filename\n");
            die_if(*host == '\0', "Missing host\n");
            command_save(host, filename);
            break;
        default:
            INVALID_CODE_PATH;
    }

    return 0;
}
