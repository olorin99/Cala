#include "Cala/ui/ImageView.h"
#include "Cala/backend/vulkan/primitives.h"
#include <imgui.h>
#include <imgui/backends/imgui_impl_vulkan.h>

cala::ui::ImageView::ImageView(ImGuiContext* context, backend::vulkan::Device *device)
    : Window(context),
    _device(device),
    _dirty(true),
    _imageSet(VK_NULL_HANDLE),
    _mode(0)
{}

cala::ui::ImageView::~ImageView() noexcept {
    _context->destroySet(_imageSet);
}

cala::ui::ImageView::ImageView(const cala::ui::ImageView &rhs)
    : Window(rhs._context),
    _device(rhs._device),
    _dirty(true),
    _imageSet(rhs._imageSet),
    _name(rhs._name),
    _width(rhs._width),
    _height(rhs._height),
    _mode(rhs._mode)
{}

void cala::ui::ImageView::render() {

    if (_dirty) {
        if (_imageSet != VK_NULL_HANDLE)
            _context->destroySet(_imageSet);

        if (_image) {
            _imageSet = ImGui_ImplVulkan_AddTexture(_device->defaultSampler()->sampler(), _image->defaultView().view, backend::vulkan::getImageLayout(_image->layout()));
        }
        _dirty = false;
    }

    ende::math::Vec<2, f32> size{ (f32)_width, (f32)_height };

    if (_imageSet != VK_NULL_HANDLE) {
        f32 width = _image->width();
        f32 height = _image->height();

        f32 u0 = 0;
        f32 u1 = 1;
        f32 v0 = 0;
        f32 v1 = 1;

        if (_mode == 0) {
            u0 = ((width / 2) - (size.x() / 2)) / width;
            u1 = ((width / 2) + (size.x() / 2)) / width;

            v0 = ((height / 2) - (size.y() / 2)) / height;
            v1 = ((height / 2) + (size.y() / 2)) / height;
        }


        ImGui::Image(_imageSet, { size.x(), size.y() }, { u0, v0 }, { u1, v1 });
    }
}