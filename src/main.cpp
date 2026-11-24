#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "cube.glsl.h"
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/Input.h"
#include "game/Game.h"
#include "physics/IPhysicsWorld.h"
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <vector>



struct VsParams { glm::mat4 mvp; glm::vec4 tint; };

// ---- cube: 24 verts (4 per face), 36 indices ----
// layout: position(3) + color(4) per vertex
static const float cube_vertices[] = {
    // +X face (pastel red)
     0.5f, -0.5f, -0.5f,  1, 0.5f, 0.5f, 1,   0.5f,  0.5f, -0.5f,  1, 0.5f, 0.5f, 1,
     0.5f,  0.5f,  0.5f,  1, 0.5f, 0.5f, 1,   0.5f, -0.5f,  0.5f,  1, 0.5f, 0.5f, 1,
    // -X face (pastel cyan)
    -0.5f, -0.5f,  0.5f,  0.5f, 1, 1, 1,  -0.5f,  0.5f,  0.5f,  0.5f, 1, 1, 1,
    -0.5f,  0.5f, -0.5f,  0.5f, 1, 1, 1,  -0.5f, -0.5f, -0.5f,  0.5f, 1, 1, 1,
    // +Y face (pastel green)
    -0.5f,  0.5f, -0.5f,  0.5f, 1, 0.5f, 1,  -0.5f,  0.5f,  0.5f,  0.5f, 1, 0.5f, 1,
     0.5f,  0.5f,  0.5f,  0.5f, 1, 0.5f, 1,   0.5f,  0.5f, -0.5f,  0.5f, 1, 0.5f, 1,
    // -Y face (pastel magenta)
    -0.5f, -0.5f,  0.5f,  1, 0.5f, 1, 1,  -0.5f, -0.5f, -0.5f,  1, 0.5f, 1, 1,
     0.5f, -0.5f, -0.5f,  1, 0.5f, 1, 1,   0.5f, -0.5f,  0.5f,  1, 0.5f, 1, 1,
    // +Z face (pastel blue)
    -0.5f, -0.5f,  0.5f,  0.5f, 0.5f, 1, 1,   0.5f, -0.5f,  0.5f,  0.5f, 0.5f, 1, 1,
     0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 1, 1,  -0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 1, 1,
    // -Z face (pastel yellow)
     0.5f, -0.5f, -0.5f,  1, 1, 0.5f, 1,  -0.5f, -0.5f, -0.5f,  1, 1, 0.5f, 1,
    -0.5f,  0.5f, -0.5f,  1, 1, 0.5f, 1,   0.5f,  0.5f, -0.5f,  1, 1, 0.5f, 1,
};

static const uint16_t cube_indices[] = {
     0,  1,  2,   0,  2,  3,    // +X
     4,  5,  6,   4,  6,  7,    // -X
     8,  9, 10,   8, 10, 11,    // +Y
    12, 13, 14,  12, 14, 15,    // -Y
    16, 17, 18,  16, 18, 19,    // +Z
    20, 21, 22,  20, 22, 23,    // -Z
};

// ---- sokol GPU state ----
static sg_buffer ibuf = {};
static sg_pass_action pass_action = {};
static sg_buffer vbuf = {};
static sg_pipeline pip = {};
static sg_bindings bind = {};
static sg_buffer grid_vbuf = {};
static sg_buffer grid_ibuf = {};
static sg_bindings bind_grid = {};
static int grid_index_count = 0;

// ---- project-owned game state ----
static Game game;

// ---- fixed timestep (readme §6, integrated into sokol's frame callback) ----
static constexpr double FIXED_DT = 1.0 / 60.0;
static double accumulator = 0.0;

// debug counters: prove updates/sec stay ~60 regardless of frames/sec
static double dbg_time = 0.0;
static int    dbg_updates = 0;
static int    dbg_frames = 0;

static void event_cb(const sapp_event* ev) {
    Input::Key key;
    switch (ev->key_code) {
        case SAPP_KEYCODE_W:      key = Input::KEY_W; break;
        case SAPP_KEYCODE_A:      key = Input::KEY_A; break;
        case SAPP_KEYCODE_S:      key = Input::KEY_S; break;
        case SAPP_KEYCODE_D:      key = Input::KEY_D; break;
        case SAPP_KEYCODE_SPACE:  key = Input::KEY_SPACE; break;
        case SAPP_KEYCODE_ESCAPE: key = Input::KEY_ESCAPE; break;
        default: return;
    }
    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN) {
        Input::SetKey(key, true);
        if (key == Input::KEY_ESCAPE) {
            sapp_quit();
        }
    } else if (ev->type == SAPP_EVENTTYPE_KEY_UP) {
        Input::SetKey(key, false);
    }
}

