#ifndef MICROBIT_DROPLET_SCHEDULER_H
#define MICROBIT_DROPLET_SCHEDULER_H

#include "CodalConfig.h"
#include "Timer.h"

#define MICROBIT_DROPLET_STANDARD_SLOTS 50
#define MICROBIT_DROPLET_ADVERTISEMENT_SLOTS 1
#define MICROBIT_DROPLET_SLOT_DURATION 1 / MICROBIT_DROPLET_STANDARD_SLOTS
#define MICROBIT_DROPLET_SLOTS MICROBIT_DROPLET_STANDARD_SLOTS + MICROBIT_DROPLET_ADVERTISEMENT_SLOTS

namespace codal
{
    struct DropletSlot;
    class DropletScheduler;
    struct DropletFrameBuffer;
};

namespace codal
{
    struct DropletSlot
    {
        uint64_t deviceIdentifier;
        uint8_t slotIdentifier;
        uint8_t expiration;
        uint8_t distance:4, flags:4;
        uint8_t errors;
        bool unused;

        DropletSlot() : unused(true)
        {

        }
    };

    class DropletScheduler
    {
    private:
        DropletSlot slots[MICROBIT_DROPLET_SLOTS];
        uint8_t currentSlot;
        Timer &timer;
    public:
        DropletScheduler(Timer &timer);
        uint8_t getSlotsToSleepFor();
        void markSlotAsTaken(uint8_t id);
        bool isNextSlotMine();
        void incrementError(uint8_t slotId);
        void analysePacket(DropletFrameBuffer *buffer);
        static DropletScheduler *instance;
    };
};
#endif