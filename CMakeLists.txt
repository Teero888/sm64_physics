cmake_minimum_required(VERSION 3.22)
project(sm64_physics LANGUAGES C)

# The source files in upstream/ are never patched for the host build.
add_library(sm64_physics_core STATIC
    extracted/object_lifetime.c
    extracted/behavior_helpers.c
    host/behavior.c
    extracted/object_math.c
    extracted/object_slots.c
    host/objects.c
    extracted/memory_pool.c
    extracted/save.c
    host/save.c
    extracted/sound_control.c
    extracted/mario_sound.c
    extracted/object_sound.c
    extracted/object_animation_query.c
    extracted/sound_spawner.c
    host/audio.c
    extracted/animation_loader.c
    extracted/mario_animation.c
    host/animation_bank.c
    extracted/controller.c
    extracted/mario_input.c
    host/controller.c
    extracted/animation.c
    extracted/math_util.c
    extracted/object_nodes.c
    extracted/main_pool.c
    extracted/terrain_load.c
    extracted/terrain_queries.c
    extracted/object_transforms.c
    extracted/object_helpers.c
    host/animation.c
    host/address.c
    host/object_nodes.c
    host/pool.c
    host/main_pool.c
    extracted/area_terrain.c
    extracted/mario.c
    extracted/mario_step.c
    extracted/mario_ground.c
    extracted/mario_collision_helpers.c
    extracted/mario_geometry_input.c
    extracted/mario_action_setup.c
    extracted/mario_surface_class.c
    upstream/src/game/interaction.c
    extracted/behavior_script.c
    extracted/random.c
    extracted/object_collision.c
    extracted/object_list_processor.c
    extracted/object_scheduler.c
    extracted/platform_displacement.c
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
target_include_directories(sm64_physics_core PRIVATE upstream/src/engine upstream/src/game)
target_include_directories(sm64_physics_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set_source_files_properties(extracted/main_pool.c PROPERTIES COMPILE_DEFINITIONS SM64_PHYSICS_POOL_IMPLEMENTATION)
set_source_files_properties(extracted/terrain_load.c extracted/terrain_queries.c
    extracted/mario_ground.c extracted/mario_collision_helpers.c
    extracted/mario_geometry_input.c
    PROPERTIES COMPILE_DEFINITIONS SM64_PHYSICS_TERRAIN_IMPLEMENTATION)
target_compile_definitions(sm64_physics_core PUBLIC VERSION_US _LANGUAGE_C NON_MATCHING AVOID_UB NO_SEGMENTED_MEMORY)
set_target_properties(sm64_physics_core PROPERTIES C_STANDARD 11 POSITION_INDEPENDENT_CODE ON)
if(NOT MSVC)
    target_compile_options(sm64_physics_core PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/host/mario_motion.h")
    target_compile_options(sm64_physics_core PRIVATE -fno-strict-aliasing -ffp-contract=off)
    target_link_libraries(sm64_physics_core PUBLIC m)
else()
    target_compile_options(sm64_physics_core PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/host/mario_motion.h")
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_executable(sm64_scheduler_test tests/scheduler.c)
    target_link_libraries(sm64_scheduler_test PRIVATE sm64_physics_core)
    add_test(NAME native_object_scheduler COMMAND sm64_scheduler_test)
    add_executable(sm64_behavior_test tests/behavior.c)
    target_link_libraries(sm64_behavior_test PRIVATE sm64_physics_core)
    add_test(NAME native_behavior_execution COMMAND sm64_behavior_test)
    add_executable(sm64_random_test tests/random.c)
    target_link_libraries(sm64_random_test PRIVATE sm64_physics_core)
    add_test(NAME native_random_state COMMAND sm64_random_test)
    add_executable(sm64_platform_test tests/platform.c)
    target_link_libraries(sm64_platform_test PRIVATE sm64_physics_core)
    add_test(NAME native_platform_displacement COMMAND sm64_platform_test)
    add_executable(sm64_object_collision_test tests/object_collision.c)
    target_link_libraries(sm64_object_collision_test PRIVATE sm64_physics_core)
    add_test(NAME native_object_collision COMMAND sm64_object_collision_test)
    add_executable(sm64_objects_test tests/objects.c)
    target_link_libraries(sm64_objects_test PRIVATE sm64_physics_core)
    add_test(NAME native_object_lifetime COMMAND sm64_objects_test)
    add_executable(sm64_memory_pool_test tests/memory_pool.c)
    target_link_libraries(sm64_memory_pool_test PRIVATE sm64_physics_core)
    add_test(NAME native_memory_pool COMMAND sm64_memory_pool_test)
    add_executable(sm64_save_test tests/save.c)
    target_link_libraries(sm64_save_test PRIVATE sm64_physics_core)
    add_test(NAME native_save_progress COMMAND sm64_save_test)
    add_executable(sm64_audio_test tests/audio.c)
    target_link_libraries(sm64_audio_test PRIVATE sm64_physics_core)
    add_test(NAME native_audio_events COMMAND sm64_audio_test)
    add_test(NAME upstream_fidelity COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_upstream.py)
    add_executable(sm64_collision_test tests/collision.c)
    target_link_libraries(sm64_collision_test PRIVATE sm64_physics_core)
    add_test(NAME native_collision COMMAND sm64_collision_test)
    add_executable(sm64_animation_test tests/animation.c)
    target_link_libraries(sm64_animation_test PRIVATE sm64_physics_core)
    add_test(NAME native_animation COMMAND sm64_animation_test)
    add_executable(sm64_animation_bank_test tests/animation_bank.c)
    target_link_libraries(sm64_animation_bank_test PRIVATE sm64_physics_core)
    add_test(NAME native_animation_bank COMMAND sm64_animation_bank_test)
    add_executable(sm64_object_nodes_test tests/object_nodes.c)
    target_link_libraries(sm64_object_nodes_test PRIVATE sm64_physics_core)
    add_test(NAME native_object_nodes COMMAND sm64_object_nodes_test)
    add_executable(sm64_main_pool_test tests/main_pool.c)
    target_link_libraries(sm64_main_pool_test PRIVATE sm64_physics_core)
    add_test(NAME native_main_pool COMMAND sm64_main_pool_test)
    add_executable(sm64_moving_surfaces_test tests/moving_surfaces.c)
    target_link_libraries(sm64_moving_surfaces_test PRIVATE sm64_physics_core)
    add_test(NAME native_moving_surfaces COMMAND sm64_moving_surfaces_test)
    add_executable(sm64_mario_ground_test tests/mario_ground.c)
    target_link_libraries(sm64_mario_ground_test PRIVATE sm64_physics_core)
    add_test(NAME native_mario_ground COMMAND sm64_mario_ground_test)
    add_executable(sm64_mario_air_test tests/mario_air.c)
    target_link_libraries(sm64_mario_air_test PRIVATE sm64_physics_core)
    add_test(NAME native_mario_air COMMAND sm64_mario_air_test)
    add_executable(sm64_controller_test tests/controller.c)
    target_link_libraries(sm64_controller_test PRIVATE sm64_physics_core)
    add_test(NAME native_controller COMMAND sm64_controller_test)
    add_executable(sm64_mario_geometry_input_test tests/mario_geometry_input.c)
    target_link_libraries(sm64_mario_geometry_input_test PRIVATE sm64_physics_core)
    add_test(NAME native_mario_geometry_input COMMAND sm64_mario_geometry_input_test)
    add_executable(sm64_mario_action_setup_test tests/mario_action_setup.c)
    target_link_libraries(sm64_mario_action_setup_test PRIVATE sm64_physics_core)
    add_test(NAME native_mario_action_setup COMMAND sm64_mario_action_setup_test)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(Threads REQUIRED)
        add_executable(sm64_terrain_threads_test tests/terrain_threads.c)
        target_link_libraries(sm64_terrain_threads_test PRIVATE sm64_physics_core Threads::Threads)
        add_test(NAME native_terrain_threads COMMAND sm64_terrain_threads_test)
    endif()
endif()
