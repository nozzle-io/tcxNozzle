#include "tcxNozzleReceiver.h"
#include <TrussC.h>
#include <nozzle/nozzle.hpp>
#include <nozzle/pixel_access.hpp>
#include <cstring>

namespace tcx {

static NozzleBackend to_tcx_backend(nozzle::backend_type b) {
    switch (b) {
        case nozzle::backend_type::d3d11:   return NozzleBackend::D3D11;
        case nozzle::backend_type::metal:   return NozzleBackend::Metal;
        case nozzle::backend_type::opengl:  return NozzleBackend::OpenGL;
        case nozzle::backend_type::dma_buf: return NozzleBackend::DmaBuf;
        default:                            return NozzleBackend::Unknown;
    }
}

// Map a nozzle format to a TrussC TextureFormat for the GPU-blit destination.
// Returns false for formats we don't allocate a matching texture for (→ CPU path).
static bool nozzle_to_tc_format(nozzle::texture_format f, tc::TextureFormat &out) {
    switch (f) {
        case nozzle::texture_format::rgba8_unorm:  out = tc::TextureFormat::RGBA8;   return true;
        // macOS/Metal negotiates BGRA8 for the shared IOSurface; within tcxNozzle
        // the bytes are RGBA-ordered, so an RGBA8 destination samples correctly.
        case nozzle::texture_format::bgra8_unorm:  out = tc::TextureFormat::RGBA8;   return true;
        case nozzle::texture_format::rgba8_srgb:   out = tc::TextureFormat::RGBA8;   return true;
        case nozzle::texture_format::bgra8_srgb:   out = tc::TextureFormat::RGBA8;   return true;
        case nozzle::texture_format::rgba16_float: out = tc::TextureFormat::RGBA16F; return true;
        case nozzle::texture_format::rgba32_float: out = tc::TextureFormat::RGBA32F; return true;
        case nozzle::texture_format::r8_unorm:     out = tc::TextureFormat::R8;      return true;
        case nozzle::texture_format::r16_float:    out = tc::TextureFormat::R16F;    return true;
        case nozzle::texture_format::r32_float:    out = tc::TextureFormat::R32F;    return true;
        case nozzle::texture_format::rg8_unorm:    out = tc::TextureFormat::RG8;     return true;
        case nozzle::texture_format::rg16_float:   out = tc::TextureFormat::RG16F;   return true;
        case nozzle::texture_format::rg32_float:   out = tc::TextureFormat::RG32F;   return true;
        default:                                   return false;
    }
}

struct NozzleReceiver::Impl {
    nozzle::receiver receiver_{};
    std::string sender_name_{};
    int width_{0};
    int height_{0};
    nozzle::texture_format format_{nozzle::texture_format::unknown};
    nozzle::texture_format semantic_format_{nozzle::texture_format::unknown};
    bool connected_{false};
    bool frame_new_{false};
    std::unique_ptr<tc::Pixels> temp_pixels_;

    ~Impl() { disconnect(); }

