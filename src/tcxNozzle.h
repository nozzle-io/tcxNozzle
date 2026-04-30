#pragma once

#include <string>
#include <vector>

namespace tc { class Pixels; class Texture; class Fbo; }

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

    bool setup(const std::string& name, int width = 0, int height = 0);
    void close();
    bool isSetup() const;

    bool send(const unsigned char* pixels, int width, int height, int channels = 4);
    bool send(const tc::Pixels& pixels);
    bool send(tc::Fbo& fbo);
    bool send(const tc::Texture& texture);

    std::string getName() const;
    int getWidth() const;
    int getHeight() const;
    uint64_t getFrameCount() const;

private:
    void *sender_ = nullptr;
    std::string name_;
    int width_ = 0;
    int height_ = 0;
    bool setup_ = false;
    uint64_t frame_count_ = 0;
    std::vector<unsigned char> pixel_buffer_;
    void *temp_fbo_ = nullptr;
    uint32_t last_nozzle_format_ = 0;
    int last_width_ = 0;
    int last_height_ = 0;
};

class NozzleReceiver {
public:
    NozzleReceiver() = default;
    ~NozzleReceiver() { disconnect(); }

    NozzleReceiver(const NozzleReceiver&) = delete;
    NozzleReceiver& operator=(const NozzleReceiver&) = delete;

    static std::vector<NozzleSenderInfo> findSenders();

    bool connect(const std::string& senderName);
    bool connect(const NozzleSenderInfo& source);
    void disconnect();
    bool isConnected() const;

    bool receive(tc::Pixels& pixels);
    bool receive(tc::Texture& texture);

    bool isFrameNew() const;

    std::string getSenderName() const;
    int getWidth() const;
    int getHeight() const;
    int getSenderFrameCount() const;

private:
    void *receiver_ = nullptr;
    std::string sender_name_;
    int width_ = 0;
    int height_ = 0;
    bool connected_ = false;
    bool frame_new_ = false;
    void *temp_pixels_ = nullptr;
};

} // namespace tcx
