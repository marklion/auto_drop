#include "event_sc/ad_event_sc.h"

int main(int argc, char const *argv[])
{
    auto external_sc = std::make_shared<AD_EVENT_SC>();
    external_sc->startTimer(10, [&](){
        external_sc->stopEventLoop();
    });
    external_sc->runEventLoop();
    return 0;
}
