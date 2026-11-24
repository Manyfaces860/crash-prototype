# Rendering Concepts

Render passes, where pixels live (buffers and targets), and the frame as a painting.

- [1. Render passes](#1-render-passes)
- [2. Buffers and targets](#2-buffers-and-targets)
- [3. The frame as a painting](#3-the-frame-as-a-painting)

Related: [GPU pipeline fundamentals](gpu-pipeline.md) for shaders, vertices, and rasterization.

---

## 1. Render passes

### What is a pass?

A **pass** is one complete "session" of drawing to a target.

Painting analogy:
1. **Pick up the canvas** (begin pass) — decide *where* you're drawing and *whether to wipe it clean first*
2. **Paint** (draw calls)
3. **Put it down** (end pass) — the canvas is finished for this round
4. **Hang it on the wall** (commit) — show it to the viewer

```cpp
sg_begin_pass(&pass);  // pick up canvas, clear to blue-grey
sg_end_pass();         // done for now
sg_commit();           // show it
```

### Related terminology

**Render pass** — one unit of work: a **target** + **actions** (what to do with old pixels) + **draws**.

```text
begin_pass  →  [draw triangle]  →  end_pass
   │                                    │
   └── one render pass ─────────────────┘
```

**Target (where you draw)**
- **Swapchain / backbuffer** — the window's hidden buffer.
- **Framebuffer** — general term: attachments (colour + optional depth) you can draw into.

```cpp
pass.swapchain = sglue_swapchain();  // "draw to the window"
```

**Load action (what happens to old pixels when pass starts)**

| Value | Meaning |
|-------|---------|
| `SG_LOADACTION_CLEAR` | **Wipe** old pixels → set to `clear_value` |
| `SG_LOADACTION_LOAD` | **Keep** old pixels (draw on top) |
| `SG_LOADACTION_DONTCARE` | Old pixels irrelevant (will overwrite everything) |

```cpp
pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
pass_action.colors[0].clear_value = {0.1f, 0.1f, 0.2f, 1.0f};
```

**Store action (what happens when pass ends)**

| Value | Meaning |
|-------|---------|
| `SG_STOREACTION_STORE` | **Keep** result (usual for screen) |
| `SG_STOREACTION_DONTCARE` | Throw away (e.g. intermediate pass) |

**Clear** — filling the target with a solid colour (load action `CLEAR` + `clear_value`). That's your background colour.

**Draw call** — instruction: "render these vertices with this pipeline." (`sg_draw` later.) One pass can contain many draw calls.

**Pipeline** — a prepared GPU state bundle: shaders + blend mode + vertex layout. Create once, apply per pass/draw.

**Commit** — end of frame: "all passes and draws recorded — submit to GPU and present." One `sg_commit` per frame.

### How many passes?

| Passes per frame | Use case |
|------------------|----------|
| **1** | Simple game — clear + draw everything (current path) |
| **2+** | Shadow map → main scene → post-FX → UI |

```text
Pass 1: render shadow map   (target: depth texture,  clear)
Pass 2: render scene        (target: swapchain,      clear + draw world)
Pass 3: render UI           (target: swapchain,      LOAD + draw on top)
commit
```

---

## 2. Buffers and targets

**"Buffer" means two different things** in graphics:

1. **Memory you upload** (vertices, uniforms) — CPU → GPU data
2. **Images you render into** (swapchain, framebuffers) — where pixels go

### Part A: Where you draw (pixel targets)

#### Framebuffer (the general concept)

A **framebuffer** is a bundle of attachments you can draw into:
- **Color attachment** — where RGB pixels land (a texture/image)
- **Depth attachment** — depth testing (how far each pixel is)
- **(optional) Stencil** — masking bits

```text
Framebuffer = [color image] + [depth image] + ...
```

#### Swapchain (the window's buffers)

The **swapchain** is a small set (usually 2–3) of framebuffers **owned by the windowing system**, sized to the window.

```text
GPU renders → backbuffer (1 of the swapchain images)
     ↓
"Present" swaps front ↔ back
     ↓
Screen shows the front buffer while you draw the next backbuffer
```

Double/triple buffering lets the GPU render frame N+1 while the screen displays frame N.

In code:

```cpp
pass.swapchain = sglue_swapchain();  // "target = window's current backbuffer"
```

#### Off-screen buffer / render target

A framebuffer **not** tied to the window. You create it yourself (usually as a texture) to render into, then *use that texture later*.

| Use | Example |
|-----|---------|
| Shadow map | Render depth from light's view → sample in main pass |
| Post-processing | Render scene → blur/bloom → draw to screen |
| Reflections | Render mirrored scene → sample as texture |
| UI layer | Render to texture, composite later |

```text
Pass 1: scene  → off-screen texture (1024×1024)
Pass 2: screen → sample that texture, draw full-screen quad
```

Also called: "render target", "FBO" (OpenGL's FrameBuffer Object).

#### Depth buffer

Special attachment storing **depth per pixel**, not color. Used so nearer objects hide farther ones (`z-test`). Usually part of the same framebuffer as color.

#### Backbuffer vs frontbuffer

- **Backbuffer** — you draw here (hidden)
- **Frontbuffer** — what's on screen right now
- Present = swap them

### Part B: Memory you upload (GPU data buffers)

#### Vertex buffer

Array of vertices: position, normal, UV, colour, etc.

```text
[vertex0][vertex1][vertex2]...  → GPU draws triangles from these
```

#### Index buffer

List of vertex indices defining which vertices form triangles — avoids duplicating vertices.

```text
indices: 0,1,2,  2,1,3  → two triangles sharing vertices
```

#### Uniform buffer (UBO)

Small per-draw/per-pass data: MVP matrix, time, light colour.

```text
CPU writes matrix → shader reads uniform each vertex/fragment
```

#### Storage buffer (SSBO) / compute buffer

Large GPU-writable buffers (compute shaders, big arrays). Not needed early on.

#### Staging buffer

A **temporary** buffer used to *transfer* data (CPU → GPU uploads, GPU → CPU readbacks).

#### Instance buffer

Vertex-like buffer with per-instance data (position of each tree, etc.) for instanced drawing.

### Part C: Images/textures (related, distinct)

| Type | Role |
|------|------|
| **Texture** | Sampled in shaders (albedo maps, shadow maps) |
| **Render target** | Written by GPU in a pass (can also be sampled later) |
| **Cubemap** | 6 faces — skyboxes, env maps |

A texture can be: *sampled only*, *rendered only*, or *both*.

### Quick map for this project

| Term | Used for |
|------|----------|
| **Swapchain** | Drawing the game each frame (now) |
| **Off-screen target** | Shadows, post-FX (later phases) |
| **Depth buffer** | 3D depth testing (cube milestone) |
| **Vertex buffer** | Cube vertices (next milestone) |
| **Index buffer** | Cube faces (next milestone) |
| **Uniform buffer** | MVP matrix for rotating cube (next milestone) |

### Mental model

```text
                    ┌─ Memory types (CPU→GPU data)
                    │   vertex / index / uniform / staging
GPU resources ──────┤
                    │   color / depth  (pixel storage)
                    └─ Image types (where pixels live)
                            │
                            ├─ swapchain images (window)
                            └─ off-screen images (your textures)
```

---

## 3. The frame as a painting

### The analogy in one sentence

> **A frame is a multi-stage painting session: prepare canvases, paint layers in order, varnish, then hang on the wall.**

### The cast (painting studio)

| Graphics term | Painting equivalent |
|---------------|---------------------|
| **Swapchain** | The **wall gallery** — 2–3 identical frames hanging in rotation; one faces the audience, one is behind being painted |
| **Backbuffer** | The **fresh canvas** on the easel (nobody sees it yet) |
| **Frontbuffer** | The canvas **currently hanging on the wall** (audience sees this) |
| **Off-screen target** | **Study sketches / stencil boards** on the side table |
| **Texture** | **Reference photos, stencils, transfer sheets** you look at while painting |
| **Vertex buffer** | The **sketch of shapes** — lists of points that define outlines |
| **Index buffer** | The **order list** — "connect points 0-1-2 to make triangle A" |
| **Uniform buffer** | The **instruction sticky notes** — angle, scale, "rotate 30°" |
| **Depth buffer** | **Wetness/height map** — "this blob is closer to viewer than that one" |
| **Load action CLEAR** | **Gesso / wipe the canvas clean** before painting |
| **Load action LOAD** | **Paint on top without wiping** |
| **Pass** | One **complete painting session** |
| **Draw call** | A **single brush stroke** |
| **Pipeline** | Your **brush + paint set + technique** prepared for a job |
| **Commit / present** | **Varnish + hang the finished canvas** on the gallery wall |
| **Shader** | The **painter's rule for each point** |

### A full frame, step by step

#### Step 0 — Studio setup (once at startup = `init`)

```text
sg_setup  =  build the studio
  • walls, easels, lighting
  • buy paint (create default resources)
  • register with the gallery (Metal device)
```

`pass_action` = pre-painted instruction: "every new canvas starts wiped to dark blue-grey."

#### Step 1 — Gallery rotation (each frame starts)

```text
Gallery has 3 frames: A (front, audience), B (being painted), C (previous)

Audience stares at A.
Painter picks up B from behind → this becomes your BACKBUFFER.
```

Computer: Metal gives you a `drawable` — one free image from the swapchain.

#### Step 2 — Choose the canvas (`begin_pass`)

```text
sg_pass:
  swapchain = which frame you grabbed     → "paint on frame B"
  action.clear = wipe rule                → "start blank / blue-grey"
```

**`sg_begin_pass` = clamp a fresh canvas to the easel and apply the wipe rule.**

| Load action | Painting meaning |
|-------------|------------------|
| CLEAR | Wipe to blue-grey (fresh canvas) |
| LOAD | Keep last frame's painting (motion blur, UI overlay) |
| DONTCARE | "I'll cover every inch anyway" |

#### Step 3 — Attach the depth board

```text
Canvas (color)          Depth board
┌────────────┐         ┌────────────┐
│ visible    │         │ distances  │
│ pixels     │         │ from eye   │
└────────────┘         └────────────┘
```

**Depth test:** before pasting a new piece, check the depth board — *"Is this piece closer to the viewer than what's already marked here?"*
- Yes → paste it (paint over)
- No → skip it (hidden behind a closer shape)

That's how a wall behind a crate stays "behind" even though you draw it in the same session. The depth board is cleared to "far away" (1.0) each pass.

#### Step 4 — Load your reference material (bind resources)

| Action | Painting meaning |
|--------|------------------|
| Bind **vertex sketch** | Open your outline drawing of the cube |
| Bind **index order** | "Follow this path to close the shapes" |
| Bind **texture reference** | Tape the wood-grain photo to the easel |
| Apply **pipeline** | "Use flat opaque brush for this stage" |
| Write **uniform note** | Stick note: `MVP = rotate 35° this frame` |

None of this is visible yet — it's **staging**.

#### Step 5 — Brush strokes (`sg_draw`)

```text
For each shape:
  shader reads vertex sketch + sticky note (MVP)
    → "put this corner here on the canvas"
  GPU rasterizes the triangle
  for each tiny square (pixel/fragment):
    shader looks at textures + depth board
      → "what colour? is it in front? write to canvas AND depth board"
```

1. **Vertex stage** — follow the sketch, place each corner in final position
2. **Raster stage** — fill in between corners (paint inside the outline)
3. **Fragment stage** — for each wet spot: mix reference colours, check depth board

#### Step 6 — Close the session (`sg_end_pass`)

Brush down. This pass's rules no longer apply. The canvas may be used as a reference later (e.g. post-process).

Multi-pass example:

```text
Pass 1 (side table): paint SHADOW study onto separate board
Pass 2 (main easel): wipe main canvas; use shadow study as reference
                     to darken areas; paint the world
Pass 3 (same canvas, LOAD): paint HUD text on top without wiping scene
```

#### Step 7 — Hang it (`sg_commit` → present)

```text
Varnish (submit commands to GPU — GPU finishes all strokes)
Hang: swap front ↔ back
  • Finished canvas B → wall (audience sees it)
  • Old front A → easel (becomes next backbuffer)
```

Triple buffering = 3 frames in the gallery rotation so painting never waits for the audience to look away.

### What one complete frame "loads"

| Resource | When created | When used in frame |
|----------|--------------|--------------------|
| Swapchain image | App start (3 of them) | Each `begin_pass` picks one |
| Depth image | Init or pass setup | Cleared + written every 3D pass |
| Vertex/index buffers | When mesh created (cube) | Bound before `sg_draw` |
| Textures | Load time (later) | Bound as references |
| Uniform (MVP etc.) | Every frame | Updated per draw/frame |
| Pipeline | Once (shader ready) | Applied in each pass that needs it |
| Pass action | Once (clear colour) | Executed at `begin_pass` |

**Key insight:** most things are **prepared once**. Each frame only: grab canvas → wipe if needed → refresh sticky notes → paint strokes → hang.

### One diagram, whole mechanism

```text
   GALLERY (swapchain)
   ┌─────────┐ ┌─────────┐ ┌─────────┐
   │ FRONT   │ │ BACK    │ │spare    │   ← audience sees FRONT
   │ (seen)  │ │ (paint) │ │         │
   └─────────┘ └────┬────┘ └─────────┘
                    │ present (hang)
   EASEL            ▼
   ┌──────────────────────────────────┐
   │ BEGIN PASS: pick canvas, wipe    │
   │ DEPTH BOARD: reset to "far"      │
   │                                  │
   │ bind: sketch + index + refs      │
   │ sticky: MVP note                 │
   │                                  │
   │ brush strokes (draw calls)       │
   │   shader: place → fill → colour  │
   │   check depth board each stroke  │
   │                                  │
   │ END PASS                         │
   └──────────────────────────────────┘
                    │ commit (varnish + hang)
                    ▼
              Gallery wall (screen)
```

### Map back to code

```cpp
// "Grab canvas + wipe to blue-grey"
sg_begin_pass(&pass);

// (later) "tape reference photos, stick MVP note, paint cube"
// sg_apply_pipeline / sg_apply_bindings / sg_draw

// "brush down"
sg_end_pass();

// "varnish + hang on wall"
sg_commit();
```
