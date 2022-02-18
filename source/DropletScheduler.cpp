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

        DropletNetworkClock::instance->slotStartTime();
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
}

DropletScheduler::DropletScheduler(Timer &timer) : currentFrame(0), timer(timer), frames()
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

    this->timer.eventEvery(1000, DEVICE_ID_RADIO, MICROBIT_DROPLET_EXPIRATION_EVENT);
    uBit.messageBus.listen(DEVICE_ID_RADIO, MICROBIT_DROPLET_EXPIRATION_EVENT, onExpirationCounterEvent);
    //uBit.messageBus.listen(DEVICE_ID_RADIO, MICROBIT_DROPLET_SCHEDULE_EVENT, onNextSlotEvent);
    uBit.messageBus.listen(DEVICE_ID_RADIO, MICROBIT_DROPLET_WAKE_UP_EVENT, onRadioWakeUpEvent);
}

uint32_t DropletScheduler::analysePacket(DropletFrameBuffer *buffer)
{
    // TODO: Here we need to check if we have already processed a packet with the same frame buffer
    DropletSlot *slot = &slots[buffer->slotIdentifier];

    // Invalid packet
    if (slot->deviceIdentifier != buffer->deviceIdentifier)
    {
        slot->errors++;
        return MICROBIT_INVALID_PARAMETER;
    }

    // Duplicate packet
    if (frames[buffer->frameIdentifier])
    {
        return MICROBIT_DROPLET_DUPLICATE_PACKET;
    }

    frames[buffer->frameIdentifier] = buffer;

    slot->expiration = MICROBIT_DROPLET_EXPIRATION;

    uint8_t frameId = buffer->frameIdentifier;

    // On receive, reset the expiration counter
    // Shouldn't actually need to use the KEEP_ALIVE flag
    // But it will be kept just in case
    slot->expiration = MICROBIT_DROPLET_EXPIRATION;

    if (isFirstPacket)
    {   
        DropletNetworkClock::instance->setNetworkTime(buffer->startTime);
        isFirstPacket = false;
    }
    // TODO: Once we get here, ignore all packets from device, even if it is its slot

    if ((buffer->flags & MICROBIT_DROPLET_DONE) == MICROBIT_DROPLET_DONE)
    {
        // We have received the last packet, so shut down the radio
        // TODO: Only do this if the next slot is empty
        uint8_t slotsToSleepFor = getSlotsToSleepFor();

        if (slotsToSleepFor > 0)
        {
            // timeToNextSlot + slotsToSleepFor * MICROBIT_DROPLET_SLOT_DURATION - preparationWindow
            // TODO: Only update the time if this is the first packet
            // TODO: getNetworkTime() should actually be the local time (synced to the network)
            uint32_t t = (DropletNetworkClock::instance->getPredictedNetworkTime() + slotsToSleepFor * MICROBIT_DROPLET_SLOT_DURATION_MILLISECONDS) * 1000 - MICROBIT_DROPLET_PREPARATION_WINDOW_MICROSECONDS;
            //timer.eventAfterUs(t, DEVICE_ID_RADIO, MICROBIT_DROPLET_WAKE_UP_EVENT); 
            system_timer_event_every_us(t, DEVICE_ID_RADIO, MICROBIT_DROPLET_WAKE_UP_EVENT);
            currentSlot = (currentSlot + slotsToSleepFor) % MICROBIT_DROPLET_SLOTS;
            Droplet::instance->disable();
        }
        
        maxFrameId = frameId;
        isFirstPacket = true;
    }

    return MICROBIT_OK;
}

uint8_t DropletScheduler::getSlotsToSleepFor()
{
    // Circular array
    for (int i = currentSlot; i < currentSlot + MICROBIT_DROPLET_SLOTS; i++)
    {
        // TODO: This might not work as intended with advertisment slots
        if ((slots[(i % MICROBIT_DROPLET_SLOTS)].flags & MICROBIT_DROPLET_FREE) != MICROBIT_DROPLET_FREE)
        {
            return (i % MICROBIT_DROPLET_SLOTS) - 1;
        }
    }

    return 0;
}

void DropletScheduler::markSlotAsTaken(uint8_t id)
{
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
