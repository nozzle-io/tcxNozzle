#include "tcApp.h"

void TestApp::run() {
    tc::logNotice("tcxNozzle") << "=== tcxNozzle tests ===";

    {
        tcx::NozzleSender sender;
        tcxTest(!sender.isSetup(), "sender default: not setup");
        tcxTestEq(sender.getName(), std::string{}, "sender default: empty name");
        tcxTestEq(sender.getWidth(), 0, "sender default: zero width");
        tcxTestEq(sender.getHeight(), 0, "sender default: zero height");
        tcxTestEq(sender.getFrameCount(), uint64_t(0), "sender default: zero frame count");
    }

    {
        tcx::NozzleSender sender;
        bool ok = sender.setup("tcx-test-sender", 256, 256);
        tcxTest(ok, "sender setup: succeeds");
        tcxTest(sender.isSetup(), "sender setup: is setup");
        tcxTestEq(sender.getName(), std::string("tcx-test-sender"), "sender setup: name matches");
        sender.close();
        tcxTest(!sender.isSetup(), "sender close: not setup");
    }

    {
        tcx::NozzleSender sender;
        tcxTest(sender.setup("tcx-test-1"), "sender setup twice: first setup");
        tcxTest(sender.setup("tcx-test-2"), "sender setup twice: second setup");
        tcxTestEq(sender.getName(), std::string("tcx-test-2"), "sender setup twice: name updated");
    }

    {
        tcx::NozzleSender sender;
        sender.setup("tcx-pixel-test", 4, 4);
        std::vector<unsigned char> pixels(4 * 4 * 4, 128);
        tcxTest(sender.send(pixels.data(), 4, 4, 4), "sender send: raw RGBA pixels");
        tcxTestEq(sender.getWidth(), 4, "sender send: width updated");
        tcxTestEq(sender.getHeight(), 4, "sender send: height updated");
        tcxTestEq(sender.getFrameCount(), uint64_t(1), "sender send: frame count incremented");
    }

    {
        tcx::NozzleSender sender;
        sender.setup("tcx-r-test", 4, 4);
        std::vector<unsigned char> pixels(4 * 4, 200);
        tcxTest(sender.send(pixels.data(), 4, 4, 1), "sender send: single channel (R8)");
    }

    {
        tcx::NozzleSender sender;
        sender.setup("tcx-null-test", 4, 4);
        tcxTest(!sender.send(nullptr, 4, 4, 4), "sender send: null pixels returns false");
    }

    {
        tcx::NozzleReceiver receiver;
        tcxTest(!receiver.isConnected(), "receiver default: not connected");
        tcxTestEq(receiver.getSenderName(), std::string{}, "receiver default: empty name");
        tcxTestEq(receiver.getWidth(), 0, "receiver default: zero width");
        tcxTestEq(receiver.getHeight(), 0, "receiver default: zero height");
    }

    {
        tcx::NozzleReceiver receiver;
        bool ok = receiver.connect("nonexistent-sender-xyz");
        tcxTest(!ok, "receiver connect: nonexistent sender fails");
        tcxTest(!receiver.isConnected(), "receiver connect: not connected");
    }

    {
        tcx::NozzleReceiver receiver;
        receiver.disconnect();
        tcxTest(!receiver.isConnected(), "receiver disconnect: still not connected");
    }

    {
        auto senders = tcx::NozzleReceiver::findSenders();
        tcxTestGt(senders.size(), size_t(0), "discovery: found at least one sender");
        bool found = false;
        for (auto &s : senders) {
            if (s.name == "tcx-pixel-test" || s.name == "tcx-test-sender") {
                found = true;
                break;
            }
        }
        tcxTest(found, "discovery: found our test sender");
    }

    {
        tcx::NozzleSender sender;
        sender.setup("tcx-cross-test", 8, 8);
        std::vector<unsigned char> pixels(8 * 8 * 4, 255);
        tcxTest(sender.send(pixels.data(), 8, 8, 4), "cross-process: send succeeds");

        tcx::NozzleReceiver receiver;
        tcxTest(receiver.connect("tcx-cross-test"), "cross-process: connect succeeds");
        tcxTest(receiver.isConnected(), "cross-process: is connected");

        tc::Pixels received;
        bool received_ok = receiver.receive(received);
        tcxTest(received_ok, "cross-process: receive succeeds");
        if (received_ok) {
            tcxTestEq(receiver.getWidth(), 8, "cross-process: width matches");
            tcxTestEq(receiver.getHeight(), 8, "cross-process: height matches");
            tcxTest(receiver.isFrameNew(), "cross-process: frame is new");
        }
    }

    {
        tcx::NozzleSender sender;
        sender.setup("tcx-multi-test", 2, 2);
        std::vector<unsigned char> pixels(2 * 2 * 4, 0);
        for (int i = 0; i < 5; i++) {
            sender.send(pixels.data(), 2, 2, 4);
        }
        tcxTestEq(sender.getFrameCount(), uint64_t(5), "multi-send: frame count is 5");
    }
}
