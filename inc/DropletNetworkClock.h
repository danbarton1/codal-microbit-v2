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
        uint32_t time; 
        uint32_t nextSlotTime;
        Timer &timer;

        uint32_t networkTime;
        uint32_t localTime;
    public:
        static DropletNetworkClock *instance; 
        DropletNetworkClock(Timer &timer);
        void init(uint32_t t);
        void updateTime(uint32_t packetSendTime);
        uint32_t getTime() const;
        uint32_t getTimeUntilNextSlot() const;
        void setTime(uint32_t t);

        void setNetworkTime(uint32_t time);

        /**
         * Network time is only updated on the first packet received for each slot
         * Use the relative difference between network time and local time to work out 
         */ 
        uint32_t getNetworkTime();
        void setLocalTime(uint32_t time);
        uint32_t getLocalTime();

    };
}

#endif