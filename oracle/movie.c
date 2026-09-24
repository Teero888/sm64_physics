#include "movie.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *read_file(const char *path, size_t *size) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long length = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *data = length > 0 ? malloc((size_t)length + 1) : NULL;
  if (data && fread(data, 1, (size_t)length, f) != (size_t)length) {
    free(data);
    data = NULL;
  }
  fclose(f);
  if (data) {
    data[length] = 0;
    *size = (size_t)length;
  }
  return data;
}

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

// Format: https://tasvideos.org/EmulatorResources/Mupen/M64
static bool load_m64(const uint8_t *data, size_t size, movie *out, char *error, size_t error_size) {
  if (size < 0x400 || memcmp(data, "M64\x1a", 4) != 0) {
    snprintf(error, error_size, "not an .m64 movie");
    return false;
  }
  const uint32_t version = le32(data + 4), samples = le32(data + 0x18);
  const unsigned controllers = data[0x15], start = data[0x1c] | data[0x1d] << 8;
  if (version != 3) {
    snprintf(error, error_size, ".m64 version %u is not supported", version);
    return false;
  }
  if (controllers != 1 || start != 2) {
    snprintf(error, error_size, "only one-controller movies from power-on are supported (controllers %u, start %u)",
             controllers, start);
    return false;
  }
  // Some old movies carry junk past the recorded samples.
  const size_t available = (size - 0x400) / 4;
  if (samples > available) {
    snprintf(error, error_size, "header lists %u samples but the file holds %zu", samples, available);
    return false;
  }
  out->inputs = malloc(sizeof(uint32_t) * (samples ? samples : 1));
  if (!out->inputs) return false;
  for (uint32_t i = 0; i < samples; ++i) out->inputs[i] = le32(data + 0x400 + 4 * i);
  out->count = samples;
  out->per_vi = false;
  memcpy(out->rom_name, data + 0xc4, 32);
  out->rom_name[32] = 0;
  out->country = (char)data[0xe8];
  return true;
}

// BizHawk's "Input Log.txt", N64 with one controller:
// |RP|  X,  Y,<A Up A Down A Left A Right DU DD DL DR Start Z B A CU CD CR CL L R>|
static bool load_bk2_log(const char *log, movie *out, char *error, size_t error_size) {
  // Bits of mupen64plus's BUTTONS for the 14 digital buttons, in log order.
  static const int bits[14] = {3, 2, 1, 0, 4, 5, 6, 7, 11, 10, 8, 9, 13, 12};
  size_t capacity = 1 << 16, count = 0;
  uint32_t *inputs = malloc(sizeof(uint32_t) * capacity);
  if (!inputs) return false;
  for (const char *line = log; line && *line; line = strchr(line, '\n'), line = line ? line + 1 : NULL) {
    if (line[0] != '|') continue;
    const char *flags = line + 1, *bar = strchr(flags, '|');
    if (!bar || bar - flags != 2) goto malformed;
    if (flags[0] != '.' || (count > 0 && flags[1] != '.')) {
      snprintf(error, error_size, "frame %zu resets or powers the console, which is not supported", count);
      free(inputs);
      return false;
    }
    int x, y, consumed;
    if (sscanf(bar + 1, "%d,%d,%n", &x, &y, &consumed) != 2 || x < -128 || x > 127 || y < -128 || y > 127)
      goto malformed;
    const char *buttons = bar + 1 + consumed;
    uint32_t value = (uint32_t)(uint8_t)(int8_t)x << 16 | (uint32_t)(uint8_t)(int8_t)y << 24;
    for (int i = 0; i < 18; ++i) {
      if (buttons[i] == '|' || buttons[i] == '\0') goto malformed;
      if (buttons[i] == '.') continue;
      // The four "A" stick directions are BizHawk's digital stick overrides.
      if (i < 4) {
        snprintf(error, error_size, "frame %zu uses a digital stick direction, which is not supported", count);
        free(inputs);
        return false;
      }
      value |= 1u << bits[i - 4];
    }
    if (count == capacity) {
      capacity *= 2;
      uint32_t *grown = realloc(inputs, sizeof(uint32_t) * capacity);
      if (!grown) { free(inputs); return false; }
      inputs = grown;
    }
    inputs[count++] = value;
  }
  out->inputs = inputs;
  out->count = count;
  out->per_vi = true;
  return true;
malformed:
  snprintf(error, error_size, "malformed input log line at frame %zu", count);
  free(inputs);
  return false;
}

bool movie_load(const char *path, movie *out, char *error, size_t error_size) {
  memset(out, 0, sizeof(*out));
  const size_t length = strlen(path);
  // A .bk2 is a zip; the caller extracts "Input Log.txt" and passes that.
  const bool log = length > 4 && strcmp(path + length - 4, ".txt") == 0;
  size_t size = 0;
  uint8_t *data = read_file(path, &size);
  if (!data) {
    snprintf(error, error_size, "cannot read %s", path);
    return false;
  }
  const bool ok = log ? load_bk2_log((const char *)data, out, error, error_size)
                      : load_m64(data, size, out, error, error_size);
  free(data);
  return ok;
}

void movie_free(movie *m) {
  free(m->inputs);
  memset(m, 0, sizeof(*m));
}
