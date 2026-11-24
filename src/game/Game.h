#pragma once
#include "../physics/IPhysicsWorld.h"
#include <memory>

// Pure game state + fixed-timestep update. No sokol, no Jolt types —
// main.cpp renders whatever this exposes.
class Game {
public:
    void Init();
    void Update(float fixed_dt);
    void Shutdown();

    // floor tiles (read by renderer)
    static constexpr int   kTileCount   = 5;      // 5×5 tiles
    static constexpr float kTileSpacing = 1.0f;   // tile centers, 1 unit apart (edges on grid lines)
    static constexpr float kTileCenterY = 0.5f;   // tile center height (1-unit tile rests on y=0)
    static float TileX(int gx) { return (gx - kTileCount / 2) * kTileSpacing; }
    static float TileZ(int gz) { return (gz - kTileCount / 2) * kTileSpacing; }

    // player cube (read by renderer)
    float player_x() const { return px_; }
    float player_z() const { return pz_; }
    static constexpr float kPlayerSize     = 1.5f;                        // cube edge length
    static constexpr float kPlayerHalf     = kPlayerSize * 0.5f;          // 0.75, half the cube
    static constexpr float kPlayerCenterY  = 1.0f + kPlayerHalf;          // 1.75: center when standing on platform top (y=1.0)

    IPhysicsWorld* physics() const { return physics_.get(); }

private:
    float px_ = 0.0f;
    float pz_ = 0.0f;
    std::unique_ptr<IPhysicsWorld> physics_;
};