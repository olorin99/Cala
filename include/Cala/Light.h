#ifndef CALA_LIGHT_H
#define CALA_LIGHT_H

#include <Ende/math/Vec.h>
#include <Cala/Transform.h>
#include <Cala/Camera.h>
#include <Cala/vulkan/Handle.h>
#include <Cala/shaderBridge.h>

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

        GPULight data() const;

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

        f32 getSize() const { return _size; }
        void setSize(f32 size) { _size = size; }

        void setRange(f32 range);

        f32 getNear() const { return 0.1f; }

        f32 getFar() const { return getNear() + _range; }

        bool isDirty() const { return _dirty; }

        void setDirty(bool dirty) { _dirty = dirty; }

        f32 getShadowBias() const { return _shadowBias; }

        void setShadowBias(f32 bias);

        void setShadowMap(vk::ImageHandle shadowMap);

        i32 getCascadeCount() const { return _cascadeCount; }
        void setCascadeCount(i32 count) { _cascadeCount = count; }

        f32 getCascadeSplit(i32 cascade) const { return _cascadeSplits[cascade]; }
        void setCascadeSplit(i32 cascade, f32 range) { _cascadeSplits[cascade] = range; }

        void setCascadeShadowMap(i32 cascade, vk::ImageHandle shadowMap);

        i32 getCameraIndex() const { return _cameraIndex; }
        void setCameraIndex(i32 index) { _cameraIndex = index; }

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
        f32 _size;
        f32 _range;
        i32 _cascadeCount = 0;
        f32 _cascadeSplits[10] = {};
        vk::ImageHandle _cascadeMaps[10] = {};
        i32 _cameraIndex = -1;

        vk::ImageHandle _shadowMap = {};
    };

}

#endif //CALA_LIGHT_H
