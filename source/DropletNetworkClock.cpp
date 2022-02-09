#include "DropletNetworkClock.h"
#include "Microbit.h"
#include <string>
#include <sstream>
#include <iostream>

using namespace codal;
extern MicroBit uBit;

#define MILLISECONDS_TO_NANO 1000

DropletNetworkClock* DropletNetworkClock::instance = NULL;

void timerEveryUsEvent(MicroBitEvent e)
{
    //DropletNetworkClock::instance->setTime(0);
    DMESG("Timer network clock event");
}

DropletNetworkClock::DropletNetworkClock(Timer &timer) : timer(timer)
{
    instance = this;
    uBit.messageBus.listen(DEVICE_ID_RADIO, MICROBIT_DROPLET_SCHEDULE_EVENT, timerEveryUsEvent);
}

void DropletNetworkClock::init(uint32_t t)
{
    time = t;
    // DMESG("Clock init: %d", time);

    std::ostringstream ss;
    ss << time;

    DMESG("Clock init time: %s", ss.str().c_str());
    //nextSlotTime = time + MICROBIT_DROPLET_SLOT_DURATION * MILLISECONDS_TO_NANO;
}

void DropletNetworkClock::updateTime(uint32_t packetSendTime)
{
    // uint64_t startTime = ((uint64_t)localSlotTime + packetSlotTime) / 2;
    // MICROBIT_DROPLET_SLOT_DURATION needs to be in milliseconds 

    // Current time
    // Packet send time

    // This method should not be called after packet 0 for each slot

    // The packet send time is the time from the first micro:bit. It should be sent at the very start of the slot (time 0)
    // Therefore, we can use this time to sync our clock 
    // Could be worth calculating the average drift
    // So, keep a total of the difference and the number of times it has been calculated
    // Total difference += (packet send time - local slot start time)
    // Average drift = (difference) / (times)
    // synced time =  
    // Get 
    
    /*uint32_t estimatedCurrentSlotStartTime = ((uint64_t)packetSendTime + nextSlotTime - MICROBIT_DROPLET_SLOT_DURATION * MILLISECONDS_TO_NANO) / 2;
    uint32_t estimatedNextSlotStartTime = ((uint64_t)packetSendTime + nextSlotTime + MICROBIT_DROPLET_SLOT_DURATION * MILLISECONDS_TO_NANO) / 2;
    uint32_t timeUntilNextSlot = estimatedNextSlotStartTime - estimatedCurrentSlotStartTime;
    DMESG("Update time: %d %d %d %d", estimatedCurrentSlotStartTime, estimatedNextSlotStartTime, timeUntilNextSlot, packetSendTime);
    nextSlotTime = estimatedNextSlotStartTime;
    timer.cancel(DEVICE_ID_RADIO, MICROBIT_DROPLET_SCHEDULE_EVENT);
    timer.eventEveryUs(timeUntilNextSlot, DEVICE_ID_RADIO, MICROBIT_DROPLET_SCHEDULE_EVENT); */
}

uint32_t DropletNetworkClock::getTime()
{
    return time;
}

void DropletNetworkClock::setTime(uint32_t t)
{
    time = t;
}