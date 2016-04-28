#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <hiredis/hiredis.h>
#include <futile/coord.h>

int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Usage: %s <redis-host>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *redis_host = argv[1];

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

    unsigned int zoom_counts[21] = {0};

    futile_coord_s coord = {0, 0, 0};
    for (size_t i = 0; i < reply->elements; i++) {
        redisReply *element = reply->element[i];
        char *coord_str = element->str;
        uint64_t coord_int;
        int scanned = sscanf(coord_str, "%" SCNu64, &coord_int);
        if (scanned != 1) {
            printf("Could not convert %s to uint64\n", coord_str);
            continue;
        }
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

    freeReplyObject(reply);

    redisFree(context);

    return 0;
}
