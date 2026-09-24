cmake_minimum_required(VERSION 3.22)
project(sm64_physics LANGUAGES C)

# The game is the n64decomp/sm64 source compiled natively. SM64_DECOMP_DIR is
# a checkout of it (at the pinned commit) that has been built once for
# SM64_VERSION from the user's ROM: that build generates the assets the game
# data includes (animations, demo inputs, text, textures, sound banks).
set(SM64_DECOMP_DIR "$ENV{HOME}/software/sm64-decomp" CACHE PATH "n64decomp/sm64 checkout built for SM64_VERSION")
set(SM64_VERSION jp CACHE STRING "Game version: jp or us")
set_property(CACHE SM64_VERSION PROPERTY STRINGS jp us)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(D "${SM64_DECOMP_DIR}")
set(GEN "${D}/build/${SM64_VERSION}")
if(NOT EXISTS "${GEN}/assets/mario_anim_data.c")
  message(FATAL_ERROR "${GEN} has no generated assets: build the decomp with `make VERSION=${SM64_VERSION}` first")
endif()

set(SM64_SOURCE_DIRS src/engine src/game src/menu src/audio src/buffers src/goddard src/goddard/dynlists actors bin data sound)
set(SM64_SOURCES)
foreach(dir ${SM64_SOURCE_DIRS})
  file(GLOB found "${D}/${dir}/*.c")
  list(APPEND SM64_SOURCES ${found})
endforeach()
file(GLOB level_dirs LIST_DIRECTORIES true "${D}/levels/*")
foreach(level ${level_dirs})
  if(EXISTS "${level}/header.h")
    foreach(part leveldata script geo)
      if(EXISTS "${level}/${part}.c")
        list(APPEND SM64_SOURCES "${level}/${part}.c")
      endif()
    endforeach()
  endif()
endforeach()
list(APPEND SM64_SOURCES "${D}/levels/entry.c" "${D}/levels/scripts.c"
     "${GEN}/assets/mario_anim_data.c" "${GEN}/assets/demo_data.c")
# Sound banks and sequences in the host's layout, in place of the N64's.
set(SM64_NATIVE_SOUND "${CMAKE_CURRENT_BINARY_DIR}/native_sound")
add_custom_command(OUTPUT "${SM64_NATIVE_SOUND}/sound_data.c"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/native_sound.py" "${D}" ${SM64_VERSION}
          "${SM64_NATIVE_SOUND}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/native_sound.py" VERBATIM)
list(REMOVE_ITEM SM64_SOURCES "${D}/sound/sound_data.c")
list(APPEND SM64_SOURCES "${SM64_NATIVE_SOUND}/sound_data.c")
file(GLOB skyboxes "${GEN}/bin/*_skybox.c")
list(APPEND SM64_SOURCES ${skyboxes})
# The SDK's own C: matrix helpers and the audio bank loader.
foreach(ultra guMtxF2L guOrthoF guPerspectiveF guRotateF guScaleF guTranslateF guLookAtRef guNormalize alBnkfNew)
  list(APPEND SM64_SOURCES "${D}/lib/src/${ultra}.c")
endforeach()
# Other versions' files and the iQue's.
list(FILTER SM64_SOURCES EXCLUDE REGEX "/(cn_common_syms_[12]|crash_screen|audio_session_presets_sh|load_sh|port_sh|synthesis_sh|shindou_debug_prints|port_eu)\\.c$")

