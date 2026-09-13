#!/usr/bin/env python3
"""Copy selected complete functions verbatim out of mixed upstream files.

No replacement bodies, regex substitutions, or source patches are applied.
The manifest records both the original file and each selected byte range.
"""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EXTRACTIONS = {
    'mario_holding.c': {'source': 'src/game/interaction.c', 'headers': ['host/mario_holding_state.h'], 'functions': ['mario_stop_riding_object', 'mario_grab_used_object', 'mario_drop_held_object', 'mario_throw_held_object', 'mario_stop_riding_and_holding']},
    'interaction.c': {'source': 'src/game/interaction.c', 'omit': ['mario_stop_riding_object', 'mario_grab_used_object', 'mario_drop_held_object', 'mario_throw_held_object', 'mario_stop_riding_and_holding']},
    'object_held_state.c': {'source': 'src/game/object_helpers.c', 'headers': ['host/mario_holding_state.h'], 'functions': ['obj_set_held_state']},
    'mario_transitions.c': {'source': 'src/game/mario.c', 'headers': ['sm64.h', 'game/mario.h', 'game/area.h', 'game/interaction.h', 'engine/math_util.h', 'engine/surface_collision.h'], 'functions': ['mario_floor_is_slope', 'mario_floor_is_steep', 'find_floor_height_relative_polar', 'find_floor_slope', 'set_steep_jump_action', 'set_jump_from_landing', 'set_jumping_action', 'drop_and_set_mario_action', 'hurt_and_set_mario_action', 'check_common_action_exits', 'check_common_hold_action_exits']},

    "mario_status.c": {'source': 'src/game/mario.c',
 'headers': ['host/mario_status_state.h'],
 'functions': ['update_mario_health',
               'update_mario_info_for_cam',
               'mario_reset_bodystate',
               'sink_mario_in_quicksand',
               'sCapFlickerFrames',
               'update_and_return_cap_flags',
               'mario_update_hitbox_and_cap_model']},
    "object_frame.c": {
        "source": "src/game/object_list_processor.c",
        "headers": ["host/object_frame_state.h"],
        "functions": ["update_objects"],
    },
    "debug_stub.c": {
        "source": "src/game/debug.c",
        "headers": ["sm64.h"],
        "functions": ["stub_debug_5"],
    },
    'object_scheduler.c': {'source': 'src/game/object_list_processor.c',
 'headers': ['host/scheduler_state.h'],
 'functions': ['sObjectListUpdateOrder',
               'update_objects_starting_at',
               'update_objects_during_time_stop',
               'update_objects_in_list',
               'unload_deactivated_objects_in_list',
               'set_object_respawn_info_bits',
               'update_terrain_objects',
               'update_non_terrain_objects',
               'unload_deactivated_objects']},
    'object_list_processor.c': {'source': 'src/game/object_list_processor.c',
 'omit': ['update_objects', 'sObjectListUpdateOrder',
          'update_objects_starting_at',
          'update_objects_during_time_stop',
          'update_objects_in_list',
          'unload_deactivated_objects_in_list',
          'set_object_respawn_info_bits',
          'update_terrain_objects',
          'update_non_terrain_objects',
          'unload_deactivated_objects']},
    "behavior_helpers.c": {'source': 'src/game/object_helpers.c',
 'headers': ['host/behavior_helpers.h'],
 'functions': ['cur_obj_has_behavior',
               'cur_obj_hide',
               'cur_obj_move_xz_using_fvel_and_yaw',
               'cur_obj_move_y_with_terminal_vel',
               'cur_obj_scale',
               'obj_angle_to_object',
               'obj_build_transform_relative_to_parent',
               'obj_copy_pos_and_angle',
               'obj_set_face_angle_to_move_angle',
               'obj_set_throw_matrix_from_transform',
               'spawn_object_at_origin',
               'cur_obj_enable_rendering_if_mario_in_room',
               'obj_apply_scale_to_transform',
               'obj_copy_pos',
               'obj_copy_angle',
               'spawn_object',
               'random_f32_around_zero',
               'obj_translate_xz_random',
               'obj_translate_xyz_random',
               'obj_scale',
               'is_item_in_array',
               'sLevelsWithRooms',
               'bhv_init_room',
               'cur_obj_enable_rendering',
               'cur_obj_disable_rendering',
               'spawn_water_droplet']},
    'behavior_script.c': {'source': 'src/engine/behavior_script.c',
 'headers': ['host/behavior_state.h'],
 'functions': ['obj_update_gfx_pos_and_angle',
               'cur_obj_bhv_stack_push',
               'cur_obj_bhv_stack_pop',
               'bhv_cmd_hide',
               'bhv_cmd_disable_rendering',
               'bhv_cmd_billboard',
               'bhv_cmd_set_model',
               'bhv_cmd_spawn_child',
               'bhv_cmd_spawn_obj',
               'bhv_cmd_spawn_child_with_param',
               'bhv_cmd_deactivate',
               'bhv_cmd_break',
               'bhv_cmd_break_unused',
               'bhv_cmd_call',
               'bhv_cmd_return',
               'bhv_cmd_delay',
               'bhv_cmd_delay_var',
               'bhv_cmd_goto',
               'bhv_cmd_begin_repeat_unused',
               'bhv_cmd_begin_repeat',
               'bhv_cmd_end_repeat',
               'bhv_cmd_end_repeat_continue',
               'bhv_cmd_begin_loop',
               'bhv_cmd_end_loop',
               'bhv_cmd_call_native',
               'bhv_cmd_set_float',
               'bhv_cmd_set_int',
               'bhv_cmd_set_int_unused',
               'bhv_cmd_set_random_float',
               'bhv_cmd_set_random_int',
               'bhv_cmd_set_int_rand_rshift',
               'bhv_cmd_add_random_float',
               'bhv_cmd_add_int_rand_rshift',
               'bhv_cmd_add_float',
               'bhv_cmd_add_int',
               'bhv_cmd_or_int',
               'bhv_cmd_bit_clear',
               'bhv_cmd_load_animations',
               'bhv_cmd_animate',
               'bhv_cmd_drop_to_floor',
               'bhv_cmd_nop_1',
               'bhv_cmd_nop_3',
               'bhv_cmd_nop_2',
               'bhv_cmd_sum_float',
               'bhv_cmd_sum_int',
               'bhv_cmd_set_hitbox',
               'bhv_cmd_set_hurtbox',
               'bhv_cmd_set_hitbox_with_offset',
               'bhv_cmd_nop_4',
               'bhv_cmd_begin',
               'bhv_cmd_load_collision_data',
               'bhv_cmd_set_home',
               'bhv_cmd_set_interact_type',
               'bhv_cmd_set_interact_subtype',
               'bhv_cmd_scale',
               'bhv_cmd_set_obj_physics',
               'bhv_cmd_parent_bit_clear',
               'bhv_cmd_spawn_water_droplet',
               'bhv_cmd_animate_texture',
               'stub_behavior_script_2',
               'BehaviorCmdTable',
               'cur_obj_update']},
    'random.c': {'source': 'src/engine/behavior_script.c',
 'headers': ['host/behavior_state.h'],
 'functions': ['random_u16', 'random_float', 'random_sign']},
    "object_math.c": {
        "source": "src/game/object_helpers.c",
        "headers": ["sm64.h"],
        "functions": ["absf", "absi", "linear_mtxf_mul_vec3f", "linear_mtxf_transpose_mul_vec3f"],
    },
    "platform_displacement.c": {
        "source": "src/game/platform_displacement.c",
        "headers": ["host/objects_state.h", "game/platform_displacement.h"],
        "functions": ["update_mario_platform", "get_mario_pos", "set_mario_pos",
                      "apply_platform_displacement", "apply_mario_platform_displacement", "clear_mario_platform"],
    },
    "object_collision.c": {
        "source": "src/game/object_collision.c",
        "headers": ["host/objects_state.h", "game/interaction.h"],
        "functions": ["detect_object_hitbox_overlap", "detect_object_hurtbox_overlap",
                      "clear_object_collision", "check_collision_in_list", "check_player_object_collision",
                      "check_pushable_object_collision", "check_destructive_object_collision",
                      "detect_object_collisions"],
    },
    "object_lifetime.c": {
        "source": "src/game/spawn_object.c",
        "headers": ["host/objects_state.h"],
        "functions": ["try_allocate_object", "deallocate_object", "init_free_object_list",
                      "clear_object_lists", "unload_object", "allocate_object", "snap_object_to_floor",
                      "create_object", "mark_obj_for_deletion"],
    },
    "object_slots.c": {
        "source": "src/game/object_helpers.c",
        "headers": ["host/objects_state.h"],
        "functions": ["find_unimportant_object", "count_unimportant_objects"],
    },
    "memory_pool.c": {
        "source": "src/game/memory.c",
        "headers": ["host/memory_pool.h"],
        "functions": ["mem_pool_init", "mem_pool_alloc", "mem_pool_free"],
    },
    "save.c": {
        "source": "src/game/save_file.c",
        "headers": ["host/save_state.h"],
        "functions": ["gLevelToCourseNumTable", "stub_save_file_1", "calc_checksum",
                      "verify_save_block_signature", "add_save_block_signature",
                      "restore_main_menu_data", "save_main_menu_data", "wipe_main_menu_data",
                      "get_coin_score_age", "set_coin_score_age", "touch_coin_score_age",
                      "touch_high_score_ages", "restore_save_file_data", "save_file_do_save",
                      "save_file_erase", "save_file_copy", "save_file_load_all", "save_file_reload",
                      "save_file_collect_star_or_key", "save_file_exists", "save_file_get_max_coin_score",
                      "save_file_get_course_star_count", "save_file_get_total_star_count",
                      "save_file_set_flags", "save_file_clear_flags", "save_file_get_flags",
                      "save_file_get_star_flags", "save_file_set_star_flags", "save_file_get_course_coin_score",
                      "save_file_is_cannon_unlocked", "save_file_set_cannon_unlocked",
                      "save_file_set_cap_pos", "save_file_get_cap_pos", "save_file_set_sound_mode", "save_file_get_sound_mode",
                      "save_file_move_cap_to_default_location", "disable_warp_checkpoint",
                      "check_if_should_set_warp_checkpoint", "check_warp_checkpoint"],
    },
    "mario_sound.c": {
        "source": "src/game/mario.c",
        "headers": ["sm64.h", "game/mario.h", "audio/external.h"],
        "functions": ["play_sound_if_no_flag", "play_mario_jump_sound", "adjust_sound_for_speed",
                      "play_sound_and_spawn_particles", "play_mario_action_sound", "play_mario_landing_sound",
                      "play_mario_landing_sound_once", "play_mario_heavy_landing_sound",
                      "play_mario_heavy_landing_sound_once", "play_mario_sound"],
    },
    "object_animation_query.c": {
        "source": "src/game/object_helpers.c",
        "headers": ["host/object_context.h"],
        "functions": ["cur_obj_check_anim_frame", "cur_obj_check_anim_frame_in_range"],
    },
    "sound_control.c": {
        "source": "src/game/sound_init.c",
        "headers": ["host/audio_state.h"],
        "functions": ["sSoundMenuModeToSoundMode", "set_sound_mode",
                      "reset_volume", "lower_background_noise", "raise_background_noise",
                      "disable_background_sound", "enable_background_sound",
                      "fadeout_music", "fadeout_level_music", "play_cutscene_music",
                      "play_shell_music", "stop_shell_music", "play_cap_music",
                      "fadeout_cap_music", "stop_cap_music"],
    },
    "object_sound.c": {
        "source": "src/game/spawn_sound.c",
        "headers": ["sm64.h", "audio/external.h", "game/object_list_processor.h",
                    "game/object_helpers.h", "game/spawn_sound.h"],
        "functions": ["exec_anim_sound_state", "cur_obj_play_sound_1", "cur_obj_play_sound_2"],
    },
    "sound_spawner.c": {
        "source": "src/game/spawn_sound.c",
        "headers": ["sm64.h", "behavior_data.h", "game/object_list_processor.h", "game/object_helpers.h"],
        "functions": ["create_sound_spawner"],
    },
    "animation_loader.c": {
        "source": "src/game/memory.c",
        "headers": ["sm64.h", "host/animation_bank.h"],
        "functions": ["load_patchable_table"],
    },
    "mario_animation.c": {
        "source": "src/game/mario.c",
        "headers": ["sm64.h", "game/mario.h", "game/memory.h", "engine/graph_node.h", "engine/math_util.h"],
        "functions": ["is_anim_at_end", "is_anim_past_end", "set_mario_animation", "set_mario_anim_with_accel",
                      "set_anim_to_frame", "is_anim_past_frame", "find_mario_anim_flags_and_translation",
                      "update_mario_pos_for_anim", "return_mario_anim_y_translation"],
    },
    "mario_action_setup.c": {
        "source": "src/game/mario.c",
        "headers": ["sm64.h", "game/mario.h", "game/mario_step.h", "engine/math_util.h"],
        "functions": ["set_mario_y_vel_based_on_fspeed", "set_mario_action_airborne",
                      "set_mario_action_moving", "set_mario_action_submerged",
                      "set_mario_action_cutscene", "set_mario_action"],
    },
    "mario_surface_class.c": {
        "source": "src/game/mario.c",
        "headers": ["sm64.h", "game/mario.h", "game/area.h", "engine/math_util.h"],
        "functions": ["mario_get_floor_class", "mario_floor_is_slippery",
                      "mario_facing_downhill", "mario_set_forward_vel"],
    },
    "controller.c": {
        "source": "src/game/game_init.c",
        "headers": ["sm64.h"],
        "functions": ["adjust_analog_stick"],
    },
    "mario_input.c": {
        "source": "src/game/mario.c",
        "headers": ["sm64.h", "game/mario.h", "game/area.h", "game/camera.h", "engine/math_util.h"],
        "functions": ["update_mario_button_inputs", "update_mario_joystick_inputs"],
    },
    "mario_ground.c": {
        "source": "src/game/mario_step.c",
        "headers": ["host/terrain_state.h", "engine/math_util.h"],
        "functions": ["get_additive_y_vel_for_jumps", "stub_mario_step_1", "perform_ground_quarter_step", "perform_ground_step",
                      "check_ledge_grab", "perform_air_quarter_step", "apply_vertical_wind", "perform_air_step",
                      "apply_twirl_gravity", "should_strengthen_gravity_for_jump_ascent",
                      "apply_gravity", "set_vel_from_pitch_and_yaw", "set_vel_from_yaw"],
    },
    "mario_step.c": {
        "source": "src/game/mario_step.c",
        "omit": ["get_additive_y_vel_for_jumps", "stub_mario_step_1", "perform_ground_quarter_step", "perform_ground_step",
                 "check_ledge_grab", "perform_air_quarter_step", "apply_vertical_wind", "perform_air_step",
                 "apply_twirl_gravity", "should_strengthen_gravity_for_jump_ascent",
                 "apply_gravity", "set_vel_from_pitch_and_yaw", "set_vel_from_yaw"],
    },
    "mario_collision_helpers.c": {
        "source": "src/game/mario.c",
        "headers": ["host/terrain_state.h", "level_table.h", "engine/math_util.h"],
        "functions": ["sTerrainSounds", "mario_get_terrain_sound_addend",
                      "resolve_and_return_wall_collisions", "vec3f_find_ceil"],
    },
    "mario_geometry_input.c": {
        "source": "src/game/mario.c",
        "headers": ["host/terrain_state.h", "engine/math_util.h"],
        "functions": ["update_mario_geometry_inputs", "update_mario_inputs"],
    },
    "mario.c": {
        "source": "src/game/mario.c",
        "omit": ['mario_floor_is_slope', 'mario_floor_is_steep', 'find_floor_height_relative_polar', 'find_floor_slope', 'set_steep_jump_action', 'set_jump_from_landing', 'set_jumping_action', 'drop_and_set_mario_action', 'hurt_and_set_mario_action', 'check_common_action_exits', 'check_common_hold_action_exits', 'update_mario_health', 'update_mario_info_for_cam', 'mario_reset_bodystate', 'sink_mario_in_quicksand', 'sCapFlickerFrames', 'update_and_return_cap_flags', 'mario_update_hitbox_and_cap_model', "play_sound_if_no_flag", "play_mario_jump_sound", "adjust_sound_for_speed",
                 "play_sound_and_spawn_particles", "play_mario_action_sound", "play_mario_landing_sound",
                 "play_mario_landing_sound_once", "play_mario_heavy_landing_sound",
                 "play_mario_heavy_landing_sound_once", "play_mario_sound",
                 "is_anim_at_end", "is_anim_past_end", "set_mario_animation", "set_mario_anim_with_accel",
                 "set_anim_to_frame", "is_anim_past_frame", "find_mario_anim_flags_and_translation",
                 "update_mario_pos_for_anim", "return_mario_anim_y_translation",
                 "sTerrainSounds", "mario_get_terrain_sound_addend", "resolve_and_return_wall_collisions",
                 "vec3f_find_ceil", "update_mario_button_inputs", "update_mario_joystick_inputs",
                 "mario_get_floor_class", "mario_floor_is_slippery", "update_mario_geometry_inputs", "update_mario_inputs",
                 "mario_facing_downhill", "mario_set_forward_vel", "set_mario_y_vel_based_on_fspeed",
                 "set_mario_action_airborne", "set_mario_action_moving", "set_mario_action_submerged",
                 "set_mario_action_cutscene", "set_mario_action"],
    },
    "area_terrain.c": {
        "source": "src/engine/surface_load.c",
        "omit": ["clear_dynamic_surfaces", "transform_object_vertices",
                 "load_object_surfaces", "load_object_collision_model"],
    },
    "terrain_load.c": {
        "source": "src/engine/surface_load.c",
        "headers": ["host/terrain_state.h"],
        "functions": ["alloc_surface_node", "alloc_surface", "add_surface_to_cell",
                      "min_3", "max_3", "lower_cell_index", "upper_cell_index",
                      "add_surface", "read_surface_data", "surface_has_force",
                      "surf_has_no_cam_collision", "clear_spatial_partition",
                      "clear_dynamic_surfaces", "transform_object_vertices",
                      "load_object_surfaces", "load_object_collision_model"],
        "footer": "host/terrain.c",
    },
    "object_transforms.c": {
        "source": "src/game/object_helpers.c",
        "headers": ["sm64.h", "engine/math_util.h", "game/object_helpers.h"],
        "functions": ["obj_apply_scale_to_matrix", "dist_between_objects",
                      "obj_build_transform_from_pos_and_angle"],
    },
    "object_helpers.c": {
        "source": "src/game/object_helpers.c",
        "omit": ['obj_set_held_state', 'cur_obj_has_behavior', 'cur_obj_hide', 'cur_obj_move_xz_using_fvel_and_yaw', 'cur_obj_move_y_with_terminal_vel', 'cur_obj_scale', 'obj_angle_to_object', 'obj_build_transform_relative_to_parent', 'obj_copy_pos_and_angle', 'obj_set_face_angle_to_move_angle', 'obj_set_throw_matrix_from_transform', 'spawn_object_at_origin', 'cur_obj_enable_rendering_if_mario_in_room', 'obj_apply_scale_to_transform', 'obj_copy_pos', 'obj_copy_angle', 'spawn_object', 'random_f32_around_zero', 'obj_translate_xz_random', 'obj_translate_xyz_random', 'obj_scale', 'is_item_in_array', 'sLevelsWithRooms', 'bhv_init_room', 'cur_obj_enable_rendering', 'cur_obj_disable_rendering', 'spawn_water_droplet', "absf", "absi", "linear_mtxf_mul_vec3f", "linear_mtxf_transpose_mul_vec3f",
                 "find_unimportant_object", "count_unimportant_objects",
                 "cur_obj_check_anim_frame", "cur_obj_check_anim_frame_in_range",
                 "obj_apply_scale_to_matrix", "dist_between_objects",
                 "obj_build_transform_from_pos_and_angle"],
    },
    "terrain_queries.c": {
        "source": "src/engine/surface_collision.c",
        "headers": ["host/terrain_state.h"],
        "functions": ["find_wall_collisions_from_list", "f32_find_wall_collision",
                      "find_wall_collisions", "find_ceil_from_list", "find_ceil",
                      "unused_obj_find_floor_height", "find_floor_height_and_data",
                      "find_floor_from_list", "find_floor_height", "unused_find_dynamic_floor",
                      "find_floor", "find_water_level", "find_poison_gas_level",
                      "unused_resolve_floor_or_ceil_collisions"],
    },
    "main_pool.c": {
        "source": "src/game/memory.c",
        "headers": ["host/main_pool.h"],
        "functions": ["main_pool_init", "main_pool_alloc", "main_pool_free",
                      "main_pool_realloc", "main_pool_available", "main_pool_push_state",
                      "main_pool_pop_state", "alloc_only_pool_init", "alloc_only_pool_resize"],
    },
    "math_util.c": {
        "source": "src/engine/math_util.c",
        "omit": ["mtxf_to_mtx", "mtxf_rotate_xy"],
    },
    "object_nodes.c": {
        "source": "src/engine/graph_node.c",
        "headers": ["sm64.h", "engine/graph_node.h", "engine/math_util.h",
                    "engine/geo_layout.h", "game/area.h", "game/memory.h"],
        "functions": ["init_scene_graph_node_links", "init_graph_node_object",
                      "geo_add_child", "geo_remove_child", "geo_make_first_child",
                      "geo_reset_object_node", "geo_obj_init", "geo_obj_init_spawninfo"],
    },
    "animation.c": {
        "source": "src/engine/graph_node.c",
        "headers": ["sm64.h", "engine/graph_node.h", "game/area.h", "game/memory.h"],
        "functions": ["geo_obj_init_animation", "geo_obj_init_animation_accel",
                      "retrieve_animation_index", "geo_update_animation_frame"],
    },
}

