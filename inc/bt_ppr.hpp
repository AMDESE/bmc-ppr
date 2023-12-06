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

#define index_file                                                             \
    ("/sys/devices/platform/ahb/ahb:apb/1e7e0000.bmc_dev/bmc-dev-queue2")
#define BMC_DEV ("/dev/bmc-device")

#define UINT8 uint8_t
#define UINT16 uint16_t
#define UINT32 uint32_t
#define UINT64 uint64_t
#define CHAR8 unsigned char
#define UINTN unsigned int_least8_t

#define MAX_RETRY 20
#define TSLEEP    1

#define PPR_SM_OFFSET          0xF0000
#define BYTE_LENGTH            16
#define PPR_BT_HEADER_SIZE     16
#define PPR_BT_DATA_SIZE       32
#define MEMBAR_BUFFER_LENGTH   1000
#define BMC_BUFFER_LENGTH      1024
#define CMD_BUFF_LEN           10
#define BOOTTIME_PPR_SIGNATURE 0x525050 // PPR
#define BOOTTIME_PPR_VERSION   0x01
#define BOOTTIME_PPR_TYPE      0x03

#define BT_PPR_CMD_DISABLE_OOB 0x0080
#define BT_PPR_CMD_BIOS_CNT    0x0081
#define BT_PPR_CMD_BMC_CNT     0x0082
#define Q2_BIOS_SIG            0x81
#define Q2_READ_CNT            4

struct BiosPprHeader
{
    UINT32 Signature : 24;
    UINT8 Version;
    UINT16 ReportSize;
    UINT8 EntryCount;
    UINT16 Command;
    UINT8 Reserved[7];
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
    UINT16 Payload[10];
    UINT8 Reserved[5];
} __attribute__((packed));

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
        sd_journal_print(LOG_ERR, "BootTimePprData: start Shared Mem Pool \n");
        poolSharedMem();
    }

    ~BootTimePprData()
    {
    }

  private:
    void* base_addr;
    std::array<PPR_Data, MAX_REPAIR_SLOTS> pprBoottimeDataIn;
    std::array<PPR_Data, MAX_REPAIR_SLOTS> pprBoottimeDataOut;
    UINT8 BiosInCnt;
    UINT8 BiosOutCnt;
    bool SystemStateOn;

    void poolSharedMem();
    bool ReadHostSharedMem();
    void ReadBootTimePprData(UINT8 entryCount);
    bool getBiosInData();
    UINT8 setBiosOutData();
    void WriteHostSharedMem();
    void compareBiosData();
};
