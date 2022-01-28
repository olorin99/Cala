#ifndef CALA_TRANSFORM_H
#define CALA_TRANSFORM_H

#include <Ende/math/Vec.h>
#include <Ende/math/Mat.h>
#include <Ende/math/Quaternion.h>

namespace cala {

    class Transform {
    public:

        Transform(const ende::math::Vec3f& pos = { 0, 0, 0 }, const ende::math::Quaternion& rot = { 0, 0, 0, 1 }, const ende::math::Vec3f& scale = { 1, 1, 1 });


        ende::math::Mat4f toMat() const;

        inline ende::math::Mat4f operator*() const { return toMat(); }


        Transform& rotate(const ende::math::Quaternion& rot);

        Transform& rotate(const ende::math::Vec3f& axis, f32 angle);

        Transform& addPos(const ende::math::Vec3f& vec);

        Transform& setPos(const ende::math::Vec3f& pos);

        Transform& setRot(const ende::math::Quaternion& rot);

        Transform& setRot(const ende::math::Vec3f& axis, f32 angle);

        Transform& setScale(const ende::math::Vec3f& scale);

        Transform& setScale(f32 scale);


        inline const ende::math::Vec3f& pos() const { return _position; }

        inline const ende::math::Quaternion& rot() const { return _rotation; }

        inline const ende::math::Vec3f& scale() const { return _scale; }

    private:

        ende::math::Vec3f _position;
        ende::math::Quaternion _rotation;
        ende::math::Vec3f _scale;

    };

}

#endif //CALA_TRANSFORM_H