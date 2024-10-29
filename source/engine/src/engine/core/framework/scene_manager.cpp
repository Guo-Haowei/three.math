#include "scene_manager.h"

#include <imgui/imgui.h>

#include "core/debugger/profiler.h"
#include "core/framework/application.h"
#include "core/framework/asset_manager.h"
#include "core/framework/common_dvars.h"
#include "core/os/timer.h"
#include "rendering/rendering_dvars.h"

namespace my {

using ecs::Entity;
namespace fs = std::filesystem;

static void create_empty_scene(Scene* scene) {
    Entity::SetSeed();

    ivec2 frame_size = DVAR_GET_IVEC2(resolution);
    scene->CreateCamera(frame_size.x, frame_size.y);

    auto root = scene->CreateTransformEntity("world");
    scene->m_root = root;
    if (0) {
        auto light = scene->CreateInfiniteLightEntity("infinite light", vec3(1));
        auto transform = scene->GetComponent<TransformComponent>(light);
        DEV_ASSERT(transform);
        constexpr float rx = glm::radians(-80.0f);
        constexpr float ry = glm::radians(0.0f);
        constexpr float rz = glm::radians(0.0f);
        transform->Rotate(vec3(rx, ry, rz));

        scene->AttachComponent(light, root);
    }
}

bool SceneManager::Initialize() {
    // create an empty scene
    Scene* scene = new Scene;
    create_empty_scene(scene);
    m_scene = scene;

    const std::string& path = DVAR_GET_STRING(project);
    if (!path.empty()) {
        requestScene(path);
    }

    return true;
}

void SceneManager::Finalize() {}

bool SceneManager::trySwapScene() {
    auto queued_scene = m_loading_queue.pop_all();

    if (queued_scene.empty()) {
        return false;
    }

    while (!queued_scene.empty()) {
        auto task = queued_scene.front();
        queued_scene.pop();
        DEV_ASSERT(task.scene);
        if (m_scene && !task.replace) {
            m_scene->Merge(*task.scene);
            m_scene->Update(0.0f);
            delete task.scene;
        } else {
            Scene* old_scene = m_scene;
            m_scene = task.scene;
            delete old_scene;
        }
        ++m_revision;
    }

    return true;
}

void SceneManager::update(float dt) {
    OPTICK_EVENT();

    trySwapScene();

    if (m_last_revision < m_revision) {
        Timer timer;
        auto event = std::make_shared<SceneChangeEvent>(m_scene);
        LOG_WARN("offload p_scene properly");
        m_app->getEventQueue().DispatchEvent(event);
        LOG("[SceneManager] Detected p_scene changed from revision {} to revision {}, took {}", m_last_revision, m_revision, timer.getDurationString());
        m_last_revision = m_revision;
    }

    SceneManager::getScene().Update(dt);
}

void SceneManager::enqueueSceneLoadingTask(Scene* p_scene, bool p_replace) {
    m_loading_queue.push({ p_replace, p_scene });
}

void SceneManager::requestScene(std::string_view p_path) {
    FilePath path{ p_path };

    std::string ext = path.Extension();
    if (ext == ".lua" || ext == ".scene") {
        AssetManager::GetSingleton().loadSceneAsync(FilePath{ p_path }, [](void* p_scene, void*) {
            DEV_ASSERT(p_scene);
            Scene* new_scene = static_cast<Scene*>(p_scene);
            new_scene->Update(0.0f);
            SceneManager::GetSingleton().enqueueSceneLoadingTask(new_scene, true);
        });
    } else {
        AssetManager::GetSingleton().loadSceneAsync(FilePath{ p_path }, [](void* p_scene, void*) {
            DEV_ASSERT(p_scene);
            Scene* new_scene = static_cast<Scene*>(p_scene);
            new_scene->Update(0.0f);
            SceneManager::GetSingleton().enqueueSceneLoadingTask(new_scene, false);
        });
    }
}

Scene& SceneManager::getScene() {
    assert(GetSingleton().m_scene);
    return *GetSingleton().m_scene;
}

}  // namespace my
