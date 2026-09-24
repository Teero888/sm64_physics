// TAS movie inputs as mupen64plus BUTTONS values (buttons in bits 0-15, stick
// X in bits 16-23, stick Y in bits 24-31).
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct movie {
  uint32_t *inputs;
  size_t count;
  // .m64 holds one input per controller poll; .bk2 holds one per vertical
  // interrupt, and a poll reads whichever frame it lands in.
  bool per_vi;
  char rom_name[33];
  char country;
} movie;

bool movie_load(const char *path, movie *out, char *error, size_t error_size);
void movie_free(movie *m);
