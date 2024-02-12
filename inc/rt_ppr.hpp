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
#include <config.h>

#include <iostream>
#include <tuple> // for tuple
#include <vector>

// Library effective with Linux
#include <ctype.h>
#include <systemd/sd-journal.h>
#include <unistd.h>

#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>
#include <experimental/filesystem>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <regex>
#include <string_view>
#include <utility>
#include <xyz/openbmc_project/Collection/DeleteAll/server.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/PostPackageRepair/PprData/server.hpp>

extern "C" {
#include <sys/stat.h>

#include "apml.h"
#include "esmi_cpuid_msr.h"
#include "esmi_mailbox.h"
#include "esmi_mailbox_nda.h"
#include "esmi_rmi.h"
#include "i2c/smbus.h"
#include "linux/i2c-dev.h"
}

#define PPR_DIR "/var/lib/amd-ppr/"
#define BTJSON_FILE "BtPPRData.json"
#define BTJsonFilePath "/var/lib/amd-ppr/BtPPRData.json"
#define RTJSON_FILE "RtPPRData.json"
#define RTJsonFilePath "/var/lib/amd-ppr/RtPPRData.json"
#define PPR_NODE "PprData"
#define RUNTIME "RunTime"
#define BOOTTIME "BootTime"

extern void* base_addr;

using namespace std;
namespace fs = std::filesystem;
constexpr std::string_view kPprDir = "/var/lib/amd-ppr/";

const int JSON_SLEEP_TIME = 1;
const int MAX_RETRIES = 10;
const int RAS_ACTION_ID_RUNTIME_PPR = 0;
const int PAYLOAD_SIZE = 10;
const int PAYLOAD_6 = 6;
const int MAX_REPAIR_SLOTS = 64;
const int MAX_CURRENT_Runtime_PPR = 8;
const int MAX_DIMM_SLOT = 24;
const int DIMM_SOCKET_PL_NUM = 3;
const int DIMM_SOCKET_MASK = 0xE000;
const int DIMM_SOCKET_SHIFT = 13;
const int DIMM_SOCKET_0 = 0;
const int DIMM_CH_PL_NUM = 3;
const int DIMM_CH_MASK = 0x01E0;
const int DIMM_CH_SHIFT = 5;
const int DIMM_CHIP_PL_NUM = 0;
const int DIMM_CHIP_MASK = 0x6000;
const int DIMM_CHIP_SHIFT = 13;
const int DIMM_CHIP_2DPC = 2;
const int DIMM_SN0_MASK = 0x0000FFFF;
const int DIMM_SN1_MASK = 0xFFFF0000;
const int DIMM_SN1_SHIFT = 16;
const int DIMM_SN_CH_MASK = 0x07;
const int DIMM_SN_CHIP_MASK = 0x03;
const int DIMM_SN_CHIP_SHIFT = 4;

// PPR Service
const static constexpr char* pprDataInPath =
    "/xyz/openbmc_project/PostPackageRepair/PprData";
const static constexpr char* PropertiesIntf = "org.freedesktop.DBus.Properties";

const int PPR_TYPE_RUNTIME_MASK = 0x00;
const int PPR_TYPE_BOOTTIME_MASK = 0x8000;
const int PPR_TYPE_SOFT_MASK = 0x00;
const int PPR_TYPE_HARD_MASK = 0x01;
const int PPR_TYPE_MBIST_MASK = 0x03;

enum REPAIRTYPE
{
    PPR_TYPE_RUNTIME_SOFT = (PPR_TYPE_RUNTIME_MASK | PPR_TYPE_SOFT_MASK),
    PPR_TYPE_RUNTIME_HARD = (PPR_TYPE_RUNTIME_MASK | PPR_TYPE_HARD_MASK),
    PPR_TYPE_RUNTIME_MBIST = (PPR_TYPE_RUNTIME_MASK | PPR_TYPE_MBIST_MASK),
    PPR_TYPE_BOOTTIME_SOFT = (PPR_TYPE_BOOTTIME_MASK | PPR_TYPE_SOFT_MASK),
    PPR_TYPE_BOOTTIME_HARD = (PPR_TYPE_BOOTTIME_MASK | PPR_TYPE_HARD_MASK),
    PPR_TYPE_BOOTTIME_MBIST = (PPR_TYPE_BOOTTIME_MASK | PPR_TYPE_MBIST_MASK),
};

