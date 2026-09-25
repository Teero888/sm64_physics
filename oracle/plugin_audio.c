// An audio plugin that plays nothing: it records what the game hands the
// audio interface, every buffer as the game writes AI_LEN, as stereo 16-bit
// samples (left first, host byte order) into the file the frontend names. The
// library's sm64_audio returns the same stream.
#define M64P_PLUGIN_PROTOTYPES 1
#include <stdio.h>
#include <m64p_common.h>
#include <m64p_plugin.h>
#include <m64p_types.h>

#include "oracle_plugins.h"

static AUDIO_INFO g_info;
static FILE *g_out;

EXPORT void CALL oracle_audio_bind(FILE *out) { g_out = out; }

EXPORT m64p_error CALL PluginStartup(m64p_dynlib_handle core, void *context, void (*debug)(void *, int, const char *)) {
  (void)core; (void)context; (void)debug;
  return M64ERR_SUCCESS;
}
EXPORT m64p_error CALL PluginShutdown(void) { return M64ERR_SUCCESS; }
EXPORT m64p_error CALL PluginGetVersion(m64p_plugin_type *type, int *version, int *api, const char **name, int *caps) {
  if (type) *type = M64PLUGIN_AUDIO;
  if (version) *version = 0x000100;
  if (api) *api = 0x020000;
  if (name) *name = "FrameTee oracle audio";
  if (caps) *caps = 0;
  return M64ERR_SUCCESS;
}

EXPORT int CALL InitiateAudio(AUDIO_INFO info) {
  g_info = info;
  return 1;
}
EXPORT void CALL AiDacrateChanged(int system) { (void)system; }
// RDRAM holds 32-bit words in host order: each is a sample pair, left in the
// high half.
EXPORT void CALL AiLenChanged(void) {
  const unsigned length = *g_info.AI_LEN_REG & 0x3fff8, address = *g_info.AI_DRAM_ADDR_REG & 0xfffff8;
  if (!g_out) return;
  for (unsigned i = 0; i < length; i += 4) {
    const unsigned word = *(const unsigned *)(g_info.RDRAM + address + i);
    const short pair[2] = { (short)(word >> 16), (short)word };
    fwrite(pair, sizeof(pair), 1, g_out);
  }
}
EXPORT void CALL ProcessAList(void) {}
EXPORT int CALL RomOpen(void) { return 1; }
EXPORT void CALL RomClosed(void) {}
EXPORT void CALL SetSpeedFactor(int percent) { (void)percent; }
EXPORT void CALL VolumeUp(void) {}
EXPORT void CALL VolumeDown(void) {}
EXPORT int CALL VolumeGetLevel(void) { return 100; }
EXPORT void CALL VolumeSetLevel(int level) { (void)level; }
EXPORT void CALL VolumeMute(void) {}
EXPORT const char *CALL VolumeGetString(void) { return "100%"; }
