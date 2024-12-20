#include "ray.h"

namespace my {

#define C(a)  glm::vec3(a.x, a.y, a.z)
#define C2(a) NewVector3f(a.x, a.y, a.z)

Vector3f Ray::Direction() const {
    return glm::normalize(C(m_end) - C(m_start));
}

Ray Ray::Inverse(const Matrix4x4f& p_inverse_matrix) const {
    CRASH_NOW();
    Vector3f inversed_start = Vector3f(p_inverse_matrix * Vector4f(C(m_start), 1.0f));
    Vector3f inversed_end = Vector3f(p_inverse_matrix * Vector4f(C(m_end), 1.0f));
    Ray inversed_ray(C2(inversed_start), C2(inversed_end));
    inversed_ray.m_dist = m_dist;
    return inversed_ray;
}

}  // namespace my
