#pragma once
#include "rt_ppr.hpp"

#include <ctype.h>
#include <endian.h>
#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <boost/asio.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>
#include <experimental/filesystem>
#include <filesystem>
#include <fstream>
#include <future>
#include <gpiod.hpp>
#include <iostream>
#include <mutex>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <regex>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/asio/property.hpp>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <utility>
#include <xyz/openbmc_project/Collection/DeleteAll/server.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/State/Host/server.hpp>

#define UINT8 uint8_t
#define UINT16 uint16_t
#define UINT32 uint32_t
#define UINT64 uint64_t
#define CHAR8 unsigned char
#define UINTN unsigned int_least8_t

const int MAX_RETRY = 600;
const int TSLEEP = 2;
const int PPR_SM_OFFSET = 0xF0000;
const int BYTE_LENGTH = 16;
const int PPR_BT_HEADER_SIZE = 16;
const int PPR_BT_DATA_SIZE = 32;
const int MEMBAR_BUFFER_LENGTH = 1000;
const int BMC_BUFFER_LENGTH = 1024;
const int CMD_BUFF_LEN = 10;
const int BOOTTIME_PPR_SIGNATURE = 0x525050; // PPR
const int BOOTTIME_PPR_VERSION = 0x02;
const int BOOTTIME_PPR_TYPE = 0x03;

const int BT_PPR_CMD_DISABLE_OOB = 0x0080;
const int BT_PPR_CMD_BIOS_CNT = 0x0081;
const int BT_PPR_CMD_BMC_CNT = 0x0082;
const int Q2_BIOS_SIG = 0x81;
const int Q2_READ_CNT = 4;

const UINT8 BT_MATCH = 0xFF;
const UINT8 BT_NOT_MATCH = 0x00;

struct BiosPprHeader
{
    UINT32 Signature : 24;
    UINT8 Version;
    UINT16 ReportSize;
    UINT8 EntryCount;
    UINT8 Command;
    UINT8 Reserve[8];
} __attribute__((packed));

struct BiosPprData
{
    UINT8 Type;
    UINT8 Version;
    UINT8 Length;
    UINT8 RepairEntryNumber;
    UINT8 RepairType;
    UINT8 SocNum;
    UINT8 RepairResult;
    UINT8 Reserved1;
    UINT16 Payload[10];
    UINT8 Reserved2[4];
};

class BootTimePprDataHolder
{
    static BootTimePprDataHolder* instance;

    BootTimePprDataHolder()
    {
    }

  public:
    static BootTimePprDataHolder* getInstance()
    {
        if (!instance)
            instance = new BootTimePprDataHolder;
        return instance;
    }

    const static constexpr char* PropertiesIntf =
        "org.freedesktop.DBus.Properties";
    const static constexpr char* HostStatePathPrefix =
        "/xyz/openbmc_project/state/host0";
};

struct EventDeleter1
{
    void operator()(sd_event* event) const
    {
        event = sd_event_unref(event);
    }
};

using EventPtr1 = std::unique_ptr<sd_event, EventDeleter1>;
namespace StateServer = sdbusplus::xyz::openbmc_project::State::server;

class BootTimePprData
{
  public:
    commonPPR* globalBT = globalBT->getInstance();

    BootTimePprData()
    {
        sd_journal_print(LOG_INFO, "BootTimePprData: start Shared Mem Poll \n");
        pollSharedMem();
    }

    ~BootTimePprData()
    {
    }

  private:
    std::array<PPR_Data, MAX_REPAIR_SLOTS> pprBoottimeDataIn;
    std::array<PPR_Data, MAX_REPAIR_SLOTS> pprBoottimeDataOut;
    UINT8 BiosInCnt;
    UINT8 BiosOutCnt;
    UINT8 MatchIndex[MAX_REPAIR_SLOTS];
    bool SystemStateOn;

    void pollSharedMem();
    void ReadBootTimePprData(UINT8 entryCount);
    bool getBiosInData();
    UINT8 setBiosOutData();
    void WriteHostSharedMem();
    UINT8 compareBiosData();
};