def function_range(data, name):
    # Blank comments and literals without changing offsets. Braces in comments
    # and strings must not terminate a function body.
    token = rb'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    masked = re.sub(token, lambda m: bytes(10 if b == 10 else 32 for b in m[0]),
                    data, flags=re.S)
    pattern = rb'^[A-Za-z_][^\n;{}]*\b' + name.encode() + rb'\s*\([^;{}]*\)\s*\{'
    matches = list(re.finditer(pattern, masked, re.M))
    if not matches:
        # Preserve initialized tables as complete declarations too. This is
        # needed for dependencies such as Mario's terrain sound lookup table.
        pattern = rb'^[A-Za-z_][^\n;{}]*\b' + name.encode() + rb'\s*\[[^;{}]*=\s*\{'
        matches = list(re.finditer(pattern, masked, re.M))
    if len(matches) != 1:
        scalar = rb'^[A-Za-z_][^\n;{}]*\b' + name.encode() + rb'\s*=\s*[^;{}]+;'
        scalars = list(re.finditer(scalar, masked, re.M))
        if len(scalars) == 1:
            return scalars[0].span()
        raise ValueError(f"Expected one definition of {name}, found {len(matches)}")
    start = matches[0].start()
    depth = 1
    end = matches[0].end()
    while depth and end < len(masked):
        depth += (masked[end] == ord('{')) - (masked[end] == ord('}'))
        end += 1
    if depth:
        raise ValueError(f"Unterminated function {name}")
    if end < len(masked) and masked[end] == ord(';'):
        end += 1
    return start, end

