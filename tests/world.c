#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "sm64_physics.h"
#include "sm64.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

int main(void) {
    /* Floor of 2 triangles covering (-2000, -2000) to (2000, 2000) at y=0 */
    sm64_terrain_triangle triangles[] = {
        {.vertices = {{-2000, 0, 2000}, {2000, 0, -2000}, {-2000, 0, -2000}}, .type = 0},
        {.vertices = {{-2000, 0, 2000}, {2000, 0, 2000}, {2000, 0, -2000}}, .type = 0}
    };

    /* 1. Test sm64_world_create */
    sm64_sim_world *world = sm64_world_create(triangles, 2, NULL, 0, 0, 0.0f, 500.0f, 0.0f, 0);
    CHECK(world != NULL);

    sm64_view view;
    CHECK(sm64_world_view(world, &view) == 0);
    CHECK(view.pos[1] == 500.0f);

    /* 2. Step without input: Mario falls to ground */
    sm64_input neutral = {0};
    char err[256];
    for (int i = 0; i < 60; ++i) {
        CHECK(sm64_world_step(world, neutral, err, sizeof(err)));
    }
    sm64_world_view(world, &view);
    printf("After 60 frames: pos=(%f, %f, %f) vel=(%f, %f, %f) action=0x%x\n",
           view.pos[0], view.pos[1], view.pos[2],
           view.vel[0], view.vel[1], view.vel[2], view.action);
    /* Mario should have landed on y=0 */
    CHECK(fabsf(view.pos[1] - 0.0f) < 1.0f);
    CHECK(view.vel[1] == 0.0f);

    /* 3. Step with stick input: Mario moves forward */
    sm64_input walk = {.stick_y = 64};
    for (int i = 0; i < 30; ++i) {
        CHECK(sm64_world_step(world, walk, err, sizeof(err)));
    }
    sm64_world_view(world, &view);
    CHECK(view.action == ACT_WALKING);

    /* 4. Step with jump input */
    sm64_input jump = {.buttons = A_BUTTON, .stick_y = 64};
    CHECK(sm64_world_step(world, jump, err, sizeof(err)));
    sm64_world_view(world, &view);
    CHECK(view.action == ACT_JUMP);
    CHECK(view.vel[1] > 0.0f);

    /* 5. Check sm64_world_pose and sm64_world_camera */
    sm64_scene_mario scene_mario;
    CHECK(sm64_world_pose(world, &scene_mario));
    CHECK(scene_mario.valid);

    sm64_camera cam;
    CHECK(sm64_world_camera(world, 16.0f / 9.0f, &cam));
    CHECK(cam.fov_y > 0.0f);
    CHECK(cam.near_z > 0.0f);
    CHECK(cam.far_z > cam.near_z);

    /* 6. Test sm64_world_clone */
    sm64_sim_world *clone = sm64_world_clone(world);
    CHECK(clone != NULL);

    /* Step world with right turn, step clone with left turn */
    sm64_input right = {.stick_x = 64};
    sm64_input left = {.stick_x = -64};
    for (int i = 0; i < 15; ++i) {
        CHECK(sm64_world_step(world, right, err, sizeof(err)));
        CHECK(sm64_world_step(clone, left, err, sizeof(err)));
    }
    sm64_view v1, v2;
    sm64_world_view(world, &v1);
    sm64_world_view(clone, &v2);
    /* Positions or velocities should diverge */
    CHECK(v1.pos[0] != v2.pos[0] || v1.pos[2] != v2.pos[2]);

    sm64_world_destroy(clone);

    /* 7. Test save / load state */
    size_t state_size = sm64_world_save(world, NULL, 0, err, sizeof(err));
    CHECK(state_size > 0);
    uint8_t *state = malloc(state_size);
    CHECK(state != NULL);
    size_t saved_size = sm64_world_save(world, state, state_size, err, sizeof(err));
    CHECK(saved_size == state_size);

    /* Step world 30 more frames */
    for (int i = 0; i < 30; ++i) {
        CHECK(sm64_world_step(world, walk, err, sizeof(err)));
    }
    sm64_view v_stepped;
    sm64_world_view(world, &v_stepped);
    CHECK(v_stepped.pos[0] != v1.pos[0] || v_stepped.pos[2] != v1.pos[2]);

    /* Load state back */
    CHECK(sm64_world_load(world, state, state_size, err, sizeof(err)));
    free(state);

    sm64_view v_restored;
    sm64_world_view(world, &v_restored);
    CHECK(fabsf(v_restored.pos[0] - v1.pos[0]) < 0.001f);
    CHECK(fabsf(v_restored.pos[1] - v1.pos[1]) < 0.001f);
    CHECK(fabsf(v_restored.pos[2] - v1.pos[2]) < 0.001f);
    CHECK(v_restored.action == v1.action);

    /* 8. Test sm64_world_physics_create with worker & checkpoints */
    sm64_physics *phys = sm64_world_physics_create(world, err, sizeof(err));
    CHECK(phys != NULL);
    CHECK(phys->step != NULL);
    CHECK(phys->view != NULL);
    CHECK(phys->capture != NULL);
    CHECK(phys->restore != NULL);
    CHECK(phys->destroy != NULL);

    for (int i = 0; i < 10; ++i) {
        phys->step(walk);
    }
    sm64_checkpoint *cp = phys->capture(phys, err, sizeof(err));
    CHECK(cp != NULL);

    float pos_at_cp[3];
    pos_at_cp[0] = phys->view->pos[0];
    pos_at_cp[1] = phys->view->pos[1];
    pos_at_cp[2] = phys->view->pos[2];

    for (int i = 0; i < 20; ++i) {
        phys->step(walk);
    }
    CHECK(phys->view->pos[0] != pos_at_cp[0] || phys->view->pos[2] != pos_at_cp[2]);

    CHECK(phys->restore(phys, cp, err, sizeof(err)));
    CHECK(fabsf(phys->view->pos[0] - pos_at_cp[0]) < 0.001f);
    CHECK(fabsf(phys->view->pos[2] - pos_at_cp[2]) < 0.001f);

    phys->free_checkpoint(cp);
    phys->destroy(phys);

    sm64_world_destroy(world);

    printf("World physics test passed!\n");
    return 0;
}
