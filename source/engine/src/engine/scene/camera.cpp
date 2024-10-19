#include "camera.h"

#include "core/io/archive.h"

namespace my {

void Camera::update() {
    if (isDirty()) {
        m_front.x = m_yaw.cos() * m_pitch.cos();
        m_front.y = m_pitch.sin();
        m_front.z = m_yaw.sin() * m_pitch.cos();

        m_right = glm::cross(m_front, vec3(0, 1, 0));

        m_view_matrix = glm::lookAt(m_position, m_position + m_front, vec3(0, 1, 0));
        m_projection_matrix = glm::perspective(m_fovy.toRad(), getAspect(), m_near, m_far);
        m_projection_view_matrix = m_projection_matrix * m_view_matrix;

        setDirty(false);
    }
}

void Camera::setDimension(int p_width, int p_height) {
    if (m_width != p_width || m_height != p_height) {
        m_width = p_width;
        m_height = p_height;
        setDirty();
    }
}

void Camera::serialize(Archive& p_archive, uint32_t) {
    if (p_archive.isWriteMode()) {
        p_archive << m_flags;
        p_archive << m_near;
        p_archive << m_far;
        p_archive << m_fovy;
        p_archive << m_width;
        p_archive << m_height;
        p_archive << m_pitch;
        p_archive << m_yaw;
        p_archive << m_position;
    } else {
        p_archive >> m_flags;
        p_archive >> m_near;
        p_archive >> m_far;
        p_archive >> m_fovy;
        p_archive >> m_width;
        p_archive >> m_height;
        p_archive >> m_pitch;
        p_archive >> m_yaw;
        p_archive >> m_position;

        setDirty();
    }
}

}  // namespace my