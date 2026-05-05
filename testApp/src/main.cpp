#include "tcApp.h"

int main() {
    trussc::WindowSettings settings;
    settings.setSize(128, 128);
    return TC_RUN_APP(TestApp, settings);
}
