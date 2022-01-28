#include <Cala/shapes.h>
#include <Ende/math/math.h>

cala::Mesh cala::shapes::triangle(f32 width, f32 height) {
    Mesh data;
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

    for (u32 ring = 0; ring < rings; ring++) {
        f32 angle1 = (ring + 1) * ende::math::PI / (rings + 1);

        for (u32 sector = 0; sector < sectors; sector++) {
            f32 angle2 = sector * (ende::math::PI * 2) / sectors;

            f32 x = std::sin(angle1) * std::cos(angle2);
            f32 y = std::cos(angle1);
            f32 z = std::sin(angle1) * std::sin(angle2);

            f32 u = 0.5 + (std::atan2(z, x) / (2 * ende::math::PI));
            f32 v = 0.5 - (std::asin(y) / ende::math::PI);

            data.addVertex({ { x * radius, y * radius, z * radius }, { x, y, z }, { u, v }, { 1, 0, 0 }, { 0, 1, 0 } });
        }
    }

    for (u32 sector = 0; sector < sectors; sector++) {
        u32 a = sector + 1;
        u32 b = (sector + 1) % sectors + 1;
        data.addIndex(0);
        data.addIndex(b);
        data.addIndex(a);
    }

    for (u32 ring = 0; ring < rings; ring++) {
        u32 aStart = ring * sectors + 1;
        u32 bStart = (ring + 1) * sectors + 1;
        for (u32 sector = 0; sector < sectors; sector++) {
            u32 a = aStart + sector;
            u32 a1 = aStart + (sector + 1) % sectors;
            u32 b = bStart + sector;
            u32 b1 = bStart + (sector + 1) % sectors;

            data.addIndex(a);
            data.addIndex(a1);
            data.addIndex(b1);

            data.addIndex(a);
            data.addIndex(b1);
            data.addIndex(b);
        }
    }

    for (u32 sector = 0; sector < sectors; sector++) {
        u32 a = sector + sectors * (rings - 2) + 1;
        u32 b = (sector + 1) % sectors + sectors * (rings - 2) + 1;
        data.addIndex(data.vertexCount());
        data.addIndex(a);
        data.addIndex(b);
    }

    return data;
}

cala::Mesh cala::shapes::frustum(const ende::math::Mat4f &matrix) {
    Mesh data;
    return data;
}