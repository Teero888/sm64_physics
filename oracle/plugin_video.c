// A video plugin that renders nothing. The RSP plugin still runs every task
// and raises the interrupts the game waits on, so game logic is unaffected.
#define M64P_PLUGIN_PROTOTYPES 1
#include <m64p_common.h>
#include <m64p_plugin.h>
#include <m64p_types.h>
#include <stddef.h>

#include "oracle_plugins.h"

static GFX_INFO g_info;
static oracle_vi_fn g_on_vi;
static void *g_user;

EXPORT void CALL oracle_video_bind(oracle_vi_fn on_vi, void *user) {
  g_on_vi = on_vi;
  g_user = user;
}

EXPORT uint8_t *CALL oracle_video_rdram(size_t *size) {
  if (size) *size = g_info.version >= 2 && g_info.RDRAM_SIZE ? *g_info.RDRAM_SIZE : 0x400000;
  return g_info.RDRAM;
}

EXPORT m64p_error CALL PluginStartup(m64p_dynlib_handle core, void *context, void (*debug)(void *, int, const char *)) {
  (void)core; (void)context; (void)debug;
  return M64ERR_SUCCESS;
}
EXPORT m64p_error CALL PluginShutdown(void) { return M64ERR_SUCCESS; }
EXPORT m64p_error CALL PluginGetVersion(m64p_plugin_type *type, int *version, int *api, const char **name, int *caps) {
  if (type) *type = M64PLUGIN_GFX;
  if (version) *version = 0x000100;
  if (api) *api = 0x020200;
  if (name) *name = "FrameTee oracle video";
  if (caps) *caps = 0;
  return M64ERR_SUCCESS;
}

EXPORT int CALL InitiateGFX(GFX_INFO info) {
  g_info = info;
  return 1;
}
// Every vertical interrupt ends in UpdateScreen.
EXPORT void CALL UpdateScreen(void) {
  if (g_on_vi) g_on_vi(g_user);
}
EXPORT int CALL RomOpen(void) { return 1; }
EXPORT void CALL RomClosed(void) {}
EXPORT void CALL ChangeWindow(void) {}
EXPORT void CALL MoveScreen(int x, int y) { (void)x; (void)y; }
// A real plugin raises the DP interrupt when the list reaches its full sync;
// SM64 ends every frame's list with one and waits for that interrupt.
EXPORT void CALL ProcessDList(void) {
  *g_info.MI_INTR_REG |= 0x20; // MI_INTR_DP
  g_info.CheckInterrupts();
}
EXPORT void CALL ProcessRDPList(void) {}
EXPORT void CALL ShowCFB(void) {}
EXPORT void CALL ViStatusChanged(void) {}
EXPORT void CALL ViWidthChanged(void) {}
EXPORT void CALL ReadScreen2(void *dest, int *width, int *height, int front) {
  (void)dest; (void)front;
  if (width) *width = 0;
  if (height) *height = 0;
}
EXPORT void CALL SetRenderingCallback(void (*callback)(int)) { (void)callback; }
EXPORT void CALL ResizeVideoOutput(int width, int height) { (void)width; (void)height; }
EXPORT void CALL FBRead(unsigned int addr) { (void)addr; }
EXPORT void CALL FBWrite(unsigned int addr, unsigned int size) { (void)addr; (void)size; }
EXPORT void CALL FBGetFrameBufferInfo(void *p) { (void)p; }
