#ifndef AVEN_GLM_H
#define AVEN_GLM_H

#include "aven.h"

#define AVEN_GLM_PI_D 3.14159265358979323846264338327950288
#define AVEN_GLM_PI_F 3.14159265358979323846264338327950288f

typedef struct { float data[2]; } Vec2;
typedef struct { float data[3]; } Vec3;
typedef struct { float data[4]; } Vec4;

typedef struct { Vec2 data[2]; } Mat2;
typedef struct { Vec3 data[3]; } Mat3;
typedef struct { Vec4 data[4]; } Mat4;

typedef struct { Vec2 data[3]; } Mat2x3;
typedef struct { Vec2 data[4]; } Mat2x4;

typedef struct { Vec3 data[2]; } Mat3x2;
typedef struct { Vec3 data[4]; } Mat3x4;

typedef struct { Vec4 data[2]; } Mat4x2;
typedef struct { Vec4 data[3]; } Mat4x3;

#define vec_get(v, i) v.data[i]
#define mat_get(m, i, j) vec_get(m.data[i], j)

static Mat2 mat2_mul_mat2(Mat2 a, Mat2 b) {
    Mat2 dest;
    mat_get(dest, 0, 0) = mat_get(a, 0, 0) * mat_get(b, 0, 0) +
        mat_get(a, 1, 0) * mat_get(b, 0, 1);
    mat_get(dest, 0, 1) = mat_get(a, 0, 1) * mat_get(b, 0, 0) +
        mat_get(a, 1, 1) * mat_get(b, 0, 1);
    mat_get(dest, 1, 0) = mat_get(a, 0, 0) * mat_get(b, 1, 0) +
        mat_get(a, 0, 1) * mat_get(b, 1, 1);
    mat_get(dest, 1, 1) = mat_get(a, 0, 1) * mat_get(b, 1, 0) +
        mat_get(a, 1 , 1) * mat_get(b, 1, 1);
    return dest;
}

#endif // AVEN_GLM_H
