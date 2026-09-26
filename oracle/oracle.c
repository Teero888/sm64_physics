// Runs a TAS movie on mupen64plus and records the game's state at every
// controller poll, as the reference the native library must reproduce.
//
//   sm64_oracle --rom ROM --movie MOVIE [--symbols syms.tsv --fields fields.txt]
//               [--trace out.trace] [--polls out.polls] [--dump-at POLL]...
//
// MOVIE is an .m64, or the "Input Log.txt" of a .bk2. --skip POLL:COUNT drops
// COUNT movie samples from POLL on (for finding where a movie desyncs).
//
// Trace format (little endian): "SM64ORC1", u32 field count, then per field
// u16 name length, name, u32 address, u32 size, u32 stride; then one record
// per poll: u32 poll, u32 vertical interrupt, u32 input, then each field: its
// bytes in N64 (big endian) order when stride is 0, otherwise one CRC-32 per
// stride bytes (for large arrays such as the object pool). Polls file: one u32 input per poll, the same layout as
// an .m64 body. RDRAM dumps are 4 MiB in N64 byte order.
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <m64p_common.h>
#include <m64p_config.h>
#include <m64p_debugger.h>
#include <m64p_frontend.h>
#include <m64p_types.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "movie.h"
#include "oracle_plugins.h"

#define MAX_FIELDS 256
#define MAX_DUMPS 64

typedef struct field {
  char name[64];
  uint32_t address, size, stride;
} field;

static uint32_t crc32_ram(const uint8_t *ram, uint32_t address, uint32_t size);

typedef struct oracle {
  movie movie;
  field fields[MAX_FIELDS];
  int field_count;
  uint32_t dump_at[MAX_DUMPS];
  int dump_count;
  const char *dump_dir;
  FILE *trace, *polls;
  uint8_t *record;
  size_t record_size;
  ptr_oracle_video_rdram rdram;
  ptr_CoreDoCommand command;
  uint32_t poll, vi;
  int vi_offset;   // .bk2: which movie frame a poll reads, relative to the interrupt count
  int poll_offset; // .m64: which sample a poll reads, relative to the poll count
  uint32_t max_polls;
  // Movie samples dropped at a poll: (poll, count).
  uint32_t skip_at[64], skip_count[64];
  int skips;
  bool stopping, failed;
  // --break, --watch: breakpoints armed at poll debug_from (the cached
  // interpreter or the pure one: the dynarec does not check them).
  m64p_breakpoint breakpoints[16];
  int breakpoint_count;
  uint32_t debug_from;
  bool debug_armed;
  // --poke POLL:ADDRESS:VALUE: a word written to RAM before the frame after
  // POLL, to find when the game last wrote it.
  uint32_t poke_at[16], poke_address[16], poke_value[16];
  int poke_count;
} oracle;

static oracle *sDebugOracle;
static ptr_DebugSetRunState sDebugSetRunState;
static ptr_DebugStep sDebugStep;
static ptr_DebugGetCPUDataPtr sDebugGetCPUDataPtr;
static ptr_DebugBreakpointCommand sDebugBreakpointCommand;
static ptr_DebugBreakpointTriggeredBy sDebugBreakpointTriggeredBy;
static ptr_DebugMemRead32 sDebugMemRead32;

static void debug_init(void) { sDebugSetRunState(M64P_DBG_RUNSTATE_RUNNING); }
static void debug_vi(void) {}

// A breakpoint hit: logs where, with the stack pointer and return address,
// and for a write what the watched word holds now.
static void debug_update(unsigned int pc) {
  const int64_t *regs = sDebugGetCPUDataPtr(M64P_CPU_REG_REG);
  uint32_t flags = 0, address = 0;
  sDebugBreakpointTriggeredBy(&flags, &address);
  if (flags & M64P_BKP_FLAG_WRITE) {
    // Called before the store: the value is the store's source register.
    // (Memory is read through KSEG0: a physical address would go through
    // the TLB.)
    const uint32_t op = sDebugMemRead32(pc);
    fprintf(stderr, "watch: poll %u pc %08x op %08x stores %08x at %08x (was %08x) sp %08x ra %08x\n",
            sDebugOracle->poll, pc, op, (uint32_t)regs[(op >> 16) & 31], address,
            sDebugMemRead32((address & 0x1ffffffcu) | 0x80000000u), (uint32_t)regs[29], (uint32_t)regs[31]);
  } else {
    fprintf(stderr, "break: poll %u pc %08x sp %08x ra %08x a0 %08x\n", sDebugOracle->poll, pc, (uint32_t)regs[29],
            (uint32_t)regs[31], (uint32_t)regs[4]);
  }
  sDebugSetRunState(M64P_DBG_RUNSTATE_RUNNING);
  sDebugStep();
}

