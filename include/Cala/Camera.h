#ifndef CALA_CAMERA_H
#define CALA_CAMERA_H

#include <Ende/math/Mat.h>
#include <Cala/Transform.h>

namespace cala {

    class Camera {
    public:

        Camera(const ende::math::Mat4f& projection, Transform& transform);

        inline ende::math::Mat4f projection() const { return _projection; }

        ende::math::Mat4f view() const;

        ende::math::Mat4f viewProjection() const;

        Transform& transform() const;

        struct Data {
            ende::math::Mat4f projection;
            ende::math::Mat4f view;
            alignas(16) ende::math::Vec3f position;
        };

        Data data() const;

    private:

        ende::math::Mat4f _projection;
        Transform& _transform;

    };

}

#endif //CALA_CAMERA_H
