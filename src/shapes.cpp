#include <Cala/shapes.h>
#include <Ende/math/math.h>
#include <Ende/math/Frustum.h>

cala::MeshData cala::shapes::triangle(f32 width, f32 height) {
    MeshData data;

    f32 halfWidth = width / 2.f;
    f32 halfHeight = height / 2.f;

    data.addVertex({ { -halfWidth, -halfHeight, 0 }, { 0, 0, 1 }, { 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 } });
    data.addVertex({ { halfWidth, -halfHeight, 0 }, { 0, 0, 1 }, { 1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } });
    data.addVertex({ { 0, halfHeight, 0 }, { 0, 0, 1 }, { 0.5, 1 }, { 1, 0, 0 }, { 0, 0, 1 } });

    data.addTriangle(0, 1, 2);

    return data;
}

cala::MeshData cala::shapes::quad(f32 edge) {
    MeshData data;

    data.addVertex({ { -edge, -edge, -edge }, { 0, 0, -1 }, { 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, edge, -edge }, { 0, 0, -1 }, { 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { edge, -edge, -edge }, { 0, 0, -1 }, { 1, 0 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { edge, edge, -edge }, { 0, 0, -1 }, { 1, 1 }, { 1, 0, 0 }, { 0, 1, 0 } });

    data.addQuad(0, 1, 2, 3);

    return data;
}

cala::MeshData cala::shapes::cube(f32 edge) {
    MeshData data;

    //top
    data.addVertex({ { -edge, edge, -edge }, { 0, 1, 0 }, { 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 } });
    data.addVertex({ { -edge, edge, edge }, { 0, 1, 0 }, { 0, 1 }, { 1, 0, 0 }, { 0, 0, 1 } });
    data.addVertex({ { edge, edge, edge }, { 0, 1, 0 }, { 1, 1 }, { 1, 0, 0 }, { 0, 0, 1 } });

    data.addVertex({ { edge, edge, edge }, { 0, 1, 0 }, { 1, 1 }, { 1, 0, 0 }, { 0, 0, 1 } });
    data.addVertex({ { edge, edge, -edge }, { 0, 1, 0 }, { 1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } });
    data.addVertex({ { -edge, edge, -edge }, { 0, 1, 0 }, { 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 } });


    //front
    data.addVertex({ { -edge, -edge, -edge }, { 0, 0, -1 }, { 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, edge, -edge }, { 0, 0, -1 }, { 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { edge, edge, -edge }, { 0, 0, -1 }, { 1, 1 }, { 1, 0, 0 }, { 0, 1, 0 } });

    data.addVertex({ { edge, edge, -edge }, { 0, 0, -1 }, { 1, 1 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { edge, -edge, -edge }, { 0, 0, -1 }, { 1, 0 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, -edge, -edge }, { 0, 0, -1 }, { 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } });


    //left
    data.addVertex({ { -edge, -edge, edge }, { -1, 0, 0 }, { 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, edge, edge }, { -1, 0, 0 }, { 0, 1 }, { 0, 0, -1 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, edge, -edge }, { -1, 0, 0 }, { 1, 1 }, { 0, 0, -1 }, { 0, 1, 0 } });

    data.addVertex({ { -edge, edge, -edge }, { -1, 0, 0 }, { 1, 1 }, { 0, 0, -1 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, -edge, -edge }, { -1, 0, 0 }, { 1, 0 }, { 0, 0, -1 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, -edge, edge }, { -1, 0, 0 }, { 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } });


    //right
    data.addVertex({ { edge, -edge, -edge }, { 1, 0, 0 }, { 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } });
    data.addVertex({ { edge, edge, -edge }, { 1, 0, 0 }, { 0, 1 }, { 0, 0, 1 }, { 0, 1, 0 } });
    data.addVertex({ { edge, edge, edge }, { 1, 0, 0 }, { 1, 1 }, { 0, 0, 1 }, { 0, 1, 0 } });

    data.addVertex({ { edge, edge, edge }, { 1, 0, 0 }, { 1, 1 }, { 0, 0, 1 }, { 0, 1, 0 } });
    data.addVertex({ { edge, -edge, edge }, { 1, 0, 0 }, { 1, 0 }, { 0, 0, 1 }, { 0, 1, 0 } });
    data.addVertex({ { edge, -edge, -edge }, { 1, 0, 0 }, { 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } });


    //bottom
    data.addVertex({ { -edge, -edge, edge }, { 0, -1, 0 }, { 0, 0 }, { 1, 0, 0 }, { 0, 0, -1 } });
    data.addVertex({ { -edge, -edge, -edge }, { 0, -1, 0 }, { 0, 1 }, { 1, 0, 0 }, { 0, 0, -1 } });
    data.addVertex({ { edge, -edge, -edge }, { 0, -1, 0 }, { 1, 1 }, { 1, 0, 0 }, { 0, 0, -1 } });

    data.addVertex({ { edge, -edge, -edge }, { 0, -1, 0 }, { 1, 1 }, { 1, 0, 0 }, { 0, 0, -1 } });
    data.addVertex({ { edge, -edge, edge }, { 0, -1, 0 }, { 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } });
    data.addVertex({ { -edge, -edge, edge }, { 0, -1, 0 }, { 0, 0 }, { 1, 0, 0 }, { 0, 0, -1 } });


    //back
    data.addVertex({ { edge, -edge, edge }, { 0, 0, 1 }, { 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { edge, edge, edge }, { 0, 0, 1 }, { 0, 1 }, { -1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, edge, edge }, { 0, 0, 1 }, { 1, 1 }, { -1, 0, 0 }, { 0, 1, 0 } });

    data.addVertex({ { -edge, edge, edge }, { 0, 0, 1 }, { 1, 1 }, { -1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, -edge, edge }, { 0, 0, 1 }, { 1, 0 }, { -1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { edge, -edge, edge }, { 0, 0, 1 }, { 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 } });


    for (u32 i = 0; i < 36; i++)
        data.addIndex(i);

    return data;
}

namespace sphereUtils {

    ende::math::Vec<2, f32> normalToUV(const ende::math::Vec3f& normal) {
        f32 u = 0.5 + (std::atan2(normal.z(), normal.x()) / (2 * ende::math::PI));
        f32 v = 0.5 - (std::asin(normal.y()) / ende::math::PI);
        return {u, v};
    }


}




cala::MeshData cala::shapes::sphereUV(f32 radius, u32 rings, u32 sectors) {
    MeshData data;

    ende::math::Vec<2, f32> uv = sphereUtils::normalToUV({0, 0.5, 0});

    data.addVertex({{0, radius, 0}, {0, 1, 0}, {uv.x(), uv.y()}, {1, 0, 0}, {0, 0, 1}});
    for (u32 i = 0; i < rings - 1; i++) {
        f32 polar = ende::math::PI * (f32)(i + 1) / (f32)rings;
        f32 sp = std::sin(polar);
        f32 cp = std::cos(polar);
        for (u32 j = 0; j < sectors; j++) {
            f32 azimuth = 2.f * ende::math::PI * (f32)j / (f32)sectors;
            f32 sa = std::sin(azimuth);
            f32 ca = std::cos(azimuth);
            f32 x = sp * ca;
            f32 y = cp;
            f32 z = sp * sa;

            ende::math::Vec<3, f32> normal{x, y, z};
            normal = normal.unit();
            uv = sphereUtils::normalToUV(normal);

            ende::math::Vec<3, f32> tangent({sa * cp, sa * sp, ca});
            ende::math::Vec<3, f32> bitangent = normal.cross(tangent);

            data.addVertex({{x * radius, y * radius, z * radius}, {normal.x(), normal.y(), normal.x()}, {uv.x(), uv.y()}, {(f32)tangent.x(), (f32)tangent.y(), (f32)tangent.z()}, {(f32)bitangent.x(), (f32)bitangent.y(), (f32)bitangent.z()}});
        }
    }
    uv = sphereUtils::normalToUV({0, -0.5, 0});
    data.addVertex({{0, -radius, 0}, {0, -1, 0}, {uv.x(), uv.y()}, {-1, 0, 0}, {0, 0, 1}});

    for (u32 i = 0; i < sectors; i++) {
        u32 a = i + 1;
        u32 b = (i + 1) % sectors + 1;
        data.addTriangle(0, b, a);
    }

    for (u32 i = 0; i < rings; i++) {
        u32 aStart = i * sectors + 1;
        u32 bStart = (i + 1) * sectors + 1;
        for (u32 j = 0; j < sectors; j++) {
            u32 a = aStart + j;
            u32 a1 = aStart + (j + 1) % sectors;
            u32 b = bStart + j;
            u32 b1 = bStart + (j + 1) % sectors;
            data.addQuad(a, a1, b, b1);
        }
    }

    for (u32 i = 0; i < sectors; i++) {
        u32 a = i + sectors * (rings - 2) + 1;
        u32 b = (i + 1) % sectors + sectors * (rings - 2) + 1;
        data.addTriangle(data.vertexCount() - 1, a, b);
    }

    return data;
}


static const ende::math::Vec3f origins[6] = {
        {-1, -1, -1},
        {1, -1, -1},
        {1, -1, 1},
        {-1, -1, 1},
        {-1, 1, -1},
        {-1, -1, 1}
};

static const ende::math::Vec3f rights[6] = {
        {2, 0, 0},
        {0, 0, 2},
        {-2, 0, 0},
        {0, 0, -2},
        {2, 0, 0},
        {2, 0, 0}
};

static const ende::math::Vec3f ups[6] = {
        {0, 2, 0},
        {0, 2, 0},
        {0, 2, 0},
        {0, 2, 0},
        {0, 0, 2},
        {0, 0, -2}
};


ende::math::Vec3f mult(const ende::math::Vec3f& lhs, const ende::math::Vec3f& rhs) {
    return {lhs.x() * rhs.x(), lhs.y() * rhs.y(), lhs.z() * rhs.z()};
}


cala::MeshData cala::shapes::sphereNormalized(f32 radius, u32 divisions) {
    MeshData data;

    f32 step = 1.0f / (f32)divisions;
    ende::math::Vec3f step3{step, step, step};

    for (u32 face = 0; face < 6; face++) {
        ende::math::Vec3f origin = origins[face];
        ende::math::Vec3f right = rights[face];
        ende::math::Vec3f up = ups[face];
        for (u32 i = 0; i < divisions + 1; i++) {
            ende::math::Vec3f i3{static_cast<float>(i), static_cast<float>(i), static_cast<float>(i)};
            for (u32 j = 0; j < divisions + 1; j++) {
                ende::math::Vec3f j3{static_cast<f32>(j), static_cast<f32>(j), static_cast<f32>(j)};
                ende::math::Vec3f p = origin + mult(step3, (mult(j3, right) + mult(i3, up)));
                ende::math::Vec3f normal = p.unit();
                p = normal * radius;
                auto uv = sphereUtils::normalToUV(normal);
                data.addVertex({{p.x(), p.y(), p.z()}, {normal.x(), normal.y(), normal.z()}, {uv.x(), uv.y()}, {-1, 0, 0}, {0, 0, 1}});
            }
        }
    }

    u32 k = divisions + 1;
    for (u32 face = 0; face < 6; face++) {
        for (u32 i = 0; i < divisions; i++) {
            bool bottom = i < (divisions / 2);
            for (u32 j = 0; j < divisions; j++) {
                bool left = j < (divisions / 2);
                u32 a = (face * k + i) * k + j;
                u32 b = (face * k + i) * k + j + 1;
                u32 c = (face * k + i + 1) * k + j;
                u32 d = (face * k + i + 1) * k + j + 1;
                data.addQuad(a, c, b, d);
            }
        }
    }

    return data;
}

cala::MeshData cala::shapes::sphereCube(f32 radius, u32 divisions) {
    MeshData data;

    f32 step = 1.0f / (f32)divisions;
    ende::math::Vec3f step3{step, step, step};
    for (u32 face = 0; face < 6; face++) {
        ende::math::Vec3f origin = origins[face];
        ende::math::Vec3f right = rights[face];
        ende::math::Vec3f up = ups[face];
        for (u32 i = 0; i < divisions + 1; i++) {
            ende::math::Vec3f i3{static_cast<f32>(i), static_cast<f32>(i), static_cast<f32>(i)};
            for (u32 j = 0; j < divisions + 1; j++) {
                ende::math::Vec3f j3{static_cast<f32>(j), static_cast<f32>(j), static_cast<f32>(j)};
                ende::math::Vec3f p = origin + mult(step3, (mult(j3, right) + mult(i3, up)));
                ende::math::Vec3f p2 = mult(p, p);
                ende::math::Vec3f n{
                    p.x() * (f32)std::sqrt(1.0 - 0.5 * (p2.y() + p2.z()) + p2.y() * p2.z() / 3.0),
                    p.y() * (f32)std::sqrt(1.0 - 0.5 * (p2.z() + p2.x()) + p2.z() * p2.x() / 3.0),
                    p.z() * (f32)std::sqrt(1.0 - 0.5 * (p2.x() + p2.y()) + p2.x() * p2.y() / 3.0),
                };

                ende::math::Vec3f normal = n.unit();
                n = n * radius;
                auto uv = sphereUtils::normalToUV(normal);
                data.addVertex({{n.x(), n.y(), n.z()}, {normal.x(), normal.y(), normal.z()}, {uv.x(), uv.y()}, {-1, 0, 0}, {0, 0, 1}});
            }
        }
    }

    u32 k = divisions + 1;
    for (u32 face = 0; face < 6; face++) {
        for (u32 i = 0; i < divisions; i++) {
            bool bottom = i < (divisions / 2);
            for (u32 j = 0; j < divisions; j++) {
                bool left = j < (divisions / 2);
                u32 a = (face * k + i) * k + j;
                u32 b = (face * k + i) * k + j + 1;
                u32 c = (face * k + i + 1) * k + j;
                u32 d = (face * k + i + 1) * k + j + 1;
                data.addQuad(a, c, b, d);
            }
        }
    }

    return data;
}

#define POSITION(a, b, c) \
    pos = {a, b, c};        \
    normal = pos.unit(); \
    uv = sphereUtils::normalToUV(normal); \
    pos = normal * radius;                \
    data.addVertex({{pos.x(), pos.y(), pos.z()}, {normal.x(), normal.y(), normal.z()}, {uv.x(), uv.y()}, {-1, 0, 0}, {0, 0, 1}});

cala::MeshData cala::shapes::icosahedron(f32 radius) {
    MeshData data;

    f32 t = (1.0f + std::sqrt(5.0f)) / 2.0f;

    ende::math::Vec3f pos = {-1, t, 0.0};
    ende::math::Vec3f normal = pos.unit();
    ende::math::Vec<2, f32> uv = sphereUtils::normalToUV(normal);
    pos = normal * radius;
    data.addVertex({{pos.x(), pos.y(), pos.z()}, {normal.x(), normal.y(), normal.z()}, {uv.x(), uv.y()}, {-1, 0, 0}, {0, 0, 1}});

    POSITION({1, t, 0})
    POSITION({-1, -t, 0})
    POSITION({1, -t, 0})
    POSITION({0, -1, t})
    POSITION({0, 1, t})
    POSITION({0, -1, -t})
    POSITION({0, 1, -t})
    POSITION({t, 0, -1})
    POSITION({t, 0, 1})
    POSITION({-t, 0, -1})
    POSITION({-t, 0, 1})

    data.addTriangle(0, 11, 5);
    data.addTriangle(0, 5, 1);
    data.addTriangle(0, 1, 7);
    data.addTriangle(0, 7, 10);
    data.addTriangle(0, 10, 11);
    data.addTriangle(1, 5, 9);
    data.addTriangle(5, 11, 4);
    data.addTriangle(11, 10, 2);
    data.addTriangle(10, 7, 6);
    data.addTriangle(7, 1, 8);
    data.addTriangle(3, 9, 4);
    data.addTriangle(3, 4, 2);
    data.addTriangle(3, 2, 6);
    data.addTriangle(3, 6, 8);
    data.addTriangle(3, 8, 9);
    data.addTriangle(4, 9, 5);
    data.addTriangle(2, 4, 11);
    data.addTriangle(6, 2, 10);
    data.addTriangle(8, 6, 7);
    data.addTriangle(9, 8, 1);

    return data;
}



cala::MeshData cala::shapes::frustum(const ende::math::Mat4f &matrix) {

    ende::math::Mat4f inv = matrix.inverse();

    ende::math::Vec4f a({-1, -1, 1, 1});
    ende::math::Vec4f b({1, -1, 1, 1});
    ende::math::Vec4f c({-1, 1, 1, 1});
    ende::math::Vec4f d({1, 1, 1, 1});

    ende::math::Vec4f e({-1, -1, -1, 1});
    ende::math::Vec4f f({1, -1, -1, 1});
    ende::math::Vec4f g({-1, 1, -1, 1});
    ende::math::Vec4f h({1, 1, -1, 1});


    auto i = inv.transform(a); // far top left
    auto j = inv.transform(b); // far top right
    auto k = inv.transform(c); // far bottom left
    auto l = inv.transform(d); // far bottom right

    auto m = inv.transform(e); // near top left
    auto n = inv.transform(f); // near top right
    auto o = inv.transform(g); // near bottom left
    auto p = inv.transform(h); // near bottom right

    i = i / i.w();
    j = j / j.w();
    k = k / k.w();
    l = l / l.w();
    m = m / m.w();
    n = n / n.w();
    o = o / o.w();
    p = p / p.w();


    MeshData data;

    //top
    data.addVertex({ { o.x(), o.y(), o.z() }, { 0, 1, 0 }, { 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 } });
    data.addVertex({ { k.x(), k.y(), k.z() }, { 0, 1, 0 }, { 0, 1 }, { 1, 0, 0 }, { 0, 0, 1 } });
    data.addVertex({ { l.x(), l.y(), l.z() }, { 0, 1, 0 }, { 1, 1 }, { 1, 0, 0 }, { 0, 0, 1 } });

    data.addVertex({ { l.x(), l.y(), l.z() }, { 0, 1, 0 }, { 1, 1 }, { 1, 0, 0 }, { 0, 0, 1 } });
    data.addVertex({ { p.x(), p.y(), p.z() }, { 0, 1, 0 }, { 1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } });
    data.addVertex({ { o.x(), o.y(), o.z() }, { 0, 1, 0 }, { 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 } });


    //front
    data.addVertex({ { m.x(), m.y(), m.z() }, { 0, 0, -1 }, { 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { o.x(), o.y(), o.z() }, { 0, 0, -1 }, { 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { p.x(), p.y(), p.z() }, { 0, 0, -1 }, { 1, 1 }, { 1, 0, 0 }, { 0, 1, 0 } });

    data.addVertex({ { p.x(), p.y(), p.z() }, { 0, 0, -1 }, { 1, 1 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { n.x(), n.y(), n.z() }, { 0, 0, -1 }, { 1, 0 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { m.x(), m.y(), m.z() }, { 0, 0, -1 }, { 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } });


    //left
    data.addVertex({ { i.x(), i.y(), i.z() }, { -1, 0, 0 }, { 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } });
    data.addVertex({ { k.x(), k.y(), k.z() }, { -1, 0, 0 }, { 0, 1 }, { 0, 0, -1 }, { 0, 1, 0 } });
    data.addVertex({ { o.x(), o.y(), o.z() }, { -1, 0, 0 }, { 1, 1 }, { 0, 0, -1 }, { 0, 1, 0 } });

    data.addVertex({ { o.x(), o.y(), o.z() }, { -1, 0, 0 }, { 1, 1 }, { 0, 0, -1 }, { 0, 1, 0 } });
    data.addVertex({ { m.x(), m.y(), m.z() }, { -1, 0, 0 }, { 1, 0 }, { 0, 0, -1 }, { 0, 1, 0 } });
    data.addVertex({ { i.x(), i.y(), i.z() }, { -1, 0, 0 }, { 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } });


    //right
    data.addVertex({ { n.x(), n.y(), n.z() }, { 1, 0, 0 }, { 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } });
    data.addVertex({ { p.x(), p.y(), p.z() }, { 1, 0, 0 }, { 0, 1 }, { 0, 0, 1 }, { 0, 1, 0 } });
    data.addVertex({ { l.x(), l.y(), l.z() }, { 1, 0, 0 }, { 1, 1 }, { 0, 0, 1 }, { 0, 1, 0 } });

    data.addVertex({ { l.x(), l.y(), l.z() }, { 1, 0, 0 }, { 1, 1 }, { 0, 0, 1 }, { 0, 1, 0 } });
    data.addVertex({ { j.x(), j.y(), j.z() }, { 1, 0, 0 }, { 1, 0 }, { 0, 0, 1 }, { 0, 1, 0 } });
    data.addVertex({ { n.x(), n.y(), n.z() }, { 1, 0, 0 }, { 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } });


    //bottom
    data.addVertex({ { i.x(), i.y(), i.z() }, { 0, -1, 0 }, { 0, 0 }, { 1, 0, 0 }, { 0, 0, -1 } });
    data.addVertex({ { m.x(), m.y(), m.z() }, { 0, -1, 0 }, { 0, 1 }, { 1, 0, 0 }, { 0, 0, -1 } });
    data.addVertex({ { n.x(), n.y(), n.z() }, { 0, -1, 0 }, { 1, 1 }, { 1, 0, 0 }, { 0, 0, -1 } });

    data.addVertex({ { n.x(), n.y(), n.z() }, { 0, -1, 0 }, { 1, 1 }, { 1, 0, 0 }, { 0, 0, -1 } });
    data.addVertex({ { j.x(), j.y(), j.z() }, { 0, -1, 0 }, { 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } });
    data.addVertex({ { i.x(), i.y(), i.z() }, { 0, -1, 0 }, { 0, 0 }, { 1, 0, 0 }, { 0, 0, -1 } });


    //back
    data.addVertex({ { j.x(), j.y(), j.z() }, { 0, 0, 1 }, { 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { l.x(), l.y(), l.z() }, { 0, 0, 1 }, { 0, 1 }, { -1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { k.x(), k.y(), k.z() }, { 0, 0, 1 }, { 1, 1 }, { -1, 0, 0 }, { 0, 1, 0 } });

    data.addVertex({ { k.x(), k.y(), k.z() }, { 0, 0, 1 }, { 1, 1 }, { -1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { i.x(), i.y(), i.z() }, { 0, 0, 1 }, { 1, 0 }, { -1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { j.x(), j.y(), j.z() }, { 0, 0, 1 }, { 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 } });


    for (u32 i = 0; i < 36; i++)
        data.addIndex(i);

    return data;
}