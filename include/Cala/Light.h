#ifndef CALA_LIGHT_H
#define CALA_LIGHT_H

#include <Ende/math/Vec.h>
#include <Cala/Transform.h>
#include <Cala/Camera.h>
#include <Cala/vulkan/Handle.h>

namespace cala {

    class Light {
    public:

        enum LightType {
            DIRECTIONAL = 0,
            POINT = 1,
            SPOT = 2
        };

        Light(LightType type, bool shadows);

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
            i32 cameraIndex = -1;
        };

        Data data() const;

        LightType type() const { return _type; }

        void setType(LightType type) { _type = type; }

        bool shadowing() const { return _shadowing; }

        void setShadowing(bool shadowing = false);

        void setDirection(const ende::math::Quaternion& dir);

        ende::math::Quaternion getDirection() const { return _rotation; }

        void setPosition(const ende::math::Vec3f& pos);

        ende::math::Vec3f getPosition() const { return _position; }

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

        bool isDirty() const { return _dirty; }

        void setDirty(bool dirty) { _dirty = dirty; }

        f32 getShadowBias() const { return _shadowBias; }

        void setShadowBias(f32 bias);

        void setShadowMap(vk::ImageHandle shadowMap);

    private:
        ende::math::Vec3f _position;
        ende::math::Quaternion _rotation;
        LightType _type;
        bool _shadowing;
        bool _dirty;

        ende::math::Vec3f _colour;
        f32 _intensity;
        f32 _constant;
        f32 _shadowBias;
        f32 _radius;
        f32 _range;

        vk::ImageHandle _shadowMap;
    };

}

#endif //CALA_LIGHT_H
