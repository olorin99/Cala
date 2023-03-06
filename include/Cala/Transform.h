#ifndef CALA_TRANSFORM_H
#define CALA_TRANSFORM_H

#include <Ende/math/Vec.h>
#include <Ende/math/Mat.h>
#include <Ende/math/Quaternion.h>

namespace cala {

    class Transform {
    public:

        Transform(const ende::math::Vec3f& pos = { 0, 0, 0 }, const ende::math::Quaternion& rot = { 0, 0, 0, 1 }, const ende::math::Vec3f& scale = { 1, 1, 1 }, Transform* parent = nullptr);


        ende::math::Mat4f local() const;

        ende::math::Mat4f world();

        inline ende::math::Mat4f operator*() const { return local(); }


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

        bool isDirty() const { return _dirty; }

        void setDirty(bool dirty) { _dirty = dirty; }

        Transform* parent() { return _parent; }

        bool updateWorld();

    private:

        ende::math::Vec3f _position;
        ende::math::Quaternion _rotation;
        ende::math::Vec3f _scale;
        bool _dirty;
        ende::math::Mat4f _world;
        Transform* _parent;

    };

}

#endif //CALA_TRANSFORM_H
