// Hooks the oracle's two plugins export for the frontend. The frontend dlopens
// each plugin, attaches it to the core like any other, and binds these.
#pragma once
#include <stddef.h>
#include <stdint.h>

// Video plugin: draws nothing. It exposes RDRAM and reports every vertical
// interrupt, which is the frame unit of BizHawk movies.
typedef void (*oracle_vi_fn)(void *user);
typedef void (*ptr_oracle_video_bind)(oracle_vi_fn on_vi, void *user);
typedef uint8_t *(*ptr_oracle_video_rdram)(size_t *size);

// Input plugin: every controller poll asks the frontend which input to return.
// That moment is the start of a game frame: the state then is the state the
// previous frame left behind.
typedef uint32_t (*oracle_poll_fn)(void *user, int controller);
typedef void (*ptr_oracle_input_bind)(oracle_poll_fn on_poll, void *user);
