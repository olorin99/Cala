//
// Created by christian on 24/10/22.
//

#ifndef CALA_ENGINE_H
#define CALA_ENGINE_H

#include <Ende/Vector.h>
#include <Ende/Shared.h>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/Image.h>

namespace cala {

    class Engine {
    public:


    private:

        ende::Vector<ende::Shared<backend::vulkan::Buffer>> _buffers;
        ende::Vector<ende::Shared<backend::vulkan::Image>> _images;


    };

}

#endif //CALA_ENGINE_H