// cmd 0x67 Get PPR RAS Action Status
// Repair Status
enum PPR_STATUS
{
    PPR_STATUS_REPAIR_FAIL = 0x0,
    PPR_STATUS_REPAIR_PASS = 0x1,
    PPR_STATUS_REPAIR_RANK_MISS_MATCH_ERROR = 0x2,
    PPR_STATUS_REPAIR_RANK_INVALID_ERROR = 0x4,
    PPR_STATUS_REPAIR_BANK_INVALID_ERROR = 0x08,
    PPR_STATUS_REPAIR_SOCKET_INVALID_ERROR = 0x10,
    PPR_STATUS_REPAIR_CHANNEL_INVALID_ERROR = 0x20,
    PPR_STATUS_REPAIR_DEVICE_INVALID_ERROR = 0x40,
    PPR_STATUS_REPAIR_DEVICE_MISMATCH_ERROR = 0x80,
    PPR_STATUS_REPAIR_NOT_PROCESSED = 256,
};

struct PPR_Data
{
    uint16_t repairEntryNum;
    uint16_t repairType;
    uint16_t socNum;
    uint16_t repairResult;
    uint16_t payload[PAYLOAD_SIZE];
};

struct PprJsonData
{
    int index;
    std::string pprType;
    uint16_t repairEntryNum;
    uint16_t repairType;
    uint16_t socNum;
    uint16_t repairResult;
    std::vector<uint16_t> payload;
};

template <typename Archive>
void serialize(Archive& archive, PprJsonData& jsonData)
{
    archive(cereal::make_nvp("index", jsonData.index),
            cereal::make_nvp("pprType", jsonData.pprType),
            cereal::make_nvp("repairEntryNum", jsonData.repairEntryNum),
            cereal::make_nvp("repairType", jsonData.repairType),
            cereal::make_nvp("socNum", jsonData.socNum),
            cereal::make_nvp("repairResult", jsonData.repairResult),
            cereal::make_nvp("payload", jsonData.payload));
}

struct EventDeleter
{
    void operator()(sd_event* event) const
    {
        event = sd_event_unref(event);
    }
};

using EventPtr = std::unique_ptr<sd_event, EventDeleter>;

using ppr_data =
    sdbusplus::xyz::openbmc_project::PostPackageRepair::server::PprData;
using delete_all =
    sdbusplus::xyz::openbmc_project::Collection::server::DeleteAll;

class commonPPR
{
    static commonPPR* instance;
    commonPPR()
    {
    }

  public:
    static commonPPR* getInstance()
    {
        if (!instance)
            instance = new commonPPR;
        return instance;
    }

    uint16_t getBTindex()
    {
        return m_pprBoottimeIndex;
    }

    void setBTindex(uint16_t index)
    {
        m_pprBoottimeIndex = index;
    }

    std::tuple<uint16_t, uint16_t, uint16_t, uint16_t, std::vector<uint16_t>>
        getBTdata(uint16_t index)
    {
        std::tuple<uint16_t, uint16_t, uint16_t, uint16_t,
                   std::vector<uint16_t>>
            tup;

        if (index < MAX_REPAIR_SLOTS)
        {
            std::vector<uint16_t> vec;
            for (int i = 0; i < PAYLOAD_SIZE; i++)
                vec.push_back(m_pprBoottimeData[index].payload[i]);
            tup = std::make_tuple(m_pprBoottimeData[index].repairEntryNum,
                                  m_pprBoottimeData[index].repairType,
                                  m_pprBoottimeData[index].socNum,
                                  m_pprBoottimeData[index].repairResult, vec);
        }
        return tup;
    }

