// Minimal host app. Its only job is to give trussc_app() a target to attach
// the headless functional test to (see local.cmake) — the real tests live in
// test_nozzle.cpp and run as a POST_BUILD step.
#include <TrussC.h>
#include <tcxNozzle.h>

class App : public trussc::App {
    void setup() override {}
    void draw() override {}
};

int main() {
    trussc::WindowSettings settings;
    settings.setSize(128, 128);
    settings.setTitle("tcxNozzle tests (host)");
    return TC_RUN_APP(App, settings);
}