static void die(const char *format, ...) {
  va_list args;
  va_start(args, format);
  fputs("sm64_oracle: ", stderr);
  vfprintf(stderr, format, args);
  fputc('\n', stderr);
  va_end(args);
  exit(1);
}

// mupen64plus keeps RDRAM as host-order 32-bit words.
static uint8_t ram_byte(const uint8_t *ram, uint32_t address) { return ram[(address & 0x7fffff) ^ 3]; }

#ifdef SM64_LOCKSTEP
// The native library's comparator (tools/lockstep): compares this poll's RAM
// with the native state and steps the native game. Nonzero stops the run.
int lockstep_poll(const uint8_t *ram, uint32_t poll, uint32_t input);
void lockstep_rom(const uint8_t *rom, size_t size);
#endif

static void stop(oracle *o) {
  if (o->stopping) return;
  o->stopping = true;
  o->command(M64CMD_STOP, 0, NULL);
}

static void write_dump(oracle *o, const uint8_t *ram, size_t ram_size) {
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/rdram_%06u.bin", o->dump_dir, o->poll);
  FILE *f = fopen(path, "wb");
  if (!f) die("cannot write %s", path);
  for (size_t i = 0; i < ram_size; ++i) fputc(ram[i ^ 3], f);
  fclose(f);
}

static void on_vi(void *user) {
  oracle *o = user;
  ++o->vi;
  if (o->movie.per_vi && o->vi >= o->movie.count) stop(o);
}

static uint32_t on_poll(void *user, int controller) {
  oracle *o = user;
  if (controller != 0 || o->stopping) return 0;
  long long skipped = 0;
  for (int k = 0; k < o->skips; ++k)
    if (o->skip_at[k] <= o->poll) skipped += o->skip_count[k];
  const long long at = o->movie.per_vi ? (long long)o->vi + o->vi_offset
                                       : (long long)o->poll + o->poll_offset + skipped;
  // Polls before the movie's first input read a neutral controller.
  if (at >= (long long)o->movie.count) {
    stop(o);
    return 0;
  }
  const uint32_t input = at < 0 ? 0 : o->movie.inputs[at];
  size_t ram_size = 0;
  const uint8_t *ram = o->rdram(&ram_size);
  if (!ram) {
    o->failed = true;
    stop(o);
    return 0;
  }
  // The state at a poll is what the previous game frame left behind.
  if (o->trace) {
    uint8_t *p = o->record;
    memcpy(p, &o->poll, 4);
    memcpy(p + 4, &o->vi, 4);
    memcpy(p + 8, &input, 4);
    p += 12;
    for (int f = 0; f < o->field_count; ++f) {
      const field *fd = &o->fields[f];
      if (fd->stride == 0) {
        for (uint32_t i = 0; i < fd->size; ++i) *p++ = ram_byte(ram, fd->address + i);
        continue;
      }
      for (uint32_t at = 0; at < fd->size; at += fd->stride) {
        const uint32_t crc = crc32_ram(ram, fd->address + at, fd->stride);
        memcpy(p, &crc, 4);
        p += 4;
      }
    }
    fwrite(o->record, 1, o->record_size, o->trace);
  }
  if (o->polls) fwrite(&input, 4, 1, o->polls);
  if (o->breakpoint_count && !o->debug_armed && o->poll >= o->debug_from) {
    for (int b = 0; b < o->breakpoint_count; ++b) sDebugBreakpointCommand(M64P_BKP_CMD_ADD_STRUCT, 0, &o->breakpoints[b]);
    o->debug_armed = true;
  }
#ifdef SM64_LOCKSTEP
  if (lockstep_poll(ram, o->poll, input)) {
    o->failed = true;
    stop(o);
  }
#endif
  for (int d = 0; d < o->dump_count; ++d)
    if (o->dump_at[d] == o->poll) write_dump(o, ram, ram_size);
  for (int p = 0; p < o->poke_count; ++p)
    if (o->poke_at[p] == o->poll) memcpy((uint8_t *)ram + (o->poke_address[p] & 0x7ffffc), &o->poke_value[p], 4);
  ++o->poll;
  if (o->max_polls && o->poll >= o->max_polls) stop(o);
  if (!o->movie.per_vi && at + 1 >= (long long)o->movie.count) stop(o);
  return input;
}

