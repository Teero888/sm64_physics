cmake_minimum_required(VERSION 3.22)
project(sm64_physics LANGUAGES C)

# The game: game/ holds its code, imported from n64decomp/sm64 (tools/vendor.py)
# and the library's own from then on. One library per game version.
set(SM64_VERSION jp CACHE STRING "Game version: jp, us or eu")
# Shared: one copy of the game for a program and its plugins, as a world holds
# addresses of the library's code.
option(SM64_PHYSICS_SHARED "Build sm64_physics as a shared library" OFF)
set_property(CACHE SM64_VERSION PROPERTY STRINGS jp us eu)
# A program with several versions adds this directory once per version, each
# time with its own suffix for the targets (sm64_physics_us, ...).
set(S "${SM64_TARGET_SUFFIX}")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(G "${CMAKE_CURRENT_SOURCE_DIR}/game")
# Generated from the decomp's own sources for this version (text, headers).
set(GV "${G}/gen/${SM64_VERSION}")

set(SM64_SOURCE_DIRS src/engine src/game src/menu src/audio src/buffers src/goddard src/goddard/dynlists actors bin data)
set(SM64_SOURCES)
foreach(dir ${SM64_SOURCE_DIRS})
  file(GLOB found "${G}/${dir}/*.c")
  list(APPEND SM64_SOURCES ${found})
endforeach()
file(GLOB level_dirs LIST_DIRECTORIES true "${G}/levels/*")
foreach(level ${level_dirs})
  if(EXISTS "${level}/header.h")
    foreach(part leveldata script geo)
      if(EXISTS "${level}/${part}.c")
        list(APPEND SM64_SOURCES "${level}/${part}.c")
      endif()
    endforeach()
  endif()
endforeach()
file(GLOB skyboxes "${GV}/bin/*_skybox.c")
list(APPEND SM64_SOURCES "${G}/levels/entry.c" "${G}/levels/scripts.c"
     "${GV}/assets/mario_anim_data.c" "${GV}/assets/demo_data.c" ${skyboxes})
# The SDK's own C: matrix helpers and the audio bank loader.
foreach(ultra guMtxF2L guOrthoF guPerspectiveF guRotateF guScaleF guTranslateF guLookAtRef guNormalize alBnkfNew)
  list(APPEND SM64_SOURCES "${G}/lib/src/${ultra}.c")
endforeach()
# EU: its text in three languages, and its own audio port.
if(SM64_VERSION STREQUAL "eu")
  file(GLOB translations "${G}/bin/eu/*.c")
  list(APPEND SM64_SOURCES ${translations})
else()
  list(FILTER SM64_SOURCES EXCLUDE REGEX "/port_eu\\.c$")
endif()
# Other versions' files and the iQue's.
list(FILTER SM64_SOURCES EXCLUDE REGEX "/(cn_common_syms_[12]|crash_screen|audio_session_presets_sh|load_sh|port_sh|synthesis_sh|shindou_debug_prints|sound_data)\\.c$")
# The RSP microcode the version's display lists are for.
if(SM64_VERSION STREQUAL "eu")
  set(SM64_MICROCODE F3D_NEW)
else()
  set(SM64_MICROCODE F3D_OLD)
endif()

# What the decomp extracts from the ROM is not part of the library: the
# textures are stand-ins of their size that say where their pixels are
# (game/rom_assets/<version>.tsv lists them, drawing reads them from the
# user's ROM), the sound banks are empty.
set(SM64_ROM_STUBS "${CMAKE_CURRENT_BINARY_DIR}/rom_stubs")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/rom_stubs.py"
          "${G}/rom_assets/${SM64_VERSION}.tsv" "${SM64_ROM_STUBS}"
  RESULT_VARIABLE stubs_result)
if(NOT stubs_result EQUAL 0)
  message(FATAL_ERROR "tools/rom_stubs.py failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${G}/rom_assets/${SM64_VERSION}.tsv"
             "${CMAKE_CURRENT_SOURCE_DIR}/tools/rom_stubs.py")