    void BtJsonRead()
    {
        char filepath[] = BTJsonFilePath;
        if (fs::exists(filepath))
        {
            try
            {
                std::ifstream input(PPR_DIR BTJSON_FILE);
                cereal::JSONInputArchive archive(input);
                archive(BtPprJsonData);
            }
            catch (cereal::Exception& e)
            {
                sd_journal_print(
                    LOG_INFO,
                    "BtJsonRead: Error reading BT PPR json file %s \n",
                    e.what());
            }
        }
    }

    void updateBTfromCache()
    {
        uint16_t result, repairEntryNum, repairType, socNum;
        std::vector<uint16_t> payload;

        if (BtPprJsonData.size() > 0)
        {
            // check if index exist
            for (int vecIndex = 0; vecIndex < (int)BtPprJsonData.size();
                 vecIndex++)
            {
                repairType = BtPprJsonData.at(vecIndex).repairType;
                if ((repairType & PPR_TYPE_BOOTTIME_MASK) != 0)
                {
                    repairEntryNum = BtPprJsonData.at(vecIndex).repairEntryNum;
                    repairType = BtPprJsonData.at(vecIndex).repairType;
                    socNum = BtPprJsonData.at(vecIndex).socNum;
                    payload = BtPprJsonData.at(vecIndex).payload;
                    result = BtPprJsonData.at(vecIndex).repairResult;

                    setBTdata(false, BOOTTIME, repairEntryNum, repairType,
                              socNum, result, payload);
                }
            }
        }
        sd_journal_print(LOG_INFO, "update PPR BT: Index = %d ", getBTindex());
    }

    void setBTdata(bool writeJson, std::string pprType, uint16_t repairEntryNum,
                   uint16_t repairType, uint16_t socNum, uint16_t result,
                   std::vector<uint16_t> payload)
    {
        m_pprBoottimeData[m_pprBoottimeIndex].repairEntryNum = repairEntryNum;
        m_pprBoottimeData[m_pprBoottimeIndex].repairType = repairType;
        m_pprBoottimeData[m_pprBoottimeIndex].socNum = socNum;
        m_pprBoottimeData[m_pprBoottimeIndex].repairResult = result;
        for (int j = 0; j < PAYLOAD_SIZE; j++)
            m_pprBoottimeData[m_pprBoottimeIndex].payload[j] = payload[j];

        if (writeJson)
        {
            try
            {
                if (BtPprJsonData.size() > 0)
                {
                    // check if index exist
                    for (int vecIndex = 0; vecIndex < (int)BtPprJsonData.size();
                         vecIndex++)
                    {
                        if (BtPprJsonData.at(vecIndex).index ==
                            m_pprBoottimeIndex)
                        {
                            sd_journal_print(
                                LOG_INFO,
                                "setBTdata: PPR Data already exist \n");
                            return;
                        }
                    }
                }

                PprJsonData pprObj;
                pprObj.index = m_pprBoottimeIndex;
                pprObj.pprType = pprType;
                pprObj.repairEntryNum = repairEntryNum;
                pprObj.repairType = repairType;
                pprObj.socNum = socNum;
                pprObj.repairResult = result;
                pprObj.payload = payload;
                // add ppr data in vector
                BtPprJsonData.push_back(pprObj);
                // create json file
                char filepath[] = BTJsonFilePath;
                if (fs::exists(filepath))
                {
                    std::remove(filepath);
                    sleep(JSON_SLEEP_TIME);
                }
                std::ofstream output(PPR_DIR BTJSON_FILE);
                cereal::JSONOutputArchive oarchive(output);
                oarchive(cereal::make_nvp(PPR_NODE, BtPprJsonData));
            }
            catch (cereal::Exception& e)
            {
                sd_journal_print(LOG_INFO,
                                 "setBTdata: Failed to write json file  %s \n",
                                 e.what());
            }
        }

        m_pprBoottimeIndex++;
    }

