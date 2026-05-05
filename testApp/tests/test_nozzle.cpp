#include <TrussC.h>
#include <tcxNozzle.h>
#include <iostream>
#include <chrono>
#include <cstdlib>

static int g_total = 0;
static int g_passed = 0;
static int g_failed = 0;
static int g_skipped = 0;

static bool test(bool cond, const char *name, const char *file, int line) {
    g_total++;
    if (cond) {
        std::cout << "\033[1;32m[ PASS ]\033[0m " << name << std::endl;
        g_passed++;
        return true;
    }
    std::cout << "\033[1;31m[ FAIL ]\033[0m " << name << std::endl;
    std::cout << "         at " << file << ":" << line << std::endl;
    g_failed++;
    return false;
}

template<typename T1, typename T2>
static bool test_eq(T1 actual, T2 expected, const char *name, const char *file, int line) {
    g_total++;
    if (actual == expected) {
        std::cout << "\033[1;32m[ PASS ]\033[0m " << name << std::endl;
        g_passed++;
        return true;
    }
    std::cout << "\033[1;31m[ FAIL ]\033[0m " << name << std::endl;
    std::cout << "         expected: " << expected << std::endl;
    std::cout << "         actual:   " << actual << std::endl;
    std::cout << "         at " << file << ":" << line << std::endl;
    g_failed++;
    return false;
}

template<typename T1, typename T2>
static bool test_gt(T1 actual, T2 expected, const char *name, const char *file, int line) {
    g_total++;
    if (actual > expected) {
        std::cout << "\033[1;32m[ PASS ]\033[0m " << name << std::endl;
        g_passed++;
        return true;
    }
    std::cout << "\033[1;31m[ FAIL ]\033[0m " << name << std::endl;
    std::cout << "         expected: " << actual << " > " << expected << std::endl;
    std::cout << "         actual:   " << actual << " <= " << expected << std::endl;
    std::cout << "         at " << file << ":" << line << std::endl;
    g_failed++;
    return false;
}

static void skip(const char *name) {
    g_total++;
    g_skipped++;
    std::cout << "\033[1;33m[ SKIP ]\033[0m " << name << std::endl;
}

#define TEST(x, ...)      test(x, __VA_ARGS__, __FILE__, __LINE__)
#define TEST_EQ(x, y, ...) test_eq(x, y, __VA_ARGS__, __FILE__, __LINE__)
#define TEST_GT(x, y, ...) test_gt(x, y, __VA_ARGS__, __FILE__, __LINE__)
#define SKIP(...)         skip(__VA_ARGS__)

