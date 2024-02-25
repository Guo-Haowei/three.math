#pragma once
#include "scene/scene.h"

namespace my {

class EditorLayer;

class EditorItem {
public:
    inline static constexpr const char* DRAG_DROP_ENV = "DRAG_DROP_ENV";

    EditorItem(EditorLayer& p_editor) : m_editor(p_editor) {}
    virtual ~EditorItem() = default;

    virtual void update(Scene&) = 0;

protected:
    void open_add_entity_popup(ecs::Entity p_parent);

    EditorLayer& m_editor;
};

}  // namespace my
