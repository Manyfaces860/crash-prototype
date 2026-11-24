#include "JoltBackend.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <cstdarg>
#include <cstdio>
#include <thread>
#include <vector>

JPH_SUPPRESS_WARNINGS
using namespace JPH;
using namespace JPH::literals;

namespace {

// ---- trace / asserts (HelloWorld port) ----
void TraceImpl(const char* inFMT, ...) {
    va_list list; va_start(list, inFMT);
    char buffer[1024]; vsnprintf(buffer, sizeof(buffer), inFMT, list); va_end(list);
    std::printf("%s\n", buffer);
}
#ifdef JPH_ENABLE_ASSERTS
bool AssertFailedImpl(const char* e, const char* m, const char* f, uint line) {
    std::printf("%s:%u: (%s) %s\n", f, line, e, m ? m : "");
    return true;
}
#endif

// ---- object + broadphase layers (HelloWorld port) ----
namespace Layers {
    constexpr ObjectLayer NON_MOVING = 0;
    constexpr ObjectLayer MOVING     = 1;
    constexpr ObjectLayer NUM_LAYERS = 2;
}

class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter {
public:
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override {
        switch (a) {
            case Layers::NON_MOVING: return b == Layers::MOVING;
            case Layers::MOVING:     return true;
            default: JPH_ASSERT(false); return false;
        }
    }
};

namespace BroadPhaseLayers {
    constexpr BroadPhaseLayer NON_MOVING(0);
    constexpr BroadPhaseLayer MOVING(1);
    constexpr uint NUM_LAYERS(2);
}

class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        mMap[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mMap[Layers::MOVING]     = BroadPhaseLayers::MOVING;
    }
    uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override {
        JPH_ASSERT(l < Layers::NUM_LAYERS);
        return mMap[l];
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(BroadPhaseLayer l) const override {
        switch ((BroadPhaseLayer::Type)l) {
            case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:     return "MOVING";
            default: JPH_ASSERT(false); return "INVALID";
        }
    }
#endif
private:
    BroadPhaseLayer mMap[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(ObjectLayer l1, BroadPhaseLayer l2) const override {
        switch (l1) {
            case Layers::NON_MOVING: return l2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:     return true;
            default: JPH_ASSERT(false); return false;
        }
    }
};

// ---- one-time global Jolt init/teardown (runs at load, before main) ----
struct JoltGlobal {
    JoltGlobal() {
        RegisterDefaultAllocator();
        Trace = TraceImpl;
        JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)
        Factory::sInstance = new Factory();
        RegisterTypes();
    }
    ~JoltGlobal() {
        UnregisterTypes();
        delete Factory::sInstance;
        Factory::sInstance = nullptr;
    }
};
static JoltGlobal s_global;

// ---- the world ----
struct BodyRec { BodyID id; IPhysicsWorld::BodyKind kind; };

class JoltWorld final : public IPhysicsWorld {
public:
    JoltWorld()
      : mTemp(10 * 1024 * 1024),
        mJobs(cMaxPhysicsJobs, cMaxPhysicsBarriers,
              std::thread::hardware_concurrency() > 1
                  ? std::thread::hardware_concurrency() - 1 : 1)
    {
        // maxBodies, bodyMutexes, maxBodyPairs, maxContactConstraints (HelloWorld values)
        mSystem.Init(1024, 0, 1024, 1024,
                     mBpInterface, mObjVsBpFilter, mObjPairFilter);
        mBody = &mSystem.GetBodyInterface();
    }

    void Update(float dt) override {
        if (mHasPlayer) {
            // kinematic move: computes velocity internally, pushes dynamic bodies aside
            mBody->MoveKinematic(mPlayerId,
                                 RVec3(mTgtX, mTgtY, mTgtZ),
                                 Quat::sIdentity(), dt);
        }
        if (!mOptimized) { mSystem.OptimizeBroadPhase(); mOptimized = true; }
        mSystem.Update(dt, 1, &mTemp, &mJobs);   // 1 collision step per 1/60 s
    }

    void CreateStaticBox(float x, float y, float z,
                         float hx, float hy, float hz) override {
        BodyCreationSettings s(new BoxShape(Vec3(hx, hy, hz)),
                               RVec3(x, y, z), Quat::sIdentity(),
                               EMotionType::Static, Layers::NON_MOVING);
        mBodies.push_back({mBody->CreateAndAddBody(s, EActivation::DontActivate),
                           KIND_STATIC});
        mOptimized = false;
    }

    void CreateDynamicBox(float x, float y, float z, float half) override {
        BodyCreationSettings s(new BoxShape(Vec3(half, half, half)),
                               RVec3(x, y, z), Quat::sIdentity(),
                               EMotionType::Dynamic, Layers::MOVING);
        mBodies.push_back({mBody->CreateAndAddBody(s, EActivation::Activate),
                           KIND_DYNAMIC});
        mOptimized = false;
    }

    void CreateKinematicBox(float x, float y, float z, float half) override {
        BodyCreationSettings s(new BoxShape(Vec3(half, half, half)),
                               RVec3(x, y, z), Quat::sIdentity(),
                               EMotionType::Kinematic, Layers::MOVING);
        mPlayerId = mBody->CreateAndAddBody(s, EActivation::DontActivate);
        mBodies.push_back({mPlayerId, KIND_KINEMATIC});
        mTgtX = x; mTgtY = y; mTgtZ = z;
        mHasPlayer = true;
        mOptimized = false;
    }

    void MoveKinematic(float x, float y, float z) override {
        mTgtX = x; mTgtY = y; mTgtZ = z;
    }

    int BodyCount() const override { return (int)mBodies.size(); }

    bool BodyPose(int i, float pos[3], float rot[4]) const override {
        if (i < 0 || i >= (int)mBodies.size()) return false;
        RVec3 p = mBody->GetPosition(mBodies[i].id);
        Quat  q = mBody->GetRotation(mBodies[i].id);
        pos[0] = (float)p.GetX(); pos[1] = (float)p.GetY(); pos[2] = (float)p.GetZ();
        rot[0] = q.GetX(); rot[1] = q.GetY(); rot[2] = q.GetZ(); rot[3] = q.GetW();
        return true;
    }

    BodyKind Kind(int i) const override { return mBodies[i].kind; }

    void Shutdown() override {
        for (auto& b : mBodies) {
            mBody->RemoveBody(b.id);
            mBody->DestroyBody(b.id);
        }
        mBodies.clear();
        mHasPlayer = false;
    }

private:
    // declaration order matters: filters must outlive mSystem (constructed first)
    BPLayerInterfaceImpl             mBpInterface;
    ObjectVsBroadPhaseLayerFilterImpl mObjVsBpFilter;
    ObjectLayerPairFilterImpl        mObjPairFilter;
    PhysicsSystem                    mSystem;
    TempAllocatorImpl                 mTemp;
    JobSystemThreadPool               mJobs;
    BodyInterface*                    mBody = nullptr;
    std::vector<BodyRec>              mBodies;
    BodyID                            mPlayerId;
    bool                              mHasPlayer = false;
    float                             mTgtX = 0, mTgtY = 0, mTgtZ = 0;
    bool                              mOptimized = false;
};

} // namespace

std::unique_ptr<IPhysicsWorld> CreateJoltWorld() {
    return std::make_unique<JoltWorld>();
}