// Reference nozzle RECEIVER — uses the nozzle C++ API directly (NOT the
// tcxNozzle wrapper). Connects to a sender, reads one frame with top-left
// origin (nozzle's documented contract), and renders it to the terminal as
// ASCII so orientation / channel order is visible without a GUI:
//
//   '#' white   'R' red   'G' green   'B' blue   '.' other
//
// It also dumps the raw RGBA bytes at the four corners. Compare against what
// reference_sender publishes (top=RED, white@TL, green@TR):
//   - top ASCII rows show B instead of R  -> R/B channel swap
//   - '#' appears on the BOTTOM            -> vertical flip
//   - '#' appears on the RIGHT             -> horizontal flip
//
//   ./bin/reference_receiver [name] [--loop]   (default: tcxNozzle-example)

#include <nozzle/nozzle.hpp>
#include <nozzle/pixel_access.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <csignal>
#include <thread>

static volatile std::sig_atomic_t g_running = 1;
static void on_sigint(int) { g_running = 0; }

static char classify(int r, int g, int b) {
    if (r > 160 && g > 160 && b > 160) return '#';      // white
    int mx = r; char c = 'R';
    if (g > mx) { mx = g; c = 'G'; }
    if (b > mx) { mx = b; c = 'B'; }
    if (mx < 60) return '.';                            // too dark to tell
    return c;
}

static void render(const unsigned char *data, int w, int h, uint32_t row_bytes) {
    const int cols = 32, rows = 16;
    std::printf("  +"); for (int i = 0; i < cols; ++i) std::printf("-"); std::printf("+\n");
    for (int ry = 0; ry < rows; ++ry) {
        std::printf("  |");
        for (int rx = 0; rx < cols; ++rx) {
            int x = (rx * w) / cols + w / (cols * 2);
            int y = (ry * h) / rows + h / (rows * 2);
            const unsigned char *p = data + static_cast<size_t>(y) * row_bytes + static_cast<size_t>(x) * 4;
            std::printf("%c", classify(p[0], p[1], p[2]));
        }
        std::printf("|\n");
    }
    std::printf("  +"); for (int i = 0; i < cols; ++i) std::printf("-"); std::printf("+\n");

    auto dump = [&](const char *label, int x, int y) {
        const unsigned char *p = data + static_cast<size_t>(y) * row_bytes + static_cast<size_t>(x) * 4;
        std::printf("  %s (%d,%d) = R%d G%d B%d A%d\n", label, x, y, p[0], p[1], p[2], p[3]);
    };
    dump("TL", 4, 4);
    dump("TR", w - 5, 4);
    dump("BL", 4, h - 5);
    dump("BR", w - 5, h - 5);
}

int main(int argc, char **argv) {
    const char *name = "tcxNozzle-example";
    bool loop = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--loop") == 0) loop = true;
        else name = argv[i];
    }

    std::signal(SIGINT, on_sigint);

    nozzle::receiver_desc desc{};
    desc.name = name;
    desc.application_name = "nozzle-reference";

    // Wait for the sender to appear.
    nozzle::receiver receiver;
    for (int tries = 0; tries < 100 && g_running; ++tries) {
        auto rr = nozzle::receiver::create(desc);
        if (rr.ok()) { receiver = std::move(rr.value()); break; }
        if (tries == 0) std::printf("reference_receiver: waiting for sender \"%s\"...\n", name);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!receiver.valid()) {
        std::fprintf(stderr, "reference_receiver: could not connect to \"%s\"\n", name);
        return 1;
    }
    std::printf("reference_receiver: connected to \"%s\"\n", name);

    int printed = 0;
    while (g_running) {
        auto fr = receiver.acquire_frame();
        if (!fr.ok()) { std::this_thread::sleep_for(std::chrono::milliseconds(33)); continue; }
        auto &frm = fr.value();
        auto info = frm.info();
        auto lr = nozzle::lock_frame_pixels_with_origin(frm, nozzle::texture_origin::top_left);
        if (!lr.ok()) { frm.release(); continue; }
        auto &m = lr.value();
        std::printf("frame %dx%d format=%d (top-left origin):\n",
                    (int)info.width, (int)info.height, (int)info.format);
        render(static_cast<const unsigned char *>(m.data), (int)info.width, (int)info.height, m.row_stride_bytes);
        nozzle::unlock_frame_pixels(frm);
        frm.release();
        if (!loop) break;
        ++printed;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return 0;
}
