#ifndef MICROBIT_MESH_SCHEDULER_H
#define MICROBIT_MESH_SCHEDULER_H

#include "CodalConfig.h"
#include "Timer.h"

#define MICROBIT_MESH_STANDARD_SLOTS 50
#define MICROBIT_MESH_ADVERTISEMENT_SLOTS 1
#define MICROBIT_MESH_SLOT_DURATION_MILLISECONDS 1000 / MICROBIT_MESH_STANDARD_SLOTS
#define MICROBIT_MESH_SLOTS MICROBIT_MESH_STANDARD_SLOTS + MICROBIT_MESH_ADVERTISEMENT_SLOTS

#define MICROBIT_MESH_EXPIRATION 5

#define MICROBIT_MESH_MAX_FRAMES 10

#define MICROBIT_MESH_PREPARATION_WINDOW_MICROSECONDS 1000
#define MICROBIT_MESH_PREPARATION_WINDOW_RECEPTION_MICROSECONDS MICROBIT_MESH_PREPARATION_WINDOW_MICROSECONDS / 2

#define MICROBIT_MESH_DUPLICATE_PACKET 50
#define MICROBIT_MESH_NO_SLOTS 51

#define MICROBIT_MESH_OWNER 0
#define MICROBIT_MESH_PARTICIPANT 1

#define MESH_MAX_FRAMES 10

namespace codal
{
    struct MeshSlot;
    class MeshScheduler;
    struct MeshFrameBuffer;
};

namespace codal
{
    struct MeshSlot
    {
        uint64_t deviceId;
        uint8_t slotId;
        uint8_t expiration;
        uint8_t distance:4, flags:4;
        uint8_t errors;

        MeshSlot()
        {
            expiration = MICROBIT_MESH_EXPIRATION;
        }
    };

    class MeshScheduler
    {
    private:
        MeshSlot slots[MICROBIT_MESH_SLOTS];
        uint8_t frames[MESH_MAX_FRAMES];
        uint8_t currentSlot;
        Timer &timer;
        void setupListeners();
        void setupSlots();
    public:
        MeshScheduler(Timer &timer);
        void incrementCurrentSlot();
        bool isDuplicatePacket(MeshFrameBuffer *buffer);
        int analysePacket(MeshFrameBuffer *buffer);
        void clearFrames();
        static MeshScheduler *instance;
    };
};
#endif