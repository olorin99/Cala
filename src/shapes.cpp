#include <Cala/shapes.h>
#include <Ende/math/math.h>

cala::Mesh cala::shapes::triangle(f32 width, f32 height) {
    Mesh data;

    f32 halfWidth = width / 2.f;
    f32 halfHeight = height / 2.f;

    data.addVertex({ { -halfWidth, -halfHeight, 0 }, { 0, 0, 1 }, { 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 } });
    data.addVertex({ { halfWidth, -halfHeight, 0 }, { 0, 0, 1 }, { 1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } });
    data.addVertex({ { 0, halfHeight, 0 }, { 0, 0, 1 }, { 0.5, 1 }, { 1, 0, 0 }, { 0, 0, 1 } });

    return data;
}

cala::Mesh cala::shapes::quad(f32 edge) {
    Mesh data;

    data.addVertex({ { -edge, -edge, -edge }, { 0, 0, -1 }, { 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { edge, -edge, -edge }, { 0, 0, -1 }, { 1, 0 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { edge, edge, -edge }, { 0, 0, -1 }, { 1, 1 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, edge, -edge }, { 0, 0, -1 }, { 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } });

    data.addIndex(0);
    data.addIndex(1);
    data.addIndex(2);

    data.addIndex(2);
    data.addIndex(3);
    data.addIndex(0);

    return data;
}

cala::Mesh cala::shapes::cube(f32 edge) {
    Mesh data;

    //top
    data.addVertex({ { -edge, edge, -edge }, { 0, 1, 0 }, { 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 } });
    data.addVertex({ { edge, edge, -edge }, { 0, 1, 0 }, { 1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } });
    data.addVertex({ { edge, edge, edge }, { 0, 1, 0 }, { 1, 1 }, { 1, 0, 0 }, { 0, 0, 1 } });

    data.addVertex({ { edge, edge, edge }, { 0, 1, 0 }, { 1, 1 }, { 1, 0, 0 }, { 0, 0, 1 } });
    data.addVertex({ { -edge, edge, edge }, { 0, 1, 0 }, { 0, 1 }, { 1, 0, 0 }, { 0, 0, 1 } });
    data.addVertex({ { -edge, edge, -edge }, { 0, 1, 0 }, { 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 } });

    //front
    data.addVertex({ { -edge, -edge, -edge }, { 0, 0, -1 }, { 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { edge, -edge, -edge }, { 0, 0, -1 }, { 1, 0 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { edge, edge, -edge }, { 0, 0, -1 }, { 1, 1 }, { 1, 0, 0 }, { 0, 1, 0 } });

    data.addVertex({ { edge, edge, -edge }, { 0, 0, -1 }, { 1, 1 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, edge, -edge }, { 0, 0, -1 }, { 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, -edge, -edge }, { 0, 0, -1 }, { 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } });

    //left
    data.addVertex({ { -edge, -edge, edge }, { -1, 0, 0 }, { 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, -edge, -edge }, { -1, 0, 0 }, { 1, 0 }, { 0, 0, -1 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, edge, -edge }, { -1, 0, 0 }, { 1, 1 }, { 0, 0, -1 }, { 0, 1, 0 } });

    data.addVertex({ { -edge, edge, -edge }, { -1, 0, 0 }, { 1, 1 }, { 0, 0, -1 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, edge, edge }, { -1, 0, 0 }, { 0, 1 }, { 0, 0, -1 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, -edge, edge }, { -1, 0, 0 }, { 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } });

    //right
    data.addVertex({ { edge, -edge, -edge }, { 1, 0, 0 }, { 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } });
    data.addVertex({ { edge, -edge, edge }, { 1, 0, 0 }, { 1, 0 }, { 0, 0, 1 }, { 0, 1, 0 } });
    data.addVertex({ { edge, edge, edge }, { 1, 0, 0 }, { 1, 1 }, { 0, 0, 1 }, { 0, 1, 0 } });

    data.addVertex({ { edge, edge, edge }, { 1, 0, 0 }, { 1, 1 }, { 0, 0, 1 }, { 0, 1, 0 } });
    data.addVertex({ { edge, edge, -edge }, { 1, 0, 0 }, { 0, 1 }, { 0, 0, 1 }, { 0, 1, 0 } });
    data.addVertex({ { edge, -edge, -edge }, { 1, 0, 0 }, { 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } });

    //bottom
    data.addVertex({ { -edge, -edge, edge }, { 0, -1, 0 }, { 0, 0 }, { 1, 0, 0 }, { 0, 0, -1 } });
    data.addVertex({ { edge, -edge, edge }, { 0, -1, 0 }, { 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } });
    data.addVertex({ { edge, -edge, -edge }, { 0, -1, 0 }, { 1, 1 }, { 1, 0, 0 }, { 0, 0, -1 } });

    data.addVertex({ { edge, -edge, -edge }, { 0, -1, 0 }, { 1, 1 }, { 1, 0, 0 }, { 0, 0, -1 } });
    data.addVertex({ { -edge, -edge, -edge }, { 0, -1, 0 }, { 0, 1 }, { 1, 0, 0 }, { 0, 0, -1 } });
    data.addVertex({ { -edge, -edge, edge }, { 0, -1, 0 }, { 0, 0 }, { 1, 0, 0 }, { 0, 0, -1 } });

    //back
    data.addVertex({ { edge, -edge, edge }, { 0, 0, 1 }, { 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, -edge, edge }, { 0, 0, 1 }, { 1, 0 }, { -1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { -edge, edge, edge }, { 0, 0, 1 }, { 1, 1 }, { -1, 0, 0 }, { 0, 1, 0 } });

    data.addVertex({ { -edge, edge, edge }, { 0, 0, 1 }, { 1, 1 }, { -1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { edge, edge, edge }, { 0, 0, 1 }, { 0, 1 }, { -1, 0, 0 }, { 0, 1, 0 } });
    data.addVertex({ { edge, -edge, edge }, { 0, 0, 1 }, { 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 } });

    return data;
}

cala::Mesh cala::shapes::sphere(f32 radius, u32 rings, u32 sectors) {
    Mesh data;

    f32 x, y, z, u, v, d;
    f64 theta;
    f64 phi = 0.f;
    f64 thetad = 2 * ende::math::PI / sectors;
    f64 phid = ende::math::PI / (rings + 1);

    u = 0.5 + (std::atan2(0, 0) / (2 * ende::math::PI));
    v = 0.5 - (std::asin(0.5) / ende::math::PI);

    data.addVertex({{0, radius, 0}, {0, 1, 0}, {u, v}, {1, 0, 0}, {0, 0, 1}});
    phi = phid;
    for (u32 i = 0; i < rings; i++) {
        y = std::cos(phi);
        d = std::sin(phi);
        theta = 0;
        for (u32 j = 0; j < sectors; j++) {
            x = std::sin(theta) * d;
            z = std::cos(theta) * d;
            u = 0.5 + (std::atan2(z, x) / (2 * ende::math::PI));
            v = 0.5 - (std::asin(y) / ende::math::PI);

            ende::math::Vec<3, f64> tangent({std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi), std::cos(theta)});
            ende::math::Vec<3, f64> normal({x, y, z});
            ende::math::Vec<3, f64> bitangent = normal.unit().cross(tangent);

            data.addVertex({{x * radius, y * radius, z * radius}, {x, y, z}, {u, v}, {(f32)tangent.x(), (f32)tangent.y(), (f32)tangent.z()}, {(f32)bitangent.x(), (f32)bitangent.y(), (f32)bitangent.z()}});
            theta += thetad;
        }
        phi += phid;
    }
    v = 0.5 - (std::asin(-0.5) / ende::math::PI);
    data.addVertex({{0, -radius, 0}, {0, -1, 0}, {u, v}, {1, 0, 0}, {0, 0, -1}});

    for (u32 i = 0; i < sectors; i++) {
        data.addIndex(0);
        data.addIndex(i + 2);
        data.addIndex(i + 1);
    }

    for (u32 i = 0; i < sectors; i++) {
        data.addIndex(i + 1);
        data.addIndex(i + sectors);
        data.addIndex(i);

        data.addIndex(i + sectors);
        data.addIndex(i + 1);
        data.addIndex(i + sectors + 1);
    }

    for (u32 i = 0; i < rings - 2; i++) {
        for (u32 j = 0; j < sectors; j++) {
            u32 v1 = i * sectors + sectors + j;
            u32 v2 = v1 + 1;
            u32 v3 = v1 + sectors;
            u32 v4 = v2 + sectors;

            data.addIndex(v2);
            data.addIndex(v3);
            data.addIndex(v1);

            data.addIndex(v3);
            data.addIndex(v2);
            data.addIndex(v4);
        }
    }

    for (u32 i = 0; i < sectors; i++) {
        data.addIndex(data.vertexCount() - (i + 1));
        data.addIndex(data.vertexCount() - i);
        data.addIndex(data.vertexCount() - 1);
    }


//    data.addVertex({{0, radius, 0}, {0, 1, 0}, {0, 0}, {1, 0, 0}, {0, 0, 1}});
//    for (u32 ring = 0; ring < rings; ring++) {
//        f32 angle1 = (ring + 1) * ende::math::PI / (rings + 1);
//
//        for (u32 sector = 0; sector < sectors; sector++) {
//            f32 angle2 = sector * (ende::math::PI * 2) / sectors;
//
//            f32 x = std::sin(angle1) * std::cos(angle2);
//            f32 y = std::cos(angle1);
//            f32 z = std::sin(angle1) * std::sin(angle2);
//
//            f32 u = 0.5f + (std::atan2(z, x) / (2 * ende::math::PI));
//            f32 v = 0.5f - (std::asin(y) / ende::math::PI);
//
//
//            ende::math::Vec3f tangent({std::sin(angle2) * std::cos(angle1), std::sin(angle2) * std::sin(angle1), std::cos(angle2)});
//            ende::math::Vec3f normal({x, y, z});
//            ende::math::Vec3f bitangent = normal.unit().cross(tangent);
//
//            data.addVertex({ { x * radius, y * radius, z * radius }, { x, y, z }, { u, v }, { tangent.x(), tangent.y(), tangent.z() }, { bitangent.x(), bitangent.y(), bitangent.z() } });
//        }
//    }
//    data.addVertex({{0, -radius, 0}, {0, -1, 0}, {0, 0}, {1, 0, 0}, {0, 0, -1}});
//
//    for (u32 sector = 0; sector < sectors; sector++) {
//        u32 a = sector + 1;
//        u32 b = (sector + 1) % sectors + 1;
//        data.addIndex(0);
//        data.addIndex(b);
//        data.addIndex(a);
//    }
//
//    for (u32 ring = 0; ring < rings; ring++) {
//        u32 aStart = ring * sectors + 1;
//        u32 bStart = (ring + 1) * sectors + 1;
//        for (u32 sector = 0; sector < sectors; sector++) {
//            u32 a = aStart + sector;
//            u32 a1 = aStart + (sector + 1) % sectors;
//            u32 b = bStart + sector;
//            u32 b1 = bStart + (sector + 1) % sectors;
//
//            data.addIndex(a);
//            data.addIndex(a1);
//            data.addIndex(b1);
//
//            data.addIndex(a);
//            data.addIndex(b1);
//            data.addIndex(b);
//        }
//    }
//
//    for (u32 sector = 0; sector < sectors; sector++) {
//        u32 a = sector + sectors * (rings - 2) + 1;
//        u32 b = (sector + 1) % sectors + sectors * (rings - 2) + 1;
//        data.addIndex(data.vertexCount());
//        data.addIndex(a);
//        data.addIndex(b);
//    }

    return data;
}

cala::Mesh cala::shapes::frustum(const ende::math::Mat4f &matrix) {
    Mesh data;
    return data;
}