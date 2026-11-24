# CrashPrototype

A C++20 3D platformer prototype inspired by Crash Bandicoot, built as a learning project for understanding game development and gradually building a custom game engine.

## 1. Project Vision

The goal is to build a small, playable 3D platformer while progressively learning the underlying technologies involved in game engine development.

The project follows a **high-level-to-low-level development approach**:

1. Initially use established open-source libraries for rendering, physics, window management, and asset loading.
2. Write the game loop, gameplay systems, architecture, and game logic ourselves in C++.
3. Gradually replace third-party systems with custom implementations.
4. Eventually have a playable game running on a largely self-developed engine.

This is not intended to be a game built using an existing game engine such as Unity or Unreal Engine.

The long-term objective is to understand and implement the fundamentals of a game engine, including:

- Rendering and graphics programming.
- Physics and collision detection.
- Memory management.
- Game loops and timing.
- Input handling.
- Asset management.
- Animation.
- Scene and entity management.

## 2. Development Philosophy

Follow these principles throughout development:

- **Build incrementally:** Every milestone should leave the project in a working state.
- **Understand before abstracting:** Prefer simple implementations over premature architectural complexity.
- **Own the game logic:** Core gameplay systems must be written in C++ by us.
- **Keep dependencies replaceable:** Third-party libraries should be isolated behind small interfaces.
- **Avoid unnecessary frameworks:** Do not introduce an ECS, scripting engine, or large middleware stack without a concrete need.
- **Prioritize functionality:** Use primitive geometry and placeholder assets before investing in polished graphics.
- **Maintain portability:** Prefer cross-platform C++ and avoid unnecessary platform-specific dependencies.

## 3. Technology Stack

### Primary language
- C++20

### Build system
- CMake
- Ninja

### Compiler
- Apple Clang on macOS.
- Maintain compatibility with Clang and GCC where practical.

### Graphics and window management
- Sokol
  - `sokol_app.h` for application lifecycle and window management.
  - `sokol_gfx.h` for graphics rendering.
  - Use the Metal backend on macOS.

### Physics
- Jolt Physics

Initially, Jolt handles:
- Rigid-body simulation.
- Gravity.
- Collision detection.
- Collision response.
- Raycasts and shape casts.
- Character controller support.

### Mathematics
- GLM

Used for:
- Vectors.
- Matrices.
- Quaternions.
- Transformations.
- Camera mathematics.

### Asset loading
- cgltf
- glTF 2.0 / GLB asset format.

### Modelling
- Blender

### Version control
- Git

## 4. Project Architecture

Maintain a modular architecture.

Proposed structure:

```text
CrashPrototype/
│
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── .gitignore
│
├── external/
│   ├── sokol/
│   └── other_dependencies/
│
├── assets/
│   ├── models/
│   ├── textures/
│   ├── shaders/
│   └── levels/
│
├── src/
│   ├── main.cpp
│   │
│   ├── core/
│   │   ├── Application.h
│   │   ├── Application.cpp
│   │   ├── GameLoop.h
│   │   ├── GameLoop.cpp
│   │   ├── Time.h
│   │   └── Input.h
│   │
│   ├── game/
│   │   ├── Game.h
│   │   ├── Game.cpp
│   │   ├── Player.h
│   │   ├── Player.cpp
│   │   ├── Camera.h
│   │   ├── Camera.cpp
│   │   ├── Crate.h
│   │   ├── Crate.cpp
│   │   ├── Collectible.h
│   │   ├── Collectible.cpp
│   │   ├── Level.h
│   │   └── Level.cpp
│   │
│   ├── physics/
│   │   ├── IPhysicsWorld.h
│   │   ├── PhysicsWorld.cpp
│   │   └── JoltBackend.cpp
│   │
│   ├── graphics/
│   │   ├── IRenderer.h
│   │   ├── Renderer.cpp
│   │   ├── SokolRenderer.cpp
│   │   ├── Mesh.h
│   │   ├── Material.h
│   │   └── CameraRenderer.cpp
│   │
│   └── math/
│       └── Transform.h
│
└── build/
```

This is a proposed structure, not a requirement to create every file immediately. Add files when their functionality is needed.

## 5. Architectural Boundaries

The most important architectural requirement is that game logic must not become tightly coupled to third-party libraries.

### Physics abstraction

Gameplay systems should interact with physics through project-owned interfaces.

