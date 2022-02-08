/*
The MIT License (MIT)

Copyright (c) 2016 British Broadcasting Corporation.
    This software is provided by Lancaster University by arrangement with the BBC.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
                                          Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
        DEALINGS IN THE SOFTWARE.
            */

#include "Droplet.h"
#include "MicroBit.h"
#include <vector>

extern MicroBit uBit;

using namespace codal;

/**
  * Provides a simple broadcast radio abstraction, built upon the raw nrf51822 RADIO module.
  *
  * This class provides the ability to broadcast simple text or binary messages to other micro:bits in the vicinity
  * It is envisaged that this would provide the basis for children to experiment with building their own, simple,
  * custom protocols.
  *
  * @note This API does not contain any form of encryption, authentication or authorisation. Its purpose is solely for use as a
  * teaching aid to demonstrate how simple communications operates, and to provide a sandpit through which learning can take place.
  * For serious applications, BLE should be considered a substantially more secure alternative.
 */

/**
* Constructor.
*
* Creates an instance of a MicroBitRadioDatagram which offers the ability
* to broadcast simple text or binary messages to other micro:bits in the vicinity
*
* @param r The underlying radio module used to send and receive data.
 */
DropletDatagram::DropletDatagram(Droplet &r) : radio(r)
{
    this->rxQueue = NULL;
}

/**
  * Retrieves packet payload data into the given buffer.
  *
  * If a data packet is already available, then it will be returned immediately to the caller.
  * If no data is available then DEVICE_INVALID_PARAMETER is returned.
  *
  * @param buf A pointer to a valid memory location where the received data is to be stored
  *
  * @param len The maximum amount of data that can safely be stored in 'buf'
  *
  * @return The length of the data stored, or DEVICE_INVALID_PARAMETER if no data is available, or the memory regions provided are invalid.
 */
int DropletDatagram::recv(uint8_t *buf, int len)
{
    if (buf == NULL || rxQueue == NULL || len < 0)
        return DEVICE_INVALID_PARAMETER;

    // Take the first buffer from the queue.
    DropletFrameBuffer *p = rxQueue;
    rxQueue = rxQueue->next;

    int l = min(len, p->length - (MICROBIT_DROPLET_HEADER_SIZE - 1));

    // Fill in the buffer provided, if possible.
    memcpy(buf, p->payload, l);

    delete p;
    return l;
}

/**
  * Retreives packet payload data into the given buffer.
  *
  * If a data packet is already available, then it will be returned immediately to the caller
  * in the form of a PacketBuffer.
  *
  * @return the data received, or an empty PacketBuffer if no data is available.
 */
PacketBuffer DropletDatagram::recv()
{
    if (rxQueue == NULL)
        return PacketBuffer::EmptyPacket;

    DropletFrameBuffer *p = rxQueue;
    rxQueue = rxQueue->next;

    PacketBuffer packet(p->payload, p->length - (MICROBIT_DROPLET_HEADER_SIZE - 1), p->rssi);

    delete p;
    return packet;
}

#include <iostream>
#include <sstream>

/**
  * Transmits the given buffer onto the broadcast radio.
  *
  * This is a synchronous call that will wait until the transmission of the packet
  * has completed before returning.
  *
  * @param buffer The packet contents to transmit.
  *
  * @param len The number of bytes to transmit.
  *
  * @return DEVICE_OK on success, or DEVICE_INVALID_PARAMETER if the buffer is invalid,
  *         or the number of bytes to transmit is greater than `MICROBIT_RADIO_MAX_PACKET_SIZE + MICROBIT_DROPLET_HEADER_SIZE`.
 */
int DropletDatagram::send(uint8_t *buffer, int len)
{
    if (buffer == NULL || len < 0 || len > MICROBIT_DROPLET_MAX_PACKET_SIZE + MICROBIT_DROPLET_HEADER_SIZE - 1)
        return DEVICE_INVALID_PARAMETER;

    DropletFrameBuffer buf;

    uint32_t t  = uBit.systemTime();

    buf.length = len + MICROBIT_DROPLET_HEADER_SIZE - 1;
    buf.deviceIdentifier = uBit.getSerialNumber();
    buf.startTime = t;

    std::ostringstream ss;
    ss << t;
    DMESG("Send time: %s", ss.str().c_str());

    // buf.version = 1;
    // buf.group = 0;
    buf.protocol = MICROBIT_RADIO_PROTOCOL_DATAGRAM;
    memcpy(buf.payload, buffer, len);

    return radio.send(&buf);
}

