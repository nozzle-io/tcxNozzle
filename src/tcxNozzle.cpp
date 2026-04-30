#pragma once

#include "tcxNozzle.h"
#include <tc/TrussC.h>

namespace tcx {

bool NozzleSender::send(const tc::Pixels& pixels) {
    if (!pixels.isAllocated()) return false;
    return send(pixels.getData(), pixels.getWidth(), pixels.getHeight(), pixels.getChannels());
}

bool NozzleSender::send(tc::Fbo& fbo) {
    if (!setup_ || !fbo.isAllocated()) return false;
    int w = fbo.getWidth();
    int h = fbo.getHeight();
    size_t required = (size_t)w * h * 4;
    if (pixel_buffer_.size() != required) {
        pixel_buffer_.resize(required);
    }
    if (!fbo.readPixels(pixel_buffer_.data())) return false;
    return send(pixel_buffer_.data(), w, h, 4);
}

bool NozzleSender::send(const tc::Texture& texture) {
    if (!setup_ || !texture.isAllocated()) return false;
    int w = texture.getWidth();
    int h = texture.getHeight();

    if (!temp_fbo_.isAllocated() ||
        temp_fbo_.getWidth() != w || temp_fbo_.getHeight() != h) {
        temp_fbo_.allocate(w, h);
    }

    temp_fbo_.begin();
    texture.draw(0, 0);
    temp_fbo_.end();

    return send(temp_fbo_);
}

bool NozzleReceiver::receive(tc::Pixels& pixels) {
    if (!connected_ || !receiver_) return false;
    frame_new_ = false;

    NozzleAcquireDesc acq{};
    NozzleFrame* frame = nullptr;
    NozzleErrorCode err = nozzle_receiver_acquire_frame(receiver_, &acq, &frame);
    if (err != NOZZLE_OK || !frame) return false;

    NozzleFrameInfo info{};
    nozzle_frame_get_info(frame, &info);

    width_ = info.width;
    height_ = info.height;

    NozzleMappedPixels mapped{};
    err = nozzle_frame_lock_pixels(frame, &mapped);
    if (err != NOZZLE_OK) {
        nozzle_frame_release(frame);
        return false;
    }

    int channels = 4;
    if (!pixels.isAllocated() || pixels.getWidth() != width_ || pixels.getHeight() != height_) {
        pixels.allocate(width_, height_, channels);
    }

    uint32_t src_row_bytes = mapped.row_bytes;
    uint32_t dst_row_bytes = width_ * channels;

    if (src_row_bytes == dst_row_bytes) {
        std::memcpy(pixels.getData(), mapped.data, (size_t)height_ * dst_row_bytes);
    } else {
        auto* src = static_cast<const unsigned char*>(mapped.data);
        auto* dst = pixels.getData();
        for (int y = 0; y < height_; y++) {
            std::memcpy(dst + y * dst_row_bytes, src + y * src_row_bytes, dst_row_bytes);
        }
    }

    nozzle_frame_unlock_pixels(frame);
    nozzle_frame_release(frame);
    frame_new_ = true;
    return true;
}

bool NozzleReceiver::receive(tc::Texture& texture) {
    if (!receive(temp_pixels_)) return false;

    int w = temp_pixels_.getWidth();
    int h = temp_pixels_.getHeight();

    if (!texture.isAllocated() ||
        texture.getWidth() != w || texture.getHeight() != h) {
        texture.allocate(w, h, 4, tc::TextureUsage::Dynamic);
    }

    texture.loadData(temp_pixels_);
    return true;
}

} // namespace tcx