static bool ipc_available() {
    tcx::NozzleSender probe;
    if (!probe.setup("_nozzle_ipc_probe__", 1, 1)) return false;
    unsigned char px[4] = {0, 0, 0, 0};
    bool send_ok = probe.send(px, 1, 1, 4);
    probe.close();
    return send_ok;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "=== tcxNozzle headless tests ===" << std::endl;

    const bool have_ipc = ipc_available();
    if (!have_ipc) {
        std::cout << "\033[1;33mIPC not available — skipping IPC-dependent tests\033[0m" << std::endl;
    }

    // --- Unit tests (no IPC required) ---

    {
        tcx::NozzleSender sender;
        TEST(!sender.isSetup(), "sender default: not setup");
        TEST_EQ(sender.getName(), std::string{}, "sender default: empty name");
        TEST_EQ(sender.getWidth(), 0, "sender default: zero width");
        TEST_EQ(sender.getHeight(), 0, "sender default: zero height");
        TEST_EQ(sender.getFrameCount(), uint64_t(0), "sender default: zero frame count");
    }

    {
        tcx::NozzleReceiver receiver;
        TEST(!receiver.isConnected(), "receiver default: not connected");
        TEST_EQ(receiver.getSenderName(), std::string{}, "receiver default: empty name");
        TEST_EQ(receiver.getWidth(), 0, "receiver default: zero width");
        TEST_EQ(receiver.getHeight(), 0, "receiver default: zero height");
    }

    {
        tcx::NozzleReceiver receiver;
        bool ok = receiver.connect("nonexistent-sender-xyz");
        TEST(!ok, "receiver connect: nonexistent sender fails");
        TEST(!receiver.isConnected(), "receiver connect: not connected");
    }

    {
        tcx::NozzleReceiver receiver;
        receiver.disconnect();
        TEST(!receiver.isConnected(), "receiver disconnect: still not connected");
    }

    // --- IPC tests ---

    if (!have_ipc) {
        SKIP("sender setup: succeeds");
        SKIP("sender setup: is setup");
        SKIP("sender setup: name matches");
        SKIP("sender close: not setup after close");
        SKIP("sender setup twice: first setup");
        SKIP("sender setup twice: second setup");
        SKIP("sender setup twice: name updated");
        SKIP("sender send: raw RGBA pixels");
        SKIP("sender send: width updated");
        SKIP("sender send: height updated");
        SKIP("sender send: frame count incremented");
        SKIP("sender send: single channel (R8)");
        SKIP("sender send: null pixels returns false");
        SKIP("discovery: found at least one sender");
        SKIP("discovery: found our test sender");
        SKIP("cross-process: send succeeds");
        SKIP("cross-process: connect succeeds");
        SKIP("cross-process: is connected");
        SKIP("cross-process: receive succeeds");
        SKIP("cross-process: width matches");
        SKIP("cross-process: height matches");
        SKIP("cross-process: frame is new");
        SKIP("multi-send: frame count is 5");
    } else {
        {
            tcx::NozzleSender sender;
            bool ok = sender.setup("tcx-test-sender", 256, 256);
            TEST(ok, "sender setup: succeeds");
            TEST(sender.isSetup(), "sender setup: is setup");
            TEST_EQ(sender.getName(), std::string("tcx-test-sender"), "sender setup: name matches");
            sender.close();
            TEST(!sender.isSetup(), "sender close: not setup after close");
        }

        {
            tcx::NozzleSender sender;
            TEST(sender.setup("tcx-test-1"), "sender setup twice: first setup");
            TEST(sender.setup("tcx-test-2"), "sender setup twice: second setup");
            TEST_EQ(sender.getName(), std::string("tcx-test-2"), "sender setup twice: name updated");
        }

        {
            tcx::NozzleSender sender;
            sender.setup("tcx-pixel-test", 4, 4);
            std::vector<unsigned char> pixels(4 * 4 * 4, 128);
            TEST(sender.send(pixels.data(), 4, 4, 4), "sender send: raw RGBA pixels");
            TEST_EQ(sender.getWidth(), 4, "sender send: width updated");
            TEST_EQ(sender.getHeight(), 4, "sender send: height updated");
            TEST_EQ(sender.getFrameCount(), uint64_t(1), "sender send: frame count incremented");
        }

        {
            tcx::NozzleSender sender;
            sender.setup("tcx-r-test", 4, 4);
            std::vector<unsigned char> pixels(4 * 4, 200);
            TEST(sender.send(pixels.data(), 4, 4, 1), "sender send: single channel (R8)");
        }

        {
            tcx::NozzleSender sender;
            sender.setup("tcx-null-test", 4, 4);
            TEST(!sender.send(nullptr, 4, 4, 4), "sender send: null pixels returns false");
        }

        {
            tcx::NozzleSender sender;
            sender.setup("tcx-discover-test", 4, 4);
            std::vector<unsigned char> px(4 * 4 * 4, 0);
            sender.send(px.data(), 4, 4, 4);

            auto senders = tcx::NozzleReceiver::findSenders();
            TEST_GT(senders.size(), size_t(0), "discovery: found at least one sender");
            bool found = false;
            for (auto &s : senders) {
                if (s.name == "tcx-discover-test") {
                    found = true;
                    break;
                }
            }
            TEST(found, "discovery: found our test sender");
        }

        {
            tcx::NozzleSender sender;
            sender.setup("tcx-cross-test", 8, 8);
            std::vector<unsigned char> pixels(8 * 8 * 4, 255);
            TEST(sender.send(pixels.data(), 8, 8, 4), "cross-process: send succeeds");

            tcx::NozzleReceiver receiver;
            TEST(receiver.connect("tcx-cross-test"), "cross-process: connect succeeds");
            TEST(receiver.isConnected(), "cross-process: is connected");

            tc::Pixels received;
            bool received_ok = receiver.receive(received);
            TEST(received_ok, "cross-process: receive succeeds");
            if (received_ok) {
                TEST_EQ(receiver.getWidth(), 8, "cross-process: width matches");
                TEST_EQ(receiver.getHeight(), 8, "cross-process: height matches");
                TEST(receiver.isFrameNew(), "cross-process: frame is new");
            }
        }

        {
            tcx::NozzleSender sender;
            sender.setup("tcx-multi-test", 2, 2);
            std::vector<unsigned char> pixels(2 * 2 * 4, 0);
            for (int i = 0; i < 5; i++) {
                sender.send(pixels.data(), 2, 2, 4);
            }
            TEST_EQ(sender.getFrameCount(), uint64_t(5), "multi-send: frame count is 5");
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (g_failed == 0) {
        std::cout << std::endl << "\033[1;32m" << g_passed << "/" << g_total << " tests passed";
        if (g_skipped > 0) std::cout << " (" << g_skipped << " skipped)";
        std::cout << "\033[0m" << std::endl;
    } else {
        std::cout << std::endl << "\033[1;31m" << g_failed << "/" << g_total << " tests failed\033[0m" << std::endl;
    }
    std::cout << "took " << ms << "ms" << std::endl;

    return g_failed;
}
