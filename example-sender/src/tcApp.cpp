#include "tcApp.h"

void tcApp::setup() {
    if (!sender_.setup(kSenderName, kShareW, kShareH)) {
        logError() << "NozzleSender setup failed";
    }
    fbo_.allocate(kShareW, kShareH);
}

void tcApp::update() {
    // Render the shared frame into the Fbo, then publish it.
    float t = getElapsedTimef();

    fbo_.begin();
    clear(0.05f, 0.06f, 0.09f, 1.0f);

    // A ring of orbiting circles — something obviously animated to confirm the
    // receiver is live.
    const int n = 12;
    float cx = kShareW * 0.5f;
    float cy = kShareH * 0.5f;
    float radius = kShareH * 0.32f;
    for (int i = 0; i < n; i++) {
        float a = TAU * i / n + t * 0.6f;
        float x = cx + cos(a) * radius;
        float y = cy + sin(a) * radius;
        float hue = float(i) / n;
        setColor(0.5f + 0.5f * sin(TAU * hue),
                 0.5f + 0.5f * sin(TAU * (hue + 0.33f)),
                 0.5f + 0.5f * sin(TAU * (hue + 0.66f)));
        drawCircle(x, y, 26.0f);
    }

    setColor(1.0f);
    drawCircle(cx, cy, 18.0f + 6.0f * sin(t * 2.0f));
    fbo_.end();

    // Send the Fbo's texture via the GPU zero-copy path (blits straight into
    // nozzle's shared texture; no CPU readback).
    sender_.send(fbo_.getTexture());
}

void tcApp::draw() {
    clear(0.0f);
    fbo_.draw(0, 0, getWindowWidth(), getWindowHeight());

    setColor(1.0f);
    drawBitmapString("sender: \"" + sender_.getName() + "\"", 16, 24);
    drawBitmapString("frames sent: " + to_string(sender_.getFrameCount()), 16, 44);
}