    void UpdateBtResult(uint16_t index, uint16_t result)
    {
        bool update = false;
        try
        {
            m_pprBoottimeData[index].repairResult = result;
            if (BtPprJsonData.size() > 0)
            {
                // check if index exist
                for (int vecIndex = 0; vecIndex < (int)BtPprJsonData.size();
                     vecIndex++)
                {
                    if (BtPprJsonData.at(vecIndex).index == index)
                    {
                        if (BtPprJsonData.at(vecIndex).repairResult != result)
                        {
                            BtPprJsonData.at(vecIndex).repairResult = result;
                            update = true;
                        }
                        break;
                    }
                }
            }

            if (update)
            { // create json file
                char filepath[] = BTJsonFilePath;
                if (fs::exists(filepath))
                {
                    std::remove(filepath);
                    sleep(JSON_SLEEP_TIME);
                }
                std::ofstream output(PPR_DIR BTJSON_FILE);
                cereal::JSONOutputArchive oarchive(output);
                oarchive(cereal::make_nvp(PPR_NODE, BtPprJsonData));
            }
        }
        catch (cereal::Exception& e)
        {
            sd_journal_print(LOG_ERR,
                             "UpdateBtResult: Failed to write json file  %s \n",
                             e.what());
        }
    }

  private:
    uint16_t m_pprBoottimeIndex;
    std::array<PPR_Data, MAX_REPAIR_SLOTS> m_pprBoottimeData;
    vector<PprJsonData> BtPprJsonData;
};

// PPR Class
struct childPprData : sdbusplus::server::object_t<ppr_data, delete_all>
{

    commonPPR* globalBT = globalBT->getInstance();

    childPprData(sdbusplus::bus::bus& bus, const char* path, EventPtr& event) :
        sdbusplus::server::object_t<ppr_data, delete_all>(bus, path), bus(bus),
        event(event)
    {

        m_pprRuntimeIndex = 0;
        m_currentRuntimeIndex = 0;
        m_currentRuntimeCnt = 0;
        globalBT->setBTindex(0);
        globalBT->BtJsonRead();
        globalBT->updateBTfromCache();
        sd_journal_print(LOG_INFO, "PPR Data Constructor - Done \n");
    }

    ~childPprData()
    {
    }

    void deleteAll() override;
    // Set values of repair data
    bool setPostPackageRepairData(uint16_t repairEntryNum, uint16_t repairType,
                                  uint16_t socNum,
                                  std::vector<uint16_t> payload) override;

    // Start run time post package repair
    uint32_t startRuntimeRepair(uint16_t repairSlot) override;

    // Get status of repair
    std::vector<std::tuple<uint16_t, uint16_t, uint16_t, uint16_t,
                           std::vector<uint16_t>>>
        getPostPackageRepairStatus() override;

    // Set value of RecordAd
    virtual bool recordAdd(bool value) override;
    void UpdatePprResult(int index, uint16_t repairResult, uint16_t repairType);
    void WriteBtPprFile(std::string type, int index, uint16_t repairEntryNum,
                        uint16_t repairType, uint16_t socNum,
                        uint16_t repairResult, std::vector<uint16_t> payload);
    uint16_t GetRuntimeIndex(void);
    std::tuple<uint16_t, uint16_t, uint16_t, uint16_t, std::vector<uint16_t>>
        getRuntimeData(uint16_t index, uint16_t slot);
    std::tuple<uint16_t, uint16_t, uint16_t, uint16_t, std::vector<uint16_t>>
        getBoottimeData(uint16_t index);

  private:
    sdbusplus::bus::bus& bus;
    EventPtr& event;

    std::array<PPR_Data, MAX_REPAIR_SLOTS> m_pprRuntimeData;
    uint32_t updateRuntimeRepairStatus(uint16_t index, uint16_t slot);
    uint16_t m_pprRuntimeIndex;
    uint16_t m_currentRuntimeIndex;
    uint16_t m_currentRuntimeCnt;
    void SetBTfromRT(int index);
    int GetDimmSerialNum(uint16_t Socket, uint16_t Ch, uint16_t Chip);
    uint16_t DimmSN0[MAX_DIMM_SLOT];
    uint16_t DimmSN1[MAX_DIMM_SLOT];
};