list(APPEND SM64_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/platform/sound_data.c")

# libultra's sinf and cosf, which the game called and whose results differ from
# the host's libm, under other names (see platform/ultra_math.h). Built apart:
# they must not see the host's <math.h>, whose NAN they would clash with.
add_library(sm64_ultra_math${S} OBJECT "${G}/lib/src/math/sinf.c" "${G}/lib/src/math/cosf.c" "${G}/lib/src/NaN.c")
target_compile_definitions(sm64_ultra_math${S} PRIVATE sinf=ultra_sinf cosf=ultra_cosf NAN=ultra_nan)
target_include_directories(sm64_ultra_math${S} PRIVATE "${G}/include" "${G}/lib/src")
target_compile_options(sm64_ultra_math${S} PRIVATE -fwrapv -ffp-contract=off -fno-fast-math -fPIC -fvisibility=protected -w)

# The N64 stack pointer model (platform/n64stack.h): the frames of the
# functions from which the N64 can reach the place where the original leaks a
# stack address, from tools/n64stack/<version>.tsv (the other versions' tables
# name the functions with no frame in this one).
set(SM64_N64STACK_DIR "${CMAKE_CURRENT_BINARY_DIR}/n64stack")
file(GLOB n64stack_tables "${CMAKE_CURRENT_SOURCE_DIR}/tools/n64stack/*.tsv")
set(n64stack_table "${CMAKE_CURRENT_SOURCE_DIR}/tools/n64stack/${SM64_VERSION}.tsv")
list(REMOVE_ITEM n64stack_tables "${n64stack_table}")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/n64stack/frames.py" header
          "${SM64_N64STACK_DIR}/n64_frames.h" "${n64stack_table}" ${n64stack_tables}
  RESULT_VARIABLE n64stack_result)
if(NOT n64stack_result EQUAL 0)
  message(FATAL_ERROR "tools/n64stack/frames.py failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/n64stack/frames.py"
             "${n64stack_table}" ${n64stack_tables})

string(TOUPPER "${SM64_VERSION}" SM64_VERSION_UPPER)
# The game's headers and definitions, for game sources and the tools that
# read the game's variables.
function(sm64_game_headers target)
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${G}" "${GV}"
                             "${GV}/include" "${SM64_ROM_STUBS}" "${G}/include" "${G}/src"
                             "${CMAKE_CURRENT_SOURCE_DIR}/platform" "${SM64_N64STACK_DIR}")
  target_compile_definitions(${target} PRIVATE
    VERSION_${SM64_VERSION_UPPER}=1 ${SM64_MICROCODE}=1 _FINALROM=1 _LANGUAGE_C
    # Required by the decomp for non-N64 builds. Every AVOID_UB site changes
    # behavior and has to be checked against the N64 (see docs/avoid_ub.md).
    NON_MATCHING AVOID_UB
    NO_SEGMENTED_MEMORY)
endfunction()

# How every game source is compiled.
function(sm64_game_settings target)
  sm64_game_headers(${target})
  # Bit parity: IEEE single precision without fused or reordered arithmetic,
  # wrapping signed integers as on the N64, signed chars as IDO's -signed.
  # Protected visibility: the shared library exports the whole game, for
  # plugins that read or call it directly, but binds the game's own references
  # to itself, so nothing of the program or a plugin interposes on them.
  target_compile_options(${target} PRIVATE -std=gnu11 -fsigned-char -fwrapv -fno-strict-aliasing
    -ffp-contract=off -fno-fast-math -fPIC -fvisibility=protected -w
    "SHELL:-include \"${CMAKE_CURRENT_SOURCE_DIR}/platform/ultra_math.h\""
    "SHELL:-include \"${CMAKE_CURRENT_SOURCE_DIR}/platform/draw.h\""
    "SHELL:-include \"${CMAKE_CURRENT_SOURCE_DIR}/platform/world.h\""
    "SHELL:-include \"${CMAKE_CURRENT_SOURCE_DIR}/platform/n64stack.h\"")
endfunction()

