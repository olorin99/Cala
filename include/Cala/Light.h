#ifndef CALA_LIGHT_H
#define CALA_LIGHT_H

#include <Ende/math/Vec.h>
#include <Cala/Transform.h>
#include <Cala/Camera.h>

namespace cala {

    struct BaseLight {
        ende::math::Vec3f position = { 0, 0, 0 };
        f32 padding = 0;
        ende::math::Vec3f colour = { 1, 1, 1 };
        f32 intensity = 1;
    };

    struct DirectionalLight : public BaseLight {
        ende::math::Vec3f direction = {0, 1, 0};
    };

    struct PointLight : public BaseLight {
        f32 constant = 1.f;
        f32 linear = 0.045;
        f32 quadratic = 0.0075;
        f32 radius = 80;
    };

    class Light {
    public:

        enum LightType {
            DIRECTIONAL,
            POINT,
            SPOT
        };

        Light(LightType type, bool shadows, Transform& transform);

        Light& operator=(const Light& rhs);

        struct Data {
            ende::math::Vec3f position;
            f32 padding;
            ende::math::Vec3f colour;
            f32 intensity;
            f32 constant;
            f32 linear;
            f32 quadratic;
            f32 radius;
        };

        Data data() const;

        LightType type() const { return _type; }

        bool shadowing() const { return _shadowing; }


        void setDirection(const ende::math::Quaternion& dir);

        void setPosition(const ende::math::Vec3f& pos);

        void setColour(const ende::math::Vec3f& colour);

        ende::math::Vec3f getColour() const {
            return _colour;
        }

        void setIntensity(f32 intensity);

        f32 getIntensity() const {
            return _intensity;
        }

        f32 getRadius() const { return _radius; }

        Transform& transform() const { return _transform; }

        Camera& camera() { return _camera; }

    private:
        Transform& _transform;
        Camera _camera;
        LightType _type;
        bool _shadowing;

        ende::math::Vec3f _colour;
        f32 _intensity;
        f32 _constant;
        f32 _linear;
        f32 _quadratic;
        f32 _radius;
    };

}

#endif //CALA_LIGHT_H
