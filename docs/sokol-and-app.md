# Sokol & App Structure

How the application starts, what the callbacks do, how Sokol names things, and where variables live.

- [1. Entry point and callbacks](#1-entry-point-and-callbacks)
- [2. Naming conventions](#2-naming-conventions-sg_-and-friends)
- [3. Variable scopes](#3-variable-scopes)

---

## 1. Entry point and callbacks

### Simple English

| Function | What it does |
|----------|-------------|
| **`sokol_main`** | Your program's real entry point (replaces `main`). You fill in a settings struct telling Sokol: window size, title, and which functions to call when. Returns that struct. |
| **`init`** | Called **once** when the window opens. Sets up the graphics system (Sokol/Metal) and prepares the clear colour (background). |
| **`frame`** | Called **every frame** (~60×/sec). Currently: clear the screen, then tell the GPU "frame done, present it". |
| **`cleanup`** | Called **once** when the app closes. Shuts down graphics and frees GPU resources. |

**Types:**

| Type | What it is |
|------|-----------|
| **`sapp_desc`** | Settings bag for the whole app — callbacks, window size, title, icon, logger. |
| **`sg_desc`** | Settings bag for graphics init — environment (Metal objects), logger, pool sizes. |
| **`sg_pass`** | One render pass — what to draw onto (swapchain) and how to clear it (action). |
| **`sg_pass_action`** | Instructions for pass start: clear colour, load/store ops. |
| **`sg_setup` / `sg_shutdown`** | Graphics on / graphics off. |
| **`sg_begin_pass` / `sg_end_pass`** | Open a render target / close it. |
| **`sg_commit`** | "This frame is finished — show it on screen." |

### Code walkthrough

#### Entry point — `sokol_main`

```cpp
sapp_desc sokol_main(int argc, char* argv[]) {
    sapp_desc desc{};                // zero-init settings bag
    desc.init_cb = init;             // call init once at startup
    desc.frame_cb = frame;           // call frame every frame
    desc.cleanup_cb = cleanup;       // call cleanup on exit
    desc.width = 1280;               // window width
    desc.height = 720;               // window height
    desc.window_title = "CrashPrototype";
    desc.icon.sokol_default = true;  // default app icon
    desc.logger.func = slog_func;    // route Sokol logs to stdout
    return desc;                     // hand settings to Sokol
}
```

- **Type:** returns `sapp_desc` (struct of function pointers + window config).
- Sokol's platform layer calls this instead of `main`, then drives `init` → `frame`×N → `cleanup`.

#### One-time setup — `init`

```cpp
static void init(void) {                 // no args, no return
    sg_desc desc{};
    desc.logger.func = slog_func;            // logging
    desc.environment = sglue_environment();  // Metal device, drawable, etc. from sokol_glue
    sg_setup(&desc);                         // create GPU context

    // prepare clear colour for every frame
    pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass_action.colors[0].clear_value = {0.1f, 0.1f, 0.2f, 1.0f}; // dark blue-grey
}
```

- **`sglue_environment()`** → packs `sokol_app`'s Metal objects into what `sokol_gfx` expects.
- **`sg_setup`** → type `void(const sg_desc*)`; must run before any other `sg_*` call.
- **`pass_action`** → static `sg_pass_action`; reused every frame (cheap).

#### Per-frame — `frame`

```cpp
static void frame(void) {
    sg_pass pass{};                         // zero-init pass descriptor
    pass.action = pass_action;              // use our clear colour
    pass.swapchain = sglue_swapchain();     // target = window backbuffer
    sg_begin_pass(&pass);                   // start rendering to window
    sg_end_pass();                          // stop (nothing drawn yet)
    sg_commit();                            // submit frame to GPU / display
}
```

- **`sg_begin_pass`** → type `void(const sg_pass*)`; binds render target + clear/load ops.
- **`sg_end_pass`** → closes the pass.
- **`sg_commit`** → flushes all commands for the frame; Sokol then presents it.
- Later: triangle/cube draws go **between** `sg_begin_pass` and `sg_end_pass`.

#### Teardown — `cleanup`

```cpp
static void cleanup(void) {
    sg_shutdown();   // destroy GPU context, pipelines, buffers, etc.
}
```

- **`sg_shutdown`** → type `void(void)`; pair of `sg_setup`.

#### Implementation TU (the `.mm` file)

```objc
#define SOKOL_IMPL
#define SOKOL_METAL
#include "sokol_app.h"
// ...
```

- The **one** place Sokol's function bodies get compiled (`SOKOL_IMPL`).
- `SOKOL_METAL` picks the Metal backend for both `sokol_app` and `sokol_gfx`.

### Call flow

```text
sokol_main()  → returns sapp_desc
     │
     ▼
  sokol platform creates window
     │
     ▼
  init()  ── once ──► sg_setup
     │
     ▼
  frame() ── every frame ──► begin_pass → end_pass → sg_commit
     │
     ▼ (user closes window)
  cleanup() ── once ──► sg_shutdown
```

---

## 2. Naming conventions (`sg_*` and friends)

**`sg` = sokol gfx** (`sokol_gfx.h`). It's a prefix so C functions don't collide with other libraries.

| Prefix | Header | Meaning |
|--------|--------|---------|
| **`sg_`** | sokol_gfx.h | **Sokol GFX** — GPU/graphics (buffers, passes, draw) |
| **`sapp_`** | sokol_app.h | **Sokol APP** — window, lifecycle, input |
| **`sglue_`** | sokol_glue.h | **Sokol GLUE** — bridges app ↔ gfx (environment, swapchain) |
| **`slog_`** | sokol_log.h | **Sokol LOG** — logging callback |

### Breaking down the `sg_*` names

| Function | Literal meaning |
|----------|-----------------|
| `sg_setup` | gfx: set up / init |
| `sg_shutdown` | gfx: tear down |
| `sg_begin_pass` | gfx: begin render pass |
| `sg_end_pass` | gfx: end render pass |
| `sg_commit` | gfx: commit/submit the frame |
| `sg_make_buffer` | gfx: create buffer |
| `sg_make_pipeline` | gfx: create pipeline |
| `sg_draw` | gfx: draw call |

### Types follow the same rule

| Type | Meaning |
|------|---------|
| `sg_desc` | gfx descriptor (settings struct) |
| `sg_pass` | gfx render pass |
| `sg_pass_action` | gfx pass action (clear/store) |
| `sapp_desc` | app descriptor |

Mental parse: **`sg_begin_pass` = "graphics library → begin a render pass."**

---

## 3. Variable scopes

The root is **Sokol's environment**: the platform layer that called `sokol_main`, owns the window, and drives every callback. Everything else nests inside or hangs off it.

### The box tree

```text
┌─ SOKOL ROOT (platform env — alive for whole app) ─────────────────────────┐
│                                                                            │
│  owns: window, Metal device, event loop                                   │
│  holds: sapp_desc (copied from sokol_main's return)                        │
│    ├── init_cb   ──────────────► points to our init()                      │
│    ├── frame_cb  ──────────────► points to our frame()                     │
│    ├── cleanup_cb ─────────────► points to our cleanup()                   │
│    ├── width/height/title/icon/logger                                      │
│    │                                                                       │
│  ┌─ SOKOL GFX STATE (created by sg_setup, alive until sg_shutdown) ─────┐ │
│  │  environment = sglue_environment()  (Metal drawable, view, etc.)     │ │
│  │  pools: shaders, pipelines, buffers, ...                             │ │
│  │  (internal — we don't see these vars)                                │ │
│  └──────────────────────────────────────────────────────────────────────┘ │
│                                                                            │
│  ┌─ OUR FILE SCOPE (statics in main.cpp — alive for whole app) ────────┐ │
│  │  pass_action : sg_pass_action                                       │ │
│  │    written once in init()  { clear = blue-grey }                    │ │
│  │    read every frame in frame()                                      │ │
│  │                                                                     │ │
│  │  (later: meshes, pipelines, camera — same box)                      │ │
│  └──────────────────────────────────────────────────────────────────────┘ │
│                                                                            │
│  ┌─ FRAME N CALL STACK (born/dies each frame) ─────────────────────────┐ │
│  │  sokol → calls frame_cb                                             │ │
│  │  ┌─ frame() locals ──────────────────────────────────────────────┐  │ │
│  │  │  pass : sg_pass                                               │  │ │
│  │  │    .action    ← COPY of file-scope pass_action                │  │ │
│  │  │    .swapchain ← from sglue_swapchain() (borrowed, not owned)  │  │ │
│  │  │  destroyed at end of frame()                                   │  │ │
│  │  └───────────────────────────────────────────────────────────────┘  │ │
│  └──────────────────────────────────────────────────────────────────────┘ │
│                                                                            │
│  ┌─ INIT CALL STACK (born/dies once at startup) ───────────────────────┐ │
│  │  init() locals:                                                      │ │
│  │    desc : sg_desc  → passed &desc to sg_setup → destroyed            │ │
│  │  (sg_setup copies what it needs into SOKOL GFX STATE)                │ │
│  └──────────────────────────────────────────────────────────────────────┘ │
│                                                                            │
│  ┌─ STARTUP CALL STACK (dies before first frame) ──────────────────────┐ │
│  │  sokol_main() locals:                                                │ │
│  │    desc : sapp_desc → returned BY VALUE into SOKOL ROOT              │ │
│  │  (local gone; root keeps its own copy)                               │ │
│  └──────────────────────────────────────────────────────────────────────┘ │
│                                                                            │
│  cleanup() — last, tears down GFX STATE + window                          │
└────────────────────────────────────────────────────────────────────────────┘
```

### Same thing as a dict

```python
sokol_root = {                          # alive: main() → exit
  "window": ...,
  "sapp_desc": {                        # copy returned by sokol_main
    "init_cb":   init,                  # function pointers
    "frame_cb":  frame,
    "cleanup_cb": cleanup,
    "width": 1280, "height": 720,
    "window_title": "CrashPrototype",
  },

  "gfx_state": {                        # born: sg_setup  ·  dies: sg_shutdown
    "environment": sglue_environment(), # Metal objects (borrowed from app)
    "pools": {...},
  },

  "file_statics": {                     # born: load time  ·  dies: process exit
    "pass_action": {
      "colors[0].load_action": CLEAR,
      "colors[0].clear_value": (0.1, 0.1, 0.2, 1),
      # written in init(), read in frame()
    },
  },

  "per_frame_stack": {                  # born+die: EVERY frame
    "frame()": {
      "pass": {
        "action":    "<copy of file_statics.pass_action>",
        "swapchain": "<borrowed from gfx/window, not owned>",
      },
    },
  },

  "startup_stack": {                    # born+die: once, before frame 1
    "sokol_main()": {
      "desc": "<moved into sokol_root.sapp_desc, local destroyed>",
    },
  },
}
```

### How values cross boxes

| From → To | Mechanism | Lifetime result |
|-----------|-----------|-----------------|
| `sokol_main` → Sokol root | **return by value** (copy) | local `desc` dies; root keeps copy |
| root → `init` / `frame` / `cleanup` | root **calls** function pointers | stack frame born per call |
| `init` → gfx_state | `sg_setup(&desc)` **copies** desc | local `desc` dies; gfx keeps what it needs |
| `init` → file statics | direct assignment `pass_action = ...` | **static** — survives across calls |
| file statics → `frame` | `pass.action = pass_action` **copy** into local | local `pass` dies each frame; static stays |
| `frame` → window | `sglue_swapchain()` **borrow** (non-owning) | Sokol root owns the real drawable |

### Key patterns

**Static = shared box outside the stacks**

```text
pass_action lives in "file_statics", NOT inside init()
init() only WRITES to it; frame() READS it later
Neither function owns it — process lifetime
```

**Locals = disposable boxes**

```text
sg_desc desc{}  — only during init()
sg_pass  pass{} — only during one frame()
```

**Root outlives everything**

```text
sokol_main runs first, dies immediately — but its RETURN VALUE
becomes part of sokol_root, which then orchestrates all calls.
Root (alongside file statics) is the only box that exists
from t=0 to t=exit continuously.
```

**Ownership vs borrowing**

```text
owns:     window, gfx pools, file statics
borrows:  swapchain handle in pass (points into root's window)
copies:   pass_action → pass.action, sapp_desc into root
```

### Timeline (which boxes are open when)

```text
t=0     [sokol_root]───────────────────────────────────────────t=exit
        [file_statics]─────────────────────────────────────────t=exit
             │
t=start [startup: sokol_main → desc copied → box dies]
             │
t=init  [init: desc local]──write──► file_statics.pass_action
        [gfx_state born]◄──sg_setup── desc local dies
             │
frame N [frame: pass local]──copy──► reads file_statics
        [pass dies at end of frame]
             │
t=exit  [cleanup] → gfx_state dies → root tears down window
```