# Sources whose variables the N64 resets when it loads their segment from ROM
# again: linked as one object whose data and bss sections have their own
# names (PREFIX_data, PREFIX_datarel, PREFIX_datarel2, PREFIX_bss), so the
# host finds and restores exactly those variables. Sets <NAME>_OBJECT.
function(sm64_section_group name prefix)
  add_library(${name}_objects OBJECT ${ARGN})
  sm64_game_settings(${name}_objects)
  set(object "${CMAKE_CURRENT_BINARY_DIR}/${name}.o")
  add_custom_command(OUTPUT "${object}"
    COMMAND ${CMAKE_LINKER} -r -o "${object}.tmp" $<TARGET_OBJECTS:${name}_objects>
    COMMAND ${CMAKE_OBJCOPY} --rename-section .data=${prefix}_data --rename-section .data.rel.local=${prefix}_datarel
            --rename-section .data.rel=${prefix}_datarel2 --rename-section .bss=${prefix}_bss "${object}.tmp" "${object}"
    DEPENDS $<TARGET_OBJECTS:${name}_objects> COMMAND_EXPAND_LISTS VERBATIM)
  set_source_files_properties("${object}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
  set(${name}_OBJECT "${object}" PARENT_SCOPE)
endfunction()

# src/menu and src/goddard are one segment, loaded by FIXED_LOAD every time a
# menu or the title screen starts (host_reload_overlay, docs/changes.md 6).
set(SM64_OVERLAY_SOURCES ${SM64_SOURCES})
list(FILTER SM64_OVERLAY_SOURCES INCLUDE REGEX "/src/(menu|goddard)/")
list(REMOVE_ITEM SM64_SOURCES ${SM64_OVERLAY_SOURCES})
sm64_section_group(sm64_overlay${S} sm64ovl ${SM64_OVERLAY_SOURCES})
set(SM64_OVERLAY_OBJECT "${sm64_overlay${S}_OBJECT}")

# Each level's data is segment 7, decompressed by LOAD_MIO0 on every level load
# (host_reload_level_data, docs/changes.md 8).
set(SM64_LEVEL_DATA_SOURCES ${SM64_SOURCES})
list(FILTER SM64_LEVEL_DATA_SOURCES INCLUDE REGEX "/levels/[^/]+/leveldata\\.c$")
list(REMOVE_ITEM SM64_SOURCES ${SM64_LEVEL_DATA_SOURCES})
sm64_section_group(sm64_level_data${S} sm64lvl ${SM64_LEVEL_DATA_SOURCES})

# Everything else, with the host's stand-ins for the console: together with
# the two groups above, the game's whole state (sm64_save_state).
# The RSP's output buffer is no state: shared by the process, with what only
# it keeps (platform/world.c). (The framebuffers and z-buffer are: the Mario
# head on the title screen uses them as heap.)
set(SM64_SHARED_SOURCES ${SM64_SOURCES})
list(FILTER SM64_SHARED_SOURCES INCLUDE REGEX "/src/buffers/gfx_output_buffer\\.c$")
list(REMOVE_ITEM SM64_SOURCES ${SM64_SHARED_SOURCES})
add_library(sm64_shared_objects${S} OBJECT ${SM64_SHARED_SOURCES} platform/world.c platform/n64stack.c platform/api.c platform/rom.c platform/os.c
            "${GV}/pointers/types.c")
sm64_game_settings(sm64_shared_objects${S})
sm64_section_group(sm64_main${S} sm64st ${SM64_SOURCES} platform/ultra.c platform/host.c)

# The linker script: a COFF object (Windows) cannot hold a section aligned to
# more than 8 KiB, and a page is all that protecting the template needs.
set(SM64_STATE_LD "${CMAKE_CURRENT_SOURCE_DIR}/platform/state.ld")
if(WIN32)
  file(READ "${SM64_STATE_LD}" state_ld)
  string(REPLACE "ALIGN(65536)" "ALIGN(4096)" state_ld "${state_ld}")
  set(SM64_STATE_LD "${CMAKE_CURRENT_BINARY_DIR}/state.ld")
  file(WRITE "${SM64_STATE_LD}" "${state_ld}")
  # With a script, MinGW's linker would write an image, which is no object.
  set(SM64_STATE_LD_FORMAT --oformat pe-x86-64)
endif()

# The library: include/sm64_physics.h is its whole interface.
# The three groups' variables as one section, sm64_state (platform/state.ld).
set(SM64_GAME_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/sm64_game.o")
add_custom_command(OUTPUT "${SM64_GAME_OBJECT}"
  COMMAND ${CMAKE_LINKER} -r ${SM64_STATE_LD_FORMAT} -T "${SM64_STATE_LD}" -o "${SM64_GAME_OBJECT}"
          "${sm64_main${S}_OBJECT}" "${SM64_OVERLAY_OBJECT}" "${sm64_level_data${S}_OBJECT}"
  DEPENDS "${sm64_main${S}_OBJECT}" "${SM64_OVERLAY_OBJECT}" "${sm64_level_data${S}_OBJECT}"
          "${SM64_STATE_LD}" VERBATIM)
set_source_files_properties("${SM64_GAME_OBJECT}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
# The addresses among the state's initial values, which move with every copy.
set(SM64_STATE_POINTERS "${CMAKE_CURRENT_BINARY_DIR}/state_pointers.c")
add_custom_command(OUTPUT "${SM64_STATE_POINTERS}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/state/init_pointers.py" "${SM64_GAME_OBJECT}"
          "${SM64_STATE_POINTERS}"
  DEPENDS "${SM64_GAME_OBJECT}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/state/init_pointers.py" VERBATIM)
if(SM64_PHYSICS_SHARED)
  set(SM64_PHYSICS_TYPE SHARED)
else()
  set(SM64_PHYSICS_TYPE STATIC)
endif()
add_library(sm64_physics${S} ${SM64_PHYSICS_TYPE} $<TARGET_OBJECTS:sm64_ultra_math${S}> $<TARGET_OBJECTS:sm64_shared_objects${S}>
            "${SM64_GAME_OBJECT}" "${SM64_STATE_POINTERS}")
set_source_files_properties("${SM64_STATE_POINTERS}" PROPERTIES COMPILE_OPTIONS "-fPIC;-fvisibility=protected")
set_target_properties(sm64_physics${S} PROPERTIES LINKER_LANGUAGE C)
target_include_directories(sm64_physics${S} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
add_dependencies(sm64_physics${S} sm64_main${S}_objects sm64_overlay${S}_objects sm64_level_data${S}_objects)
find_package(Threads REQUIRED)
target_link_libraries(sm64_physics${S} PUBLIC m Threads::Threads)

# The display lists' format (sm64_step_draw), for a renderer: the decomp's
# gbi.h with the library's definitions.
add_library(sm64_physics_gbi${S} INTERFACE)
target_include_directories(sm64_physics_gbi${S} INTERFACE "${G}/include")
target_compile_definitions(sm64_physics_gbi${S} INTERFACE ${SM64_MICROCODE}=1 _LANGUAGE_C)

# The tools read the game's variables by symbol in their own executable: they
# link the static library.
if(NOT SM64_PHYSICS_SHARED)
add_executable(sm64_run tools/run.c tools/elf_symbols.c)
sm64_game_headers(sm64_run)
target_link_libraries(sm64_run PRIVATE sm64_physics${S})

# The lockstep comparator: the oracle's emulator frontend with the native game
# linked in, compared member by member at every frame. Needs mupen64plus.
find_path(MUPEN64PLUS_INCLUDE_DIR m64p_frontend.h PATH_SUFFIXES mupen64plus)
if(MUPEN64PLUS_INCLUDE_DIR)
  # Struct layouts on the N64 (32-bit, 8-byte alignment for 64-bit types) and
  # natively, read back out of DWARF.
  set(SM64_LAYOUT "${CMAKE_CURRENT_BINARY_DIR}/layout")
  set(SM64_LAYOUT_FLAGS -D_LANGUAGE_C -DVERSION_${SM64_VERSION_UPPER}=1 -DF3D_OLD=1 -D_FINALROM=1 -DNON_MATCHING
      -DAVOID_UB -I${G}/include -I${GV} -I${GV}/include -I${G}/src -I${G} -g -gdwarf-4
      -fno-eliminate-unused-debug-types -c)
  set(SM64_LAYOUT_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tools/layout/layout_types.c")
  file(GLOB_RECURSE SM64_LAYOUT_HEADERS "${G}/include/*.h" "${G}/src/*.h")
  add_custom_command(OUTPUT "${SM64_LAYOUT}/layout_tables.h"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SM64_LAYOUT}"
    COMMAND ${CMAKE_C_COMPILER} -m32 -malign-double -nostdinc -DTARGET_N64 -I${G}/include/libc ${SM64_LAYOUT_FLAGS}
            "${SM64_LAYOUT_SOURCE}" -o "${SM64_LAYOUT}/n64.o"
    COMMAND ${CMAKE_C_COMPILER} -DNO_SEGMENTED_MEMORY ${SM64_LAYOUT_FLAGS} "${SM64_LAYOUT_SOURCE}" -o "${SM64_LAYOUT}/native.o"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/layout/layout.py" "${SM64_LAYOUT}/n64.o"
            "${SM64_LAYOUT}/native.o" "${SM64_LAYOUT}/layout_tables.h"
    DEPENDS "${SM64_LAYOUT_SOURCE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/layout/layout.py" ${SM64_LAYOUT_HEADERS} VERBATIM)

  add_library(oracle_video MODULE oracle/plugin_video.c)
  add_library(oracle_input MODULE oracle/plugin_input.c)
  set_target_properties(oracle_video oracle_input PROPERTIES PREFIX "" C_VISIBILITY_PRESET hidden)
  add_executable(sm64_lockstep oracle/oracle.c oracle/movie.c tools/lockstep/lockstep.c tools/elf_symbols.c
                 "${SM64_LAYOUT}/layout_tables.h")
  foreach(target oracle_video oracle_input sm64_lockstep)
    target_include_directories(${target} PRIVATE "${MUPEN64PLUS_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/oracle")
  endforeach()
  target_include_directories(sm64_lockstep PRIVATE "${SM64_LAYOUT}" "${CMAKE_CURRENT_SOURCE_DIR}/tools"
                             "${CMAKE_CURRENT_SOURCE_DIR}/tools/lockstep")
  target_compile_definitions(sm64_lockstep PRIVATE SM64_LOCKSTEP
    SM64_ORACLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/oracle" SM64_VERSION_NAME="${SM64_VERSION}")
  sm64_game_headers(sm64_lockstep)
  target_link_libraries(sm64_lockstep PRIVATE sm64_physics${S} ${CMAKE_DL_LIBS})
  add_dependencies(sm64_lockstep oracle_video oracle_input)
endif()
endif()
