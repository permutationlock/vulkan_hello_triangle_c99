#ifndef AVEN_GLM_H
#define AVEN_GLM_H

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

#endif // AVEN_GLM_H
