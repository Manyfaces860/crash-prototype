#pragma once

// Project-owned input state. No sokol, no platform types — main.cpp's
// event_cb translates sapp events into SetKey() calls.
namespace Input {

enum Key {
    KEY_W,
    KEY_A,
    KEY_S,
    KEY_D,
    KEY_SPACE,
    KEY_ESCAPE,
    KEY_COUNT,
};

inline bool down[KEY_COUNT]    = {};   // currently held
inline bool pressed[KEY_COUNT] = {};   // edge: true only on the frame it went down

inline void SetKey(Key key, bool is_down) {
    if (is_down && !down[key]) {
        pressed[key] = true;          // register the press edge
    }
    down[key] = is_down;
}

// Called once at the end of each frame to clear edge state.
inline void Update() {
    for (int i = 0; i < KEY_COUNT; i++) {
        pressed[i] = false;
    }
}

} // namespace Input
