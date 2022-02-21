#include "DropletScheduler.h"
#include "Droplet.h"
#include "MicroBit.h"

using namespace codal;

extern MicroBit uBit;

DropletScheduler* DropletScheduler::instance = nullptr;

void onNextSlotEvent(MicroBitEvent e)
{
    uint8_t id = DropletScheduler::instance->getCurrentSlot();
    id = (id + 1) % MICROBIT_DROPLET_SLOTS;
    DropletScheduler::instance->setCurrentSlot(id);

    DropletScheduler::instance->deleteFrames();

    // Only do this if the slot if not free
    DropletSlot slot = DropletScheduler::instance->getSlots()[id];

    if ((slot.flags & MICROBIT_DROPLET_FREE) == MICROBIT_DROPLET_FREE || (slot.flags & MICROBIT_DROPLET_ADVERT) == MICROBIT_DROPLET_ADVERT)
    {
        if (!Droplet::instance->isEnabled())
        {
            Droplet::instance->enable();
        }

        DropletNetworkClock::instance->slotStartTime();
    }
    else
    {
        if (Droplet::instance->isEnabled())
        {
            Droplet::instance->disable();
        }
    }

    // Check if the slot if advert
    // If so, increment the advert counter
    // If the advert counter >= goal
    // Send packet

    auto result = DropletScheduler::instance->sendAdvertisement(slot);

    if (result == MICROBIT_OK)
    {
        DMESG("Found slot");
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

DropletScheduler::DropletScheduler(Timer &timer) : currentFrame(0), timer(timer), frames(), advertCounter(0)
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

    // We have received an advertisement packet
    // Add it to our local schedule
    if ((buffer->flags & MICROBIT_DROPLET_ADVERT) == MICROBIT_DROPLET_ADVERT)
    {
        setSlot(buffer->slotIdentifier, buffer->deviceIdentifier);
        return;
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
    // bool isLastPacket?
    // For optimisation purposes, could be worth using uint8_t and bit packing isFirstPacket and isLastPacket

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

uint32_t codal::DropletScheduler::getFirstFreeSlot(uint8_t &slotId)
{
    for (DropletSlot slot : slots)
    {
        if ((slot.flags & MICROBIT_DROPLET_FREE) == MICROBIT_DROPLET_FREE)
        {
            slotId = slot.slotIdentifier;
            return MICROBIT_OK;
        }
    }

    return MICROBIT_DROPLET_NO_SLOTS;
}

void DropletScheduler::queueAdvertisement()
{
    // Pick a random number between 1 and 5 (with 1 being the next advertisement slot)
    // Wait for that slot
    // Pick a free slot
    // Send request

    int num = microbit_random(4) + 1;
    advertGoal = advertCounter + num;

    sendAdvertPacket = true;
}

void codal::DropletScheduler::deleteFrames() 
{   
    for (int i = 0; i < MICROBIT_DROPLET_MAX_FRAMES; i++)
    {
        delete frames[i];
    }
}

void codal::DropletScheduler::sendAdvertisement(DropletSlot slot) 
{
    if ((slot.flags & MICROBIT_DROPLET_ADVERT) == MICROBIT_DROPLET_ADVERT)
    {
        advertCounter++;

        if (advertCounter >= advertGoal && sendAdvertPacket)
        {
            uint8_t slot;

            uint32_t result = getFirstFreeSlot(slot);

            // We have found a free slot!
            if (result == MICROBIT_OK)
            {
                DMESG("Slot: %d", slot);

                uint64_t deviceId = uBit.getSerialNumber();

                DropletFrameBuffer *advert = new DropletFrameBuffer();
                advert->length = MICROBIT_DROPLET_HEADER_SIZE - 1;
                advert->flags |= MICROBIT_DROPLET_ADVERT;
                advert->slotIdentifier = slot;
                advert->deviceIdentifier = deviceId;
                advert->protocol = MICROBIT_RADIO_PROTOCOL_DATAGRAM;
                advert->initialTtl = MICROBIT_DROPLET_ADVERTISEMENT_TTL;
                advert->ttl = MICROBIT_DROPLET_ADVERTISEMENT_TTL;
                advert->startTime = uBit.systemTime();

                Droplet::instance->send(advert);
                sendAdvertPacket = false;

                // Adding myself to my local schedule
                setSlot(slot, deviceId);

                return MICROBIT_OK;
            }
            else
            {
                // Maybe try again?
                return MICROBIT_DROPLET_NO_SLOTS;
            }
        }
    }

    return MICROBIT_DROPLET_ERROR;
}

void codal::DropletScheduler::setSlot(uint8_t slotId, uint64_t deviceId) 
{
    if (slotId > MICROBIT_DROPLET_SLOTS)
        return MICROBIT_DROPLET_ERROR;

    // TODO: Check if the slot is actually an advertisement
    // If it is, return error

    slots[slotId].slotIdentifier = slotId;
    slots[slotId].deviceIdentifier = deviceId;
    slots[slotId].expiration = MICROBIT_DROPLET_EXPIRATION;
    slots[slotId].errors = 0;
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
