/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#pragma once

#include "SubtecPacket.hpp"

class WebVttSelectionPacket : public Packet
{
public:
    /**
     * Constructor.
     *
     * @param channelId
     *      Packet channelId.
     * @param counter
     *      Packet counter.
     * @param width
     *      Display width.
     * @param height
     *      Display height.
     */
    WebVttSelectionPacket(uint32_t channelId, uint32_t counter, uint32_t width, uint32_t height)
    {
        appendType(Packet::PacketType::WEBVTT_SELECTION);
        append32(counter);
        append32(WEBVTT_SELECTION_PACKET_SIZE);
        append32(channelId);
        append32(width);
        append32(height);
    }
    
private:
    static constexpr std::uint8_t WEBVTT_SELECTION_PACKET_SIZE = 12;
};

class WebVttDataPacket : public Packet
{
public:

    /**
     * Constructor.
     *
     * @param channelId
     *      Packet channelId.
     * @param counter
     *      Packet counter.
     * @param dataOffset
     *      Data offset if needed
     * @param dataBuffer
     *      Packet data.
     */
    WebVttDataPacket(std::uint32_t channelId,
                   std::uint32_t counter,
                   std::int64_t dataOffset,
                   std::vector<std::uint8_t>& dataBuffer)
    {
        auto& buffer = getBuffer();
        uint32_t size = 8 + 4 + dataBuffer.size();
        
        appendType(PacketType::WEBVTT_DATA);
        append32(counter);
        append32(size);
        append32(channelId);
        append64(dataOffset);

        for (auto &byte : dataBuffer)
            buffer.push_back(byte);
    }
};


class WebVttTimestampPacket : public Packet
{
public:

    /**
     * Constructor.
     *
     * @param counter
     *      Packet counter.
     */
    WebVttTimestampPacket(std::uint32_t channelId,
                    std::uint32_t counter,
                    std::uint64_t timestamp)
    {
        appendType(PacketType::WEBVTT_TIMESTAMP);
        append32(counter);
        append32(WEBVTT_TIMESTAMP_PACKET_SIZE);
        append32(channelId);
        append64(timestamp);
    }

private:

    static constexpr std::uint8_t WEBVTT_TIMESTAMP_PACKET_SIZE = 12;
};


class WebVttChannel : public SubtecChannel
{
public:
    WebVttChannel() : SubtecChannel() {}
    
    PacketPtr generateSelectionPacket(uint32_t width, uint32_t height) 
    { 
        return make_unique<WebVttSelectionPacket>(m_channelId, m_counter++, width, height); 
    }
    
    PacketPtr generateDataPacket(std::vector<uint8_t> data)
    { 
        return make_unique<WebVttDataPacket>(m_channelId, m_counter++, 0, data); 
    }
   
    PacketPtr generateTimestampPacket(uint64_t timestampMs)
    { 
        return make_unique<WebVttTimestampPacket>(m_channelId, m_counter++, timestampMs); 
    }
};