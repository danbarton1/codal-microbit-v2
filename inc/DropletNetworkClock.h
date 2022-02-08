#ifndef MICROBIT_DROPLET_NETWORK_CLOCK_H
#define MICROBIT_DROPLET_NETWORK_CLOCK_H

#include "CodalConfig.h"
#include "Timer.h"

namespace codel
{
    class DropletNetworkClock;
}

namespace codal
{
    class DropletNetworkClock
    {
    private:
        uint32_t time; // TODO: use ulong instead
        uint32_t nextSlotTime;
        Timer &timer;
    public:
        static DropletNetworkClock *instance; 
        DropletNetworkClock(Timer &timer);
        void init(uint32_t t);
        void updateTime(uint32_t packetSendTime);
        uint32_t getTime();
        void setTime(uint32_t t);
    };
}

#endif