#include "DropletScheduler.h"
#include "Droplet.h"
#include "MicroBit.h"

using namespace codal;

extern MicroBit uBit;

DropletScheduler *DropletScheduler::instance = nullptr;

void onNextSlotEvent(MicroBitEvent e)
{
    uint8_t id = DropletScheduler::instance->getCurrentSlot();
    id = (id + 1) % MICROBIT_DROPLET_SLOTS;
    DropletScheduler::instance->setCurrentSlot(id);

    // Only do this if the slot if not free
    DropletSlot slot = DropletScheduler::instance->getSlots()[id];

    if ((slot.flags & MICROBIT_DROPLET_FREE) == MICROBIT_DROPLET_FREE)//!slot.unused)
    {
        if (!Droplet::instance->isEnabled())
        {
            Droplet::instance->enable();
        }
    }
    else
    {
        if (!Droplet::instance->isEnabled())
        {
            Droplet::instance->disable();
        }
    }

}

void onExpirationCounterEvent(MicroBitEvent e)
{
    DropletSlot *p = DropletScheduler::instance->getSlots();

    // TODO: Set i as MICROBIT_DROPLET_ADVERTISEMENT_SLOTS
    // TODO: Only expiration-- if a packet has not been received
    // TODO: If a packet has been received, set the expiration back to max

    for (int i = 0; i < MICROBIT_DROPLET_SLOTS; i++, p++)
    {
        if ((p->flags & MICROBIT_DROPLET_ADVERT) != MICROBIT_DROPLET_ADVERT)
        {
            p->expiration--;

            if (p->expiration <= 0)
            {
                p->flags |= MICROBIT_DROPLET_FREE;
                p->expiration = MICROBIT_DROPLET_EXPIRATION;
            }
        }
    }
}

void onRadioWakeUpEvent(MicroBitEvent e)
{
    Droplet::instance->enable();
    // TODO: Update current slot
}

DropletScheduler::DropletScheduler(Timer &timer) : currentFrame(0), timer(timer)
{
    for (int i = 0; i < MICROBIT_DROPLET_ADVERTISEMENT_SLOTS; i++)
    {
        slots[i].flags |= MICROBIT_DROPLET_ADVERT;
    }

    for (int i = MICROBIT_DROPLET_ADVERTISEMENT_SLOTS; i < MICROBIT_DROPLET_SLOTS; i++)
    {
        slots[i].flags |= MICROBIT_DROPLET_FREE;
    }

    instance = this;

    this->timer.eventEvery(1000, DEVICE_ID_RADIO, MICROBIT_DROPLET_INITIALISATION_EVENT);
    uBit.messageBus.listen(DEVICE_ID_RADIO, MICROBIT_DROPLET_INITIALISATION_EVENT, onExpirationCounterEvent);
    uBit.messageBus.listen(DEVICE_ID_RADIO, MICROBIT_DROPLET_SCHEDULE_EVENT, onNextSlotEvent);
    uBit.messageBus.listen(DEVICE_ID_RADIO, MICROBIT_DROPLET_WAKE_UP_EVENT, onRadioWakeUpEvent);
}

void DropletScheduler::analysePacket(DropletFrameBuffer *buffer)
{
    // TODO: check if this is the first packet received for this slot
    // TODO: If it is, send the start time to the network clock
    // TODO: Then we can sort out when the next slot is going to happen
    DropletSlot *slot = &slots[buffer->slotIdentifier];

    // Invalid packet
    if (slot->deviceIdentifier != buffer->deviceIdentifier)
    {
        slot->errors++;
        return;
    }

    slot->expiration = MICROBIT_DROPLET_EXPIRATION;

    uint8_t frameId = buffer->frameIdentifier;

    // TODO: Maybe not the best way to check for lost frames
    if (frameId <= currentFrame)
    {
        
    }

    if ((buffer->flags & MICROBIT_DROPLET_DONE) == MICROBIT_DROPLET_DONE)
    {
        // We have received the last packet, so shut down the radio
        // TODO: Only do this if the next slot is empty
        uint8_t slotsToSleepFor = getSlotsToSleepFor();

        if (slotsToSleepFor > 0)
        {
            // TODO: Should be:
            // TODO: timeToNextSlot + slotsToSleepFor * MICROBIT_DROPLET_SLOT_DURATION - preparationWindow
            uint32_t t = (DropletNetworkClock::instance->getTime() + slotsToSleepFor * MICROBIT_DROPLET_SLOT_DURATION) * 1000 - MICROBIT_DROPLET_PREPARATION_WINDOW_MICROSECONDS;
            //timer.eventAfterUs(t, DEVICE_ID_RADIO, MICROBIT_DROPLET_WAKE_UP_EVENT); 
            system_timer_event_every_us(t, DEVICE_ID_RADIO, MICROBIT_DROPLET_WAKE_UP_EVENT);
            currentSlot = (currentSlot + slotsToSleepFor) % MICROBIT_DROPLET_SLOTS;
            Droplet::instance->disable();
        }
        
        maxFrameId = frameId;
    }
}

uint8_t DropletScheduler::getSlotsToSleepFor()
{
    // Circular array
    for (int i = currentSlot; i < currentSlot + MICROBIT_DROPLET_SLOTS; i++)
    {

        if ((slots[(i % MICROBIT_DROPLET_SLOTS)].flags & MICROBIT_DROPLET_FREE) != MICROBIT_DROPLET_FREE)
        {
            return (i % MICROBIT_DROPLET_SLOTS) - 1;
        }
    }

    return 0;
}

void DropletScheduler::markSlotAsTaken(uint8_t id)
{
    // TODO: Mark slot as not free
    slots[MICROBIT_DROPLET_ADVERTISEMENT_SLOTS + id - 1].flags &= ~MICROBIT_DROPLET_FREE; 
}

void DropletScheduler::queueAdvertisement()
{
    // Pick a random number between 1 and 5 (with 1 being the next advertisement slot)
    // Wait for that slot
    // Send request
}

bool DropletScheduler::isSlotMine(uint8_t id)
{
    return slots[id].deviceIdentifier == uBit.getSerialNumber();
}

bool DropletScheduler::isNextSlotMine()
{
    return slots[(currentSlot + 1) % MICROBIT_DROPLET_SLOTS].deviceIdentifier == uBit.getSerialNumber();
}

void DropletScheduler::incrementError()
{
    slots[currentSlot].errors++;
}

DropletSlot *DropletScheduler::getSlots()
{
    return slots;
}
uint8_t DropletScheduler::getCurrentSlot() const
{
    return currentSlot;
}
void DropletScheduler::setCurrentSlot(uint8_t id)
{
    currentSlot = id;
}
