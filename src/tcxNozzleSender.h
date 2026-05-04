#pragma once

#include "tcxNozzleConfig.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace trussc { class Pixels; class Texture; class Fbo; }
namespace tc = trussc;

namespace tcx {

class NozzleSender {
public:
    NozzleSender();
    ~NozzleSender();

    NozzleSender(const NozzleSender &) = delete;
    NozzleSender &operator=(const NozzleSender &) = delete;
    NozzleSender(NozzleSender &&) noexcept;
    NozzleSender &operator=(NozzleSender &&) noexcept;

    bool setup(const std::string &name, int width = 0, int height = 0);
    void close();
    bool isSetup() const;

    bool send(const unsigned char *pixels, int width, int height, int channels = 4);
    bool send(const tc::Pixels &pixels);
    bool send(tc::Fbo &fbo);
    bool send(const tc::Texture &texture);

    std::string getName() const;
    int getWidth() const;
    int getHeight() const;
    uint64_t getFrameCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tcx
