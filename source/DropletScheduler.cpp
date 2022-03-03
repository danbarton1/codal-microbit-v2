#include "DropletScheduler.h"
#include "Droplet.h"
#include "MicroBit.h"
#include <sstream>

using namespace codal;

extern MicroBit uBit;

DropletScheduler* DropletScheduler::instance = nullptr;

void onNextSlotEvent(MicroBitEvent e)
{
    DropletScheduler::instance->setIsFirstPacketToTrue();
    uint8_t id = DropletScheduler::instance->getCurrentSlot();
    id++;

    if (id > MICROBIT_DROPLET_SLOTS)
        id = 0;

    DropletScheduler::instance->setCurrentSlot(id);

    // DMESG("Current slot id: %d", id);

    DropletScheduler::instance->deleteFrames();

    // Only do this if the slot if not free
    DropletSlot slot = DropletScheduler::instance->getSlots()[id];

    // Either has to be not free or an advertisement slot
    if (!((slot.flags & MICROBIT_DROPLET_FREE) == MICROBIT_DROPLET_FREE) || (slot.flags & MICROBIT_DROPLET_ADVERT) == MICROBIT_DROPLET_ADVERT)
    {
        if (!Droplet::instance->isEnabled())
        {
            Droplet::instance->enable();
        }

        DropletNetworkClock::instance->slotStartTime();
        uint16_t status = DropletScheduler::instance->getCurrentSlotStatus();

        // We own this slot, send the packet
        if (status == MICROBIT_DROPLET_OWNER)
        {
            DMESG("Owner - slot id: %d", id);
            Droplet::instance->sendTx();
        }
        // The slot is open which means we are a participant
        // Shouldn't have to anything but listen
        else if ((slot.flags & MICROBIT_DROPLET_ADVERT) == MICROBIT_DROPLET_ADVERT)
        {
            DMESG("Advert - slot id: %d", id);
        }
        else
        {
            std::ostringstream ss;
            ss << slot.deviceId;
            DMESG("Participant - slot id: %d device id: %s", id, ss.str().c_str());
        }
    }
    else
    {
        //DMESG("Empty slot");
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
    deleteFrames();

    for (int i = 0; i < MICROBIT_DROPLET_ADVERTISEMENT_SLOTS; i++)
    {
        slots[i].flags |= MICROBIT_DROPLET_ADVERT;
    }

    for (int i = MICROBIT_DROPLET_ADVERTISEMENT_SLOTS; i < MICROBIT_DROPLET_SLOTS; i++)
    {
        slots[i].flags |= MICROBIT_DROPLET_FREE;
    }

    instance = this;

    this->isFirstPacket = true;
    this->sendAdvertPacket = true;
    this->advertGoal = 0;
    this->currentSlot = MICROBIT_DROPLET_ADVERTISEMENT_SLOTS;
    this->timer.eventEvery(1000, DEVICE_ID_RADIO, MICROBIT_DROPLET_EXPIRATION_EVENT);
    uBit.messageBus.listen(DEVICE_ID_RADIO, MICROBIT_DROPLET_EXPIRATION_EVENT, onExpirationCounterEvent);
    uBit.messageBus.listen(DEVICE_ID_RADIO, MICROBIT_DROPLET_SCHEDULE_EVENT, onNextSlotEvent);
    //uBit.messageBus.listen(DEVICE_ID_RADIO, MICROBIT_DROPLET_WAKE_UP_EVENT, onRadioWakeUpEvent);
}

uint32_t DropletScheduler::analysePacket(DropletFrameBuffer *buffer)
{
    std::ostringstream ss;
    ss << buffer->deviceId;
    DMESG("Analysing packet from %s", ss.str().c_str());
    // We have mismatched slot ids, assume that the networked one is correct
    // This way, for the next slot the entire network should be synchronised to the same slot
    if (buffer->slotId != currentSlot)
    {
        DMESG("Mismatch slot id - buf: %d, cur: %d", buffer->slotId, currentSlot);
        currentSlot = buffer->slotId;
    }

    DropletSlot *slot = &slots[buffer->slotId];

    // Invalid packet
    // TODO: If we haven't synced to the network, this will always return
    if (slot->deviceId != buffer->deviceId && Droplet::instance->getDropletStatus() == DropletStatus::Synchronised)
    {
        slot->errors++;
        DMESG("Invalid device id");
        return MICROBIT_INVALID_PARAMETER;
    }

    // Duplicate packet
    // Every slot the frames (should) be wiped
    // This means that every frame should be nullptr
    // If the frame is not nullptr, we have received a duplicate packet
    if (frames[buffer->frameId] != nullptr)
    {
        DMESG("Duplicate packet - frame id: ", buffer->frameId);
        return MICROBIT_DROPLET_DUPLICATE_PACKET;
    }

    // We have received an advertisement packet
    // Add it to our local schedule
    // TODO: instead of waiting for the last packet to be received to calculate the time for the next slot
    // TODO: we should calculate it on the first slot
    // TODO: this way if any packet loss occurs
    // TODO: it will still work
    // TODO: and it allows for the time to be calculated off the advert packets

    if (isFirstPacket)
    {   

        DropletNetworkClock::instance->setNetworkTime(buffer->startTime);
        isFirstPacket = false;
        isLastPacket = false;

        // uint8_t slotsToSleepFor = getSlotsToSleepFor();

        // timeToNextSlot + slotsToSleepFor * MICROBIT_DROPLET_SLOT_DURATION - preparationWindow
        // TODO: Only update the time if this is the first packet
        // TODO: getNetworkTime() should actually be the local time (synced to the network)
        //uint32_t t = (DropletNetworkClock::instance->getPredictedNetworkTime() + (slotsToSleepFor + 1) * MICROBIT_DROPLET_SLOT_DURATION_MILLISECONDS) /** 1000 */- MICROBIT_DROPLET_PREPARATION_WINDOW_MICROSECONDS;
        //timer.eventAfterUs(t, DEVICE_ID_RADIO, MICROBIT_DROPLET_WAKE_UP_EVENT); 
        uint32_t t = DropletNetworkClock::instance->getPredictedNetworkTime() + MICROBIT_DROPLET_SLOT_DURATION_MILLISECONDS - MICROBIT_DROPLET_PREPARATION_WINDOW_MICROSECONDS;
        DMESG("Time until next slot: %d", t);
        // TODO: disable the event first?
        system_timer_event_every_us(t, DEVICE_ID_RADIO, MICROBIT_DROPLET_SCHEDULE_EVENT);

        // TODO: currentSlot is also modified in nextSlotEvent
        // TODO: could cause issues
        // currentSlot = (currentSlot + slotsToSleepFor) % MICROBIT_DROPLET_SLOTS;
    }

    if ((buffer->flags & MICROBIT_DROPLET_ADVERT) == MICROBIT_DROPLET_ADVERT)
    {
        DMESG("Advert packet received in scheduler - slot id: %d", buffer->slotId);
        setSlot(buffer->slotId, buffer->deviceId);
        return MICROBIT_OK;
    }

    frames[buffer->frameId] = buffer;

    slot->expiration = MICROBIT_DROPLET_EXPIRATION;

    uint8_t frameId = buffer->frameId;

    // On receive, reset the expiration counter
    // Shouldn't actually need to use the KEEP_ALIVE flag
    // But it will be kept just in case
    slot->expiration = MICROBIT_DROPLET_EXPIRATION;

    
    // TODO: Once we get here, ignore all packets from device, even if it is its slot
    // bool isLastPacket?
    // For optimisation purposes, could be worth using uint8_t and bit packing isFirstPacket and isLastPacket
    // Set isFirstPacket to true on next slot event

    // TODO: distance
    // The distance field could allow for the packets ttl to be more efficiently set
    // However, James' doesn't make it clear how this distance field should actually be used

    if ((buffer->flags & MICROBIT_DROPLET_DONE) == MICROBIT_DROPLET_DONE)
    {
        // We have received the last packet, so shut down the radio
        // TODO: Only do this if the next slot is empty
        
        
        maxFrameId = frameId; 
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
    DMESG("mark slot - slot id: %d", id);
    slots[id].flags &= ~MICROBIT_DROPLET_FREE; 
}

uint32_t codal::DropletScheduler::getFirstFreeSlot(uint8_t &slotId)
{
    for (DropletSlot slot : slots)
    {
        if ((slot.flags & MICROBIT_DROPLET_FREE) == MICROBIT_DROPLET_FREE && (slot.flags & MICROBIT_DROPLET_ADVERT) != MICROBIT_DROPLET_ADVERT)
        {
            slotId = slot.slotId;
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

    DMESG("Queueing advertisement");

    int num = microbit_random(4) + 1;
    advertGoal = advertCounter + num;

    sendAdvertPacket = true;
}

void codal::DropletScheduler::deleteFrames() 
{   
    for (int i = 0; i < MICROBIT_DROPLET_MAX_FRAMES; i++)
    {
        frames[i] = nullptr;
    }
}

uint16_t codal::DropletScheduler::sendAdvertisement(DropletSlot slot) 
{
    if ((slot.flags & MICROBIT_DROPLET_ADVERT) == MICROBIT_DROPLET_ADVERT)
    {
        advertCounter++;

        DMESG("Advert counter: %d Advert goal: %d", advertCounter, advertGoal);

        if (advertCounter >= advertGoal && sendAdvertPacket)
        {
            DMESG("Sending advertisement...");
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
                advert->slotId = slot;
                advert->deviceId = deviceId;
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
                DMESG("No free slot");
                return MICROBIT_DROPLET_NO_SLOTS;
            }
        }
    }

    return MICROBIT_DROPLET_ERROR;
}

uint16_t codal::DropletScheduler::setSlot(uint8_t slotId, uint64_t deviceId) 
{
    if (slotId > MICROBIT_DROPLET_SLOTS)
        return MICROBIT_DROPLET_ERROR;

    // We are trying to modify an advert slot
    // That isn't allowed
    if ((slots[slotId].flags & MICROBIT_DROPLET_ADVERT) == MICROBIT_DROPLET_ADVERT)
    {
        return MICROBIT_DROPLET_ERROR;
    }

    // If the slot is already taken, return error
    if ((slots[slotId].flags & MICROBIT_DROPLET_FREE) != MICROBIT_DROPLET_FREE)
    {
        return MICROBIT_DROPLET_ERROR;
    }

    DMESG("setSlot slotId: %d", slotId);

    slots[slotId].slotId = slotId;
    slots[slotId].deviceId = deviceId;
    slots[slotId].expiration = MICROBIT_DROPLET_EXPIRATION;
    slots[slotId].errors = 0;
    markSlotAsTaken(slotId);
    return MICROBIT_OK;
}

uint16_t codal::DropletScheduler::nextSlotStatus()
{
    DropletSlot *slot = &slots[(currentSlot + 1) % MICROBIT_DROPLET_SLOTS];

    if (slot->deviceId == uBit.getSerialNumber())
    {
        return MICROBIT_DROPLET_OWNER;
    }
    else if ((slot->flags & MICROBIT_DROPLET_FREE) == MICROBIT_DROPLET_FREE)
    {
        return MICROBIT_DROPLET_FREE;
    }

    return MICROBIT_DROPLET_PARTICIPANT;
}

uint16_t codal::DropletScheduler::getCurrentSlotStatus()
{
    DropletSlot *slot = &slots[currentSlot];

    if (slot->deviceId == uBit.getSerialNumber())
    {
        return MICROBIT_DROPLET_OWNER;
    }
    else if ((slot->flags & MICROBIT_DROPLET_FREE) == MICROBIT_DROPLET_FREE)
    {
        return MICROBIT_DROPLET_FREE;
    }

    return MICROBIT_DROPLET_PARTICIPANT;
}

void codal::DropletScheduler::setIsFirstPacketToTrue() 
{
    isFirstPacket = true;
}

bool DropletScheduler::isSlotMine(uint8_t id)
{
    return slots[id].deviceId == uBit.getSerialNumber();
}

bool DropletScheduler::isNextSlotMine()
{
    return slots[(currentSlot + 1) % MICROBIT_DROPLET_SLOTS].deviceId == uBit.getSerialNumber();
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
