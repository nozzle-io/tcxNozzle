#include "tcApp.h"

void tcApp::setup() {
    font_.load(TC_FONT_SANS, 14);
    rescan();
}

void tcApp::rescan() {
    senders_ = NozzleReceiver::findSenders();
    lastScan_ = getElapsedTimef();

    // Auto-connect to the first sender we find if we aren't already connected.
    if (!receiver_.isConnected() && !senders_.empty()) {
        if (receiver_.connect(senders_.front())) {
            logNotice() << "connected to " << receiver_.getSenderName();
        }
    }
}

void tcApp::update() {
    // Re-scan for senders roughly once a second so newly launched senders show up.
    if (getElapsedTimef() - lastScan_ > 1.0f) {
        rescan();
    }

    if (receiver_.isConnected()) {
        if (receiver_.receive(tex_) && receiver_.isFrameNew()) {
            gotFrame_ = true;
        }
    }
}

void tcApp::draw() {
    clear(0.05f, 0.06f, 0.09f, 1.0f);

    if (gotFrame_ && tex_.isAllocated()) {
        tex_.draw(0, 0, getWindowWidth(), getWindowHeight());
    }

    setColor(1.0f);
    if (receiver_.isConnected()) {
        font_.drawString("connected: \"" + receiver_.getSenderName() + "\"  " +
                       to_string(receiver_.getWidth()) + "x" + to_string(receiver_.getHeight()),
                   16, 24);
    } else {
        font_.drawString("no sender connected — start example-sender", 16, 24);
    }

    // List discovered senders.
    font_.drawString("senders found: " + to_string(senders_.size()) + "  (press R to rescan)", 16, 44);
    float y = 68;
    for (auto& s : senders_) {
        font_.drawString("- " + s.name + " (" + s.application_name + ")", 28, y);
        y += 20;
    }
}

void tcApp::keyPressed(int key) {
    if (key == 'r' || key == 'R') {
        receiver_.disconnect();
        gotFrame_ = false;
        rescan();
    }
}