// symbols.tsv: name, hex address, decimal size per line.
static bool find_symbol(const char *path, const char *name, uint32_t *address, uint32_t *size) {
  FILE *f = fopen(path, "r");
  if (!f) die("cannot read %s", path);
  char line[512], symbol[256];
  unsigned long a, s;
  bool found = false;
  while (fgets(line, sizeof(line), f))
    if (sscanf(line, "%255s %lx %lu", symbol, &a, &s) == 3 && strcmp(symbol, name) == 0) {
      *address = (uint32_t)a;
      *size = (uint32_t)s;
      found = true;
      break;
    }
  fclose(f);
  return found;
}

static uint32_t crc32_ram(const uint8_t *ram, uint32_t address, uint32_t size) {
  uint32_t crc = 0xffffffffu;
  for (uint32_t i = 0; i < size; ++i) {
    crc ^= ram_byte(ram, address + i);
    for (int k = 0; k < 8; ++k) crc = crc >> 1 ^ (0xedb88320u & (0u - (crc & 1)));
  }
  return ~crc;
}

// fields.txt, one per line (# starts a comment):
//   symbol [size] [hash=STRIDE]
//   @hexaddress size label [hash=STRIDE]
// hash=STRIDE records a CRC-32 per STRIDE bytes instead of the bytes.
static void load_fields(oracle *o, const char *fields_path, const char *symbols_path) {
  FILE *f = fopen(fields_path, "r");
  if (!f) die("cannot read %s", fields_path);
  char line[512];
  while (fgets(line, sizeof(line), f)) {
    char *hash = strchr(line, '#');
    if (hash) *hash = 0;
    char name[64];
    unsigned long size = 0, address = 0, stride = 0;
    char *hash_option = strstr(line, "hash=");
    if (hash_option) {
      stride = strtoul(hash_option + 5, NULL, 0);
      *hash_option = 0;
    }
    if (line[strspn(line, " \t")] == '@') {
      if (sscanf(strchr(line, '@') + 1, "%lx %lu %63s", &address, &size, name) != 3)
        die("bad field line: %s", line);
    } else {
      int n = sscanf(line, "%63s %lu", name, &size);
      if (n < 1) continue;
      uint32_t a, s;
      if (!symbols_path) die("field %s needs --symbols", name);
      if (!find_symbol(symbols_path, name, &a, &s)) die("symbol %s not found in %s", name, symbols_path);
      address = a;
      if (n < 2) size = s;
    }
    if (size == 0 || size > 0x400000) die("field %s has size %lu", name, size);
    if (stride && size % stride) die("field %s: size %lu is not a multiple of hash=%lu", name, size, stride);
    if (o->field_count == MAX_FIELDS) die("too many fields");
    field *out = &o->fields[o->field_count++];
    snprintf(out->name, sizeof(out->name), "%s", name);
    out->address = (uint32_t)address;
    out->size = (uint32_t)size;
    out->stride = (uint32_t)stride;
  }
  fclose(f);
}

static void write_trace_header(oracle *o) {
  fwrite("SM64ORC1", 1, 8, o->trace);
  const uint32_t count = (uint32_t)o->field_count;
  fwrite(&count, 4, 1, o->trace);
  o->record_size = 12;
  for (int f = 0; f < o->field_count; ++f) {
    const uint16_t length = (uint16_t)strlen(o->fields[f].name);
    fwrite(&length, 2, 1, o->trace);
    fwrite(o->fields[f].name, 1, length, o->trace);
    fwrite(&o->fields[f].address, 4, 1, o->trace);
    fwrite(&o->fields[f].size, 4, 1, o->trace);
    fwrite(&o->fields[f].stride, 4, 1, o->trace);
    o->record_size += o->fields[f].stride ? o->fields[f].size / o->fields[f].stride * 4 : o->fields[f].size;
  }
  o->record = malloc(o->record_size);
  if (!o->record) die("out of memory");
}