    void disconnect() {
        receiver_ = {};
        temp_pixels_.reset();
        connected_ = false;
        sender_name_.clear();
        width_ = 0;
        height_ = 0;
        format_ = nozzle::texture_format::unknown;
        semantic_format_ = nozzle::texture_format::unknown;
        frame_new_ = false;
    }
};

NozzleReceiver::NozzleReceiver() : impl_{std::make_unique<Impl>()} {}
NozzleReceiver::~NozzleReceiver() = default;

NozzleReceiver::NozzleReceiver(NozzleReceiver &&other) noexcept = default;
NozzleReceiver &NozzleReceiver::operator=(NozzleReceiver &&other) noexcept = default;

std::vector<NozzleSenderInfo> NozzleReceiver::listSenders() {
    auto list = nozzle::enumerate_senders();
    std::vector<NozzleSenderInfo> result;
    result.reserve(list.size());
    for (auto &s : list) {
        NozzleSenderInfo info;
        info.name = s.name;
        info.applicationName = s.application_name;
        info.id = s.id;
        info.backend = to_tcx_backend(s.backend);
        result.push_back(std::move(info));
    }
    return result;
}

bool NozzleReceiver::connect(const std::string &senderName) {
    if (impl_->connected_) impl_->disconnect();
    nozzle::receiver_desc desc;
    desc.name = senderName;
    desc.application_name = "TrussC";
    auto result = nozzle::receiver::create(std::move(desc));
    if (!result.ok()) return false;
    impl_->receiver_ = std::move(result.value());
    impl_->sender_name_ = senderName;
    impl_->connected_ = true;
    impl_->frame_new_ = false;
    return true;
}

bool NozzleReceiver::connect(const NozzleSenderInfo &source) {
    return connect(source.name);
}

void NozzleReceiver::disconnect() {
    impl_->disconnect();
}

bool NozzleReceiver::isConnected() const {
    return impl_->connected_;
}

// Copy a frame's pixels into tc::Pixels via a CPU map. Caller owns frame lifetime.
static bool copyFrameToPixels(nozzle::frame &frm, tc::Pixels &pixels, int width, int height) {
    auto lock_result = nozzle::lock_frame_pixels_with_origin(frm, nozzle::texture_origin::top_left);
    if (!lock_result.ok()) return false;

    auto &mapped = lock_result.value();
    int channels = 4;
    if (!pixels.isAllocated() || pixels.getWidth() != width || pixels.getHeight() != height) {
        pixels.allocate(width, height, channels);
    }

    uint32_t src_row_bytes = mapped.row_stride_bytes;
    uint32_t dst_row_bytes = static_cast<uint32_t>(width) * channels;
    if (src_row_bytes == dst_row_bytes) {
        std::memcpy(pixels.getData(), mapped.data, static_cast<size_t>(height) * dst_row_bytes);
    } else {
        auto *src = static_cast<const unsigned char *>(mapped.data);
        auto *dst = pixels.getData();
        for (int y = 0; y < height; y++) {
            std::memcpy(dst + y * dst_row_bytes, src + y * src_row_bytes, dst_row_bytes);
        }
    }

    nozzle::unlock_frame_pixels(frm);
    return true;
}

bool NozzleReceiver::receive(tc::Pixels &pixels) {
    if (!impl_->connected_) return false;
    impl_->frame_new_ = false;

    auto frame_result = impl_->receiver_.acquire_frame();
    if (!frame_result.ok()) return false;

    auto &frm = frame_result.value();
    auto info = frm.info();
    impl_->width_ = static_cast<int>(info.width);
    impl_->height_ = static_cast<int>(info.height);
    impl_->format_ = info.format;
    impl_->semantic_format_ = info.semantic_format;

    bool ok = copyFrameToPixels(frm, pixels, impl_->width_, impl_->height_);
    frm.release();
    impl_->frame_new_ = ok;
    return ok;
}

bool NozzleReceiver::receive(tc::Texture &texture) {
    if (!impl_->connected_) return false;
    impl_->frame_new_ = false;

    auto frame_result = impl_->receiver_.acquire_frame();
    if (!frame_result.ok()) return false;

    auto &frm = frame_result.value();
    auto info = frm.info();
    int w = static_cast<int>(info.width);
    int h = static_cast<int>(info.height);
    impl_->width_ = w;
    impl_->height_ = h;
    impl_->format_ = info.format;
    impl_->semantic_format_ = info.semantic_format;

    // GPU-to-GPU blit: copy nozzle's shared texture straight into our Texture's
    // native GPU texture, with zero CPU readback. Only when the format maps to a
    // TrussC TextureFormat we can allocate to match (otherwise CPU fallback).
    tc::TextureFormat tcfmt = tc::TextureFormat::RGBA8;
    bool fmt_ok = nozzle_to_tc_format(info.format, tcfmt);
    bool gpu_ok = false;

#if defined(NOZZLE_HAS_METAL) || defined(NOZZLE_HAS_D3D11)
    if (fmt_ok) {
        if (!texture.isAllocated() || texture.getWidth() != w || texture.getHeight() != h ||
            texture.getPixelFormat() != tc::toSokolFormat(tcfmt)) {
            texture.allocate(w, h, tcfmt, tc::TextureUsage::Dynamic);
        }
        sg_image img = texture.getImage();
        void *dst_native = nullptr;
#if defined(NOZZLE_HAS_METAL)
        dst_native = const_cast<void *>(sg_mtl_query_image_info(img).tex[0]);
#elif defined(NOZZLE_HAS_D3D11)
        dst_native = const_cast<void *>(reinterpret_cast<const void *>(sg_d3d11_query_image_info(img).tex2d));
#endif
        if (dst_native) {
            auto r = frm.copy_to_native_texture(dst_native, static_cast<uint32_t>(w),
                                                 static_cast<uint32_t>(h), info.format);
            gpu_ok = r.ok();
        }
    }
#endif

    if (gpu_ok) {
        frm.release();
        impl_->frame_new_ = true;
        return true;
    }

    // CPU fallback: read the same frame's pixels and upload.
    if (!impl_->temp_pixels_) {
        impl_->temp_pixels_ = std::make_unique<tc::Pixels>();
    }
    bool ok = copyFrameToPixels(frm, *impl_->temp_pixels_, w, h);
    frm.release();
    if (!ok) return false;

    if (!texture.isAllocated() || texture.getWidth() != w || texture.getHeight() != h) {
        texture.allocate(w, h, 4, tc::TextureUsage::Dynamic);
    }
    texture.loadData(*impl_->temp_pixels_);
    impl_->frame_new_ = true;
    return true;
}

bool NozzleReceiver::isFrameNew() const {
    return impl_->frame_new_;
}

std::string NozzleReceiver::getSenderName() const {
    return impl_->sender_name_;
}

int NozzleReceiver::getWidth() const {
    return impl_->width_;
}

int NozzleReceiver::getHeight() const {
    return impl_->height_;
}

int NozzleReceiver::getSenderFrameCount() const {
    if (!impl_->connected_) return 0;
    return static_cast<int>(impl_->receiver_.connected_info().frame_counter);
}

nozzle::texture_format NozzleReceiver::getFormat() const {
    return impl_->format_;
}

nozzle::texture_format NozzleReceiver::getSemanticFormat() const {
    return impl_->semantic_format_;
}

} // namespace tcx
