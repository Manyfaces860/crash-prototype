#pragma once
#include "IPhysicsWorld.h"
#include <memory>

std::unique_ptr<IPhysicsWorld> CreateJoltWorld();