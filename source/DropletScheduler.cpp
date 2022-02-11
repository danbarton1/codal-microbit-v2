#include "DropletScheduler.h"
#include "Droplet.h"
#include "MicroBit.h"

using namespace codal;

extern MicroBit uBit;

DropletScheduler *DropletScheduler::instance = NULL;

void onExpirationCounterEvent(MicroBitEvent e)
{
    DropletSlot *p = DropletScheduler::instance->getSlots();

    // TODO: Set i as MICROBIT_DROPLET_ADVERTISEMENT_SLOTS

    for (int i = 0; i < MICROBIT_DROPLET_SLOTS; i++, p++)
    {
        if ((p->flags & MICROBIT_DROPLET_ADVERT) != MICROBIT_DROPLET_ADVERT)
        {
            p->expiration--;
        }
    }
}

DropletScheduler::DropletScheduler(Timer &timer) : timer(timer)
{
    for (int i = 0; i < MICROBIT_DROPLET_ADVERTISEMENT_SLOTS; i++)
    {
        slots[i].flags |= MICROBIT_DROPLET_ADVERT;
        slots[i].unused = false;
    }

    for (int i = MICROBIT_DROPLET_ADVERTISEMENT_SLOTS; i < MICROBIT_DROPLET_SLOTS; i++)
    {
        slots[i].flags |= MICROBIT_DROPLET_FREE;
    }

    instance = this;

    //this->timer.eventEvery(1000, DEVICE_ID_RADIO, MICROBIT_DROPLET_INITIALISATION_EVENT);
    // uBit.messageBus.listen(DEVICE_ID_RADIO, MICROBIT_DROPLET_INITIALISATION_EVENT, onExpirationCounterEvent);
}

void DropletScheduler::analysePacket(DropletFrameBuffer *buffer)
{
    DropletSlot slot = slots[buffer->slotIdentifier];

    if (slot.deviceIdentifier != buffer->deviceIdentifier)
    {
        slot.errors++;
    }
}

uint8_t DropletScheduler::getSlotsToSleepFor()
{
    // Circular array
    for (int i = currentSlot; i < currentSlot + MICROBIT_DROPLET_SLOTS; i++)
    {

        if (!slots[(i % MICROBIT_DROPLET_SLOTS)].unused)
        {
            return (i % MICROBIT_DROPLET_SLOTS) - 1;
        }
    }

    return 0;
}

void DropletScheduler::markSlotAsTaken(uint8_t id)
{
    // TODO: Return error if id is out of range
    slots[MICROBIT_DROPLET_ADVERTISEMENT_SLOTS + id - 1].unused = false;
}

bool DropletScheduler::isNextSlotMine()
{
    return slots[(currentSlot + 1) % MICROBIT_DROPLET_SLOTS].deviceIdentifier == uBit.getSerialNumber();
}

void DropletScheduler::incrementError(uint8_t slotId)
{
    slots[slotId].errors++;
}

DropletSlot *DropletScheduler::getSlots()
{
    return slots;
}