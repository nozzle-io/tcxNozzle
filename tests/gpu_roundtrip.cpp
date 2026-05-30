// Windowed GPU zero-copy round-trip verifier (throwaway, not part of CI).
// Sender draws a 4-quadrant pattern into an Fbo and publishes its texture via
// the GPU path; receiver receives via the GPU path; we draw the received
// texture into a check Fbo, read it back, and assert the quadrant colors match
// (verifies transfer + orientation + format). Prints [GPU-VERIFY] PASS/FAIL.
#include <TrussC.h>
#include <tcxNozzle.h>
#include <vector>
#include <cstdlib>

using namespace std;
using namespace tc;
using namespace tcx;

class VerifyApp : public App {
public:
    void setup() override {
        sender_.setup("tcx-gpu-verify", N, N);
        srcFbo_.allocate(N, N);
        checkFbo_.allocate(N, N);
    }

    void draw() override {
        clear(0.0f);

        // Draw the asymmetric pattern into the source Fbo.
        srcFbo_.begin(0.0f, 0.0f, 0.0f, 1.0f);
        setColor(1.0f, 0.0f, 0.0f); drawRect(0, 0, N / 2, N / 2);          // TL red
        setColor(0.0f, 1.0f, 0.0f); drawRect(N / 2, 0, N / 2, N / 2);      // TR green
        setColor(0.0f, 0.0f, 1.0f); drawRect(0, N / 2, N / 2, N / 2);      // BL blue
        setColor(1.0f, 1.0f, 1.0f); drawRect(N / 2, N / 2, N / 2, N / 2);  // BR white
        srcFbo_.end();

        // GPU send.
        sender_.send(srcFbo_.getTexture());

        if (!receiver_.isConnected()) receiver_.connect("tcx-gpu-verify");
        bool got = receiver_.receive(rxTex_);  // GPU receive (CPU fallback inside)

        frame_++;
        if (frame_ >= 3 && got && rxTex_.isAllocated()) {
            // Draw the received texture into a check Fbo and read it back.
            checkFbo_.begin(0.0f, 0.0f, 0.0f, 1.0f);
            setColor(1.0f);
            rxTex_.draw(0, 0, N, N);
            checkFbo_.end();

            vector<unsigned char> px(N * N * 4, 0);
            checkFbo_.readPixels(px.data());

            auto at = [&](int x, int y) {
                int i = (y * N + x) * 4;
                return array<int, 3>{px[i], px[i + 1], px[i + 2]};
            };
            int q = N / 4, r = N / 4 + N / 2;
            auto tl = at(q, q), tr = at(r, q), bl = at(q, r), br = at(r, r);
            auto near = [](int v, int t) { return std::abs(v - t) < 70; };
            bool ok =
                near(tl[0], 255) && near(tl[1], 0)   && near(tl[2], 0)   &&
                near(tr[0], 0)   && near(tr[1], 255) && near(tr[2], 0)   &&
                near(bl[0], 0)   && near(bl[1], 0)   && near(bl[2], 255) &&
                near(br[0], 255) && near(br[1], 255) && near(br[2], 255);

            logNotice() << "[GPU-VERIFY] TL=" << tl[0] << "," << tl[1] << "," << tl[2]
                        << " TR=" << tr[0] << "," << tr[1] << "," << tr[2]
                        << " BL=" << bl[0] << "," << bl[1] << "," << bl[2]
                        << " BR=" << br[0] << "," << br[1] << "," << br[2];
            if (ok) logNotice() << "[GPU-VERIFY] PASS";
            else    logError()  << "[GPU-VERIFY] FAIL (expected TL=red TR=green BL=blue BR=white)";
            requestExitApp();
        }

        if (frame_ > 240) {
            logError() << "[GPU-VERIFY] TIMEOUT (no usable frame received)";
            requestExitApp();
        }
    }

private:
    static constexpr int N = 64;
    NozzleSender sender_;
    NozzleReceiver receiver_;
    Fbo srcFbo_, checkFbo_;
    Texture rxTex_;
    int frame_ = 0;
};

int main() {
    WindowSettings settings;
    settings.setSize(256, 256);
    settings.setTitle("tcxNozzle gpu-verify");
    return TC_RUN_APP(VerifyApp, settings);
}
