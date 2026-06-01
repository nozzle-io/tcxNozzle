#pragma once

#include <TrussC.h>
#include <tcxNozzle.h>

using namespace std;
using namespace tc;
using namespace tcx;

// Draws an animated pattern into an Fbo and publishes it through nozzle every
// frame. Run this first, then launch example-receiver to see the frames appear.
class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;

private:
    static constexpr int kShareW = 512;
    static constexpr int kShareH = 512;
    static constexpr const char* kSenderName = "tcxNozzle-example";

    NozzleSender sender_;
    Fbo fbo_;
};
