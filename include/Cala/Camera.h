#ifndef CALA_CAMERA_H
#define CALA_CAMERA_H

#include <Ende/math/Mat.h>
#include <Cala/Transform.h>
#include <Ende/math/Frustum.h>
#include <Ende/math/Vec.h>
#include <vector>
#include <Cala/shaderBridge.h>

namespace cala {

    class Camera {
    public:

        Camera(f32 fov, f32 width, f32 height, f32 near, f32 far, Transform* transform = nullptr);
        Camera(const ende::math::Mat4f& projection, Transform* transform = nullptr);

        Camera& operator=(const Camera& rhs);

        void resize(f32 width, f32 height);

        void setProjection(const ende::math::Mat4f& projection) { _projection = projection; }
        inline ende::math::Mat4f projection() const { return _projection; }

        ende::math::Mat4f view() const;

        ende::math::Mat4f viewProjection() const;

        Transform& transform() const;

        const ende::math::Frustum& frustum() const { return _frustum; }

        void updateFrustum();

        // returns the 8 corners of a frustum
        std::vector<ende::math::Vec4f> getFrustumCorners() const;

        void setExposure(f32 exposure);

        f32 getExposure() const { return _exposure; }

        GPUCamera data() const;

        f32 near() const { return _near; }
        void setNear(f32 near);

        f32 far() const { return _far; }
        void setFar(f32 far);

        bool isDirty() const { return _dirty; }
        void setDirty(bool dirty) { _dirty = dirty; }

        void setTransform(Transform* transform) { _transform = transform; }

        f32 fov() const { return _fov; }
        void setFov(f32 fov);

        f32 width() const { return _width; }
        f32 height() const { return _height; }

    private:

        ende::math::Frustum _frustum;

        ende::math::Mat4f _projection;
        Transform* _transform;
        f32 _fov;
        f32 _width;
        f32 _height;
        f32 _near;
        f32 _far;
        f32 _exposure;

        bool _dirty;

    };

}

#endif //CALA_CAMERA_H
