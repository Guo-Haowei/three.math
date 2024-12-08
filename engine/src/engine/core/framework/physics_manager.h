#pragma once
#include "engine/core/base/singleton.h"
#include "engine/core/framework/event_queue.h"
#include "engine/core/framework/module.h"

class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btBroadphaseInterface;
class btSequentialImpulseConstraintSolver;
class btSoftRigidDynamicsWorld;
class btCollisionShape;
struct btSoftBodyWorldInfo;

namespace my {

class Scene;

class PhysicsManager : public Module, public EventListener {
public:
    PhysicsManager() : Module("PhysicsManager") {}

    auto InitializeImpl() -> Result<void> override;
    void FinalizeImpl() override;

    void Update(Scene& p_scene);

    void EventReceived(std::shared_ptr<IEvent> p_event) override;

protected:
    void CreateWorld(const Scene& p_scene);
    void CleanWorld();
    bool HasWorld() const { return m_collisionConfig != nullptr; }

    btDefaultCollisionConfiguration* m_collisionConfig = nullptr;
    btCollisionDispatcher* m_dispatcher = nullptr;
    btBroadphaseInterface* m_broadphase = nullptr;
    btSequentialImpulseConstraintSolver* m_solver = nullptr;
    btSoftRigidDynamicsWorld* m_dynamicWorld = nullptr;
    btSoftBodyWorldInfo* m_softBodyWorldInfo = nullptr;
    std::vector<btCollisionShape*> m_collisionShapes;
};

}  // namespace my
