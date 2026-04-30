#pragma once

#include <nozzle/nozzle_c.h>
#include <string>
#include <vector>
#include <cstring>

#ifdef _MSC_VER
namespace tc { class Pixels; class Texture; class Fbo; }
#else
namespace tc { class Pixels; class Texture; class Fbo; }
#endif

namespace tcx {

struct NozzleSenderInfo {
    std::string name;
    std::string application_name;
    int width;
    int height;
};

class NozzleSender {
public:
    NozzleSender() = default;
    ~NozzleSender() { close(); }

    NozzleSender(const NozzleSender&) = delete;
    NozzleSender& operator=(const NozzleSender&) = delete;

    bool setup(const std::string& name, int width = 0, int height = 0) {
        close();
        name_ = name;
        width_ = width;
        height_ = height;

        NozzleSenderDesc desc{};
        desc.name = name_.c_str();
        desc.application_name = "tcxNozzle";
        desc.ring_buffer_size = 3;

        NozzleErrorCode err = nozzle_sender_create(&desc, &sender_);
        if (err != NOZZLE_OK) {
            sender_ = nullptr;
            return false;
        }

        setup_ = true;
        return true;
    }

    void close() {
        if (sender_) {
            nozzle_sender_destroy(sender_);
            sender_ = nullptr;
        }
        setup_ = false;
    }

    bool isSetup() const { return setup_; }

    bool send(const unsigned char* pixels, int width, int height, int channels = 4) {
        if (!setup_ || !sender_ || !pixels) return false;

        NozzleTextureFormat fmt = (channels == 4) ? NOZZLE_FORMAT_RGBA8_UNORM
                                  : (channels == 1) ? NOZZLE_FORMAT_R8_UNORM
                                  : NOZZLE_FORMAT_RG8_UNORM;

        NozzleFrame* frame = nullptr;
        NozzleErrorCode err = nozzle_sender_acquire_writable_frame(
            sender_, width, height, fmt, &frame);
        if (err != NOZZLE_OK || !frame) return false;

        NozzleMappedPixels mapped{};
        err = nozzle_frame_lock_writable_pixels(frame, &mapped);
        if (err != NOZZLE_OK) {
            nozzle_frame_release(frame);
            return false;
        }

        uint32_t src_bpp = channels;
        uint32_t dst_bpp = (channels == 4) ? 4 : (channels == 1) ? 1 : 2;
        uint32_t copy_bytes = std::min(mapped.row_bytes, (uint32_t)(width * dst_bpp));

        auto* dst = static_cast<unsigned char*>(mapped.data);
        auto* src = static_cast<const unsigned char*>(pixels);

        if (src_bpp == dst_bpp && mapped.row_bytes == (uint32_t)(width * src_bpp)) {
            std::memcpy(dst, src, (size_t)height * mapped.row_bytes);
        } else {
            for (int y = 0; y < height; y++) {
                std::memcpy(dst + y * mapped.row_bytes,
                           src + y * width * src_bpp,
                           copy_bytes);
            }
        }

        nozzle_frame_unlock_writable_pixels(frame);

        err = nozzle_sender_commit_frame(sender_, frame);
        if (err != NOZZLE_OK) {
            nozzle_frame_release(frame);
            return false;
        }

        frame_count_++;
        return true;
    }

    bool send(const tc::Pixels& pixels);
    bool send(tc::Fbo& fbo);
    bool send(const tc::Texture& texture);

    std::string getName() const { return name_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    uint64_t getFrameCount() const { return frame_count_; }

private:
    NozzleSender* sender_ = nullptr;
    std::string name_;
    int width_ = 0;
    int height_ = 0;
    bool setup_ = false;
    uint64_t frame_count_ = 0;
    std::vector<unsigned char> pixel_buffer_;
    tc::Fbo temp_fbo_;
};

class NozzleReceiver {
public:
    NozzleReceiver() = default;
    ~NozzleReceiver() { disconnect(); }

    NozzleReceiver(const NozzleReceiver&) = delete;
    NozzleReceiver& operator=(const NozzleReceiver&) = delete;

    static std::vector<NozzleSenderInfo> findSenders(uint32_t timeoutMs = 1000) {
        std::vector<NozzleSenderInfo> result;

        int count = nozzle_discover_senders(nullptr, 0);
        if (count <= 0) return result;

        std::vector<NozzleSenderDiscovery> discovered;
        discovered.resize(count);
        count = nozzle_discover_senders(discovered.data(), count);

        for (int i = 0; i < count; i++) {
            NozzleSenderInfo info;
            info.name = discovered[i].name ? discovered[i].name : "";
            info.application_name = discovered[i].application_name ? discovered[i].application_name : "";
            info.width = discovered[i].width;
            info.height = discovered[i].height;
            result.push_back(info);
        }
        return result;
    }

    bool connect(const std::string& senderName) {
        disconnect();

        NozzleReceiverDesc desc{};
        desc.name = senderName.c_str();
        desc.application_name = "tcxNozzle";
        desc.receive_mode = NOZZLE_RECEIVE_LATEST_ONLY;

        NozzleErrorCode err = nozzle_receiver_create(&desc, &receiver_);
        if (err != NOZZLE_OK) {
            receiver_ = nullptr;
            return false;
        }

        connected_ = true;
        sender_name_ = senderName;
        return true;
    }

    bool connect(const NozzleSenderInfo& source) {
        return connect(source.name);
    }

    void disconnect() {
        if (receiver_) {
            nozzle_receiver_destroy(receiver_);
            receiver_ = nullptr;
        }
        connected_ = false;
        sender_name_.clear();
    }

    bool isConnected() const { return connected_; }

    bool receive(tc::Pixels& pixels);
    bool receive(tc::Texture& texture);

    bool isFrameNew() const { return frame_new_; }

    std::string getSenderName() const { return sender_name_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    int getSenderFrameCount() const {
        if (!receiver_) return 0;
        NozzleConnectedSenderInfo info{};
        if (nozzle_receiver_get_connected_info(receiver_, &info) != NOZZLE_OK) return 0;
        return (int)info.frame_counter;
    }

private:
    NozzleReceiver* receiver_ = nullptr;
    std::string sender_name_;
    int width_ = 0;
    int height_ = 0;
    bool connected_ = false;
    bool frame_new_ = false;
};

} // namespace tcx
