#include "DropletNetworkClock.h"

using namespace codal;

DropletNetworkClock::DropletNetworkClock(Timer &timer) : timer(timer)
{

}

void DropletNetworkClock::init(uint32_t t)
{
    time = t;
}

void DropletNetworkClock::updateTime(uint32_t localSlotTime, uint32_t packetSlotTime, uint32_t currentTime)
{
    uint64_t startTime = ((uint64_t)localSlotTime + packetSlotTime) / 2;
    uint32_t timeToNextSlot = (2 * startTime) + MICROBIT_DROPLET_SLOT_DURATION - currentTime;
    timer.cancel(DEVICE_ID_RADIO, MICROBIT_DROPLET_SCHEDULE_EVENT);
    timer.eventEveryUs(timeToNextSlot, DEVICE_ID_RADIO, MICROBIT_DROPLET_SCHEDULE_EVENT);
}