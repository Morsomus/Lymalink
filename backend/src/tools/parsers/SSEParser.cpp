/////////////////////////////////////////////////////////
// File: SSEParser.cpp
// Date: 2026-08-01
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements parser for SmartSteamEmu binary
//              achievement files
//
// Format:
// [4 bytes] Entry count, int32 little-endian
// [24 bytes x count]
//   [0-3]   CRC32 of achievement API name or metadata, reversed on disk
//   [4-7]   Reserved
//   [8-11]  Unlock time, int32 little-endian Unix timestamp
//   [12-19] Reserved
//   [20-23] Value, int32 little-endian
//           0 = locked, 1 = unlocked, >1 = stat/progress
/////////////////////////////////////////////////////////

#include "SSEParser.h"
#include "../Logger.h"
#include "../Utils.h"
#include "Defines.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

#define COMPONENT "SSEParser"

/////////////////////////////////////////////////////////////////////

SSEParser::SSEParser()
{
    // Constructor
}

SSEParser::~SSEParser()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

std::string SSEParser::GetFileName() const
{
    return "stats.bin";
}

/////////////////////////////////////////////////////////////////////

std::vector<AchievementData> SSEParser::Parse(const std::string& filePath)
{
    std::vector<AchievementData> results;

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        return results;
    }

    const std::vector<uint8_t> bytes = std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    constexpr size_t headerSize = 4;
    constexpr size_t entrySize = 24;
    if (bytes.size() < headerSize)
    {
        LOG_BE(Urgency::Warning, "SmartSteamEmu stats file too small: path=%s size=%zu", filePath.c_str(), bytes.size());
        return results;
    }

    auto ReadUint32Le = [&bytes](size_t offset) -> uint32_t
    {
        return static_cast<uint32_t>(bytes[offset]) |
            (static_cast<uint32_t>(bytes[offset + 1]) << 8U) |
            (static_cast<uint32_t>(bytes[offset + 2]) << 16U) |
            (static_cast<uint32_t>(bytes[offset + 3]) << 24U);
    };

    const uint32_t entryCount = ReadUint32Le(0);
    const uint64_t expectedSize = static_cast<uint64_t>(headerSize) + static_cast<uint64_t>(entryCount) * entrySize;
    const uint64_t maxEntryCount = (static_cast<uint64_t>(std::numeric_limits<size_t>::max()) - headerSize) / entrySize;
    if (entryCount > maxEntryCount || expectedSize > bytes.size())
    {
        LOG_BE(Urgency::Warning, "SmartSteamEmu stats file truncated or invalid: path=%s size=%zu count=%u expectedSize=%llu", filePath.c_str(), bytes.size(), entryCount, static_cast<unsigned long long>(expectedSize));
        return results;
    }

    for (uint32_t index = 0; index < entryCount; ++index)
    {
        const size_t offset = headerSize + static_cast<size_t>(index) * entrySize;
        const uint32_t crc = ReadUint32Le(offset);
        const int32_t unlockTime = static_cast<int32_t>(ReadUint32Le(offset + 8));
        const int32_t value = static_cast<int32_t>(ReadUint32Le(offset + 20));
        const std::string crcKey = "crc32:" + Utils::ToUpperHexUint32(crc);

        // Ignore progress entries for now - There seems to be no way to link progress entries to final unlock entries
        if (value > 1)
        {
            continue;
        }

        AchievementData data{};
        data.key = crcKey;
        data.achieved = value == 1;
        data.unlockTime = static_cast<int64_t>(unlockTime);
        results.push_back(data);
    }

    return results;
}
