cmake_minimum_required(VERSION 3.22)
project(sm64_physics LANGUAGES C)

# The game is the n64decomp/sm64 source compiled natively. SM64_DECOMP_DIR is
# a checkout of it (at the pinned commit) that has been built once for
# SM64_VERSION from the user's ROM: that build generates the assets the game
# data includes (animations, demo inputs, text, textures, sound banks).
set(SM64_DECOMP_DIR "$ENV{HOME}/software/sm64-decomp" CACHE PATH "n64decomp/sm64 checkout built for SM64_VERSION")
set(SM64_VERSION jp CACHE STRING "Game version: jp")
set_property(CACHE SM64_VERSION PROPERTY STRINGS jp)

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
foreach(relative ${SM64_PATCHED})
  if(relative MATCHES "\\.h$")
    message(FATAL_ERROR "${relative}: header patches need their includers overlaid too")
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

string(TOUPPER "${SM64_VERSION}" SM64_VERSION_UPPER)
# How every game source is compiled.
function(sm64_game_settings target visibility)
  target_include_directories(${target} ${visibility} "${CMAKE_CURRENT_SOURCE_DIR}/include" "${D}" "${GEN}"
                             "${GEN}/include" "${D}/include" "${D}/src")
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
    -include "${CMAKE_CURRENT_SOURCE_DIR}/platform/ultra_math.h")
endfunction()

# The N64 loads src/menu and src/goddard as one segment, from ROM, every time
# a menu or the title screen starts, which resets all of their variables. They
# are linked as one object whose data and bss sections have their own names,
# so the host finds and resets exactly those variables (host_reload_overlay).
set(SM64_OVERLAY_SOURCES ${SM64_SOURCES})
list(FILTER SM64_OVERLAY_SOURCES INCLUDE REGEX "/src/(menu|goddard)/")
list(REMOVE_ITEM SM64_SOURCES ${SM64_OVERLAY_SOURCES})
add_library(sm64_overlay_objects OBJECT ${SM64_OVERLAY_SOURCES})
sm64_game_settings(sm64_overlay_objects PRIVATE)
set(SM64_OVERLAY_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/sm64_overlay.o")
add_custom_command(OUTPUT "${SM64_OVERLAY_OBJECT}"
  COMMAND ${CMAKE_LINKER} -r -o "${SM64_OVERLAY_OBJECT}.tmp" $<TARGET_OBJECTS:sm64_overlay_objects>
  COMMAND ${CMAKE_OBJCOPY} --rename-section .data=sm64ovl_data --rename-section .data.rel.local=sm64ovl_datarel
          --rename-section .data.rel=sm64ovl_datarel2 --rename-section .bss=sm64ovl_bss
          "${SM64_OVERLAY_OBJECT}.tmp" "${SM64_OVERLAY_OBJECT}"
  DEPENDS $<TARGET_OBJECTS:sm64_overlay_objects> COMMAND_EXPAND_LISTS VERBATIM)
set_source_files_properties("${SM64_OVERLAY_OBJECT}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)

add_library(sm64_game STATIC ${SM64_SOURCES} platform/ultra.c platform/host.c $<TARGET_OBJECTS:sm64_ultra_math>
            "${SM64_OVERLAY_OBJECT}")
sm64_game_settings(sm64_game PUBLIC)
add_dependencies(sm64_game sm64_overlay_objects)
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
