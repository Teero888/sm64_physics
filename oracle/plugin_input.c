// An input plugin that asks the frontend for every poll. One standard
// controller in port 1 with no pak, as TAS movies of SM64 are recorded.
#define M64P_PLUGIN_PROTOTYPES 1
#include <m64p_common.h>
#include <m64p_plugin.h>
#include <m64p_types.h>

#include "oracle_plugins.h"

static oracle_poll_fn g_on_poll;
static void *g_user;

EXPORT void CALL oracle_input_bind(oracle_poll_fn on_poll, void *user) {
  g_on_poll = on_poll;
  g_user = user;
}

EXPORT m64p_error CALL PluginStartup(m64p_dynlib_handle core, void *context, void (*debug)(void *, int, const char *)) {
  (void)core; (void)context; (void)debug;
  return M64ERR_SUCCESS;
}
EXPORT m64p_error CALL PluginShutdown(void) { return M64ERR_SUCCESS; }
EXPORT m64p_error CALL PluginGetVersion(m64p_plugin_type *type, int *version, int *api, const char **name, int *caps) {
  if (type) *type = M64PLUGIN_INPUT;
  if (version) *version = 0x000100;
  if (api) *api = 0x020100;
  if (name) *name = "FrameTee oracle input";
  if (caps) *caps = 0;
  return M64ERR_SUCCESS;
}

EXPORT void CALL InitiateControllers(CONTROL_INFO info) {
  for (int i = 0; i < 4; ++i) {
    info.Controls[i].Present = i == 0;
    info.Controls[i].RawData = 0;
    info.Controls[i].Plugin = PLUGIN_NONE;
    info.Controls[i].Type = CONT_TYPE_STANDARD;
  }
}
EXPORT void CALL GetKeys(int controller, BUTTONS *keys) {
  keys->Value = g_on_poll ? g_on_poll(g_user, controller) : 0;
}
EXPORT void CALL ControllerCommand(int controller, unsigned char *command) { (void)controller; (void)command; }
EXPORT void CALL ReadController(int controller, unsigned char *command) { (void)controller; (void)command; }
EXPORT int CALL RomOpen(void) { return 1; }
EXPORT void CALL RomClosed(void) {}
EXPORT void CALL SDL_KeyDown(int keymod, int keysym) { (void)keymod; (void)keysym; }
EXPORT void CALL SDL_KeyUp(int keymod, int keysym) { (void)keymod; (void)keysym; }
