#include "tcxNozzleReceiver.h"
#include <TrussC.h>
#include <nozzle/nozzle.hpp>
#include <nozzle/pixel_access.hpp>
#include <cstring>

namespace tcx {

struct NozzleReceiver::Impl {
    nozzle::receiver receiver_{};
    std::string sender_name_{};
    int width_{0};
    int height_{0};
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
        frame_new_ = false;
    }
};

NozzleReceiver::NozzleReceiver() : impl_{std::make_unique<Impl>()} {}
NozzleReceiver::~NozzleReceiver() = default;

NozzleReceiver::NozzleReceiver(NozzleReceiver &&other) noexcept = default;
NozzleReceiver &NozzleReceiver::operator=(NozzleReceiver &&other) noexcept = default;

std::vector<NozzleSenderInfo> NozzleReceiver::findSenders() {
    auto list = nozzle::enumerate_senders();
    std::vector<NozzleSenderInfo> result;
    for (auto &s : list) {
        result.push_back({s.name, s.application_name});
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

bool NozzleReceiver::receive(tc::Pixels &pixels) {
    if (!impl_->connected_) return false;
    impl_->frame_new_ = false;

    auto &receiver = impl_->receiver_;
    auto frame_result = receiver.acquire_frame();
    if (!frame_result.ok()) return false;

    auto &frm = frame_result.value();
    auto info = frm.info();

    impl_->width_ = static_cast<int>(info.width);
    impl_->height_ = static_cast<int>(info.height);

    auto lock_result = nozzle::lock_frame_pixels_with_origin(frm, nozzle::texture_origin::top_left);
    if (!lock_result.ok()) {
        frm.release();
        return false;
    }

    auto &mapped = lock_result.value();
    int channels = 4;
    if (!pixels.isAllocated() || pixels.getWidth() != impl_->width_ || pixels.getHeight() != impl_->height_) {
        pixels.allocate(impl_->width_, impl_->height_, channels);
    }

    uint32_t src_row_bytes = mapped.row_stride_bytes;
    uint32_t dst_row_bytes = static_cast<uint32_t>(impl_->width_) * channels;

    if (src_row_bytes == dst_row_bytes) {
        std::memcpy(pixels.getData(), mapped.data, static_cast<size_t>(impl_->height_) * dst_row_bytes);
    } else {
        auto *src = static_cast<const unsigned char *>(mapped.data);
        auto *dst = pixels.getData();
        for (int y = 0; y < impl_->height_; y++) {
            std::memcpy(dst + y * dst_row_bytes, src + y * src_row_bytes, dst_row_bytes);
        }
    }

    nozzle::unlock_frame_pixels(frm);
    frm.release();
    impl_->frame_new_ = true;
    return true;
}

bool NozzleReceiver::receive(tc::Texture &texture) {
    if (!impl_->temp_pixels_) {
        impl_->temp_pixels_ = std::make_unique<tc::Pixels>();
    }
    if (!receive(*impl_->temp_pixels_)) return false;

    int w = impl_->temp_pixels_->getWidth();
    int h = impl_->temp_pixels_->getHeight();

    if (!texture.isAllocated() ||
        texture.getWidth() != w || texture.getHeight() != h) {
        texture.allocate(w, h, 4, tc::TextureUsage::Dynamic);
    }

    texture.loadData(*impl_->temp_pixels_);
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
    return 0;
}

} // namespace tcx
