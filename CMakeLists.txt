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

add_library(sm64_game STATIC ${SM64_SOURCES} platform/ultra.c platform/host.c $<TARGET_OBJECTS:sm64_ultra_math>)
target_include_directories(sm64_game PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${D}" "${GEN}" "${GEN}/include" "${D}/include" "${D}/src")
string(TOUPPER "${SM64_VERSION}" SM64_VERSION_UPPER)
target_compile_definitions(sm64_game PUBLIC
  VERSION_${SM64_VERSION_UPPER}=1 F3D_OLD=1 _FINALROM=1 _LANGUAGE_C
  # Required by the decomp for non-N64 builds. Every AVOID_UB site changes
  # behavior and has to be checked against the N64 (see docs/avoid_ub.md).
  NON_MATCHING AVOID_UB
  NO_SEGMENTED_MEMORY)
# Bit parity: IEEE single precision without fused or reordered arithmetic,
# wrapping signed integers as on the N64, signed chars as IDO's -signed.
target_compile_options(sm64_game PRIVATE -std=gnu11 -fsigned-char -fwrapv -fno-strict-aliasing
  -ffp-contract=off -fno-fast-math -fPIC -w
  -include "${CMAKE_CURRENT_SOURCE_DIR}/platform/ultra_math.h")
target_link_libraries(sm64_game PUBLIC m)

add_executable(sm64_run tools/run.c tools/elf_symbols.c)
target_link_libraries(sm64_run PRIVATE sm64_game)
