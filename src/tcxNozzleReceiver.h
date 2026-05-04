#pragma once

#include "tcxNozzleConfig.h"

#include <memory>
#include <string>
#include <vector>

namespace trussc { class Pixels; class Texture; }
namespace tc = trussc;

namespace tcx {

struct NozzleSenderInfo {
    std::string name;
    std::string application_name;
};

class NozzleReceiver {
public:
    NozzleReceiver();
    ~NozzleReceiver();

    NozzleReceiver(const NozzleReceiver &) = delete;
    NozzleReceiver &operator=(const NozzleReceiver &) = delete;
    NozzleReceiver(NozzleReceiver &&) noexcept;
    NozzleReceiver &operator=(NozzleReceiver &&) noexcept;

    static std::vector<NozzleSenderInfo> findSenders();

    bool connect(const std::string &senderName);
    bool connect(const NozzleSenderInfo &source);
    void disconnect();
    bool isConnected() const;

    bool receive(tc::Pixels &pixels);
    bool receive(tc::Texture &texture);

    bool isFrameNew() const;

    std::string getSenderName() const;
    int getWidth() const;
    int getHeight() const;
    int getSenderFrameCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tcx
