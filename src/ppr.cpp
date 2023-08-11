#include "ppr.hpp"

void childPprData::deleteAll() {
	sd_journal_print(LOG_ERR,
			"Delete Action not permitted for Post Package Repair Entries \n");

}

bool childPprData::setPostPackageRepairData(uint16_t repairEntryNum,
		uint16_t repairType, uint16_t socNum, std::vector<uint16_t> payload) {
	m_pprData[m_pprIndex].repairEntryNum = repairEntryNum;
	sd_journal_print(LOG_DEBUG, "repairEntryNum = %d \n",
			m_pprData[m_pprIndex].repairEntryNum);

	m_pprData[m_pprIndex].repairType = repairType;
	sd_journal_print(LOG_DEBUG, "repairType = %d \n",
			m_pprData[m_pprIndex].repairType);

	m_pprData[m_pprIndex].socNum = socNum;
	sd_journal_print(LOG_DEBUG, "socNum = %d \n", m_pprData[m_pprIndex].socNum);

	std::vector<uint16_t>::iterator it;
	int i = 0;

	for (it = payload.begin(); it != payload.end(); it++) {
		m_pprData[m_pprIndex].payload[i] = *it;
		sd_journal_print(LOG_DEBUG, "payload[%d] = %x \n", i,
				m_pprData[m_pprIndex].payload[i]);
		i++;
	}

	return true;
}

bool childPprData::recordAdd(bool value) {
	if (value == true) {
		if (m_pprIndex < MAX_REPAIR_SLOTS)
			m_pprIndex++;

		PprData::currentRepairEntry(m_pprIndex);
		sd_journal_print(LOG_INFO, "Record Added. CurrentRepairEntry = %d \n",
				PprData::currentRepairEntry());
		value = false;

	}
	return PprData::recordAdd(value, false);
}

std::tuple<uint16_t, uint16_t, uint16_t, uint16_t,
	std::vector<uint16_t>> childPprData::getPostPackageRepairData(
	uint16_t index) {

	std::tuple<uint16_t, uint16_t, uint16_t, uint16_t, std::vector<uint16_t>> tup;

	sd_journal_print(LOG_DEBUG,
			"getPostPackageRepairData() - Begin \n");

	if (index < MAX_REPAIR_SLOTS) {
		std::vector<uint16_t> vec;
		updateRuntimeRepairStatus(index);

		for (int i = 0; i < PAYLOAD_SIZE; i++) {
			vec.push_back(m_pprData[index].payload[i]);
		}

		sd_journal_print(LOG_DEBUG,
				"getPostPackageRepairdata(): repairEntryNum = %d, repairType = %d, socNum = %d, repairResult = 0x%x\n",
				m_pprData[index].repairEntryNum, m_pprData[index].repairType,
				m_pprData[index].socNum, m_pprData[index].repairResult);

		tup = std::make_tuple(m_pprData[index].repairEntryNum,
				m_pprData[index].repairType, m_pprData[index].socNum,
				m_pprData[index].repairResult, vec);

	}
	sd_journal_print(LOG_DEBUG,
			"getPostPackageRepairData() - End \n");

	return tup;
}

std::vector<std::tuple<uint16_t, uint16_t, uint16_t, uint16_t,
        std::vector<uint16_t>>> childPprData::getPostPackageRepairStatus() {

	std::vector<std::tuple<uint16_t, uint16_t, uint16_t, uint16_t, std::vector<uint16_t>>> Finalvec;
	sd_journal_print(LOG_DEBUG,	"childPprData::getPostPackageRepairStatus() - Begin \n");

	for (uint16_t index = 0; index < PprData::currentRepairEntry(); index++) {
		Finalvec.push_back(getPostPackageRepairData(index));
	}

	sd_journal_print(LOG_DEBUG," Size of vector : %d \n",Finalvec.size());
	return Finalvec;
}

uint32_t childPprData::startRuntimeRepair(uint16_t repairSlot) {
	uint8_t offset = 0;
	oob_status_t ret_oob = OOB_MAILBOX_ERR_END;
	struct set_ras_action_data_in Data;
	uint16_t retryCount = MAX_RETRIES;
	uint32_t status;

	sd_journal_print(LOG_DEBUG, "childPprData::startRuntimeRepair() - Begin \n");
	for (offset = 0; offset < PAYLOAD_SIZE; offset++) {
		Data.payload.repair_entry_num = m_pprData[repairSlot].repairEntryNum;
		Data.payload.offset = offset * 2;
		Data.payload.pay_load = m_pprData[repairSlot].payload[offset];
		Data.ras_act_id = RAS_ACTION_ID_RUNTIME_PPR;

		Data.eom_flag = 0;
		if (offset == (PAYLOAD_SIZE - 1)) {
			Data.eom_flag = 1;
		}

		ret_oob = OOB_MAILBOX_ERR_END;
		while (retryCount > 0) {
			ret_oob = set_bmc_ras_action_status(m_pprData[repairSlot].socNum,
					Data, &status);

			if (ret_oob == OOB_SUCCESS) {
				sd_journal_print(LOG_INFO,
						"set_bmc_ras_action_status = 0x%x \n", status);
				break;
			}

			retryCount--;
			sd_journal_print(LOG_WARNING,
					"Set RAS Action PPR Runtime failed. Repair Slot=%d, RetryCount=%d \n",
					repairSlot, retryCount);
		}
	}

	sd_journal_print(LOG_DEBUG, "childPprData::startRuntimeRepair() - End \n");
	return ret_oob;
}

uint32_t childPprData::updateRuntimeRepairStatus(uint16_t repairSlot) {

	struct get_ras_action_data_in Data;
	struct ras_action_status status;
	uint16_t retryCount = MAX_RETRIES;
	oob_status_t ret_oob;

	sd_journal_print(LOG_ERR,
			"childPprData::updateRuntimeRepairStatus() - Begin \n");
	Data.pay_load.repair_entry_num = m_pprData[repairSlot].repairEntryNum;
	Data.ras_action_id = RAS_ACTION_ID_RUNTIME_PPR;

	ret_oob = OOB_MAILBOX_ERR_END;
	while (retryCount > 0) {
		ret_oob = get_bmc_ras_action_status(m_pprData[repairSlot].socNum, Data,
				&status);

		if (ret_oob == OOB_SUCCESS) {
			m_pprData[repairSlot].repairResult = status.repair_result;
			sd_journal_print(LOG_INFO,
					"get_bmc_ras_action_status Repair Entry = 0x%x, Repair Result = 0x%x \n",
					status.repair_entry_num, status.repair_result);

			break;
		}
		usleep(1000 * 1000);
		retryCount--;
		sd_journal_print(LOG_WARNING,
				"Set RAS Action PPR Runtime failed. Index = %d, RetryCount = %d \n",
				repairSlot, retryCount);
	}

	sd_journal_print(LOG_DEBUG,
			"childPprData::updateRuntimeRepairStatus() - End \n");
	return ret_oob;
}

