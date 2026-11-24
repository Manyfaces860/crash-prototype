#pragma once

// Project-owned physics interface. Plain floats only — no Jolt types,
// so a future custom physics can replace Jolt without touching gameplay.
class IPhysicsWorld {
public:
    enum BodyKind { KIND_STATIC, KIND_DYNAMIC, KIND_KINEMATIC };

    virtual ~IPhysicsWorld() = default;

    virtual void Update(float dt) = 0;

    virtual void CreateStaticBox(float x, float y, float z,
                                 float hx, float hy, float hz) = 0;
    virtual void CreateDynamicBox(float x, float y, float z, float half) = 0;
    virtual void CreateKinematicBox(float x, float y, float z, float half) = 0;

    // Aim the kinematic body (player); applied at the start of the next Update.
    virtual void MoveKinematic(float x, float y, float z) = 0;

    virtual int  BodyCount() const = 0;
    virtual bool BodyPose(int i, float pos[3], float rot_quat[4]) const = 0;  // quat = x,y,z,w
    virtual BodyKind Kind(int i) const = 0;

    virtual void Shutdown() = 0;
};