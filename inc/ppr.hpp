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
#include <unistd.h>

#include <systemd/sd-journal.h>

#include <xyz/openbmc_project/Collection/DeleteAll/server.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/PostPackageRepair/PprData/server.hpp>

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

const int MAX_RETRIES = 10;
const int RAS_ACTION_ID_RUNTIME_PPR = 0;
const int PAYLOAD_SIZE = 10;
const int MAX_REPAIR_SLOTS = 64;

// PPR Service

const static constexpr char *pprDataInPath =
		"/xyz/openbmc_project/PostPackageRepair/PprData";
const static constexpr char *PropertiesIntf = "org.freedesktop.DBus.Properties";

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
struct PPR_Data {
	uint16_t repairEntryNum;
	uint16_t repairType;
	uint16_t socNum;
	uint16_t repairResult;
	uint16_t payload[PAYLOAD_SIZE];
};

struct EventDeleter {
	void operator()(sd_event *event) const {
		event = sd_event_unref(event);
	}
};
using EventPtr = std::unique_ptr<sd_event, EventDeleter>;

using repairtype_t = uint16_t;
using repairentrynum_t = uint16_t;
using socnum_t = uint16_t;
using repairresult_t = uint16_t;
//using payload_t = std::array<uint16_t, PAYLOAD_SIZE>;
//using pprdata_in_t = std::tuple<repairentrynum_t, repairtype_t, socnum_t, payload_t>;

using ppr_data =
sdbusplus::xyz::openbmc_project::PostPackageRepair::server::PprData;
using delete_all =
sdbusplus::xyz::openbmc_project::Collection::server::DeleteAll;

struct childPprData: sdbusplus::server::object_t<ppr_data, delete_all> {
	childPprData(sdbusplus::bus::bus &bus, const char *path, EventPtr &event) :
			sdbusplus::server::object_t<ppr_data, delete_all>(bus, path), bus(
					bus), event(event) {
		m_pprIndex = 0;
		sd_journal_print(LOG_ERR, "PPR Data Constructor - Check \n");

	}

	~childPprData() {
	}

	void deleteAll() override;
	/** Set values of repair data */
	bool setPostPackageRepairData(uint16_t repairEntryNum, uint16_t repairType,
			uint16_t socNum, std::vector<uint16_t> payload) override;

	/** Start run time post package repair */
	uint32_t startRuntimeRepair(uint16_t repairSlot) override;

	/** Get status of repair */
	std::vector<std::tuple<uint16_t, uint16_t, uint16_t, uint16_t,
		std::vector<uint16_t>>> getPostPackageRepairStatus() override;

	/** Set value of RecordAdd */
	virtual bool recordAdd(bool value) override;

//    void scheduled_flush();
private:
	sdbusplus::bus::bus &bus;
	EventPtr &event;

	std::array<PPR_Data, MAX_REPAIR_SLOTS> m_pprData;

	uint32_t updateRuntimeRepairStatus(uint16_t repairSlot);

	std::tuple<uint16_t, uint16_t, uint16_t, uint16_t,
		std::vector<uint16_t>> getPostPackageRepairData(
		uint16_t index);

	uint16_t m_pprIndex;
};