def main():
    checkout = Path(sys.argv[1]).resolve()
    upstream = json.loads((ROOT / "upstream.json").read_text())
    revision = subprocess.check_output(
        ["git", "-C", str(checkout), "rev-parse", "HEAD"], text=True).strip()
    if revision != upstream["revision"]:
        raise ValueError("Checkout does not match the pinned upstream revision")
    outputs = {}
    for output, spec in EXTRACTIONS.items():
        # Read committed bytes, not potentially edited working-tree files.
        data = subprocess.check_output(
            ["git", "-C", str(checkout), "show", f"{revision}:{spec['source']}"])
        contents = b"/* Generated verbatim upstream function extraction. See extracted.json. */\n"
        if "omit" in spec:
            excluded = sorted(function_range(data, name) for name in spec["omit"])
            copied = []
            previous = 0
            contents += f'#line 1 "n64decomp/{spec["source"]}"\n'.encode()
            for start, end in excluded + [(len(data), len(data))]:
                body = data[previous:start]
                contents += body
                copied.append({"start": previous, "end": start,
                               "sha256": hashlib.sha256(body).hexdigest()})
                contents += b"\n" * data[start:end].count(b"\n")
                previous = end
            (ROOT / "extracted").mkdir(exist_ok=True)
            (ROOT / "extracted" / output).write_bytes(contents)
            outputs[output] = {"source": spec["source"],
                               "source_sha256": hashlib.sha256(data).hexdigest(),
                               "sha256": hashlib.sha256(contents).hexdigest(),
                               "copied_ranges": copied, "omitted_functions": spec["omit"],
                               "functions": {}}
            continue
        for header in spec["headers"]:
            if not (ROOT / "upstream" / "include" / header).exists() and not (
                    ROOT / "upstream" / "src" / header).exists() and not (ROOT / header).exists():
                raise ValueError(f"Missing imported header {header}")
            contents += f'#include "{header}"\n'.encode()
        functions = {}
        for name in spec["functions"]:
            start, end = function_range(data, name)
            body = data[start:end]
            line = data[:start].count(b"\n") + 1
            contents += f'\n#line {line} "n64decomp/{spec["source"]}"\n'.encode() + body + b"\n"
            functions[name] = {"start": start, "end": end,
                               "sha256": hashlib.sha256(body).hexdigest()}
        if "footer" in spec:
            contents += f'\n#include "{spec["footer"]}"\n'.encode()
        (ROOT / "extracted").mkdir(exist_ok=True)
        (ROOT / "extracted" / output).write_bytes(contents)
        outputs[output] = {"source": spec["source"],
                           "source_sha256": hashlib.sha256(data).hexdigest(),
                           "sha256": hashlib.sha256(contents).hexdigest(),
                           "functions": functions}
    (ROOT / "extracted.json").write_text(json.dumps({"revision": revision, "files": outputs}, indent=2) + "\n")

if __name__ == "__main__":
    main()
