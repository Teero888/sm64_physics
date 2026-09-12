#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "sm64.h"
#include "../host/terrain.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

struct worker {
    struct sm64_terrain *terrain;
    pthread_barrier_t *barrier;
    int height;
};

static void *query(void *arg) {
    struct worker *worker = arg;
    CHECK(sm64_terrain_activate(worker->terrain) == NULL);
    pthread_barrier_wait(worker->barrier);
    struct FloorGeometry *geometry;
    for (int i = 0; i < 10000; ++i) {
        CHECK(find_floor_height_and_data(0, 1000, 0, &geometry) == worker->height);
        CHECK(geometry != NULL && geometry->originOffset == -worker->height);
    }
    struct sm64_terrain *copy = sm64_terrain_clone(worker->terrain);
    CHECK(copy != NULL);
    CHECK(find_floor_height(0, 1000, 0) == worker->height);
    CHECK(sm64_terrain_activate(copy) == worker->terrain);
    sm64_terrain_destroy(copy);
    CHECK(sm64_terrain_activate(NULL) == NULL);
    return NULL;
}

int main(void) {
    pthread_t threads[2];
    pthread_barrier_t barrier;
    CHECK(pthread_barrier_init(&barrier, NULL, 2) == 0);
    struct worker workers[2] = {{.barrier = &barrier, .height = 50}, {.barrier = &barrier, .height = 150}};
    for (int i = 0; i < 2; ++i) {
        int16_t y = workers[i].height;
        struct sm64_terrain_triangle triangle = {
            .vertices = {{-1000,y,-1000}, {0,y,1000}, {1000,y,-1000}}, .type = SURFACE_DEFAULT
        };
        workers[i].terrain = sm64_terrain_create(&triangle, 1, NULL, 0);
        CHECK(workers[i].terrain != NULL);
        CHECK(pthread_create(&threads[i], NULL, query, &workers[i]) == 0);
    }
    for (int i = 0; i < 2; ++i) {
        CHECK(pthread_join(threads[i], NULL) == 0);
        sm64_terrain_destroy(workers[i].terrain);
    }
    CHECK(pthread_barrier_destroy(&barrier) == 0);
    puts("Concurrent terrain queries keep floor geometry and active instances independent");
    return 0;
}
