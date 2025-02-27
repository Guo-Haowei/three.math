#pragma once
#include "common.h"
#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#if USING(MATH_ENABLE_SIMD_SSE)
#include "vector_math_sse.h"
#endif

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

namespace my {

template<Arithmetic T, int N>
constexpr bool operator==(const Vector<T, N>& p_lhs, const Vector<T, N>& p_rhs) {
    for (int i = 0; i < N; ++i) {
        if (p_lhs[i] != p_rhs[i]) {
            return false;
        }
    }
    return true;
}

#define VECTOR_OPERATOR_VEC_VEC(DEST, OP, LHS, RHS)          \
    do {                                                     \
        constexpr int DIM = sizeof(LHS) / sizeof(LHS.x);     \
        DEST.x = LHS.x OP RHS.x;                             \
        DEST.y = LHS.y OP RHS.y;                             \
        if constexpr (DIM >= 3) { DEST.z = LHS.z OP RHS.z; } \
        if constexpr (DIM >= 4) { DEST.w = LHS.w OP RHS.w; } \
    } while (0)

#define VECTOR_OPERATOR_VEC_SCALAR(DEST, OP, LHS, RHS)     \
    do {                                                   \
        constexpr int DIM = sizeof(LHS) / sizeof(LHS.x);   \
        DEST.x = LHS.x OP RHS;                             \
        DEST.y = LHS.y OP RHS;                             \
        if constexpr (DIM >= 3) { DEST.z = LHS.z OP RHS; } \
        if constexpr (DIM >= 4) { DEST.w = LHS.w OP RHS; } \
    } while (0)

#define VECTOR_OPERATOR_SCALAR_VEC(DEST, OP, LHS, RHS)     \
    do {                                                   \
        constexpr int DIM = sizeof(RHS) / sizeof(RHS.x);   \
        DEST.x = LHS OP RHS.x;                             \
        DEST.y = LHS OP RHS.y;                             \
        if constexpr (DIM >= 3) { DEST.z = LHS OP RHS.z; } \
        if constexpr (DIM >= 4) { DEST.w = LHS OP RHS.w; } \
    } while (0)

#pragma region VECTOR_MATH_ADD

template<Arithmetic T, int N>
constexpr Vector<T, N> operator+(const Vector<T, N>& p_lhs, const Vector<T, N>& p_rhs) {
    Vector<T, N> result;
    VECTOR_OPERATOR_VEC_VEC(result, +, p_lhs, p_rhs);
    return result;
}

template<Arithmetic T, int N, Arithmetic U>
constexpr Vector<T, N> operator+(const Vector<T, N>& p_lhs, const U& p_rhs) {
    Vector<T, N> result;
    VECTOR_OPERATOR_VEC_SCALAR(result, +, p_lhs, p_rhs);
    return result;
}

template<Arithmetic T, int N, Arithmetic U>
constexpr Vector<T, N> operator+(const U& p_lhs, const Vector<T, N>& p_rhs) {
    Vector<T, N> result;
    VECTOR_OPERATOR_SCALAR_VEC(result, +, p_lhs, p_rhs);
    return result;
}

template<Arithmetic T, int N>
constexpr Vector<T, N>& operator+=(Vector<T, N>& p_lhs, const Vector<T, N>& p_rhs) {
    VECTOR_OPERATOR_VEC_VEC(p_lhs, +, p_lhs, p_rhs);
    return p_lhs;
}

template<Arithmetic T, int N, Arithmetic U>
constexpr Vector<T, N>& operator+=(Vector<T, N>& p_lhs, const U& p_rhs) {
    VECTOR_OPERATOR_VEC_SCALAR(p_lhs, +, p_lhs, p_rhs);
    return p_lhs;
}
#pragma endregion VECTOR_MATH_ADD

#pragma region VECTOR_MATH_SUB
template<Arithmetic T, int N>
constexpr Vector<T, N> operator-(const Vector<T, N>& p_lhs, const Vector<T, N>& p_rhs) {
    Vector<T, N> result;
    VECTOR_OPERATOR_VEC_VEC(result, -, p_lhs, p_rhs);
    return result;
}

template<Arithmetic T, int N, Arithmetic U>
constexpr Vector<T, N> operator-(const Vector<T, N>& p_lhs, const U& p_rhs) {
    Vector<T, N> result;
    VECTOR_OPERATOR_VEC_SCALAR(result, -, p_lhs, p_rhs);
    return result;
}

template<Arithmetic T, int N, Arithmetic U>
constexpr Vector<T, N> operator-(const U& p_lhs, const Vector<T, N>& p_rhs) {
    Vector<T, N> result;
    VECTOR_OPERATOR_SCALAR_VEC(result, -, p_lhs, p_rhs);
    return result;
}

template<Arithmetic T, int N>
constexpr Vector<T, N>& operator-=(Vector<T, N>& p_lhs, const Vector<T, N>& p_rhs) {
    VECTOR_OPERATOR_VEC_VEC(p_lhs, -, p_lhs, p_rhs);
    return p_lhs;
}

template<Arithmetic T, int N, Arithmetic U>
constexpr Vector<T, N>& operator-=(Vector<T, N>& p_lhs, const U& p_rhs) {
    VECTOR_OPERATOR_VEC_SCALAR(p_lhs, -, p_lhs, p_rhs);
    return p_lhs;
}
#pragma endregion VECTOR_MATH_SUB

#pragma region VECTOR_MATH_MUL
template<Arithmetic T, int N>
constexpr Vector<T, N> operator*(const Vector<T, N>& p_lhs, const Vector<T, N>& p_rhs) {
    Vector<T, N> result;
    VECTOR_OPERATOR_VEC_VEC(result, *, p_lhs, p_rhs);
    return result;
}

template<Arithmetic T, int N, Arithmetic U>
constexpr Vector<T, N> operator*(const Vector<T, N>& p_lhs, const U& p_rhs) {
    Vector<T, N> result;
    VECTOR_OPERATOR_VEC_SCALAR(result, *, p_lhs, p_rhs);
    return result;
}

template<Arithmetic T, int N, Arithmetic U>
constexpr Vector<T, N> operator*(const U& p_lhs, const Vector<T, N>& p_rhs) {
    Vector<T, N> result;
    VECTOR_OPERATOR_SCALAR_VEC(result, *, p_lhs, p_rhs);
    return result;
}

template<Arithmetic T, int N>
constexpr Vector<T, N>& operator*=(Vector<T, N>& p_lhs, const Vector<T, N>& p_rhs) {
    VECTOR_OPERATOR_VEC_VEC(p_lhs, *, p_lhs, p_rhs);
    return p_lhs;
}

template<Arithmetic T, int N, Arithmetic U>
constexpr Vector<T, N>& operator*=(Vector<T, N>& p_lhs, const U& p_rhs) {
    VECTOR_OPERATOR_VEC_SCALAR(p_lhs, *, p_lhs, p_rhs);
    return p_lhs;
}
#pragma endregion VECTOR_MATH_MUL

#pragma region VECTOR_MATH_DIV
template<Arithmetic T, int N>
constexpr Vector<T, N> operator/(const Vector<T, N>& p_lhs, const Vector<T, N>& p_rhs) {
    Vector<T, N> result;
    VECTOR_OPERATOR_VEC_VEC(result, /, p_lhs, p_rhs);
    return result;
}

template<Arithmetic T, int N, Arithmetic U>
constexpr Vector<T, N> operator/(const Vector<T, N>& p_lhs, const U& p_rhs) {
    Vector<T, N> result;
    VECTOR_OPERATOR_VEC_SCALAR(result, /, p_lhs, p_rhs);
    return result;
}

template<Arithmetic T, int N, Arithmetic U>
constexpr Vector<T, N> operator/(const U& p_lhs, const Vector<T, N>& p_rhs) {
    Vector<T, N> result;
    VECTOR_OPERATOR_SCALAR_VEC(result, /, p_lhs, p_rhs);
    return result;
}

template<Arithmetic T, int N>
constexpr Vector<T, N>& operator/=(Vector<T, N>& p_lhs, const Vector<T, N>& p_rhs) {
    VECTOR_OPERATOR_VEC_VEC(p_lhs, /, p_lhs, p_rhs);
    return p_lhs;
}

template<Arithmetic T, int N, Arithmetic U>
constexpr Vector<T, N>& operator/=(Vector<T, N>& p_lhs, const U& p_rhs) {
    VECTOR_OPERATOR_VEC_SCALAR(p_lhs, /, p_lhs, p_rhs);
    return p_lhs;
}
#pragma endregion VECTOR_MATH_DIV

template<Arithmetic T, int N>
constexpr inline Vector<T, N> min(const Vector<T, N>& p_lhs, const Vector<T, N>& p_rhs) {
    Vector<T, N> result;
    result.x = min(p_lhs.x, p_rhs.x);
    result.y = min(p_lhs.y, p_rhs.y);
    if constexpr (N >= 3) {
        result.z = min(p_lhs.z, p_rhs.z);
    }
    if constexpr (N >= 4) {
        result.w = min(p_lhs.w, p_rhs.w);
    }
    return result;
}

template<Arithmetic T, int N>
constexpr inline Vector<T, N> max(const Vector<T, N>& p_lhs, const Vector<T, N>& p_rhs) {
    Vector<T, N> result;
    result.x = max(p_lhs.x, p_rhs.x);
    result.y = max(p_lhs.y, p_rhs.y);
    if constexpr (N >= 3) {
        result.z = max(p_lhs.z, p_rhs.z);
    }
    if constexpr (N >= 4) {
        result.w = max(p_lhs.w, p_rhs.w);
    }
    return result;
}

template<Arithmetic T, int N>
constexpr inline Vector<T, N> abs(const Vector<T, N>& p_lhs) {
    Vector<T, N> result;
    result.x = abs(p_lhs.x);
    result.y = abs(p_lhs.y);
    if constexpr (N >= 3) {
        result.z = abs(p_lhs.z);
    }
    if constexpr (N >= 4) {
        result.w = abs(p_lhs.w);
    }
    return result;
}

template<Arithmetic T, int N>
constexpr inline Vector<T, N> clamp(const Vector<T, N>& p_value, const Vector<T, N>& p_min, const Vector<T, N>& p_max) {
    return max(p_min, min(p_value, p_max));
}

template<FloatingPoint T, int N>
constexpr inline Vector<T, N> lerp(const Vector<T, N>& p_x, const Vector<T, N>& p_y, float p_s) {
    return (static_cast<T>(1) - p_s) * p_x + p_s * p_y;
}

template<Arithmetic T, int N>
constexpr inline T dot(const Vector<T, N>& p_lhs, const Vector<T, N>& p_rhs) {
    Vector<T, N> tmp(p_lhs * p_rhs);
    T result = tmp.x + tmp.y;
    if constexpr (N >= 3) {
        result += tmp.z;
    }
    if constexpr (N >= 4) {
        result += tmp.w;
    }
    return result;
}

template<Arithmetic T, int N>
    requires(std::is_floating_point_v<T>)
constexpr inline T length(const Vector<T, N>& p_lhs) {
    return std::sqrt(dot(p_lhs, p_lhs));
}

template<Arithmetic T, int N>
    requires(std::is_floating_point_v<T>)
constexpr inline Vector<T, N> normalize(const Vector<T, N>& p_lhs) {
    const auto inverse_length = static_cast<T>(1) / length(p_lhs);
    return p_lhs * inverse_length;
}

template<Arithmetic T>
constexpr inline Vector<T, 3> cross(const Vector<T, 3>& p_lhs, const Vector<T, 3>& p_rhs) {
    return Vector<T, 3>(
        p_lhs.y * p_rhs.z - p_rhs.y * p_lhs.z,
        p_lhs.z * p_rhs.x - p_rhs.z * p_lhs.x,
        p_lhs.x * p_rhs.y - p_rhs.x * p_lhs.y);
}

}  // namespace my
