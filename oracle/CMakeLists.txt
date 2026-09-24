# The emulator reference ("oracle"): a development tool, built on its own and
# never part of the library or FrameTee.
cmake_minimum_required(VERSION 3.22)
project(sm64_oracle LANGUAGES C)

set(CMAKE_C_STANDARD 11)
find_path(MUPEN64PLUS_INCLUDE_DIR m64p_frontend.h PATH_SUFFIXES mupen64plus REQUIRED)

add_library(oracle_video MODULE plugin_video.c)
add_library(oracle_input MODULE plugin_input.c)
add_executable(sm64_oracle oracle.c movie.c)
foreach(target oracle_video oracle_input sm64_oracle)
  target_include_directories(${target} PRIVATE "${MUPEN64PLUS_INCLUDE_DIR}")
  target_compile_options(${target} PRIVATE -Wall -Wextra)
endforeach()
set_target_properties(oracle_video oracle_input PROPERTIES PREFIX "" C_VISIBILITY_PRESET hidden)
target_link_libraries(sm64_oracle PRIVATE ${CMAKE_DL_LIBS})
