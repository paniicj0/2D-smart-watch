#pragma once

// Minimal 3D math (no external deps).
// Column-major matrices compatible with OpenGL.

#include <cmath>

struct Vec2 {
    float x = 0.0f, y = 0.0f;
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vec3() = default;
    Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
};

inline Vec3 operator+(const Vec3& a, const Vec3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vec3 operator*(const Vec3& a, float s) { return { a.x * s, a.y * s, a.z * s }; }

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return { a.y * b.z - a.z * b.y,
             a.z * b.x - a.x * b.z,
             a.x * b.y - a.y * b.x };
}

inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalize(const Vec3& v) {
    float len = length(v);
    if (len <= 1e-6f) return { 0,0,0 };
    return v * (1.0f / len);
}

struct Mat4 {
    // column-major: m[col*4 + row]
    float m[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
};

inline Mat4 identity() { return Mat4{}; }

inline Mat4 mul(const Mat4& A, const Mat4& B) {
    Mat4 R;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            R.m[c * 4 + r] =
                A.m[0 * 4 + r] * B.m[c * 4 + 0] +
                A.m[1 * 4 + r] * B.m[c * 4 + 1] +
                A.m[2 * 4 + r] * B.m[c * 4 + 2] +
                A.m[3 * 4 + r] * B.m[c * 4 + 3];
        }
    }
    return R;
}

inline Mat4 translate(const Vec3& t) {
    Mat4 R = identity();
    R.m[12] = t.x;
    R.m[13] = t.y;
    R.m[14] = t.z;
    return R;
}

inline Mat4 scale(const Vec3& s) {
    Mat4 R = identity();
    R.m[0] = s.x;
    R.m[5] = s.y;
    R.m[10] = s.z;
    return R;
}

inline Mat4 rotateX(float rad) {
    Mat4 R = identity();
    float c = std::cos(rad), s = std::sin(rad);
    R.m[5] = c;  R.m[9] = -s;
    R.m[6] = s;  R.m[10] = c;
    return R;
}

inline Mat4 rotateY(float rad) {
    Mat4 R = identity();
    float c = std::cos(rad), s = std::sin(rad);
    R.m[0] = c;  R.m[8] = s;
    R.m[2] = -s; R.m[10] = c;
    return R;
}

inline Mat4 rotateZ(float rad) {
    Mat4 R = identity();
    float c = std::cos(rad), s = std::sin(rad);
    R.m[0] = c;  R.m[4] = -s;
    R.m[1] = s;  R.m[5] = c;
    return R;
}

inline Mat4 perspective(float fovYRad, float aspect, float zNear, float zFar) {
    Mat4 R{};
    float f = 1.0f / std::tan(fovYRad * 0.5f);
    for (float& v : R.m) v = 0.0f;
    R.m[0] = f / aspect;
    R.m[5] = f;
    R.m[10] = (zFar + zNear) / (zNear - zFar);
    R.m[11] = -1.0f;
    R.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    return R;
}

inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = normalize(center - eye);
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);

    Mat4 R = identity();
    R.m[0] = s.x; R.m[4] = s.y; R.m[8] = s.z;
    R.m[1] = u.x; R.m[5] = u.y; R.m[9] = u.z;
    R.m[2] = -f.x; R.m[6] = -f.y; R.m[10] = -f.z;
    R.m[12] = -dot(s, eye);
    R.m[13] = -dot(u, eye);
    R.m[14] = dot(f, eye);
    return R;
}

struct Vec4 { float x, y, z, w; };

inline Vec4 mul(const Mat4& M, const Vec4& v) {
    Vec4 r;
    r.x = M.m[0] * v.x + M.m[4] * v.y + M.m[8] * v.z + M.m[12] * v.w;
    r.y = M.m[1] * v.x + M.m[5] * v.y + M.m[9] * v.z + M.m[13] * v.w;
    r.z = M.m[2] * v.x + M.m[6] * v.y + M.m[10] * v.z + M.m[14] * v.w;
    r.w = M.m[3] * v.x + M.m[7] * v.y + M.m[11] * v.z + M.m[15] * v.w;
    return r;
}

inline float clampf(float v, float a, float b) {
    return v < a ? a : (v > b ? b : v);
}

inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
    return { lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t) };
}