/**
  * Transmits the given string onto the broadcast radio.
  *
  * This is a synchronous call that will wait until the transmission of the packet
  * has completed before returning.
  *
  * @param data The packet contents to transmit.
  *
  * @return DEVICE_OK on success, or DEVICE_INVALID_PARAMETER if the buffer is invalid,
  *         or the number of bytes to transmit is greater than `MICROBIT_RADIO_MAX_PACKET_SIZE + MICROBIT_DROPLET_HEADER_SIZE`.
 */
int DropletDatagram::send(PacketBuffer data)
{
    return send((uint8_t *)data.getBytes(), data.length());
}

/**
  * Transmits the given string onto the broadcast radio.
  *
  * This is a synchronous call that will wait until the transmission of the packet
  * has completed before returning.
  *
  * @param data The packet contents to transmit.
  *
  * @return DEVICE_OK on success, or DEVICE_INVALID_PARAMETER if the buffer is invalid,
  *         or the number of bytes to transmit is greater than `MICROBIT_RADIO_MAX_PACKET_SIZE + MICROBIT_DROPLET_HEADER_SIZE`.
 */
int DropletDatagram::send(ManagedString data)
{
    return send((uint8_t *)data.toCharArray(), data.length());
}

void DropletDatagram::networkDiscovery(DropletFrameBuffer *packet)
{
    uint8_t slotId = packet->slotIdentifier;

    if (Droplet::instance->getDropletStatus() == DropletStatus::Initialisation)
    {
        DMESG("Initialisation complete");
        Droplet::instance->setDropletStatus(DropletStatus::Discovery);
        Droplet::instance->setInitialSlotId(slotId);
        DropletNetworkClock::instance->init(packet->startTime);
        DMESG("Entered discovery mode");
        // TODO: Synchronise to the network clock
        //uint8_t hops = packet->initialTtl - packet->ttl;
        //uint32_t transmission = (packet->length);
    }
    else if (Droplet::instance->getDropletStatus() == DropletStatus::Discovery && packet->deviceIdentifier != uBit.getSerialNumber() && (packet->flags & MICROBIT_DROPLET_ADVERT) == 0)
    {
        // TODO: Potential bug - if a slot if dropped in the middle it could cause this to never complete, must check!
        if (Droplet::instance->getLastSlotId() <= slotId && slotId >= Droplet::instance->getInitialSlotId())
        {
            DMESG("Discovery mode complete - synchronised");
            Droplet::instance->setDropletStatus(DropletStatus::Synchronised);
        }

        Droplet::instance->setLastSlotId(slotId);
    }
}

/**
  * Protocol handler callback. This is called when the radio receives a packet marked as a datagram.
  *
  * This function process this packet, and queues it for user reception.
 */
void DropletDatagram::packetReceived()
{
    DMESG("Packet received");
    DropletFrameBuffer *packet = radio.recv();
    int queueDepth = 0;

    // We add to the tail of the queue to preserve causal ordering.
    packet->next = NULL;
    // packet->ttl--;

    networkDiscovery(packet);

    if (Droplet::instance->getDropletStatus() != DropletStatus::Initialisation)
    {
        // DropletNetworkClock::instance->updateTime(packet->startTime);
    }

    if (rxQueue == NULL)
    {
        rxQueue = packet;
    }
    else
    {
        DropletFrameBuffer *p = rxQueue;
        while (p->next != NULL)
        {
            p = p->next;
            queueDepth++;
        }

        if (queueDepth >= MICROBIT_RADIO_MAXIMUM_RX_BUFFERS)
        {
            delete packet;
            return;
        }

        p->next = packet;
    }

    //if (packet->ttl > 0)
        //radio.send(packet);
    
    Event(DEVICE_ID_RADIO, MICROBIT_RADIO_EVT_DATAGRAM);
}