For example:

```cpp
class IPhysicsWorld
{
public:
    virtual void Update(float deltaTime) = 0;

    virtual ~IPhysicsWorld() = default;
};
```

The Jolt implementation should live behind this interface.

Eventually, a custom physics implementation should be able to replace Jolt without requiring changes to gameplay code.

### Rendering abstraction

Similarly, gameplay should not directly invoke Sokol rendering functions.

Use a project-owned renderer interface.

The Sokol implementation will be the initial rendering backend.

Eventually, this interface will allow a custom Metal renderer to replace Sokol.

### Important rule

Do not expose Jolt types, Sokol types, or third-party implementation details in public gameplay interfaces unless absolutely necessary.

Avoid creating excessive abstractions before they are needed.

## 6. Game Loop

The game loop must be implemented by us.

Use a fixed timestep for simulation, initially targeting 60 updates per second.

Conceptual implementation:

```cpp
constexpr double FIXED_DT = 1.0 / 60.0;

double accumulator = 0.0;
double previousTime = GetTime();

while (running)
{
    double currentTime = GetTime();
    double frameTime = currentTime - previousTime;

    previousTime = currentTime;

    frameTime = std::min(frameTime, 0.25);
    accumulator += frameTime;

    Input::Update();

    while (accumulator >= FIXED_DT)
    {
        game.Update(FIXED_DT);
        physics.Update(FIXED_DT);

        accumulator -= FIXED_DT;
    }

    float alpha = static_cast<float>(
        accumulator / FIXED_DT
    );

    renderer.BeginFrame();
    game.Render(renderer, alpha);
    renderer.EndFrame();
}
```

This is conceptual pseudocode. Since Sokol manages the platform application lifecycle, integrate this simulation structure into its frame callback rather than introducing a separate competing window loop.

The game loop should eventually support:

- Fixed-timestep simulation.
- Variable rendering frequency.
- Frame interpolation.
- Input processing.
- Game updates.
- Physics updates.
- Rendering.

## 7. Phase 1: Playable Prototype

The immediate priority is to build a playable 3D platformer using Sokol and Jolt.

### Milestone 1: Rendering foundation

- Initialize Sokol.
- Create a macOS application window.
- Clear the screen.
- Render a triangle.
- Render a rotating cube.
- Implement a basic perspective camera.

Acceptance criteria:
- The application launches successfully.
- The window responds correctly to close events.
- A rotating 3D cube is visible.

### Milestone 2: Game loop and input

- Implement the project-owned game loop.
- Add frame timing.
- Implement fixed-timestep updates.
- Process keyboard input.
- Introduce the Game class and application lifecycle.

Acceptance criteria:
- Input is processed correctly.
- Game updates are independent of rendering frequency.

### Milestone 3: Physics integration

- Integrate Jolt Physics.
- Initialize the physics world.
- Create a static floor.
- Create dynamic rigid bodies.
- Enable gravity.
- Test collision detection and response.

Acceptance criteria:
- A dynamic cube falls onto the floor.
- Objects respond to collisions correctly.

### Milestone 4: Player controller

- Create a capsule character.
- Implement walking and running.
- Implement acceleration and friction.
- Implement jumping.
- Implement grounded-state detection.
- Add a following camera.

Movement should be controlled by our own gameplay code using Jolt's character and physics facilities.

Acceptance criteria:
- The character moves predictably.
- The character can jump.
- The character remains grounded on the floor.
- The camera follows the character smoothly.

### Milestone 5: Core gameplay

- Create wooden crates.
- Implement a spin attack.
- Destroy crates when attacked.
- Add collectible fruits.
- Introduce a basic level.
- Add a restart mechanism.

Acceptance criteria:
- The player can move, jump and spin.
- Crates can be destroyed.
- Collectibles can be collected.
- The level can be restarted.

## 8. Phase 2: Custom Physics

After Phase 1, progressively replace Jolt.

Suggested order:

1. Implement a custom character controller.
2. Implement capsule collision against static geometry.
3. Implement raycasts and shape casts.
4. Implement broadphase collision detection.
5. Implement narrowphase collision detection.
6. Implement collision response.
7. Implement dynamic rigid-body simulation.

Do not remove Jolt completely until the replacement system supports the functionality required by the game.

Jolt can remain responsible for selected systems during the transition.

## 9. Phase 3: Custom Renderer

