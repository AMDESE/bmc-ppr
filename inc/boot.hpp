#pragma once
#include "ppr.hpp"
#include "boot.hpp"
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
#include <future>
#include <gpiod.hpp>
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
#include <experimental/filesystem>
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>

#include <endian.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Collection/DeleteAll/server.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/State/Host/server.hpp>


#define UINT8 uint8_t
#define UINT16 uint16_t
#define UINT32 uint32_t
#define UINT64 uint64_t
#define CHAR8 unsigned char
#define UINTN unsigned int_least8_t

#define BYTE_LENGTH              16
#define OFfSET_1                 16
#define OFfSET_2                 32
#define MEMBAR_BUFFER_LENGTH   1000
#define BMC_BUFFER_LENGTH      1024
#define CMD_BUFF_LEN             10

struct BiosPprHeader {
  UINT32 Signature : 24;
  UINT8  Version;
  UINT16 ReportSize;
  UINT8  EntryCount;
  UINT8  Reserved[10];
}__attribute__((packed));

struct BiosPprData {
  UINT8  Type;
  UINT8  Version;
  UINT8  Length;
  UINT8  RepairEntryNumber;
  UINT8  RepairType;
  UINT8  SocNum;
  UINT8  RepairResult;
  UINT16 Payload[10];
  UINT8  Reserved[4];
}__attribute__((packed));


class BootTimePprDataHolder
{
    static BootTimePprDataHolder *instance;

    BootTimePprDataHolder()
    {
    }

  public:
    static BootTimePprDataHolder *getInstance()
    {
        if (!instance)
            instance = new BootTimePprDataHolder;
        return instance;
    }

    const static constexpr char *PropertiesIntf =
        "org.freedesktop.DBus.Properties";
    const static constexpr char *HostStatePathPrefix =
        "/xyz/openbmc_project/state/host0";
};

struct EventDeleter1
{
    void operator()(sd_event *event) const
    {
        event = sd_event_unref(event);
    }
};

using EventPtr1 = std::unique_ptr<sd_event, EventDeleter1>;
namespace StateServer = sdbusplus::xyz::openbmc_project::State::server;

struct BootTimePprData
{
	BootTimePprDataHolder *bootTimePprDataHolderObj =
			bootTimePprDataHolderObj->getInstance();

	BootTimePprData(sdbusplus::bus::bus &bus, const char *path, EventPtr1 &event) :
		bus(bus),
		propertiesChangedBootTimePprDataValue(
				bus,
				sdbusplus::bus::match::rules::type::signal() +
				sdbusplus::bus::match::rules::member("PropertiesChanged") +
				sdbusplus::bus::match::rules::path(
						"/xyz/openbmc_project/PostPackageRepair") +
						sdbusplus::bus::match::rules::argN(0, "xyz.openbmc_project.PostPackageRepair") +
						sdbusplus::bus::match::rules::interface(
								bootTimePprDataHolderObj->PropertiesIntf),
								[this](sdbusplus::message::message &msg) {
		std::string objectName;
		std::map<std::string, std::variant<uint32_t,bool>> msgData;
		msg.read(objectName, msgData);
		//Since we have data only after Host reboot - second event should handle this
		//TO DO - in case in future if we need to transfer PCIe data even Host is up we can use this Property event
	}),
	propertiesChangedSignalCurrentHostState(
			bus,
			sdbusplus::bus::match::rules::type::signal() +
			sdbusplus::bus::match::rules::member("PropertiesChanged") +
			sdbusplus::bus::match::rules::path(
					bootTimePprDataHolderObj->HostStatePathPrefix)  +
					sdbusplus::bus::match::rules::interface(
							bootTimePprDataHolderObj->PropertiesIntf),
							[this](sdbusplus::message::message &msg) {
		std::string objectName;
		std::map<std::string, std::variant<std::string>> msgData;
		msg.read(objectName, msgData);
		// Check if HOST was powered-on event
		auto valPropMap = msgData.find("CurrentHostState");
		{
			if (valPropMap != msgData.end())
			{
				StateServer::Host::HostState currentHostState =
						StateServer::Host::convertHostStateFromString(
								std::get<std::string>(valPropMap->second));

				if (currentHostState != StateServer::Host::HostState::Off)
				{   // here bmc reboot + Host reboot signal will recived
					sd_journal_print(LOG_ERR, "Host UP signal received \n");
					//call entry point
					onHostPwrChange();
					if (deviceEntryCount > 0)
					{

					}
				}
			}//end of if
		}
	})
	{
		sd_journal_print(LOG_ERR, "New Shared memory Constructor - Check \n");
		if (event != nullptr)
		{
			sd_journal_print(LOG_DEBUG, "BMC after boot - code execution path %s \n", path);
		}
	}

	~BootTimePprData()
	{
	}

private:
	sdbusplus::bus::bus &bus;
	sdbusplus::bus::match_t propertiesChangedBootTimePprDataValue;
	sdbusplus::bus::match_t propertiesChangedSignalCurrentHostState;

	void  *base_addr;
	uint32_t deviceEntryCount = 0;
	std::array<PPR_Data, MAX_REPAIR_SLOTS> pprBoottimeDataIN;

	void poolMembar();
	bool ReadHostQueue();
	void ReadBootTimePprData(uint32_t entryCount);
	uint32_t getPCIeEntryCount();
	void  onHostPwrChange();

	void SyncMainDB();
	void WriteHostQueue();
};