static void debug_message(void *context, int level, const char *message) {
  (void)context;
  if (level <= M64MSG_WARNING) fprintf(stderr, "mupen64plus: %s\n", message);
}

static void *symbol(void *library, const char *name) {
  void *fn = dlsym(library, name);
  if (!fn) die("%s is missing %s", "a mupen64plus library", name);
  return fn;
}

static void set_int(ptr_ConfigSetParameter set, m64p_handle section, const char *name, int value) {
  if (set(section, name, M64TYPE_INT, &value) != M64ERR_SUCCESS) die("cannot set Core/%s", name);
}

static void *attach(void *core, ptr_CoreAttachPlugin attach_plugin, m64p_plugin_type type, const char *path) {
  void *plugin = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  if (!plugin) die("cannot load %s: %s", path, dlerror());
  ptr_PluginStartup startup = (ptr_PluginStartup)symbol(plugin, "PluginStartup");
  if (startup(core, NULL, debug_message) != M64ERR_SUCCESS) die("%s failed to start", path);
  if (attach_plugin(type, plugin) != M64ERR_SUCCESS) die("cannot attach %s", path);
  return plugin;
}

static uint8_t *read_all(const char *path, size_t *size) {
  FILE *f = fopen(path, "rb");
  if (!f) die("cannot read %s", path);
  fseek(f, 0, SEEK_END);
  long length = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *data = malloc((size_t)length);
  if (!data || fread(data, 1, (size_t)length, f) != (size_t)length) die("cannot read %s", path);
  fclose(f);
  *size = (size_t)length;
  return data;
}

int main(int argc, char **argv) {
  static oracle o;
  const char *rom_path = NULL, *movie_path = NULL, *symbols_path = NULL, *fields_path = NULL;
  const char *trace_path = NULL, *polls_path = NULL, *audio_path = NULL;
  const char *core_path = "libmupen64plus.so.2";
  const char *rsp_path = "/usr/lib/mupen64plus/mupen64plus-rsp-hle.so";
  int cpu = 2;
  int count_per_op = -1; // the core's default
  o.dump_dir = ".";
  // Mupen64-rr, which SM64 TASes are made on, gives the console's first
  // controller read after power-on no movie sample: sample N is read N + 1.
  o.poll_offset = -1;
  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i], *value = i + 1 < argc ? argv[i + 1] : NULL;
#define OPTION(flag, target) if (strcmp(arg, flag) == 0 && value) { target = value; ++i; continue; }
    OPTION("--rom", rom_path)
    OPTION("--movie", movie_path)
    OPTION("--symbols", symbols_path)
    OPTION("--fields", fields_path)
    OPTION("--trace", trace_path)
    OPTION("--polls", polls_path)
    OPTION("--audio", audio_path)
    OPTION("--dump-dir", o.dump_dir)
    OPTION("--core", core_path)
    OPTION("--rsp", rsp_path)
