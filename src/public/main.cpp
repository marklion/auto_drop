#include "event_sc/ad_event_sc.h"

int main(int argc, char const *argv[])
{
    auto sc = std::make_shared<AD_EVENT_SC>();
    sc->startTimer(4, [&](){
        std::cout << "timer 4s" << std::endl;
    });
    auto external_sc = std::make_shared<AD_EVENT_SC>();
    external_sc->registerNode(sc);
    external_sc->startTimer(10, [&](){
        external_sc->stopEventLoop();
    });
    external_sc->runEventLoop();
    return 0;
}
