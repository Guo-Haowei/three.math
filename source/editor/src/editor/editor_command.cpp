#include "editor_command.h"

#include "core/framework/common_dvars.h"
#include "drivers/windows/dialog.h"
#include "editor_layer.h"
// @TODO: refactor
#include "core/framework/scene_manager.h"

namespace my {

static std::string GenerateName(std::string_view p_name) {
    static int s_counter = 0;
    return std::format("{}-{}", p_name, ++s_counter);
}

/// EditorCommandAddEntity
void EditorCommandAddEntity::Execute(Scene& p_scene) {
    ecs::Entity id;
    switch (m_entityType) {
#define ENTITY_TYPE(ENUM, NAME, ...)                            \
    case EntityType::ENUM: {                                    \
        id = p_scene.Create##NAME##Entity(GenerateName(#NAME)); \
    } break;
        ENTITY_TYPE_LIST
#undef ENTITY_TYPE
        default:
            LOG_FATAL("Entity type {} not supported", static_cast<int>(m_entityType));
            break;
    }

    p_scene.AttachComponent(id, m_parent.IsValid() ? m_parent : p_scene.m_root);
    m_editor->SelectEntity(id);
    SceneManager::GetSingleton().BumpRevision();
}

/// EditorCommandAddComponent
void EditorCommandAddComponent::Execute(Scene& p_scene) {
    DEV_ASSERT(target.IsValid());
    switch (m_componentType) {
        case ComponentType::BOX_COLLIDER: {
            auto& collider = p_scene.Create<BoxColliderComponent>(target);
            collider.box = AABB::FromCenterSize(vec3(0), vec3(1));
        } break;
        case ComponentType::MESH_COLLIDER: {
            p_scene.Create<MeshColliderComponent>(target);
        } break;
        default: {
            CRASH_NOW();
        } break;
    }
}

/// EditorCommandRemoveEntity
void EditorCommandRemoveEntity::Execute(Scene& p_scene) {
    auto entity = m_target;
    DEV_ASSERT(entity.IsValid());
    p_scene.RemoveEntity(entity);
}

/// SaveProjectCommand
void SaveProjectCommand::Execute(Scene& p_scene) {
    const std::string& project = DVAR_GET_STRING(project);

    std::filesystem::path path{ project.empty() ? "untitled.scene" : project.c_str() };
    if (m_openDialog || project.empty()) {
// @TODO: implement
#if USING(PLATFORM_WINDOWS)
        if (!OpenSaveDialog(path)) {
            return;
        }
#else
        LOG_WARN("OpenSaveDialog not implemented");
#endif
    }

    DVAR_SET_STRING(project, path.string());

    Archive archive;
    if (!archive.OpenWrite(path.string())) {
        return;
    }

    if (p_scene.Serialize(archive)) {
        LOG_OK("scene saved to '{}'", path.string());
    }
}

/// RedoViewerCommand
void RedoViewerCommand::Execute(Scene&) {
    m_editor->GetUndoStack().Redo();
}

/// UndoViewerCommand
void UndoViewerCommand::Execute(Scene&) {
    m_editor->GetUndoStack().Undo();
}

/// TransformCommand
EntityTransformCommand::EntityTransformCommand(CommandType p_type,
                                               Scene& p_scene,
                                               ecs::Entity p_entity,
                                               const mat4& p_before,
                                               const mat4& p_after) : EditorUndoCommandBase(p_type),
                                                                      m_scene(p_scene),
                                                                      m_entity(p_entity),
                                                                      m_before(p_before),
                                                                      m_after(p_after) {
}

void EntityTransformCommand::Undo() {
    TransformComponent* transform = m_scene.GetComponent<TransformComponent>(m_entity);
    if (DEV_VERIFY(transform)) {
        transform->SetLocalTransform(m_before);
    }
}

void EntityTransformCommand::Redo() {
    TransformComponent* transform = m_scene.GetComponent<TransformComponent>(m_entity);
    if (DEV_VERIFY(transform)) {
        transform->SetLocalTransform(m_after);
    }
}

bool EntityTransformCommand::MergeCommand(const UndoCommand* p_command) {
    auto command = dynamic_cast<const EntityTransformCommand*>(p_command);
    if (!command) {
        return false;
    }

    if (command->m_entity != m_entity) {
        return false;
    }

    if (command->m_type != m_type) {
        return false;
    }

    m_after = command->m_after;
    return true;
}

}  // namespace my