static void init(void) {
    sg_desc desc{};
    desc.logger.func = slog_func;
    desc.environment = sglue_environment();
    sg_setup(&desc);

    // clear: colour + depth
    pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass_action.colors[0].clear_value = {0.1f, 0.1f, 0.2f, 1.0f};
    pass_action.depth.load_action = SG_LOADACTION_CLEAR;
    pass_action.depth.clear_value = 1.0f;

    // 1) vertex buffer (cube)
    sg_buffer_desc vbuf_desc{};
    vbuf_desc.data = SG_RANGE(cube_vertices);
    vbuf_desc.label = "cube-vbuf";
    vbuf = sg_make_buffer(&vbuf_desc);

    // 2) index buffer
    sg_buffer_desc ibuf_desc{};
    ibuf_desc.data = SG_RANGE(cube_indices);
    ibuf_desc.usage.index_buffer = true;
    ibuf_desc.label = "cube-ibuf";
    ibuf = sg_make_buffer(&ibuf_desc);

    // 3) shader
    sg_shader shd = sg_make_shader(cube_shader_desc(sg_query_backend()));

    // 4) pipeline: attrs + index type + depth test
    sg_pipeline_desc pip_desc{};
    pip_desc.shader = shd;
    pip_desc.label = "cube-pipeline";
    pip_desc.layout.attrs[ATTR_cube_position].buffer_index = 0;
    pip_desc.layout.attrs[ATTR_cube_position].format = SG_VERTEXFORMAT_FLOAT3;
    pip_desc.layout.attrs[ATTR_cube_color0].buffer_index = 0;
    pip_desc.layout.attrs[ATTR_cube_color0].format = SG_VERTEXFORMAT_FLOAT4;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.depth.compare = SG_COMPAREFUNC_LESS;
    pip_desc.depth.write_enabled = true;
    pip = sg_make_pipeline(&pip_desc);

    // 5) bindings
    bind = {};
    bind.vertex_buffers[0] = vbuf;
    bind.index_buffer = ibuf;

    // 6) Blender-style floor grid: thin quads on y=0, ±10, spacing 1.0.
    //    Same vertex layout as cubes (pos3+color4) → same pipeline/shader.
    {
        std::vector<float> gv;             // pos(3) + color(4) per vertex
        std::vector<uint16_t> gi;
        const float ext = 10.0f;           // grid reaches ±10
        const float y   = 0.0f;            // the Blender ground plane
        const float hw  = 0.01f;           // half line thickness (0.02 total)

        auto add_quad = [&](float x0, float z0, float x1, float z1,
                            float r, float g, float b) {
            const uint16_t base = (uint16_t)(gv.size() / 7);
            const float c[4][2] = {{x0, z0}, {x1, z0}, {x1, z1}, {x0, z1}};
            for (const auto& p : c) {
                gv.insert(gv.end(), {p[0], y, p[1], r, g, b, 1.0f});
            }
            for (uint16_t i : {0, 1, 2, 0, 2, 3}) gi.push_back(base + i);
        };

        for (int i = -10; i <= 10; i++) {
            const float f = (float)i;
            // lines along X: red = X axis (z=0), else gray
            add_quad(-ext, f - hw, ext, f + hw,
                     i == 0 ? 0.90f : 0.35f, i == 0 ? 0.25f : 0.35f,
                     i == 0 ? 0.25f : 0.38f);
            // lines along Z: green = Z axis (x=0), else gray
            add_quad(f - hw, -ext, f + hw, ext,
                     i == 0 ? 0.30f : 0.35f, i == 0 ? 0.90f : 0.35f,
                     i == 0 ? 0.30f : 0.38f);
        }

        sg_buffer_desc gvb{};
        gvb.data.ptr = gv.data();                       // SG_RANGE would take the
        gvb.data.size = gv.size() * sizeof(float);      // vector object, not its data
        gvb.label = "grid-vbuf";
        grid_vbuf = sg_make_buffer(&gvb);

        sg_buffer_desc gib{};
        gib.data.ptr = gi.data();
        gib.data.size = gi.size() * sizeof(uint16_t);
        gib.usage.index_buffer = true;
        gib.label = "grid-ibuf";
        grid_ibuf = sg_make_buffer(&gib);

        bind_grid = {};
        bind_grid.vertex_buffers[0] = grid_vbuf;
        bind_grid.index_buffer = grid_ibuf;
        grid_index_count = (int)gi.size();
    }

    game.Init();
}

