#pragma once
#include "core/base/ring_buffer.h"
#include "core/framework/application.h"
#include "editor/editor_command.h"
#include "editor/editor_window.h"
#include "editor/menu_bar.h"
#include "scene/scene.h"
#include "shared/undo_stack.h"

namespace my {

class EditorLayer : public Layer, public EventListener {
public:
    enum State {
        STATE_TRANSLATE,
        STATE_ROTATE,
        STATE_SCALE,
    };

    EditorLayer();

    void Attach() override;
    void Detach() override;

    void Update(float dt) override;
    void Render() override;

    void SelectEntity(ecs::Entity p_selected);
    ecs::Entity GetSelectedEntity() const { return m_selected; }
    State GetState() const { return m_state; }
    void SetState(State p_state) { m_state = p_state; }

    uint64_t GetDisplayedImage() const { return m_displayedImage; }
    void SetDisplayedImage(uint64_t p_image) { m_displayedImage = p_image; }

    void BufferCommand(std::shared_ptr<ICommand>&& p_command);
    void AddComponent(ComponentType p_type, ecs::Entity p_target);
    void AddEntity(EntityType p_type, ecs::Entity p_parent);
    void RemoveEntity(ecs::Entity p_target);

    UndoStack& GetUndoStack() { return m_undoStack; }

    void EventReceived(std::shared_ptr<IEvent> p_event) override;

private:
    void DockSpace(Scene& p_scene);
    void DrawToolbar();
    void AddPanel(std::shared_ptr<EditorItem> p_panel);

    void FlushCommand(Scene& p_scene);

    std::shared_ptr<MenuBar> m_menuBar;
    std::vector<std::shared_ptr<EditorItem>> m_panels;
    ecs::Entity m_selected;
    State m_state{ STATE_TRANSLATE };

    uint64_t m_displayedImage = 0;
    std::list<std::shared_ptr<ICommand>> m_commandBuffer;
    UndoStack m_undoStack;

    ImageHandle* m_playButtonImage{ nullptr };
    ImageHandle* m_pauseButtonImage{ nullptr };
};

}  // namespace my