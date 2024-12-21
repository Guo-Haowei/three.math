#pragma once
#include "engine/core/math/vector_math.h"

#define CHECK_VEC2(VEC, a, b)  \
    {                          \
        EXPECT_EQ((VEC).x, a); \
        EXPECT_EQ((VEC).y, b); \
    }
#define CHECK_VEC3(VEC, a, b, c) \
    {                            \
        EXPECT_EQ((VEC).x, a);   \
        EXPECT_EQ((VEC).y, b);   \
        EXPECT_EQ((VEC).z, c);   \
    }
#define CHECK_VEC4(VEC, a, b, c, d) \
    {                               \
        EXPECT_EQ((VEC).x, a);      \
        EXPECT_EQ((VEC).y, b);      \
        EXPECT_EQ((VEC).z, c);      \
        EXPECT_EQ((VEC).w, d);      \
    }

namespace my::math::detail {

using Vector2f = Vector<float, 2>;
using Vector3f = Vector<float, 3>;
using Vector4f = Vector<float, 4>;

static_assert(sizeof(Vector2f) == 8);
static_assert(sizeof(Vector3f) == 12);
static_assert(sizeof(Vector4f) == 16);
static_assert(sizeof(Vector2i) == 8);
static_assert(sizeof(Vector3i) == 12);
static_assert(sizeof(Vector4i) == 16);
static_assert(sizeof(Vector2u) == 8);
static_assert(sizeof(Vector3u) == 12);
static_assert(sizeof(Vector4u) == 16);

}  // namespace my::math::detail

namespace my::math {

}  // namespace my::math