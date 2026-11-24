#include "Game.h"
#include "../core/Input.h"
#include "../physics/JoltBackend.h"
#include <cmath>

void Game::Init() {
    px_ = 0.0f;
    pz_ = 0.0f;

    physics_ = CreateJoltWorld();

    // tile platform: top surface at y=1.0, edges align with the visible grid
    physics_->CreateStaticBox(0.0f, 0.5f, 0.0f, 2.5f, 0.5f, 2.5f);
    // ground plane: top surface at y=0 under the visible grid (±10),
    // catches anything shoved off the platform edge
    physics_->CreateStaticBox(0.0f, -0.5f, 0.0f, 10.0f, 0.5f, 10.0f);

    // 6 dynamic cubes above the floor, clear of the player start at origin
    const float xs[3] = {-1.5f, 0.0f, 1.5f};
    for (int i = 0; i < 6; i++) {
        float x = xs[i % 3];
        float z = (i < 3) ? -1.5f : 1.5f;
        float y = 3.0f + (i % 3) * 1.5f;          // staggered heights
        physics_->CreateDynamicBox(x, y, z, 0.5f);
    }

    // kinematic player body — falling cubes bounce off it
    physics_->CreateKinematicBox(px_, kPlayerCenterY, pz_, kPlayerHalf);
}

void Game::Update(float fixed_dt) {
    float dx = 0.0f, dz = 0.0f;
    if (Input::down[Input::KEY_W]) dz -= 1.0f;
    if (Input::down[Input::KEY_S]) dz += 1.0f;
    if (Input::down[Input::KEY_A]) dx -= 1.0f;
    if (Input::down[Input::KEY_D]) dx += 1.0f;

    // normalize diagonal movement so W+D isn't faster than W alone
    float len = std::sqrt(dx * dx + dz * dz);
    if (len > 0.0f) { dx /= len; dz /= len; }

    const float SPEED = 3.0f;
    px_ += dx * SPEED * fixed_dt;
    pz_ += dz * SPEED * fixed_dt;

    // keep the player fully on the platform: platform edge ±2.5, player half 0.75
    const float limit = (kTileCount - 1) * kTileSpacing * 0.5f + 0.5f - kPlayerHalf;
    if (px_ >  limit) px_ =  limit;
    if (px_ < -limit) px_ = -limit;
    if (pz_ >  limit) pz_ =  limit;
    if (pz_ < -limit) pz_ = -limit;

    // push player target into physics, then step the world (readme §6 loop)
    physics_->MoveKinematic(px_, kPlayerCenterY, pz_);
    physics_->Update(fixed_dt);
}

void Game::Shutdown() {
    if (physics_) {
        physics_->Shutdown();
        physics_.reset();
    }
}