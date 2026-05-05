#include <TrussC.h>
#include <tcxNozzle.h>

class App : public trussc::App {
    void setup() override {}
    void draw() override {}
};

int main() {
    trussc::WindowSettings settings;
    settings.setSize(128, 128);
    return TC_RUN_APP(App, settings);
}
