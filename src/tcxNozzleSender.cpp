#include "tcxNozzleSender.h"
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

struct NozzleSender::Impl {
    std::string name_{};
    bool setup_{false};
    int width_{0};
    int height_{0};
    uint64_t frame_count_{0};
    nozzle::sender sender_{};
    std::vector<unsigned char> pixel_buffer_{};
    std::unique_ptr<tc::Fbo> temp_fbo_;
    uint32_t last_nozzle_format_{0};
    int last_width_{0};
    int last_height_{0};

    ~Impl() { close(); }

    void close() {
        sender_ = {};
        temp_fbo_.reset();
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
};

NozzleSender::NozzleSender() : impl_{std::make_unique<Impl>()} {}
NozzleSender::~NozzleSender() = default;

NozzleSender::NozzleSender(NozzleSender &&other) noexcept = default;
NozzleSender &NozzleSender::operator=(NozzleSender &&other) noexcept = default;

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
    if (impl_->setup_) impl_->close();
    nozzle::sender_desc desc;
    desc.name = name;
    desc.application_name = "TrussC";
    desc.ring_buffer_size = 3;
    auto result = nozzle::sender::create(std::move(desc));
    if (!result.ok()) return false;
    impl_->sender_ = std::move(result.value());
    impl_->name_ = name;
    impl_->width_ = width;
    impl_->height_ = height;
    impl_->setup_ = true;
    return true;
}

void NozzleSender::close() {
    impl_->close();
}

bool NozzleSender::isSetup() const {
    return impl_->setup_;
}

bool NozzleSender::send(const unsigned char *pixels, int width, int height, int channels) {
    if (!impl_->setup_ || !pixels) return false;

    auto &sender = impl_->sender_;
    auto nfmt = (channels == 1) ? nozzle::texture_format::r8_unorm
              : (channels == 2) ? nozzle::texture_format::rg8_unorm
              : nozzle::texture_format::rgba8_unorm;

    nozzle::texture_desc td;
    td.width = static_cast<uint32_t>(width);
    td.height = static_cast<uint32_t>(height);
    td.format = nfmt;
    auto frame_result = sender.acquire_writable_frame(td);
    if (!frame_result.ok()) return false;

    auto &writable = frame_result.value();
    auto lock_result = nozzle::lock_writable_pixels_with_origin(writable, nozzle::texture_origin::top_left);
    if (!lock_result.ok()) return false;

    auto &mapped = lock_result.value();
    uint32_t src_row_bytes = static_cast<uint32_t>(width) * channels;
    uint32_t dst_row_bytes = mapped.row_stride_bytes;

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
    auto commit_result = sender.commit_frame(writable);
    if (!commit_result.ok()) return false;

    impl_->width_ = width;
    impl_->height_ = height;
    impl_->frame_count_++;
    return true;
}

bool NozzleSender::send(const tc::Pixels &pixels) {
    if (!pixels.isAllocated()) return false;
    return send(pixels.getData(), pixels.getWidth(), pixels.getHeight(), pixels.getChannels());
}

bool NozzleSender::send(tc::Fbo &fbo) {
    if (!impl_->setup_ || !fbo.isAllocated()) return false;
    int w = fbo.getWidth();
    int h = fbo.getHeight();
    size_t required = static_cast<size_t>(w) * h * 4;
    if (impl_->pixel_buffer_.size() != required) {
        impl_->pixel_buffer_.resize(required);
    }
    if (!fbo.readPixels(impl_->pixel_buffer_.data())) return false;
    return send(impl_->pixel_buffer_.data(), w, h, 4);
}

bool NozzleSender::send(const tc::Texture &texture) {
    if (!impl_->setup_ || !texture.isAllocated()) return false;
    int w = texture.getWidth();
    int h = texture.getHeight();
    sg_image img = texture.getImage();
    sg_pixel_format sfmt = texture.getPixelFormat();
    auto nfmt = sg_to_nozzle_format(sfmt);
    uint32_t nfmt_u32 = static_cast<uint32_t>(nfmt);

    auto &sender = impl_->sender_;
    nozzle::texture_desc td;
    td.width = static_cast<uint32_t>(w);
    td.height = static_cast<uint32_t>(h);
    td.format = nfmt;
    auto frame_result = sender.acquire_writable_frame(td);
    if (!frame_result.ok()) return false;

    auto &writable = frame_result.value();
    bool gpu_blit_ok = false;

#if NOZZLE_HAS_METAL
    {
        void *nozzle_tex = nozzle::metal::get_texture(writable.get_texture());
        if (nozzle_tex) {
            sg_mtl_image_info mtl_info = sg_mtl_query_image_info(img);
            void *sokol_tex = const_cast<void *>(mtl_info.tex[0]);
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
        auto commit_result = sender.commit_frame(writable);
        if (!commit_result.ok()) return false;
        impl_->width_ = w;
        impl_->height_ = h;
        impl_->frame_count_++;
        return true;
    }

    if (!impl_->temp_fbo_) {
        impl_->temp_fbo_ = std::make_unique<tc::Fbo>();
    }
    auto &fbo = impl_->temp_fbo_;
    if (!fbo->isAllocated() || fbo->getWidth() != w || fbo->getHeight() != h) {
        fbo->allocate(w, h);
    }
    fbo->begin();
    texture.draw(0, 0);
    fbo->end();
    return send(*fbo);
}

std::string NozzleSender::getName() const {
    return impl_->name_;
}

int NozzleSender::getWidth() const {
    return impl_->width_;
}

int NozzleSender::getHeight() const {
    return impl_->height_;
}

uint64_t NozzleSender::getFrameCount() const {
    return impl_->frame_count_;
}

} // namespace tcx
