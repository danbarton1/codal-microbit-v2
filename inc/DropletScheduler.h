#ifndef MICROBIT_DROPLET_SCHEDULER_H
#define MICROBIT_DROPLET_SCHEDULER_H

#include "CodalConfig.h"
#include "Timer.h"

#define MICROBIT_DROPLET_STANDARD_SLOTS 50
#define MICROBIT_DROPLET_ADVERTISEMENT_SLOTS 1
#define MICROBIT_DROPLET_SLOT_DURATION 1 / MICROBIT_DROPLET_STANDARD_SLOTS
#define MICROBIT_DROPLET_SLOTS MICROBIT_DROPLET_STANDARD_SLOTS + MICROBIT_DROPLET_ADVERTISEMENT_SLOTS

#define MICROBIT_DROPLET_EXPIRATION 5

#define MICROBIT_DROPLET_MAX_FRAMES 10

#define MICROBIT_DROPLET_PREPARATION_WINDOW_MICROSECONDS 1000

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
        bool unused; // TODO: Remove this field, shouldn't be needed

        DropletSlot() : unused(true)
        {
            expiration = MICROBIT_DROPLET_EXPIRATION;
        }
    };

    class DropletScheduler
    {
    private:
        DropletSlot slots[MICROBIT_DROPLET_SLOTS];
        uint8_t currentSlot;
        uint8_t currentFrame;
        Timer &timer;
        uint8_t maxFrameId;
       // DropletFrameBuffer frames[MICROBIT_DROPLET_MAX_FRAMES];
    public:
        DropletScheduler(Timer &timer);
        uint8_t getSlotsToSleepFor();
        void markSlotAsTaken(uint8_t id);
        bool isSlotMine(uint8_t id);
        bool isNextSlotMine();
        void incrementError();
        void analysePacket(DropletFrameBuffer *buffer);
        DropletSlot * getSlots();
        uint8_t getCurrentSlot() const;
        void setCurrentSlot(uint8_t id);
        static DropletScheduler *instance;
    };
};
#endif