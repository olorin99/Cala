#ifndef CALA_LIGHT_H
#define CALA_LIGHT_H

#include <Ende/math/Vec.h>
#include <Cala/Transform.h>
#include <Cala/Camera.h>

namespace cala {

    class Light {
    public:

        enum LightType {
            DIRECTIONAL = 0,
            POINT = 1,
            SPOT = 2
        };

        Light(LightType type, bool shadows, Transform& transform);

        Light& operator=(const Light& rhs);

        struct Data {
            ende::math::Vec3f position;
            u32 type;
            ende::math::Vec3f colour;
            f32 intensity;
            f32 range;
            f32 radius;
            f32 shadowBias;
            i32 shadowIndex = -1;
        };

        Data data() const;

        LightType type() const { return _type; }

        bool shadowing() const { return _shadowing; }

        void setShadowing(bool shadowing = false);

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

        void setRange(f32 range);

        f32 getNear() const { return 0.1f; }

        f32 getFar() const { return getNear() + _range; }

        Transform& transform() const { return *_transform; }

        Camera& camera() { return _camera; }

        bool isDirty() const { return _dirty; }

        void setDirty(bool dirty) { _dirty = dirty; }

        f32 getShadowBias() const { return _shadowBias; }

        void setShadowBias(f32 bias);

    private:
        Transform* _transform;
        Camera _camera;
        LightType _type;
        bool _shadowing;
        bool _dirty;

        ende::math::Vec3f _colour;
        f32 _intensity;
        f32 _constant;
        f32 _shadowBias;
        f32 _radius;
        f32 _range;
    };

}

#endif //CALA_LIGHT_H
