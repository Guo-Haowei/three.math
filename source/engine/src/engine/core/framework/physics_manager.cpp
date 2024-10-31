#include "physics_manager.h"

#include "core/framework/scene_manager.h"
#include "scene/scene.h"

#pragma warning(push)
#pragma warning(disable : 4127)
#include "bullet3/btBulletDynamicsCommon.h"
#pragma warning(pop)

namespace my {

bool PhysicsManager::Initialize() {
    return true;
}

void PhysicsManager::Finalize() {
    CleanWorld();
}

void PhysicsManager::EventReceived(std::shared_ptr<Event> p_event) {
    SceneChangeEvent* e = dynamic_cast<SceneChangeEvent*>(p_event.get());
    if (!e) {
        return;
    }

    const Scene& scene = *e->GetScene();
    // @TODO: fix
    CreateWorld(scene);
}

void PhysicsManager::Update(Scene& p_scene) {
    float delta_time = p_scene.m_elapsedTime;

    if (HasWorld()) {
        m_dynamicWorld->stepSimulation(delta_time, 10);

        for (int j = m_dynamicWorld->getNumCollisionObjects() - 1; j >= 0; j--) {
            btCollisionObject* collision_object = m_dynamicWorld->getCollisionObjectArray()[j];
            btRigidBody* rigid_body = btRigidBody::upcast(collision_object);
            btTransform transform;

            if (rigid_body && rigid_body->getMotionState()) {
                rigid_body->getMotionState()->getWorldTransform(transform);
            } else {
                transform = collision_object->getWorldTransform();
            }

            uint32_t handle = (uint32_t)(uintptr_t)collision_object->getUserPointer();
            ecs::Entity id{ handle };
            if (id.IsValid()) {
                TransformComponent& transform_component = *p_scene.GetComponent<TransformComponent>(id);
                const btVector3& origin = transform.getOrigin();
                const btQuaternion rotation = transform.getRotation();
                transform_component.SetTranslation(vec3(origin.getX(), origin.getY(), origin.getZ()));
                transform_component.SetRotation(vec4(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW()));
            }
        }
    }
}

void PhysicsManager::CreateWorld(const Scene& p_scene) {
    m_collisionConfig = new btDefaultCollisionConfiguration();
    m_dispatcher = new btCollisionDispatcher(m_collisionConfig);
    m_overlappingPairCache = new btDbvtBroadphase();
    m_solver = new btSequentialImpulseConstraintSolver;
    m_dynamicWorld = new btDiscreteDynamicsWorld(m_dispatcher, m_overlappingPairCache, m_solver, m_collisionConfig);

    m_dynamicWorld->setGravity(btVector3(0, -10, 0));

    for (auto [id, rigid_body] : p_scene.m_RigidBodyComponents) {
        const TransformComponent* transform_component = p_scene.GetComponent<TransformComponent>(id);
        DEV_ASSERT(transform_component);
        if (!transform_component) {
            continue;
        }

        btCollisionShape* shape = nullptr;
        switch (rigid_body.shape) {
            case RigidBodyComponent::SHAPE_CUBE: {
                const vec3& half = rigid_body.param.box.half_size;
                shape = new btBoxShape(btVector3(half.x, half.y, half.z));
                break;
            }
            case RigidBodyComponent::SHAPE_SPHERE: {
                shape = new btSphereShape(rigid_body.param.sphere.radius);
                break;
            }
            default:
                CRASH_NOW_MSG("unknown rigidBody.shape");
                break;
        }

        m_collisionShapes.push_back(shape);

        const vec3& origin = transform_component->GetTranslation();
        const vec4& rotation = transform_component->GetRotation();
        btTransform transform;
        transform.setIdentity();
        transform.setOrigin(btVector3(origin.x, origin.y, origin.z));
        transform.setRotation(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));

        btScalar mass(rigid_body.mass);
        bool isDynamic = (mass != 0.f);  // rigidbody is dynamic if and only if mass is non zero, otherwise static

        btVector3 localInertia(0, 0, 0);
        if (isDynamic) {
            shape->calculateLocalInertia(mass, localInertia);
        }

        // using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
        btDefaultMotionState* myMotionState = new btDefaultMotionState(transform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, shape, localInertia);
        btRigidBody* body = new btRigidBody(rbInfo);
        body->setUserPointer((void*)((size_t)id.GetId()));
        m_dynamicWorld->addRigidBody(body);
    }
}

void PhysicsManager::CleanWorld() {
    if (HasWorld()) {
        // remove the rigidbodies from the dynamics world and delete them
        for (int i = m_dynamicWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
            btCollisionObject* obj = m_dynamicWorld->getCollisionObjectArray()[i];
            btRigidBody* body = btRigidBody::upcast(obj);
            if (body && body->getMotionState()) {
                delete body->getMotionState();
            }
            m_dynamicWorld->removeCollisionObject(obj);
            delete obj;
        }

        // delete collision shapes
        for (int j = 0; j < m_collisionShapes.size(); j++) {
            btCollisionShape* shape = m_collisionShapes[j];
            m_collisionShapes[j] = 0;
            delete shape;
        }

        // delete dynamics world
        delete m_dynamicWorld;
        m_dynamicWorld = nullptr;

        // delete m_solver
        delete m_solver;
        m_solver = nullptr;

        // delete broadphase
        delete m_overlappingPairCache;
        m_overlappingPairCache = nullptr;

        // delete m_dispatcher
        delete m_dispatcher;
        m_dispatcher = nullptr;

        delete m_collisionConfig;
        m_collisionConfig = nullptr;

        m_collisionShapes.clear();
    }
}

}  // namespace my
