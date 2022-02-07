#ifndef CALA_LIGHTS_H
#define CALA_LIGHTS_H

#include <Ende/math/Vec.h>

namespace cala {

    struct BaseLight {
        ende::math::Vec3f position = { 0, 0, 0 };
        f32 padding = 0;
        ende::math::Vec3f colour = { 1, 1, 1 };
        f32 intensity = 16;
    };

    struct PointLight : public BaseLight {
        f32 constant = 1.f;
        f32 linear = 0.045;
        f32 quadratic = 0.0075;
        f32 radius = 80;
    };

}

#endif //CALA_LIGHTS_H
