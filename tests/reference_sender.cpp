// Reference nozzle SENDER — uses the nozzle C++ API directly (NOT the tcxNozzle
// wrapper), so it stands in for a "foreign" nozzle implementation. It publishes
// a deliberately asymmetric pattern (top-left origin, logical RGBA bytes) so
// that any vertical flip, horizontal flip, or R/B channel swap on the consuming
// side is immediately visible:
//
//   logical layout (top-left origin):
//     +-----------------+
//     | #  red ...   G  |   top half = RED, white(#) at TL, green(G) at TR
//     |    red ...      |
//     | blue ...        |   bottom half = BLUE
//     +-----------------+
//
//   - vertical flip   -> white anchor appears at BOTTOM-left, top half reads blue
//   - horizontal flip -> white anchor appears at top-RIGHT
//   - R/B swap         -> anchors stay at top, but top half reads BLUE (red<->blue)
//
// Run this, then point a receiver (reference_receiver, or tcxNozzle's
// example-receiver) at it and compare.
//
//   ./bin/reference_sender [name] [size]      (default: nozzle-ref 256)

#include <nozzle/nozzle.hpp>
#include <nozzle/pixel_access.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <csignal>
#include <thread>
#include <vector>

static volatile std::sig_atomic_t g_running = 1;
static void on_sigint(int) { g_running = 0; }

int main(int argc, char **argv) {
    const char *name = (argc > 1) ? argv[1] : "nozzle-ref";
    int N = (argc > 2) ? std::atoi(argv[2]) : 256;
    if (N < 16) N = 16;
    const int corner = N / 8;

    std::signal(SIGINT, on_sigint);

    nozzle::sender_desc desc{};
    desc.name = name;
    desc.application_name = "nozzle-reference";
    desc.ring_buffer_size = 3;
    auto sr = nozzle::sender::create(std::move(desc));
    if (!sr.ok()) {
        std::fprintf(stderr, "reference_sender: create failed: %s\n", sr.error().message.c_str());
        return 1;
    }
    auto sender = std::move(sr.value());
    std::printf("reference_sender: publishing \"%s\" %dx%d (RGBA, top-left origin)\n", name, N, N);
    std::printf("  top=RED bottom=BLUE, white@TL green@TR. Ctrl-C to stop.\n");

    // Build the pattern once (it's static).
    std::vector<unsigned char> pattern(static_cast<size_t>(N) * N * 4);
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            unsigned char r, g, b;
            if (x < corner && y < corner)              { r = 255; g = 255; b = 255; } // TL white
            else if (x >= N - corner && y < corner)    { r = 0;   g = 255; b = 0;   } // TR green
            else if (y < N / 2)                        { r = 255; g = 0;   b = 0;   } // top red
            else                                       { r = 0;   g = 0;   b = 255; } // bottom blue
            unsigned char *p = &pattern[(static_cast<size_t>(y) * N + x) * 4];
            p[0] = r; p[1] = g; p[2] = b; p[3] = 255;
        }
    }

    nozzle::texture_desc td{};
    td.width = static_cast<uint32_t>(N);
    td.height = static_cast<uint32_t>(N);
    td.format = nozzle::texture_format::rgba8_unorm;

    uint64_t frames = 0;
    while (g_running) {
        auto fr = sender.acquire_writable_frame(td);
        if (!fr.ok()) { std::this_thread::sleep_for(std::chrono::milliseconds(16)); continue; }
        auto &w = fr.value();
        auto lr = nozzle::lock_writable_pixels_with_origin(w, nozzle::texture_origin::top_left);
        if (!lr.ok()) { (void)sender.discard_frame(w); continue; }
        auto &m = lr.value();
        uint32_t dst_row = m.row_stride_bytes;
        uint32_t src_row = static_cast<uint32_t>(N) * 4;
        auto *dst = static_cast<unsigned char *>(m.data);
        for (int y = 0; y < N; ++y) {
            std::memcpy(dst + y * dst_row, &pattern[static_cast<size_t>(y) * src_row],
                        (src_row < dst_row) ? src_row : dst_row);
        }
        if (!nozzle::unlock_writable_pixels_checked(w).ok()) { (void)sender.discard_frame(w); continue; }
        if (!sender.commit_frame(w).ok()) continue;
        ++frames;
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    std::printf("reference_sender: stopped after %llu frames\n", (unsigned long long)frames);
    return 0;
}
