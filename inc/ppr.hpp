#pragma once

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
#include <filesystem>
#include <fstream>
#include <future>
#include <gpiod.hpp>
#include <iostream>
#include <mutex>
#include <phosphor-logging/log.hpp>
#include <regex>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/asio/property.hpp>
#include <shared_mutex>
#include <string_view>
#include <utility>
#include <regex>
#include <ctype.h>
#include <nlohmann/json.hpp>
#include <experimental/filesystem>

extern "C" {
#include <sys/stat.h>
#include "linux/i2c-dev.h"
#include "i2c/smbus.h"
#include "apml.h"
#include "esmi_cpuid_msr.h"
#include "esmi_mailbox.h"
#include "esmi_rmi.h"
#include "esmi_mailbox_nda.h"
}

// PPR Service
constexpr std::string_view pprService = "com.amd.ppr";
constexpr std::string_view pprPath = "/com/amd/ppr";
constexpr std::string_view pprInterface = "com.amd.ppr";

// Status Interface
constexpr std::string_view pprStatusInterface = "com.amd.ppr.Status";


// file Interface
constexpr std::string_view pprFileInterface = "com.amd.ppr.File";


const int MAX_RETRIES = 10;
const int RAS_ACTION_ID_RUNTIME_PPR = 0;

enum REPAIRTYPE {
	runtime = RAS_ACTION_ID_RUNTIME_PPR, boot = 1,
};

// cmd 0x67 Get PPR RAS Action Status
// Repair Status
enum PPR_STATUS {
	PPR_STATUS_REPAIR_FAIL = 0x0,
	PPR_STATUS_REPAIR_PASS = 0x1,
	PPR_STATUS_REPAIR_RANK_MISS_MATCH_ERROR = 0x2,
	PPR_STATUS_REPAIR_RANK_INVALID_ERROR = 0x4,
	PPR_STATUS_REPAIR_BANK_INVALID_ERROR = 0x08,
	PPR_STATUS_REPAIR_SOCKET_INVALID_ERROR = 0x10,
	PPR_STATUS_REPAIR_CHANNEL_INVALID_ERROR = 0x20,
	PPR_STATUS_REPAIR_DEVICE_INVALID_ERROR = 0x40,
	PPR_STATUS_REPAIR_DEVICE_MISMATCH_ERROR = 0x80,
};

//PPR File
struct PPR_Data_In {
    uint8_t  repairType;
    uint8_t  repairEntryNum;
    uint8_t  soc_num;
    uint8_t  offset;
    uint16_t payload;
};

struct PPR_Data_Status {
    uint8_t  repairType;
    uint8_t  repairEntryNum;
    uint8_t  soc_num;
    uint8_t  offset;
    uint8_t  repair_result;
    uint16_t payload;
};

struct error_time_stamp {
  uint8_t    Seconds;
  uint8_t    Minutes;
  uint8_t    Hours;
  uint8_t    Flag;
  uint8_t    Day;
  uint8_t    Month;
  uint8_t    Year;
  uint8_t    Century;
} __attribute__((packed));

typedef struct error_time_stamp ERROR_TIME_STAMP;

