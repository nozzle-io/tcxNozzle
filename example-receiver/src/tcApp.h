#pragma once

#include <TrussC.h>
#include <tcxNozzle.h>

using namespace std;
using namespace tc;
using namespace tcx;

// Discovers nozzle senders, connects to one, and draws the received texture.
// Launch example-sender first, then run this app.
class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;

    void keyPressed(int key) override;

private:
    void rescan();

    NozzleReceiver receiver_;
    Texture tex_;

    vector<NozzleSenderInfo> senders_;
    float lastScan_ = 0.0f;
    bool gotFrame_ = false;
};
