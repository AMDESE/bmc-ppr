/*
 // Copyright (c) 2023 AMD Inc.
 //
 // Licensed under the Apache License, Version 2.0 (the "License");
 // you may not use this file except in compliance with the License.
 // You may obtain a copy of the License at
 //
 //      http://www.apache.org/licenses/LICENSE-2.0
 //
 // Unless required by applicable law or agreed to in writing, software
 // distributed under the License is distributed on an "AS IS" BASIS,
 // WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 // See the License for the specific language governing permissions and
 // limitations under the License.
 */
#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <sys/inotify.h>
#include <systemd/sd-journal.h>
#include <unistd.h>

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

extern "C" {
#include <sys/stat.h>
#include "esmi_mailbox.h"
}

namespace fs = std::filesystem;

// Directory created on first boot

constexpr std::string_view kPprDir = "/var/lib/amd-ppr/";

// PPR mailbox constants

constexpr int MAX_RETRIES              = 10;
constexpr int RAS_ACTION_ID_RUNTIME_PPR = 0;
constexpr int PAYLOAD_SIZE             = 10;

// PPR repair status

enum PPR_STATUS
{
    PPR_STATUS_REPAIR_FAIL          = 0x00,
    PPR_STATUS_REPAIR_PASS          = 0x01,
    PPR_STATUS_REPAIR_NOT_PROCESSED = 256,
};

// RT PPR entry

struct RtPprEntry
{
    uint32_t repairEntryNum{0};
    uint32_t repairType{0};
    uint32_t socNum{0};
    uint16_t payload[PAYLOAD_SIZE]{};

    uint16_t repairResult{static_cast<uint16_t>(PPR_STATUS_REPAIR_NOT_PROCESSED)};
};

/**
 * RtPprManager
 * Watches RAS_WATCH_DIR for *_rtppr.json files written by amd-bmc-ras.
 */
class RtPprManager
{
  public:
    explicit RtPprManager(const std::string& watchDir,
                          const std::string& configFile);

    void run();

  private:
    std::string m_watchDir;
    int         m_maxRetries{MAX_RETRIES};
    int         m_retryDelayUs{200000};        // 200 ms
    int         m_statusPollTimeoutMs{5000};

    bool loadConfig(const std::string& configFile);
    void scanExistingFiles();
    void processFile(const fs::path& rtpprPath);
    bool parseRtPprJson(const fs::path& path,
                        std::vector<RtPprEntry>& entries);
    bool sendRepairData(std::vector<RtPprEntry>& entries);
    bool pollRepairStatus(std::vector<RtPprEntry>& entries);
    void writeStatusFile(const fs::path& rtpprPath,
                         const std::vector<RtPprEntry>& entries);
};
