#include "tcxNozzle.h"
#include <TrussC.h>
#include <nozzle/nozzle.hpp>
#include <nozzle/pixel_access.hpp>
#include <cstring>

#if NOZZLE_HAS_METAL
#include <nozzle/backends/metal.hpp>
#endif

#if NOZZLE_HAS_D3D11
#include <nozzle/backends/d3d11.hpp>
#endif

namespace tcx::blit {
bool copy_gpu_texture(void *dst, const void *src, int width, int height, uint32_t nozzle_format);
}

namespace tcx {

static nozzle::sender *get_sender(void *p) {
    return static_cast<nozzle::sender *>(p);
}

static nozzle::receiver *get_receiver(void *p) {
    return static_cast<nozzle::receiver *>(p);
}

static tc::Fbo *get_temp_fbo(void *p) {
    return static_cast<tc::Fbo *>(p);
}

static tc::Pixels *get_temp_pixels(void *p) {
    return static_cast<tc::Pixels *>(p);
}

static nozzle::texture_format sg_to_nozzle_format(sg_pixel_format fmt) {
    switch (fmt) {
        case SG_PIXELFORMAT_RGBA8: return nozzle::texture_format::rgba8_unorm;
        case SG_PIXELFORMAT_RGBA16F: return nozzle::texture_format::rgba16_float;
        case SG_PIXELFORMAT_RGBA32F: return nozzle::texture_format::rgba32_float;
        case SG_PIXELFORMAT_R8: return nozzle::texture_format::r8_unorm;
        case SG_PIXELFORMAT_R16F: return nozzle::texture_format::r16_float;
        case SG_PIXELFORMAT_R32F: return nozzle::texture_format::r32_float;
        case SG_PIXELFORMAT_RG8: return nozzle::texture_format::rg8_unorm;
        case SG_PIXELFORMAT_RG16F: return nozzle::texture_format::rg16_float;
        case SG_PIXELFORMAT_RG32F: return nozzle::texture_format::rg32_float;
        default: return nozzle::texture_format::rgba8_unorm;
    }
}

bool NozzleSender::setup(const std::string &name, int width, int height) {
    if (setup_) close();
    nozzle::sender_desc desc;
    desc.name = name;
    desc.application_name = "TrussC";
    desc.ring_buffer_size = 3;
    auto result = nozzle::sender::create(std::move(desc));
    if (!result.ok()) return false;
    sender_ = new nozzle::sender(std::move(result.value()));
    name_ = name;
    width_ = width;
    height_ = height;
    setup_ = true;
    return true;
}

void NozzleSender::close() {
    if (sender_) {
        delete get_sender(sender_);
        sender_ = nullptr;
    }
    if (temp_fbo_) {
        delete get_temp_fbo(temp_fbo_);
        temp_fbo_ = nullptr;
    }
    setup_ = false;
    name_.clear();
    width_ = 0;
    height_ = 0;
    frame_count_ = 0;
    pixel_buffer_.clear();
    last_nozzle_format_ = 0;
    last_width_ = 0;
    last_height_ = 0;
}

bool NozzleSender::isSetup() const {
    return setup_;
}

bool NozzleSender::send(const unsigned char *pixels, int width, int height, int channels) {
    if (!setup_ || !pixels) return false;

    auto *sender = get_sender(sender_);
    auto nfmt = (channels == 1) ? nozzle::texture_format::r8_unorm
              : (channels == 2) ? nozzle::texture_format::rg8_unorm
              : nozzle::texture_format::rgba8_unorm;

    nozzle::texture_desc td;
    td.width = static_cast<uint32_t>(width);
    td.height = static_cast<uint32_t>(height);
    td.format = nfmt;
    auto frame_result = sender->acquire_writable_frame(td);
    if (!frame_result.ok()) return false;

    auto &writable = frame_result.value();
    auto lock_result = nozzle::lock_writable_pixels(writable);
    if (!lock_result.ok()) return false;

    auto &mapped = lock_result.value();
    uint32_t src_row_bytes = static_cast<uint32_t>(width) * channels;
    uint32_t dst_row_bytes = mapped.row_bytes;

    if (src_row_bytes == dst_row_bytes) {
        std::memcpy(mapped.data, pixels, static_cast<size_t>(height) * dst_row_bytes);
    } else {
        auto *src = static_cast<const unsigned char *>(pixels);
        auto *dst = static_cast<unsigned char *>(mapped.data);
        uint32_t copy_bytes = (src_row_bytes < dst_row_bytes) ? src_row_bytes : dst_row_bytes;
        for (int y = 0; y < height; y++) {
            std::memcpy(dst + y * dst_row_bytes, src + y * src_row_bytes, copy_bytes);
        }
    }

    nozzle::unlock_writable_pixels(writable);
    auto commit_result = sender->commit_frame(writable);
    if (!commit_result.ok()) return false;

    width_ = width;
    height_ = height;
    frame_count_++;
    return true;
}

bool NozzleSender::send(const tc::Pixels &pixels) {
    if (!pixels.isAllocated()) return false;
    return send(pixels.getData(), pixels.getWidth(), pixels.getHeight(), pixels.getChannels());
}

bool NozzleSender::send(tc::Fbo &fbo) {
    if (!setup_ || !fbo.isAllocated()) return false;
    int w = fbo.getWidth();
    int h = fbo.getHeight();
    size_t required = static_cast<size_t>(w) * h * 4;
    if (pixel_buffer_.size() != required) {
        pixel_buffer_.resize(required);
    }
    if (!fbo.readPixels(pixel_buffer_.data())) return false;
    return send(pixel_buffer_.data(), w, h, 4);
}

bool NozzleSender::send(const tc::Texture &texture) {
    if (!setup_ || !texture.isAllocated()) return false;
    int w = texture.getWidth();
    int h = texture.getHeight();
    sg_image img = texture.getImage();
    sg_pixel_format sfmt = texture.getPixelFormat();
    auto nfmt = sg_to_nozzle_format(sfmt);
    uint32_t nfmt_u32 = static_cast<uint32_t>(nfmt);

    auto *sender = get_sender(sender_);
    nozzle::texture_desc td;
    td.width = static_cast<uint32_t>(w);
    td.height = static_cast<uint32_t>(h);
    td.format = nfmt;
    auto frame_result = sender->acquire_writable_frame(td);
    if (!frame_result.ok()) return false;

    auto &writable = frame_result.value();
    bool gpu_blit_ok = false;

#if NOZZLE_HAS_METAL
    {
        void *nozzle_tex = nozzle::metal::get_texture(writable.get_texture());
        if (nozzle_tex) {
            sg_mtl_image_info mtl_info = sg_mtl_query_image_info(img);
            void *sokol_tex = mtl_info.tex[0];
            if (sokol_tex) {
                gpu_blit_ok = tcx::blit::copy_gpu_texture(nozzle_tex, sokol_tex, w, h, nfmt_u32);
            }
        }
    }
#elif NOZZLE_HAS_D3D11
    {
        ID3D11Texture2D *nozzle_tex = nozzle::d3d11::get_texture(writable.get_texture());
        if (nozzle_tex) {
            sg_d3d11_image_info d3d_info = sg_d3d11_query_image_info(img);
            void *sokol_tex = const_cast<void *>(d3d_info.tex2d);
            if (sokol_tex) {
                gpu_blit_ok = tcx::blit::copy_gpu_texture(nozzle_tex, sokol_tex, w, h, nfmt_u32);
            }
        }
    }
#endif

    if (gpu_blit_ok) {
        auto commit_result = sender->commit_frame(writable);
        if (!commit_result.ok()) return false;
        width_ = w;
        height_ = h;
        frame_count_++;
        return true;
    }

    // CPU fallback
    if (!temp_fbo_) {
        temp_fbo_ = new tc::Fbo();
    }
    auto *fbo = get_temp_fbo(temp_fbo_);
    if (!fbo->isAllocated() || fbo->getWidth() != w || fbo->getHeight() != h) {
        fbo->allocate(w, h);
    }
    fbo->begin();
    texture.draw(0, 0);
    fbo->end();
    return send(*fbo);
}

std::string NozzleSender::getName() const {
    return name_;
}

int NozzleSender::getWidth() const {
    return width_;
}

int NozzleSender::getHeight() const {
    return height_;
}

uint64_t NozzleSender::getFrameCount() const {
    return frame_count_;
}

// ========== NozzleReceiver ==========

std::vector<NozzleSenderInfo> NozzleReceiver::findSenders() {
    auto list = nozzle::enumerate_senders();
    std::vector<NozzleSenderInfo> result;
    for (auto &s : list) {
        result.push_back({s.name, s.application_name});
    }
    return result;
}

bool NozzleReceiver::connect(const std::string &senderName) {
    if (connected_) disconnect();
    nozzle::receiver_desc desc;
    desc.name = senderName;
    desc.application_name = "TrussC";
    auto result = nozzle::receiver::create(std::move(desc));
    if (!result.ok()) return false;
    receiver_ = new nozzle::receiver(std::move(result.value()));
    sender_name_ = senderName;
    connected_ = true;
    frame_new_ = false;
    return true;
}

bool NozzleReceiver::connect(const NozzleSenderInfo &source) {
    return connect(source.name);
}

void NozzleReceiver::disconnect() {
    if (receiver_) {
        delete get_receiver(receiver_);
        receiver_ = nullptr;
    }
    if (temp_pixels_) {
        delete get_temp_pixels(temp_pixels_);
        temp_pixels_ = nullptr;
    }
    connected_ = false;
    sender_name_.clear();
    width_ = 0;
    height_ = 0;
    frame_new_ = false;
}

bool NozzleReceiver::isConnected() const {
    return connected_;
}

bool NozzleReceiver::receive(tc::Pixels &pixels) {
    if (!connected_ || !receiver_) return false;
    frame_new_ = false;

    auto *receiver = get_receiver(receiver_);
    auto frame_result = receiver->acquire_frame();
    if (!frame_result.ok()) return false;

    auto &frm = frame_result.value();
    auto info = frm.info();

    width_ = static_cast<int>(info.width);
    height_ = static_cast<int>(info.height);

    auto lock_result = nozzle::lock_frame_pixels(frm);
    if (!lock_result.ok()) {
        frm.release();
        return false;
    }

    auto &mapped = lock_result.value();
    int channels = 4;
    if (!pixels.isAllocated() || pixels.getWidth() != width_ || pixels.getHeight() != height_) {
        pixels.allocate(width_, height_, channels);
    }

    uint32_t src_row_bytes = mapped.row_bytes;
    uint32_t dst_row_bytes = static_cast<uint32_t>(width_) * channels;

    if (src_row_bytes == dst_row_bytes) {
        std::memcpy(pixels.getData(), mapped.data, static_cast<size_t>(height_) * dst_row_bytes);
    } else {
        auto *src = static_cast<const unsigned char *>(mapped.data);
        auto *dst = pixels.getData();
        for (int y = 0; y < height_; y++) {
            std::memcpy(dst + y * dst_row_bytes, src + y * src_row_bytes, dst_row_bytes);
        }
    }

    nozzle::unlock_frame_pixels(frm);
    frm.release();
    frame_new_ = true;
    return true;
}

bool NozzleReceiver::receive(tc::Texture &texture) {
    if (!temp_pixels_) {
        temp_pixels_ = new tc::Pixels();
    }
    if (!receive(*get_temp_pixels(temp_pixels_))) return false;

    int w = get_temp_pixels(temp_pixels_)->getWidth();
    int h = get_temp_pixels(temp_pixels_)->getHeight();

    if (!texture.isAllocated() ||
        texture.getWidth() != w || texture.getHeight() != h) {
        texture.allocate(w, h, 4, tc::TextureUsage::Dynamic);
    }

    texture.loadData(*get_temp_pixels(temp_pixels_));
    return true;
}

bool NozzleReceiver::isFrameNew() const {
    return frame_new_;
}

std::string NozzleReceiver::getSenderName() const {
    return sender_name_;
}

int NozzleReceiver::getWidth() const {
    return width_;
}

int NozzleReceiver::getHeight() const {
    return height_;
}

int NozzleReceiver::getSenderFrameCount() const {
    return 0;
}

} // namespace tcx
