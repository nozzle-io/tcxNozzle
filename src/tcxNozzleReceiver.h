#pragma once

#include "tcxNozzleConfig.h"

#include <memory>
#include <string>
#include <vector>

#include <nozzle/types.hpp>

namespace trussc { class Pixels; class Texture; }
namespace tc = trussc;

namespace tcx {

// GPU backend a sender is published through. Mirrors nozzle::backend_type so the
// public header doesn't force users to include <nozzle/types.hpp>.
enum class NozzleBackend { Unknown, D3D11, Metal, OpenGL, DmaBuf };

// One entry from NozzleReceiver::listSenders(). Public fields with convenience
// getters, matching the TrussC device-info convention (VideoDeviceInfo etc.).
struct NozzleSenderInfo {
    std::string name;
    std::string applicationName;
    std::string id;                              // disambiguates same-named senders
    NozzleBackend backend = NozzleBackend::Unknown;

    const std::string &getName() const { return name; }
    const std::string &getApplicationName() const { return applicationName; }
    const std::string &getId() const { return id; }
    NozzleBackend getBackend() const { return backend; }
};

class NozzleReceiver {
public:
    NozzleReceiver();
    ~NozzleReceiver();

    NozzleReceiver(const NozzleReceiver &) = delete;
    NozzleReceiver &operator=(const NozzleReceiver &) = delete;
    NozzleReceiver(NozzleReceiver &&) noexcept;
    NozzleReceiver &operator=(NozzleReceiver &&) noexcept;

    // Discover senders currently sharing on this machine. Static + returns a
    // vector, matching the TrussC convention (AudioEngine::listDevices() etc.).
    static std::vector<NozzleSenderInfo> listSenders();

    bool connect(const std::string &senderName);
    bool connect(const NozzleSenderInfo &source);
    void disconnect();
    bool isConnected() const;

    // Receive into CPU pixels (always available) or a GPU Texture. The Texture
    // overload uses a GPU-to-GPU blit when the backend supports it (zero CPU
    // readback), falling back to the CPU path otherwise.
    bool receive(tc::Pixels &pixels);
    bool receive(tc::Texture &texture);

    bool isFrameNew() const;

    std::string getSenderName() const;
    int getWidth() const;
    int getHeight() const;
    int getSenderFrameCount() const;
    nozzle::texture_format getFormat() const;
    nozzle::texture_format getSemanticFormat() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tcx
