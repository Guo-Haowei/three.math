#pragma once
#include "engine/core/framework/event_queue.h"
#include "engine/core/io/input_code.h"
#include "engine/core/math/geomath.h"

namespace my {

enum class InputState : uint8_t {
    UNKNOWN = 0,
    PRESSED,
    HOLD,
    RELEASED,
    // @TODO: TOGGLE?
};

class InputEventWithModifier : public IEvent {
public:
    bool IsAltPressed() const { return m_altPressed; }
    bool IsShiftPressed() const { return m_shiftPressed; }
    bool IsCtrlPressed() const { return m_ctrlPressed; }
    bool IsModiferPressed() const { return m_altPressed || m_shiftPressed || m_ctrlPressed; }

protected:
    bool m_altPressed;
    bool m_shiftPressed;
    bool m_ctrlPressed;

    friend class InputManager;
};

class InputEventKey : public InputEventWithModifier {
public:
    bool IsPressed() const { return m_state == InputState::PRESSED; }
    bool IsHolding() const { return m_state == InputState::HOLD; }
    bool IsReleased() const { return m_state == InputState::RELEASED; }

    KeyCode GetKey() const { return m_key; }

protected:
    KeyCode m_key;
    InputState m_state;

    friend class InputManager;
};

class MouseButtonBase {
public:
    bool IsButtonDown(MouseButton p_button) const;
    bool IsButtonUp(MouseButton p_button) const;
    bool IsButtonPressed(MouseButton p_button) const;
    bool IsButtonReleased(MouseButton p_button) const;

protected:
    MouseButtonArray m_buttons;
    MouseButtonArray m_prevButtons;
};

class InputEventMouse : public InputEventWithModifier, public MouseButtonBase {
public:
    InputEventMouse(const MouseButtonArray& p_buttons,
                    const MouseButtonArray& p_prevButtons) {
        m_buttons = p_buttons;
        m_prevButtons = p_prevButtons;
    }
};

class InputEventMouseWheel : public InputEventMouse {
public:
    InputEventMouseWheel(const MouseButtonArray& p_buttons,
                         const MouseButtonArray& p_prevButtons,
                         const Vector2f& p_scroll) : InputEventMouse(p_buttons, p_prevButtons),
                                                     m_scroll(p_scroll) {}

    float GetWheelX() const { return m_scroll.x; }
    float GetWheelY() const { return m_scroll.y; }

protected:
    Vector2f m_scroll;

    friend class InputManager;
};

class InputEventMouseMove : public InputEventMouse {
public:
    InputEventMouseMove(const MouseButtonArray& p_buttons,
                        const MouseButtonArray& p_prevButtons) : InputEventMouse(p_buttons, p_prevButtons) {}

    const Vector2f& GetPos() const { return m_pos; }
    const Vector2f& GetPrevPos() const { return m_prevPos; }
    const Vector2f GetDelta() const { return m_pos - m_prevPos; }

protected:
    Vector2f m_pos;
    Vector2f m_prevPos;

    friend class InputManager;
};

template<size_t N>
inline bool InputIsDown(const std::bitset<N>& p_array, int p_index) {
    DEV_ASSERT_INDEX(p_index, N);
    return p_array[p_index];
}

template<size_t N>
inline bool InputHasChanged(const std::bitset<N>& p_current, const std::bitset<N>& p_prev, int p_index) {
    DEV_ASSERT_INDEX(p_index, N);
    return p_current[p_index] == true && p_prev[p_index] == false;
}

}  // namespace my