After the gameplay and physics foundations are stable, progressively replace Sokol.

Target macOS Metal initially.

Suggested implementation order:

1. Metal device and command queue initialization.
2. Swapchain and drawable management.
3. Vertex and index buffers.
4. Shader compilation and pipeline states.
5. Basic mesh rendering.
6. Camera and transformation matrices.
7. Textures and materials.
8. Lighting.
9. Shadows.
10. Post-processing.

The renderer should eventually operate independently of Sokol.

## 10. Coding Conventions

- Use C++20.
- Prefer RAII and standard C++ resource management.
- Use smart pointers where ownership requires them.
- Avoid raw owning pointers.
- Use `const` wherever appropriate.
- Keep classes focused on a single responsibility.
- Separate declarations (`.h`) from implementations (`.cpp`) where useful.
- Use descriptive names.
- Avoid unnecessary global state.
- Avoid premature optimization.
- Do not introduce dependencies without justification.
- Keep platform-specific code isolated.

## 11. Build Instructions

Prerequisites on macOS:

```bash
xcode-select --install

brew install cmake ninja git
```

Configure the project:

```bash
cmake -S . -B build -G Ninja
```

Build:

```bash
cmake --build build
```

Run the executable once the target has been configured:

```bash
./build/CrashPrototype
```

The exact executable location may vary depending on the CMake generator and target configuration.

## 12. Instructions for AI Coding Agents

When working on this repository, follow these instructions:

1. Read this README before making architectural decisions.
2. Understand the existing code before modifying it.
3. Implement only the current milestone unless explicitly asked to do more.
4. Do not create the entire game or engine in one pass.
5. Do not introduce unnecessary libraries or frameworks.
6. Do not replace the chosen technology stack without discussing it first.
7. Keep third-party dependencies isolated from gameplay systems.
8. Preserve the ability to replace Jolt and Sokol later.
9. Explain important architectural decisions.
10. Prefer small, independently buildable changes.
11. Build and test after implementing a feature.
12. Fix compilation errors before moving to another feature.
13. Do not claim something works unless it has actually been built or tested.
14. Update this README when significant architectural or technology decisions change.
15. Do not write code, run install commands, or modify files unless explicitly asked. Provide advice, plans, and snippets for the user to apply themselves.

### How to approach tasks

For every requested feature:

1. Explain the proposed implementation briefly.
2. Identify which existing modules will be affected.
3. Implement the smallest working solution.
4. Build the project.
5. Report what was changed and what was tested.
6. Mention any limitations or outstanding issues.

If a requirement conflicts with the architecture, explain the trade-off before introducing a significant architectural change.

## Learning notes

Topic guides in [docs/](docs/README.md):

- [Sokol & app structure](docs/sokol-and-app.md)
- [Rendering concepts](docs/rendering-concepts.md)
- [GPU pipeline fundamentals](docs/gpu-pipeline.md)

## 13. Current Project Status

**Current phase:** Phase 1 – Playable Prototype

**Current milestone:** Milestone 4 – Player Controller

**Completed:**
- Project direction defined.
- Initial technology stack selected.
- Architecture proposed.
- Development milestones established.
- CMake project configuration.
- Sokol integration and macOS application window.
- Graphics initialization and screen clearing.
- Triangle and rotating cube rendering.
- Perspective camera (GLM), with iso/perspective split-view comparison.
- Project-owned game loop and fixed-timestep updates (M2).
- Keyboard input (M2).
- Jolt Physics integration: static platform + ground plane, 6 dynamic cubes, kinematic player (M3).
- Blender-style floor grid: thin-quad grid at y=0 (±10, spacing 1.0), colored X/Z axes, rendered in both viewports (M3).

**Not yet implemented:**
- Player controller: acceleration, friction, jump, grounded detection (M4).
- Following camera (M4).
- Gameplay systems (M5+).

## 14. Definition of Success

The Phase 1 objective is complete when we have a playable 3D platformer prototype featuring:

- A controllable player.
- Walking, running and jumping.
- A following camera.
- A basic 3D level.
- Destructible crates.
- Collectible objects.
- A working physics simulation.
- A stable custom game loop.

The project should be structured so that Jolt Physics and Sokol can eventually be replaced without rewriting the gameplay systems.

**The ultimate goal is not merely to make a game. It is to understand how the game works internally, one subsystem at a time.**