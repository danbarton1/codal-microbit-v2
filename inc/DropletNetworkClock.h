#ifndef MICROBIT_DROPLET_NETWORK_CLOCK_H
#define MICROBIT_DROPLET_NETWORK_CLOCK_H

#include "CodalConfig.h"
#include "Timer.h"
#include "Droplet.h"

namespace codel
{
    class DropletNetworkClock;
}

namespace codal
{
    class DropletNetworkClock
    {
    private:
        uint32_t time;
        Timer &timer;
    public:
        DropletNetworkClock(Timer &timer);
        void init(uint32_t t);
        void updateTime(uint32_t localSlotTime, uint32_t packetSlotTime, uint32_t currentTime);
    };
}

#endif