# Every change to the decomp is a patch in patches/, applied to copies of the
# touched files in the build tree. The patched copies are compiled instead.
file(GLOB SM64_PATCHES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/patches/*.patch")
set(SM64_OVERLAY "${CMAKE_CURRENT_BINARY_DIR}/overlay")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/overlay.py" "${D}"
          "${CMAKE_CURRENT_SOURCE_DIR}/patches" "${SM64_OVERLAY}"
  OUTPUT_VARIABLE SM64_PATCHED OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE overlay_result)
if(NOT overlay_result EQUAL 0)
  message(FATAL_ERROR "patches/ does not apply to ${D}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SM64_PATCHES})
string(REPLACE "\n" ";" SM64_PATCHED "${SM64_PATCHED}")
# A patched header is included before everything else in every game source;
# its include guard then skips the original wherever it is included.
# Headers from include/ go first: the others include them.
set(SM64_PATCHED_HEADERS)
foreach(relative ${SM64_PATCHED})
  if(relative MATCHES "^include/.*\\.h$")
    list(PREPEND SM64_PATCHED_HEADERS "${SM64_OVERLAY}/${relative}")
    continue()
  elseif(relative MATCHES "\\.h$")
    list(APPEND SM64_PATCHED_HEADERS "${SM64_OVERLAY}/${relative}")
    continue()
  endif()
  list(TRANSFORM SM64_SOURCES REPLACE "^${D}/${relative}$" "${SM64_OVERLAY}/${relative}")
  # Its quoted includes still name neighbours in the original directory.
  get_filename_component(original_dir "${D}/${relative}" DIRECTORY)
  set_source_files_properties("${SM64_OVERLAY}/${relative}" PROPERTIES INCLUDE_DIRECTORIES "${original_dir}")
endforeach()

# libultra's sinf and cosf, which the game called and whose results differ from
# the host's libm, under other names (see platform/ultra_math.h). Built apart:
# they must not see the host's <math.h>, whose NAN they would clash with.
add_library(sm64_ultra_math OBJECT "${D}/lib/src/math/sinf.c" "${D}/lib/src/math/cosf.c" "${D}/lib/src/NaN.c")
target_compile_definitions(sm64_ultra_math PRIVATE sinf=ultra_sinf cosf=ultra_cosf NAN=ultra_nan)
target_include_directories(sm64_ultra_math PRIVATE "${D}/include" "${D}/lib/src")
target_compile_options(sm64_ultra_math PRIVATE -fwrapv -ffp-contract=off -fno-fast-math -fPIC -w)

# The N64 stack pointer model (platform/n64stack.h): every function from which
# the N64 can reach the place where the original leaks a stack address moves
# the model by its N64 frame. tools/n64stack finds them in the matching build's
# code and writes a wrapper for each source that has some, which includes the
# source and registers their frame sizes; its other functions are not hooked.
set(SM64_N64STACK_DIR "${CMAKE_CURRENT_BINARY_DIR}/n64stack")
set(n64stack_specs)
foreach(source ${SM64_SOURCES})
  string(REPLACE "${SM64_OVERLAY}/" "" relative "${source}")
  string(REPLACE "${D}/" "" relative "${relative}")
  if(relative MATCHES "^src/")
    list(APPEND n64stack_specs "${relative}=${source}")
  endif()
endforeach()
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/n64stack/frames.py" "${GEN}" "sm64.${SM64_VERSION}"
          "${CMAKE_CURRENT_SOURCE_DIR}/oracle/symbols/${SM64_VERSION}.tsv" "${SM64_N64STACK_DIR}" ${n64stack_specs}
  OUTPUT_VARIABLE n64stack_hooked OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE n64stack_result)
if(NOT n64stack_result EQUAL 0)
  message(FATAL_ERROR "tools/n64stack/frames.py failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/n64stack/frames.py"
             "${CMAKE_CURRENT_SOURCE_DIR}/tools/n64stack/callgraph.py")
string(REPLACE "\n" ";" n64stack_hooked "${n64stack_hooked}")
foreach(line ${n64stack_hooked})
  string(REPLACE "\t" ";" fields "${line}")
  list(GET fields 0 relative)
  list(GET fields 1 wrapper)
  list(GET fields 2 excluded)
  list(TRANSFORM SM64_SOURCES REPLACE "^.*/${relative}$" "${wrapper}")
  get_filename_component(original_dir "${D}/${relative}" DIRECTORY)
  set_source_files_properties("${wrapper}" PROPERTIES INCLUDE_DIRECTORIES "${original_dir}" COMPILE_OPTIONS
    "-finstrument-functions;-finstrument-functions-exclude-file-list=.h;-finstrument-functions-exclude-function-list=${excluded}")
endforeach()

string(TOUPPER "${SM64_VERSION}" SM64_VERSION_UPPER)
# How every game source is compiled.
function(sm64_game_settings target visibility)
  target_include_directories(${target} ${visibility} "${CMAKE_CURRENT_SOURCE_DIR}/include" "${D}" "${GEN}"
                             "${GEN}/include" "${D}/include" "${D}/src"
                             "${CMAKE_CURRENT_SOURCE_DIR}/platform" "${SM64_N64STACK_DIR}")
  target_compile_definitions(${target} ${visibility}
    VERSION_${SM64_VERSION_UPPER}=1 F3D_OLD=1 _FINALROM=1 _LANGUAGE_C
    # Required by the decomp for non-N64 builds. Every AVOID_UB site changes
    # behavior and has to be checked against the N64 (see docs/avoid_ub.md).
    NON_MATCHING AVOID_UB
    NO_SEGMENTED_MEMORY)
  # Bit parity: IEEE single precision without fused or reordered arithmetic,
  # wrapping signed integers as on the N64, signed chars as IDO's -signed.
  target_compile_options(${target} PRIVATE -std=gnu11 -fsigned-char -fwrapv -fno-strict-aliasing
    -ffp-contract=off -fno-fast-math -fPIC -w
    "SHELL:-include \"${CMAKE_CURRENT_SOURCE_DIR}/platform/ultra_math.h\""
    "SHELL:-include \"${CMAKE_CURRENT_SOURCE_DIR}/platform/draw.h\"")
  foreach(header ${SM64_PATCHED_HEADERS})
    target_compile_options(${target} PRIVATE "SHELL:-include \"${header}\"")
  endforeach()
endfunction()

# Sources whose variables the N64 resets when it loads their segment from ROM
# again: linked as one object whose data and bss sections have their own
# names (PREFIX_data, PREFIX_datarel, PREFIX_datarel2, PREFIX_bss), so the
# host finds and restores exactly those variables. Sets <NAME>_OBJECT.
function(sm64_section_group name prefix)
  add_library(${name}_objects OBJECT ${ARGN})
  sm64_game_settings(${name}_objects PRIVATE)
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
# menu or the title screen starts (host_reload_overlay, patches/0006).
set(SM64_OVERLAY_SOURCES ${SM64_SOURCES})
list(FILTER SM64_OVERLAY_SOURCES INCLUDE REGEX "/src/(menu|goddard)/")
list(REMOVE_ITEM SM64_SOURCES ${SM64_OVERLAY_SOURCES})
sm64_section_group(sm64_overlay sm64ovl ${SM64_OVERLAY_SOURCES})
set(SM64_OVERLAY_OBJECT "${sm64_overlay_OBJECT}")

# Each level's data is segment 7, decompressed by LOAD_MIO0 on every level load
# (host_reload_level_data, patches/0008).
set(SM64_LEVEL_DATA_SOURCES ${SM64_SOURCES})
list(FILTER SM64_LEVEL_DATA_SOURCES INCLUDE REGEX "/levels/[^/]+/leveldata\\.c$")
list(REMOVE_ITEM SM64_SOURCES ${SM64_LEVEL_DATA_SOURCES})
sm64_section_group(sm64_level_data sm64lvl ${SM64_LEVEL_DATA_SOURCES})

# Everything else, with the host's stand-ins for the console: together with
# the two groups above, the game's whole state (sm64_save_state).
sm64_section_group(sm64_main sm64st ${SM64_SOURCES} platform/ultra.c platform/host.c platform/n64stack.c)

add_library(sm64_game STATIC $<TARGET_OBJECTS:sm64_ultra_math>
            "${sm64_main_OBJECT}" "${SM64_OVERLAY_OBJECT}" "${sm64_level_data_OBJECT}")
set_target_properties(sm64_game PROPERTIES LINKER_LANGUAGE C)
sm64_game_settings(sm64_game PUBLIC)
add_dependencies(sm64_game sm64_main_objects sm64_overlay_objects sm64_level_data_objects)
target_link_libraries(sm64_game PUBLIC m)

add_executable(sm64_run tools/run.c tools/elf_symbols.c)
target_link_libraries(sm64_run PRIVATE sm64_game)

# The lockstep comparator: the oracle's emulator frontend with the native game
# linked in, compared member by member at every frame. Needs mupen64plus.
find_path(MUPEN64PLUS_INCLUDE_DIR m64p_frontend.h PATH_SUFFIXES mupen64plus)
if(MUPEN64PLUS_INCLUDE_DIR)
  # Struct layouts on the N64 (32-bit, 8-byte alignment for 64-bit types) and
  # natively, read back out of DWARF.
  set(SM64_LAYOUT "${CMAKE_CURRENT_BINARY_DIR}/layout")
  set(SM64_LAYOUT_FLAGS -D_LANGUAGE_C -DVERSION_${SM64_VERSION_UPPER}=1 -DF3D_OLD=1 -D_FINALROM=1 -DNON_MATCHING
      -DAVOID_UB -I${D}/include -I${GEN} -I${GEN}/include -I${D}/src -I${D} -g -gdwarf-4
      -fno-eliminate-unused-debug-types -c)
  set(SM64_LAYOUT_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tools/layout/layout_types.c")
  add_custom_command(OUTPUT "${SM64_LAYOUT}/layout_tables.h"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SM64_LAYOUT}"
    COMMAND ${CMAKE_C_COMPILER} -m32 -malign-double -nostdinc -DTARGET_N64 -I${D}/include/libc ${SM64_LAYOUT_FLAGS}
            "${SM64_LAYOUT_SOURCE}" -o "${SM64_LAYOUT}/n64.o"
    COMMAND ${CMAKE_C_COMPILER} -DNO_SEGMENTED_MEMORY ${SM64_LAYOUT_FLAGS} "${SM64_LAYOUT_SOURCE}" -o "${SM64_LAYOUT}/native.o"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/layout/layout.py" "${SM64_LAYOUT}/n64.o"
            "${SM64_LAYOUT}/native.o" "${SM64_LAYOUT}/layout_tables.h"
    DEPENDS "${SM64_LAYOUT_SOURCE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/layout/layout.py" VERBATIM)

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
  target_link_libraries(sm64_lockstep PRIVATE sm64_game ${CMAKE_DL_LIBS})
  add_dependencies(sm64_lockstep oracle_video oracle_input)
endif()