static void frame(void) {
    // ---- fixed-timestep update (project-owned game loop) ----
    double ft = sapp_frame_duration();
    if (ft > 0.25) ft = 0.25;               // clamp long hitches
    accumulator += ft;

    int updates_this_frame = 0;
    while (accumulator >= FIXED_DT) {        // runs 0, 1, 2… times per frame
        game.Update((float)FIXED_DT);        // always exactly 1/60 s
        accumulator -= FIXED_DT;
        updates_this_frame++;
    }
    float alpha = (float)(accumulator / FIXED_DT);   // reserved: render interpolation
    (void)alpha;

    dbg_time += ft;
    dbg_updates += updates_this_frame;
    dbg_frames++;
    if (dbg_time >= 2.0) {
        printf("[loop] %.1f updates/s  %.1f frames/s\n",
               dbg_updates / dbg_time, dbg_frames / dbg_time);
        dbg_time = 0.0;
        dbg_updates = 0;
        dbg_frames = 0;
    }

    // ---- render (sokol stays here; state comes from Game) ----
    float w = sapp_widthf(), hpx = sapp_heightf();
    int iw = (int)w, ih = (int)hpx;
    int half_w = iw / 2;
    float half_wf = (float)half_w;

    glm::mat4 view = glm::lookAt(
        glm::vec3(6.0f, 6.0f, 6.0f),   // (1,1,1) diagonal: iso angle
        glm::vec3(0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));

    sg_pass pass{};
    pass.action = pass_action;
    pass.swapchain = sglue_swapchain();
    sg_begin_pass(&pass);

    sg_apply_pipeline(pip);
    sg_apply_bindings(&bind);

    VsParams params{};
    params.tint = {0.0f, 0.0f, 0.0f, 0.0f};   // floor: no colour change

    // draw every DYNAMIC body at its physics pose (tumbling cubes)
    auto draw_bodies = [&](const glm::mat4& proj) {
        IPhysicsWorld* phys = game.physics();
        for (int i = 0; i < phys->BodyCount(); i++) {
            if (phys->Kind(i) != IPhysicsWorld::KIND_DYNAMIC) continue;
            float p[3], q[4];
            if (!phys->BodyPose(i, p, q)) continue;
            glm::mat4 model =
                glm::translate(glm::mat4(1.0f), glm::vec3(p[0], p[1], p[2])) *
                glm::mat4_cast(glm::quat(q[3], q[0], q[1], q[2]));   // (w,x,y,z)
            params.mvp = proj * view * model;
            params.tint = {0.0f, 0.0f, 0.0f, 0.0f};
            sg_apply_uniforms(0, SG_RANGE(params));
            sg_draw(0, 36, 1);
        }
    };

    // draw the Blender-style grid (thin quads on y=0); restores cube bindings after
    auto draw_grid = [&](const glm::mat4& proj) {
        sg_apply_bindings(&bind_grid);
        params.tint = {0.0f, 0.0f, 0.0f, 0.0f};
        params.mvp = proj * view * glm::mat4(1.0f);
        sg_apply_uniforms(0, SG_RANGE(params));
        sg_draw(0, grid_index_count, 1);
        sg_apply_bindings(&bind);   // cube draws below keep using `bind`
    };

    // tiles + player + dynamic bodies for one viewport
    auto draw_scene = [&](const glm::mat4& proj) {
        params.tint = {0.0f, 0.0f, 0.0f, 0.0f};   // floor tiles: no tint
        for (int gz = 0; gz < Game::kTileCount; gz++) {
            for (int gx = 0; gx < Game::kTileCount; gx++) {
                glm::mat4 model = glm::translate(glm::mat4(1.0f),
                    glm::vec3(Game::TileX(gx), Game::kTileCenterY, Game::TileZ(gz)));
                params.mvp = proj * view * model;
                sg_apply_uniforms(0, SG_RANGE(params));
                sg_draw(0, 36, 1);
            }
        }
        // player cube — scaled up + orange tint so it stands out
        {
            glm::mat4 model = glm::translate(glm::mat4(1.0f),
                glm::vec3(game.player_x(), Game::kPlayerCenterY, game.player_z()));
            model = glm::scale(model, glm::vec3(Game::kPlayerSize));
            params.mvp = proj * view * model;
            params.tint = {1.0f, 0.45f, 0.1f, 0.65f};   // warm orange overlay
            sg_apply_uniforms(0, SG_RANGE(params));
            sg_draw(0, 36, 1);
        }
        draw_bodies(proj);
    };

    // LEFT — isometric: orthographic, bigger
    {
        float h_l = 4.5f;
        sg_apply_viewport(0, 0, half_w, ih, true);
        float asp_l = half_wf / hpx;
        glm::mat4 proj_l = glm::orthoRH_ZO(-h_l * asp_l, h_l * asp_l, -h_l, h_l,
                                            0.1f, 100.0f);
        draw_grid(proj_l);
        draw_scene(proj_l);
    }

    // RIGHT — perspective, smaller (wider fov)
    {
        float h_r = 6.0f;
        sg_apply_viewport(half_w, 0, iw - half_w, ih, true);
        float asp_r = (float)(iw - half_w) / hpx;
        float dist  = glm::length(glm::vec3(6.0f, 6.0f, 6.0f));
        float fov   = 2.0f * glm::atan(h_r / dist);
        glm::mat4 proj_r = glm::perspectiveRH_ZO(fov, asp_r, 0.1f, 100.0f);
        draw_grid(proj_r);
        draw_scene(proj_r);
    }

    sg_end_pass();
    sg_commit();

    Input::Update();   // clear key edges for next frame
}

static void cleanup(void) {
    game.Shutdown();
    sg_destroy_pipeline(pip);
    sg_destroy_buffer(vbuf);
    sg_destroy_buffer(ibuf);
    sg_destroy_buffer(grid_vbuf);
    sg_destroy_buffer(grid_ibuf);
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    sapp_desc desc{};
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.event_cb = event_cb;
    desc.cleanup_cb = cleanup;
    desc.width = 1280;
    desc.height = 720;
    desc.window_title = "CrashPrototype";
    desc.icon.sokol_default = true;
    desc.logger.func = slog_func;
    return desc;
}
