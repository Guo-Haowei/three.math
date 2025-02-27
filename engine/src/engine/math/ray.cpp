#include "ray.h"

#include "engine/math/matrix_transform.h"
#include "engine/math/vector.h"

namespace my {

Vector3f Ray::Direction() const {
    return normalize(m_end - m_start);
}

Ray Ray::Inverse(const Matrix4x4f& p_inverse_matrix) const {
    Vector4f inversed_start = p_inverse_matrix * Vector4f(m_start, 1.0f);
    Vector4f inversed_end = p_inverse_matrix * Vector4f(m_end, 1.0f);
    Ray inversed_ray(Vector3f(inversed_start.xyz), Vector3f(inversed_end.xyz));
    inversed_ray.m_dist = m_dist;
    return inversed_ray;
}

}  // namespace my
