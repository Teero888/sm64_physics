cmake_minimum_required(VERSION 3.22)
project(sm64_physics LANGUAGES C)

# The source files in upstream/ are never patched for the host build.
add_library(sm64_physics_core STATIC
    extracted/animation.c
    extracted/math_util.c
    extracted/object_nodes.c
    host/animation.c
    host/address.c
    host/object_nodes.c
    host/pool.c
    upstream/src/engine/surface_collision.c
    upstream/src/engine/surface_load.c
    upstream/src/game/mario.c
    upstream/src/game/mario_step.c
    upstream/src/game/interaction.c
    upstream/src/engine/behavior_script.c
    upstream/src/game/object_collision.c
    upstream/src/game/object_helpers.c
    upstream/src/game/object_list_processor.c
    upstream/src/game/platform_displacement.c
    upstream/src/game/spawn_object.c
    upstream/src/game/macro_special_objects.c
    upstream/src/game/obj_behaviors.c
    upstream/src/game/obj_behaviors_2.c
    upstream/src/game/behavior_actions.c
    upstream/data/behavior_data.c
    upstream/src/game/mario_actions_airborne.c
    upstream/src/game/mario_actions_automatic.c
    upstream/src/game/mario_actions_cutscene.c
    upstream/src/game/mario_actions_moving.c
    upstream/src/game/mario_actions_object.c
    upstream/src/game/mario_actions_stationary.c
    upstream/src/game/mario_actions_submerged.c
)
target_include_directories(sm64_physics_core PUBLIC upstream/include upstream/src upstream)
target_include_directories(sm64_physics_core PRIVATE upstream/src/engine)
target_compile_definitions(sm64_physics_core PUBLIC VERSION_US _LANGUAGE_C NON_MATCHING AVOID_UB NO_SEGMENTED_MEMORY)
set_target_properties(sm64_physics_core PROPERTIES C_STANDARD 11 POSITION_INDEPENDENT_CODE ON)
if(NOT MSVC)
    target_compile_options(sm64_physics_core PRIVATE -fno-strict-aliasing -ffp-contract=off)
    target_link_libraries(sm64_physics_core PUBLIC m)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME upstream_fidelity COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_upstream.py)
    add_executable(sm64_collision_test tests/collision.c tests/collision_state.c)
    target_link_libraries(sm64_collision_test PRIVATE sm64_physics_core)
    add_test(NAME native_collision COMMAND sm64_collision_test)
    add_executable(sm64_animation_test tests/animation.c)
    target_link_libraries(sm64_animation_test PRIVATE sm64_physics_core)
    add_test(NAME native_animation COMMAND sm64_animation_test)
    add_executable(sm64_object_nodes_test tests/object_nodes.c tests/collision_state.c)
    target_link_libraries(sm64_object_nodes_test PRIVATE sm64_physics_core)
    add_test(NAME native_object_nodes COMMAND sm64_object_nodes_test)
endif()