#undef OPTION
    if (strcmp(arg, "--cpu") == 0 && value) { cpu = atoi(value); ++i; continue; }
    if (strcmp(arg, "--count-per-op") == 0 && value) { count_per_op = atoi(value); ++i; continue; }
    if (strcmp(arg, "--vi-offset") == 0 && value) { o.vi_offset = atoi(value); ++i; continue; }
    if (strcmp(arg, "--poll-offset") == 0 && value) { o.poll_offset = atoi(value); ++i; continue; }
    if (strcmp(arg, "--skip") == 0 && value && o.skips < 64) {
      if (sscanf(value, "%u:%u", &o.skip_at[o.skips], &o.skip_count[o.skips]) == 2) ++o.skips;
      ++i;
      continue;
    }
    if ((strcmp(arg, "--break") == 0 || strcmp(arg, "--watch") == 0) && value && o.breakpoint_count < 16) {
      m64p_breakpoint *b = &o.breakpoints[o.breakpoint_count++];
      unsigned size = 1;
      b->address = (uint32_t)strtoul(value, NULL, 0);
      if (strchr(value, ':')) size = (unsigned)strtoul(strchr(value, ':') + 1, NULL, 0);
      b->endaddr = b->address + size - 1;
      b->flags = M64P_BKP_FLAG_ENABLED | (arg[2] == 'b' ? M64P_BKP_FLAG_EXEC : M64P_BKP_FLAG_WRITE);
      ++i;
      continue;
    }
    if (strcmp(arg, "--poke") == 0 && value && o.poke_count < 16) {
      if (sscanf(value, "%u:%x:%x", &o.poke_at[o.poke_count], &o.poke_address[o.poke_count],
                 &o.poke_value[o.poke_count]) == 3)
        ++o.poke_count;
      ++i;
      continue;
    }
    if (strcmp(arg, "--debug-from") == 0 && value) { o.debug_from = (uint32_t)atoi(value); ++i; continue; }
    if (strcmp(arg, "--max-polls") == 0 && value) { o.max_polls = (uint32_t)atoi(value); ++i; continue; }
    if (strcmp(arg, "--dump-at") == 0 && value && o.dump_count < MAX_DUMPS) {
      o.dump_at[o.dump_count++] = (uint32_t)strtoul(value, NULL, 0);
      ++i;
      continue;
    }
    die("unknown or incomplete option %s", arg);
  }
  if (!rom_path || !movie_path) die("usage: sm64_oracle --rom ROM --movie MOVIE [options]");

  char error[256];
  if (!movie_load(movie_path, &o.movie, error, sizeof(error))) die("%s: %s", movie_path, error);
  if (fields_path) load_fields(&o, fields_path, symbols_path);
  if (trace_path) {
    if (!(o.trace = fopen(trace_path, "wb"))) die("cannot write %s", trace_path);
    write_trace_header(&o);
  }
  if (polls_path && !(o.polls = fopen(polls_path, "wb"))) die("cannot write %s", polls_path);

  // Plugins sit next to this executable.
  char self[PATH_MAX], video_path[PATH_MAX + 32], input_path[PATH_MAX + 32], audio_plugin_path[PATH_MAX + 32];
  const ssize_t self_length = readlink("/proc/self/exe", self, sizeof(self) - 1);
  if (self_length <= 0) die("cannot locate the executable");
  self[self_length] = 0;
  *strrchr(self, '/') = 0;
  snprintf(video_path, sizeof(video_path), "%s/oracle_video.so", self);
  snprintf(input_path, sizeof(input_path), "%s/oracle_input.so", self);
  snprintf(audio_plugin_path, sizeof(audio_plugin_path), "%s/oracle_audio.so", self);

  void *core = dlopen(core_path, RTLD_NOW | RTLD_GLOBAL);
  if (!core) die("cannot load %s: %s", core_path, dlerror());
  ptr_CoreStartup core_startup = (ptr_CoreStartup)symbol(core, "CoreStartup");
  ptr_CoreShutdown core_shutdown = (ptr_CoreShutdown)symbol(core, "CoreShutdown");
  ptr_CoreAttachPlugin attach_plugin = (ptr_CoreAttachPlugin)symbol(core, "CoreAttachPlugin");
  ptr_CoreDetachPlugin detach_plugin = (ptr_CoreDetachPlugin)symbol(core, "CoreDetachPlugin");
  ptr_ConfigOpenSection open_section = (ptr_ConfigOpenSection)symbol(core, "ConfigOpenSection");
  ptr_ConfigSetParameter set_parameter = (ptr_ConfigSetParameter)symbol(core, "ConfigSetParameter");
  o.command = (ptr_CoreDoCommand)symbol(core, "CoreDoCommand");

  // A private, empty config and save directory: every run starts from a
  // cleared EEPROM, as movies recorded from power-on do.
  char home[PATH_MAX];
  snprintf(home, sizeof(home), "/tmp/sm64_oracle.%d", (int)getpid());
  if (mkdir(home, 0700) != 0 && errno != EEXIST) die("cannot create %s", home);
  if (core_startup(0x020001, home, NULL, NULL, debug_message, NULL, NULL) != M64ERR_SUCCESS) die("core startup failed");
  m64p_handle section;
  if (open_section("Core", &section) != M64ERR_SUCCESS) die("no Core config section");
  set_int(set_parameter, section, "R4300Emulator", cpu);
  if (count_per_op >= 0) set_int(set_parameter, section, "CountPerOp", count_per_op);
  // Interrupt timing must be deterministic, and match BizHawk's 4 MiB setup.
  int off = 0, on = 1;
  set_parameter(section, "RandomizeInterrupt", M64TYPE_BOOL, &off);
  set_parameter(section, "DisableExtraMem", M64TYPE_BOOL, &on);
  set_parameter(section, "OnScreenDisplay", M64TYPE_BOOL, &off);
  set_parameter(section, "SaveSRAMPath", M64TYPE_STRING, home);
  set_parameter(section, "SaveStatePath", M64TYPE_STRING, home);
  if (o.breakpoint_count) {
    set_parameter(section, "EnableDebugger", M64TYPE_BOOL, &on);
    sDebugOracle = &o;
    sDebugSetRunState = (ptr_DebugSetRunState)symbol(core, "DebugSetRunState");
    sDebugStep = (ptr_DebugStep)symbol(core, "DebugStep");
    sDebugGetCPUDataPtr = (ptr_DebugGetCPUDataPtr)symbol(core, "DebugGetCPUDataPtr");
    sDebugBreakpointCommand = (ptr_DebugBreakpointCommand)symbol(core, "DebugBreakpointCommand");
    sDebugBreakpointTriggeredBy = (ptr_DebugBreakpointTriggeredBy)symbol(core, "DebugBreakpointTriggeredBy");
    sDebugMemRead32 = (ptr_DebugMemRead32)symbol(core, "DebugMemRead32");
    if (((ptr_DebugSetCallbacks)symbol(core, "DebugSetCallbacks"))(debug_init, debug_update, debug_vi) != M64ERR_SUCCESS)
      die("the core has no debugger");
  }

  size_t rom_size = 0;
  uint8_t *rom = read_all(rom_path, &rom_size);
