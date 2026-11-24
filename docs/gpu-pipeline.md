# GPU Pipeline Fundamentals

What shaders are, what the rasterizer does, how vertices become pixels, and why things are named the way they are.

- [1. What is a shader](#1-what-is-a-shader)
- [2. Vertices, colours, and "lines"](#2-vertices-colours-and-lines)
- [3. The rasterizer](#3-the-rasterizer)
- [4. Vertex shader vs fragment shader](#4-vertex-shader-vs-fragment-shader)
- [5. What is a pixel in a shader](#5-what-is-a-pixel-in-a-shader)
- [6. Why "shader"](#6-why-shader)
- [7. Full pipeline diagram](#7-full-pipeline-diagram)

Related: [Rendering concepts](rendering-concepts.md) for passes and buffers · [Sokol & app structure](sokol-and-app.md) for callbacks.

---

## 1. What is a shader

A **shader** is a small program that runs **on the GPU**, not the CPU. Two stages matter first:

| Stage | Runs once per | Job |
|-------|---------------|-----|
| **Vertex shader (VS)** | **vertex** | "Where does this corner go on screen?" |
| **Fragment shader (FS)** | **pixel** (fragment) | "What colour is this pixel?" |

Shaders are written in **GLSL** (OpenGL Shading Language) — a C-like language. Metal uses MSL; `sokol-shdc` translates our GLSL for the Metal backend.

### Is `.glsl` a model file?

**No.** It's a text file with shader code — instructions, not geometry.

| File type | Contains | Example |
|-----------|----------|---------|
| **Model** (`.gltf`, `.glb`) | Vertices, faces, UVs — *shapes* | `assets/models/cube.glb` |
| **Shader** (`.glsl`) | GPU programs — *how to draw* | `assets/shaders/cube.glsl` |

Analogy: a model is the **wood and nails**; a shader is the **carpenter's instructions**.

---

## 2. Vertices, colours, and lines

### Vertices are points — with labels

A vertex is a point **plus attached data** (colour, UV, normal, …). Colour is a label glued on before the shape is filled:

```text
vertex data:
  #0: position = (0, 0.5, 0)     colour = (1, 0, 0)   ← red
  #1: position = (0.5, -0.5, 0)  colour = (0, 1, 0)   ← green
  #2: position = (-0.5, -0.5, 0) colour = (0, 0, 1)   ← blue
```

The GPU **interpolates** (smoothly blends) between corner labels across the surface — see [rasterizer](#3-the-rasterizer).

Why store colour on points? Cheapest place — 3 numbers per corner instead of one per pixel. The GPU expands it during fill.

### "Lines" are not drawn

You might imagine a wireframe:

```text
    ●───────●
   /│      /│
  / │     / │      ← "lines connecting points"
 ●──│────●  │
 │  ●────│──●
 │ /     │ /
 │/      │/
 ●───────●
```

**Reality:** only the 8 corner points are stored. The "lines" are an **invisible rule**: *"these points form triangles."*

```text
stored:   8 vertices + index list: (0,1,2), (2,1,3), ...
meaning:  "fill the inside of triangle 0-1-2"
```

Lines appear only in **wireframe debug mode**. In the final image nobody draws lines — the GPU **fills the area between** vertices.

### Why triangles?

- Simplest shape **guaranteed flat** (3 points always define a plane)
- Hardware optimized for triangles for decades
- Curved surfaces = many small triangles (mosaic)

### Real painting vs digital

| Painting | Digital |
|----------|---------|
| Draw a line, then colour inside | Declare corners; GPU fills |
| Lines are visible strokes | Lines are recipe edges |
| Brush = outline and fill | Outline optional; fill = rasterizer's job |

---

## 3. The rasterizer

The rasterizer is **fixed-function hardware** (not a shader). It sits between VS and FS.

### What it does

1. **Coverage test** — which pixels are inside the triangle?
2. **Interpolation** — compute per-pixel attribute values from vertex labels
3. **Hand-off** — produce one *fragment candidate* per covered pixel for the FS

```text
        V0 ●────────● V1
           │╲       │
           │  ╲     │     for each pixel in the bounding box:
           │    ╲   │       "inside the triangle?"
           │      ╲ │         → yes: fragment candidate
        V2 ●────────┘          → no:  skip (never reaches FS)
```

### Barycentric coordinates (the interpolation recipe)

For any pixel **P** inside the triangle:

```text
P = a·V0 + b·V1 + c·V2     where  a + b + c = 1,  a,b,c ≥ 0
```

*"P is a% toward V0, b% toward V1, c% toward V2."*

- Dead centre → `a = b = c = 1/3`
- Near V0 → `a ≈ 0.9`, others small

### Interpolate every attribute with the same weights

| Attribute | Formula at pixel P |
|-----------|-------------------|
| colour | `a·colour0 + b·colour1 + c·colour2` |
| uv | `a·uv0 + b·uv1 + c·uv2` |
| depth | `a·depth0 + b·depth1 + c·depth2` |
| normal | `a·n0 + b·n1 + c·n2` |

Red/green/blue triangle example:

```text
Pixel near V0     → mostly red
Edge V0–V1        → red+green mix (yellow-ish)
Dead centre       → equal mix of all three
```

**Yes — the rasterizer interpolates vertex colours for in-between pixels.** Same weights interpolate UVs, depth, normals.

### What the FS receives (the "fill")

Not a final colour — a **bundle of interpolated values**:

```text
For pixel (12, 34) inside the triangle:

{
  pixel_coord: (12, 34),
  colour:      (0.8, 0.2, 0.1)   ← interpolated
  uv:          (0.45, 0.62)      ← interpolated
  depth:       0.73              ← interpolated
  ...
}
        │
        ▼
   FRAGMENT SHADER writes real colour
```

---

## 4. Vertex shader vs fragment shader

| Stage | Runs on | Input → Output |
|-------|---------|----------------|
| **VS** | once per **vertex** (8 for a cube) | model-space position → **screen position** + pass along attributes |
| **Rasterizer** | once per **covered pixel** | triangle vertices → **interpolated bundle** |
| **FS** | once per **fragment** | interpolated bundle → **final colour** |

```text
3 vertices ──VS──► 3 screen positions
                      │
                      ▼
              ┌───────────────┐
              │  RASTERIZER   │  coverage + barycentric + interpolate
              └───────┬───────┘
                      │  thousands of fragments
                      ▼
              ┌───────────────┐
              │  FS           │  texture lookup, lighting…
              └───────┬───────┘
                      ▼
                 final RGB pixels
```

Cube: **8 VS runs** → **possibly 100,000+ FS runs** per frame. GPUs have thousands of cores for this parallelism.

### What VS does with MVP

VS puts vertices on screen by transforming positions:

```text
model space (cube's own coords)
      │  × model matrix (rotate cube)
      ▼
world space
      │  × view matrix (camera)
      ▼
camera/view space
      │  × projection matrix (perspective)
      ▼
clip space → NDC → screen coords
```

VS **never** decides pixel colours or fills anything — only *"where are my corners, and here are the labels to interpolate."*

### Who does what with colour / texture

**Solid / vertex colours:**

```text
VS: pass vertex colours
Rasterizer: interpolate colours across pixels
FS: output that colour as-is
```

**Textured:**

```text
VS: pass UV coordinates (not the texture itself!)
Rasterizer: interpolate UVs across pixels
FS: at interpolated UV, sample the texture → RGB
```

Texture is **not** filled by the rasterizer — the FS **looks it up**.

---

## 5. What is a pixel in a shader

**A fragment/pixel in a shader is not the physical LED.** It's a logical sample the GPU asks you to colour.

```text
Physical screen          What GPU thinks
┌────────────────┐       ┌────────────────┐
│ LED LED LED LED│       │                │
│ LED LED LED LED│  ──►  │  grid cells:   │
│ LED LED LED LED│       │  "cell (12,34),
│ LED LED LED LED│       │   what RGB?"   │
└────────────────┘       └────────────────┘
 hardware                 address / index
```

- **Hardware pixel** = one lit square on the panel (shows whatever colour it's given)
- **Shader pixel/fragment** = a *request*: *"I'm about to light cell (12,34) — you tell me the colour"*

Analogy: the LED is a **bulb**; the FS is the **electrician deciding which colour bulb to switch on**.

Resolution = how many cells the GPU asks about (1280×720 ≈ 921,600 questions per frame).

---

## 6. Why "shader"

### Historical meaning

Originally "shading" = computing **how light hits a surface** — turning flat geometry into lit gradients (Gouraud, Phong). That math ran in small programs → "shader programs."

```text
flat shape  ──shading──►  looks 3D (light, shadow, gradient)
```

### Modern meaning

**A small program on the GPU** — even when it doesn't involve light:

| Era | What it "shaded" |
|-----|------------------|
| Early | Light/shadow on surfaces |
| 2000s | Also positions (VS), textures (FS) |
| Now | Virtually any per-vertex / per-pixel math |

- **Vertex shader** = "shade" (compute) each **vertex**
- **Fragment shader** = "shade" (compute) each **pixel**

**One-line:** *shader = a tiny program that answers the GPU's questions: "where does this vertex go?" / "what colour is this pixel?"*

---

## 7. Full pipeline diagram

```text
YOUR VERTEX BUFFER (CPU memory → GPU)
  points + colour labels + UVs + ...
        │
        ▼
┌─────────────────────────────────────────────┐
│ 1. VERTEX SHADER  (runs once per vertex)    │  ← SHADER
│    apply MVP; output position + labels      │
└─────────────────────────────────────────────┘
        │  transformed corners only
        ▼
┌─────────────────────────────────────────────┐
│ 2. RASTERIZER  (fixed-function hardware)    │  ← NOT a shader
│    - assemble triangles from index list     │
│    - test: which pixels are inside?         │
│    - interpolate labels (colour, UV, depth) │
│    - NO lines drawn — straight to fill      │
└─────────────────────────────────────────────┘
        │  one fragment candidate per covered pixel
        ▼
┌─────────────────────────────────────────────┐
│ 3. FRAGMENT SHADER  (runs once per pixel)   │  ← SHADER
│    - sample texture at interpolated UV      │
│    - lighting, fog, etc.                    │
│    - output final colour for this pixel     │
└─────────────────────────────────────────────┘
        │
        ▼
┌─────────────────────────────────────────────┐
│ 4. DEPTH TEST  (hardware)                   │  ← NOT a shader
│    - compare depth vs depth board           │
│    - keep or discard (nearer wins)          │
└─────────────────────────────────────────────┘
        │
        ▼
   colour written to framebuffer cell
   (later: physical LED shows it)
```

### Glossary

| Term | Means |
|------|-------|
| **Vertex** | A point with attached data (position, colour, UV…) |
| **Edge/line** | Conceptual connection; only drawn in wireframe mode |
| **Triangle** | The actual fillable unit |
| **Pixel (in shader)** | Logical colour question, not the LED |
| **Vertex shader** | Per-corner program (placement) |
| **Fragment shader** | Per-pixel program (colour) |
| **Rasterizer** | Hardware that fills triangles (not a shader) |
| **Depth test** | Hardware that keeps nearer pixels (not a shader) |
| **Barycentric weights** | `a,b,c` mix ratios used to interpolate across a triangle |

### Corrected mental model of "an object on screen"

```text
A cube is a recipe, not a wire sculpture:
  • points with labels              ← vertex buffer
  • "triangles = 0,1,2, 2,1,3, ..." ← index buffer
  • rules for corners (VS)          ← vertex shader
  • rules for pixels inside (FS)    ← fragment shader

Per frame:
  VS: move recipe into camera space (MVP)
  rasterizer: fill pixels of each triangle
  FS: decide colour of each filled pixel
  depth test: nearer pixel wins
  → framebuffer → screen
```
