#pragma once

// Swizzle2
#pragma region SWIZZLE2
#define _SWIZZLE2_HELPER(MAX, INDEX1, INDEX2, C1, C2) \
    Swizzle2<T, MAX, INDEX1, INDEX2, -1, -1> C1##C2

#define VECTOR2_SWIZZLE2             \
    Swizzle2<T, 2, 0, 0, -1, -1> xx; \
    Swizzle2<T, 2, 0, 1, -1, -1> xy; \
    Swizzle2<T, 2, 1, 0, -1, -1> yx; \
    Swizzle2<T, 2, 1, 1, -1, -1> yy

#define VECTOR3_SWIZZLE2_XO(INDEX1, C1)    \
    _SWIZZLE2_HELPER(3, INDEX1, 0, C1, x); \
    _SWIZZLE2_HELPER(3, INDEX1, 1, C1, y); \
    _SWIZZLE2_HELPER(3, INDEX1, 2, C1, z)

#define VECTOR4_SWIZZLE2_XO(INDEX1, C1)    \
    _SWIZZLE2_HELPER(4, INDEX1, 0, C1, x); \
    _SWIZZLE2_HELPER(4, INDEX1, 1, C1, y); \
    _SWIZZLE2_HELPER(4, INDEX1, 2, C1, z); \
    _SWIZZLE2_HELPER(4, INDEX1, 3, C1, w)

#define VECTOR3_SWIZZLE2       \
    VECTOR3_SWIZZLE2_XO(0, x); \
    VECTOR3_SWIZZLE2_XO(1, y); \
    VECTOR3_SWIZZLE2_XO(2, z)

#define VECTOR4_SWIZZLE2       \
    VECTOR4_SWIZZLE2_XO(0, x); \
    VECTOR4_SWIZZLE2_XO(1, y); \
    VECTOR4_SWIZZLE2_XO(2, z); \
    VECTOR4_SWIZZLE2_XO(3, w)

#pragma endregion SWIZZLE2

// Swizzle3
#pragma region SWIZZLE3
#define _SWIZZLE3_HELPER(MAX, INDEX1, INDEX2, INDEX3, C1, C2, C3) \
    Swizzle3<T, MAX, INDEX1, INDEX2, INDEX3, -1> C1##C2##C3

#define VECTOR3_SWIZZLE3_XXO(INDEX1, INDEX2, C1, C2)   \
    _SWIZZLE3_HELPER(3, INDEX1, INDEX2, 0, C1, C2, x); \
    _SWIZZLE3_HELPER(3, INDEX1, INDEX2, 1, C1, C2, y); \
    _SWIZZLE3_HELPER(3, INDEX1, INDEX2, 2, C1, C2, z)

#define VECTOR3_SWIZZLE3_XOO(INDEX1, C1)    \
    VECTOR3_SWIZZLE3_XXO(INDEX1, 0, C1, x); \
    VECTOR3_SWIZZLE3_XXO(INDEX1, 1, C1, y); \
    VECTOR3_SWIZZLE3_XXO(INDEX1, 2, C1, z)

#define VECTOR3_SWIZZLE3        \
    VECTOR3_SWIZZLE3_XOO(0, x); \
    VECTOR3_SWIZZLE3_XOO(1, y); \
    VECTOR3_SWIZZLE3_XOO(2, z)

#define VECTOR4_SWIZZLE3_XXO(INDEX1, INDEX2, C1, C2)   \
    _SWIZZLE3_HELPER(4, INDEX1, INDEX2, 0, C1, C2, x); \
    _SWIZZLE3_HELPER(4, INDEX1, INDEX2, 1, C1, C2, y); \
    _SWIZZLE3_HELPER(4, INDEX1, INDEX2, 2, C1, C2, z); \
    _SWIZZLE3_HELPER(4, INDEX1, INDEX2, 3, C1, C2, w)

#define VECTOR4_SWIZZLE3_XOO(INDEX1, C1)    \
    VECTOR4_SWIZZLE3_XXO(INDEX1, 0, C1, x); \
    VECTOR4_SWIZZLE3_XXO(INDEX1, 1, C1, y); \
    VECTOR4_SWIZZLE3_XXO(INDEX1, 2, C1, z); \
    VECTOR4_SWIZZLE3_XXO(INDEX1, 3, C1, w)

#define VECTOR4_SWIZZLE3        \
    VECTOR4_SWIZZLE3_XOO(0, x); \
    VECTOR4_SWIZZLE3_XOO(1, y); \
    VECTOR4_SWIZZLE3_XOO(2, z); \
    VECTOR4_SWIZZLE3_XOO(3, w)
#pragma endregion SWIZZLE3

// Swizzle4
#pragma region SWIZZLE4

#define VECTOR4_SWIZZLE4_XXXO(INDEX1, INDEX2, INDEX3, C1, C2, C3) \
    Swizzle4<T, 4, INDEX1, INDEX2, INDEX3, 0> C1##C2##C3##x;      \
    Swizzle4<T, 4, INDEX1, INDEX2, INDEX3, 1> C1##C2##C3##y;      \
    Swizzle4<T, 4, INDEX1, INDEX2, INDEX3, 2> C1##C2##C3##z;      \
    Swizzle4<T, 4, INDEX1, INDEX2, INDEX3, 3> C1##C2##C3##w

#define VECTOR4_SWIZZLE4_XXOO(INDEX1, INDEX2, C1, C2)    \
    VECTOR4_SWIZZLE4_XXXO(INDEX1, INDEX2, 0, C1, C2, x); \
    VECTOR4_SWIZZLE4_XXXO(INDEX1, INDEX2, 1, C1, C2, y); \
    VECTOR4_SWIZZLE4_XXXO(INDEX1, INDEX2, 2, C1, C2, z); \
    VECTOR4_SWIZZLE4_XXXO(INDEX1, INDEX2, 3, C1, C2, w)

#define VECTOR4_SWIZZLE4_XOOO(INDEX1, C1)    \
    VECTOR4_SWIZZLE4_XXOO(INDEX1, 0, C1, x); \
    VECTOR4_SWIZZLE4_XXOO(INDEX1, 1, C1, y); \
    VECTOR4_SWIZZLE4_XXOO(INDEX1, 2, C1, z); \
    VECTOR4_SWIZZLE4_XXOO(INDEX1, 3, C1, w)

#define VECTOR4_SWIZZLE4         \
    VECTOR4_SWIZZLE4_XOOO(0, x); \
    VECTOR4_SWIZZLE4_XOOO(1, y); \
    VECTOR4_SWIZZLE4_XOOO(2, z); \
    VECTOR4_SWIZZLE4_XOOO(3, w)

#pragma endregion SWIZZLE4