#ifdef SM64_LOCKSTEP
  lockstep_rom(rom, rom_size);
#endif
  if (o.command(M64CMD_ROM_OPEN, (int)rom_size, rom) != M64ERR_SUCCESS) die("cannot open %s", rom_path);
  free(rom);

  void *video = attach(core, attach_plugin, M64PLUGIN_GFX, video_path);
  // (The core wants the audio plugin between video and input.)
  FILE *audio_out = NULL;
  if (audio_path) {
    if (!(audio_out = fopen(audio_path, "wb"))) die("cannot write %s", audio_path);
    void *audio = attach(core, attach_plugin, M64PLUGIN_AUDIO, audio_plugin_path);
    ((ptr_oracle_audio_bind)symbol(audio, "oracle_audio_bind"))(audio_out);
  }
  void *input = attach(core, attach_plugin, M64PLUGIN_INPUT, input_path);
  void *rsp = attach(core, attach_plugin, M64PLUGIN_RSP, rsp_path);
  ((ptr_oracle_video_bind)symbol(video, "oracle_video_bind"))(on_vi, &o);
  ((ptr_oracle_input_bind)symbol(input, "oracle_input_bind"))(on_poll, &o);
  o.rdram = (ptr_oracle_video_rdram)symbol(video, "oracle_video_rdram");

  int unlimited = 0;
  o.command(M64CMD_CORE_STATE_SET, M64CORE_SPEED_LIMITER, &unlimited);
  o.command(M64CMD_EXECUTE, 0, NULL);

  detach_plugin(M64PLUGIN_GFX);
  detach_plugin(M64PLUGIN_INPUT);
  detach_plugin(M64PLUGIN_RSP);
  if (audio_out) {
    detach_plugin(M64PLUGIN_AUDIO);
    fclose(audio_out);
  }
  o.command(M64CMD_ROM_CLOSE, 0, NULL);
  core_shutdown();
  (void)rsp;
  if (o.trace) fclose(o.trace);
  if (o.polls) fclose(o.polls);
  fprintf(stderr, "sm64_oracle: %u polls over %u vertical interrupts (movie has %zu %s)\n", o.poll, o.vi,
          o.movie.count, o.movie.per_vi ? "frames" : "polls");
  movie_free(&o.movie);
  return o.failed ? 1 : 0;
}
