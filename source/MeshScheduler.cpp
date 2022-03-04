#include "MeshScheduler.h"
#include "MicroBit.h"

#define MESH_SCHEDULE_EVENT 50
#define MESH_COUNT_EVENT 51

using namespace codal;

extern MicroBit uBit;

MeshScheduler *MeshScheduler::instance = nullptr;

uint32_t counter = 0;

/*
* This is called every time a new slot 
*/
void onSlotEvent(MicroBitEvent e)
{
    counter++;
}

/*
* After 1 second, this is fired and outputs how many slot events have fired
*/
void onSlotCountEvent(MicroBitEvent e)
{
    DMESG("Slot counter: %d", counter);
    counter = 0;
}

MeshScheduler::MeshScheduler(Timer &timer) : timer(timer)
{
    setupListeners();
    setupSlots();
    clearFrames();

    instance = this;
}

void MeshScheduler::setupListeners()
{
    uBit.messageBus.listen(DEVICE_ID_RADIO, MESH_SCHEDULE_EVENT, onSlotEvent);
    uBit.messageBus.listen(DEVICE_ID_RADIO, MESH_COUNT_EVENT, onSlotCountEvent);

    system_timer_event_every(20, DEVICE_ID_RADIO, MESH_SCHEDULE_EVENT);
    system_timer_event_every(1000, DEVICE_ID_RADIO, MESH_COUNT_EVENT);
}

void MeshScheduler::setupSlots()
{
    for (int i = 0; i < MICROBIT_MESH_ADVERTISEMENT_SLOTS; i++)
    {
        slots[i].flags = MICROBIT_MESH_ADVERT;
    }

    for (int i = MICROBIT_MESH_ADVERTISEMENT_SLOTS; i < MICROBIT_MESH_ADVERTISEMENT_SLOTS + MICROBIT_MESH_STANDARD_SLOTS; i++)
    {
        slots[i].flags = MICROBIT_MESH_FREE;
    }
}

void MeshScheduler::incrementCurrentSlot()
{
    currentSlot++;

    if (currentSlot > MICROBIT_MESH_SLOTS - 1)
    {
        currentSlot = 0;
    }
}

bool MeshScheduler::isDuplicatePacket(MeshFrameBuffer *buffer)
{

}

int MeshScheduler::analysePacket(MeshFrameBuffer *buffer)
{
    // Duplicate frame
    if (frames[buffer->frameId] == 1)
    {
        return;
    }

    frames[buffer->frameId] = 1;
}

void MeshScheduler::clearFrames()
{
    for (int i = 0; i < MESH_MAX_FRAMES; i++)
    {
        frames[i] = 0;
    }
}