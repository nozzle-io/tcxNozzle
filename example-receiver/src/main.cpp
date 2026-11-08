#include <TrussC.h>
#include "tcApp.h"

int main() {
    WindowSettings settings;
    settings.setSize(512, 512);
    settings.setTitle("tcxNozzle - receiver");
    return TC_RUN_APP(tcApp, settings);
}
