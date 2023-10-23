#include "rt_ppr.hpp"
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/error.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/property.hpp>

std::array<PPR_Data, MAX_REPAIR_SLOTS> g_pprBoottimeData;
uint16_t g_pprBoottimeIndex;

void childPprData::WritePprFile(int index, uint16_t repairEntryNum,
                  uint16_t repairType, uint16_t socNum, std::vector<uint16_t> payload)
{
	std::string pprFile;

	if ((repairType & PPR_TYPE_BOOTTIME_MASK) == 0)
		pprFile = kPprDir.data() + getPprRuntimeFilename((int)index);
	else
		pprFile = kPprDir.data() + getPprBoottimeFilename((int)index);

	nlohmann::json jsonPpr = {
		 { "repairEntryNum" , repairEntryNum },
		 { "repairType"     , repairType },
		 { "socNum"         , socNum },
		 { "repairResult"   , PPR_STATUS_REPAIR_NOT_PROCESSED },
		 { "payload"        , payload }
	 };
	 std::ofstream jsonWrite(pprFile);
	 jsonWrite << jsonPpr;
	 jsonWrite.close();
}

void childPprData::UpdatePprResult(int index, uint16_t repairResult, uint16_t repairType)
{
	std::string pprFile;

	if ((repairType & PPR_TYPE_BOOTTIME_MASK) == 0)
		pprFile = kPprDir.data() + getPprRuntimeFilename((int)index);
	else
		pprFile = kPprDir.data() + getPprBoottimeFilename((int)index);
        std::ifstream jsonRead(pprFile);
        nlohmann::json data = nlohmann::json::parse(jsonRead);

        data["repairResult"]   = repairResult;
        std::ofstream jsonWrite(pprFile);
        jsonWrite << data;
        jsonRead.close();
        jsonWrite.close();
}

void childPprData::deleteAll() {
	sd_journal_print(LOG_ERR,
			"Delete Action not permitted for Post Package Repair Entries \n");
}

void childPprData::updateBTfromBoottimeRepair()
{
	int i, j;
	std::string pprFile;
	struct stat buffer;
	std::vector<uint16_t> payload;
	uint16_t    result;

	for (i=0; i < MAX_REPAIR_SLOTS; i++)
	{
		memset(&buffer, 0, sizeof(buffer));
		pprFile = kPprDir.data() + getPprBoottimeFilename(i);
		if (stat(pprFile.c_str(), &buffer) == 0)
		{
			sd_journal_print(LOG_INFO,"updateBTfromBoottimeRepair: File %s exist", pprFile.c_str());
			std::ifstream jsonRead(pprFile);
			nlohmann::json data = nlohmann::json::parse(jsonRead);
			result = data["repairResult"];
			if (result == PPR_STATUS_REPAIR_NOT_PROCESSED)
			{
				g_pprBoottimeData[g_pprBoottimeIndex].repairEntryNum = data["repairEntryNum"];
				g_pprBoottimeData[g_pprBoottimeIndex].repairType     = data["repairType"];
				g_pprBoottimeData[g_pprBoottimeIndex].socNum         = data["socNum"];
				g_pprBoottimeData[g_pprBoottimeIndex].repairResult   = data["repairResult"];
				payload  = data.at("payload").get<std::vector<uint16_t>>();
				for (j=0; j < PAYLOAD_SIZE; j++)
					g_pprBoottimeData[g_pprBoottimeIndex].payload[j] = payload[j];
				g_pprBoottimeIndex++;
			}
		}
	}
	sd_journal_print(LOG_INFO,"updateBTfromBoottimeRepair: PPR BT Index = %d", g_pprBoottimeIndex);
}

void childPprData::updateBTfromRuntimeRepair()
{
	int i, j;
	std::string pprFile;
	struct stat buffer;
	std::vector<uint16_t> payload;
	uint16_t    result;

	for (i=0; i < MAX_REPAIR_SLOTS; i++)
	{
		memset(&buffer, 0, sizeof(buffer));
		pprFile = kPprDir.data() + getPprRuntimeFilename(i);
		if (stat(pprFile.c_str(), &buffer) == 0)
		{
			sd_journal_print(LOG_INFO,"updateBTfromRuntimeRepair: File %s exist", pprFile.c_str());
			std::ifstream jsonRead(pprFile);
			nlohmann::json data = nlohmann::json::parse(jsonRead);
			result = data["repairResult"];
			if (result == PPR_STATUS_REPAIR_PASS)
			{
				g_pprBoottimeData[g_pprBoottimeIndex].repairEntryNum = data["repairEntryNum"];
				g_pprBoottimeData[g_pprBoottimeIndex].repairType     = data["repairType"];
				g_pprBoottimeData[g_pprBoottimeIndex].socNum         = data["socNum"];
				g_pprBoottimeData[g_pprBoottimeIndex].repairResult   = data["repairResult"];
				payload  = data.at("payload").get<std::vector<uint16_t>>();
				for (j=0; j < PAYLOAD_SIZE; j++)
					g_pprBoottimeData[g_pprBoottimeIndex].payload[j] = payload[j];
				g_pprBoottimeIndex++;
			}
		}
	}
	sd_journal_print(LOG_INFO,"updateBTfromRuntimeRepair: PPR BT Index = %d, PPR RT Index = %d",
			g_pprBoottimeIndex, m_pprRuntimeIndex);
}

bool childPprData::setPostPackageRepairData(uint16_t repairEntryNum,
		uint16_t repairType, uint16_t socNum, std::vector<uint16_t> payload) {
	std::vector<uint16_t>::iterator it;
	int i = 0;

	if ((repairType & PPR_TYPE_BOOTTIME_MASK) == 0) {
		if (m_currentRuntimeCnt == 0) {
			m_currentRuntimeIndex = m_pprRuntimeIndex;
		}
		m_currentRuntimeCnt++;
		if (m_currentRuntimeCnt > MAX_CURRENT_Runtime_PPR) {
			sd_journal_print(LOG_INFO, "setPPRData: Reach Max Runtime Curr Cnt = %d , Curr Index = %d",
                                m_currentRuntimeCnt, m_currentRuntimeIndex);
			return false;
		}
		sd_journal_print(LOG_INFO, "setPPRData: Runtime Curr Cnt = %d , Curr Index = %d  Index = %d Entry Num = %d\n",
                                m_currentRuntimeCnt, m_currentRuntimeIndex, m_pprRuntimeIndex, repairEntryNum);
		m_pprRuntimeData[m_pprRuntimeIndex].repairResult = PPR_STATUS_REPAIR_NOT_PROCESSED;
		m_pprRuntimeData[m_pprRuntimeIndex].repairEntryNum = repairEntryNum;
		m_pprRuntimeData[m_pprRuntimeIndex].repairType = repairType;
		m_pprRuntimeData[m_pprRuntimeIndex].socNum = socNum;
		for (it = payload.begin(); it != payload.end(); it++) {
			m_pprRuntimeData[m_pprRuntimeIndex].payload[i] = *it;
			i++;
		}
		WritePprFile((int)m_pprRuntimeIndex, repairEntryNum, repairType, socNum, payload);
		if ((m_pprRuntimeIndex + g_pprBoottimeIndex) < MAX_REPAIR_SLOTS)
		{
			m_pprRuntimeIndex++;
		}
	}
	else {
		sd_journal_print(LOG_INFO, "setPPRData: Boottime Entry Num = %d, Index = %d \n", repairEntryNum, g_pprBoottimeIndex);
		g_pprBoottimeData[g_pprBoottimeIndex].repairResult = PPR_STATUS_REPAIR_NOT_PROCESSED;
		g_pprBoottimeData[g_pprBoottimeIndex].repairEntryNum = repairEntryNum;
		g_pprBoottimeData[g_pprBoottimeIndex].repairType = repairType;
		g_pprBoottimeData[g_pprBoottimeIndex].socNum = socNum;
		for (it = payload.begin(); it != payload.end(); it++) {
			g_pprBoottimeData[g_pprBoottimeIndex].payload[i] = *it;
			i++;
		}
		WritePprFile((int)g_pprBoottimeIndex, repairEntryNum, repairType, socNum, payload);
		if ((m_pprRuntimeIndex + g_pprBoottimeIndex) < MAX_REPAIR_SLOTS)
		{
			g_pprBoottimeIndex++;
		}
	}

	return true;
}

bool childPprData::recordAdd(bool value) {
	if (value == true) {
		PprData::currentRepairEntry(m_pprRuntimeIndex);
		sd_journal_print(LOG_INFO, "Record Added. Runtime Curr Cnt = %d , Curr Index = %d  Index = %d \n",
				 m_currentRuntimeCnt, m_currentRuntimeIndex, m_pprRuntimeIndex);
		value = false;

	}
	return PprData::recordAdd(value, false);
}

uint16_t childPprData::GetRuntimeIndex(void)
{
        return m_pprRuntimeIndex;
}

std::tuple<uint16_t, uint16_t, uint16_t, uint16_t,
	std::vector<uint16_t>> childPprData::getRuntimeData(uint16_t index, uint16_t slot)
{

	std::tuple<uint16_t, uint16_t, uint16_t, uint16_t, std::vector<uint16_t>> tup;

	sd_journal_print(LOG_INFO, "getRuntimeData: - Begin , Index = %d, Slot = %d\n", index, slot);
	std::vector<uint16_t> vec;

	if((m_pprRuntimeData[index].repairResult == PPR_STATUS_REPAIR_NOT_PROCESSED) &&
	   (slot < MAX_REPAIR_SLOTS))
	    updateRuntimeRepairStatus(index, slot);

	for (int i = 0; i < PAYLOAD_SIZE; i++)
		vec.push_back(m_pprRuntimeData[index].payload[i]);

	sd_journal_print(LOG_INFO,
		"getRuntimeData(): Index = %d, repairEntryNum = %d, repairType = %d, socNum = %d, repairResult = 0x%x\n",
		index, m_pprRuntimeData[index].repairEntryNum, m_pprRuntimeData[index].repairType,
		m_pprRuntimeData[index].socNum, m_pprRuntimeData[index].repairResult);

	tup = std::make_tuple(m_pprRuntimeData[index].repairEntryNum,
		m_pprRuntimeData[index].repairType, m_pprRuntimeData[index].socNum,
		m_pprRuntimeData[index].repairResult, vec);

	sd_journal_print(LOG_INFO,
			"getRuntimeData: - End \n");

	return tup;
}

std::tuple<uint16_t, uint16_t, uint16_t, uint16_t,
        std::vector<uint16_t>> childPprData::getBoottimeData(uint16_t index)
{

	std::tuple<uint16_t, uint16_t, uint16_t, uint16_t, std::vector<uint16_t>> tup;

	sd_journal_print(LOG_INFO, "getBoottimeData: - Begin , Index = %d\n", index);

	if (index < MAX_REPAIR_SLOTS) {
		std::vector<uint16_t> vec;

		for (int i = 0; i < PAYLOAD_SIZE; i++) {
			vec.push_back(g_pprBoottimeData[index].payload[i]);
		}

		sd_journal_print(LOG_INFO,
                                "getBoottimeData(): repairEntryNum = %d, repairType = %d, socNum = %d, repairResult = 0x%x\n",
                                g_pprBoottimeData[index].repairEntryNum, g_pprBoottimeData[index].repairType,
                                g_pprBoottimeData[index].socNum, g_pprBoottimeData[index].repairResult);

		tup = std::make_tuple(g_pprBoottimeData[index].repairEntryNum,
                                g_pprBoottimeData[index].repairType, g_pprBoottimeData[index].socNum,
                                g_pprBoottimeData[index].repairResult, vec);
	}
	sd_journal_print(LOG_INFO, "getBoottimeData: - End \n");

	return tup;
}

std::vector<std::tuple<uint16_t, uint16_t, uint16_t, uint16_t,
        std::vector<uint16_t>>> childPprData::getPostPackageRepairStatus() {

	std::vector<std::tuple<uint16_t, uint16_t, uint16_t, uint16_t, std::vector<uint16_t>>> Finalvec;
	uint16_t slot = 0;
	sd_journal_print(LOG_INFO,	"childPprData::getPostPackageRepairStatus() - Begin \n");

	for (uint16_t index = 0; index < PprData::currentRepairEntry(); index++) {
		if(m_pprRuntimeData[index].repairResult == PPR_STATUS_REPAIR_NOT_PROCESSED) {
			Finalvec.push_back(getRuntimeData(index, slot));
			slot++;
		}
		else
			Finalvec.push_back(getRuntimeData(index, MAX_REPAIR_SLOTS));

	}
	for (uint16_t index = 0; index < g_pprBoottimeIndex; index++) {
		Finalvec.push_back(getBoottimeData(index));
	}

	return Finalvec;
}

uint32_t childPprData::startRuntimeRepair(uint16_t repairSlot) {
	uint8_t offset = 0;
	oob_status_t ret_oob = OOB_MAILBOX_ERR_END;
	struct set_ras_action_data_in Data;
	uint16_t retryCount = MAX_RETRIES;
	uint32_t status;
	uint16_t slot;

	sd_journal_print(LOG_INFO, "startRuntimeRepair: Slot = %d Runtime Curr Cnt = %d\n", repairSlot, m_currentRuntimeCnt);
	if (repairSlot < m_currentRuntimeCnt) {
		slot = repairSlot + m_currentRuntimeIndex;
		sd_journal_print(LOG_INFO, "startRuntimeRepair: Begin Runtime Repair for Slot = %d\n", slot);
		for (offset = 0; offset < PAYLOAD_SIZE; offset++) {
			Data.payload.repair_entry_num = repairSlot;
			Data.payload.offset = offset * 2;
			Data.payload.pay_load = m_pprRuntimeData[slot].payload[offset];
			Data.ras_act_id = RAS_ACTION_ID_RUNTIME_PPR;

			Data.eom_flag = 0;
			if ((offset == (PAYLOAD_SIZE - 1)) &&
			   ((m_currentRuntimeCnt - repairSlot) == 1))
				Data.eom_flag = 1;

			ret_oob = OOB_MAILBOX_ERR_END;
			while (retryCount > 0) {
				ret_oob = set_bmc_ras_action_status(m_pprRuntimeData[slot].socNum, Data, &status);

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

		sd_journal_print(LOG_INFO, "childPprData::startRuntimeRepair() - End \n");
	}
	else {
		// Boottime PPR
		ret_oob = OOB_SUCCESS;
	}
	return ret_oob;
}

uint32_t childPprData::updateRuntimeRepairStatus(uint16_t index, uint16_t slot) {

	struct get_ras_action_data_in Data;
	struct ras_action_status status;
	uint16_t retryCount = MAX_RETRIES;
	oob_status_t ret_oob;

	sd_journal_print(LOG_INFO, "updateRuntimeRepairStatus: Index = %d Slot = %d Curr Cnt = %d\n", index, slot, m_currentRuntimeCnt);
	if (slot < m_currentRuntimeCnt) {
		sd_journal_print(LOG_INFO, "updateRuntimeRepairStatus: Begin Runtime Slot = %d \n", slot);
		Data.pay_load.repair_entry_num = slot;
		Data.ras_action_id = RAS_ACTION_ID_RUNTIME_PPR;

		ret_oob = OOB_MAILBOX_ERR_END;
		while (retryCount > 0) {
			ret_oob = get_bmc_ras_action_status(m_pprRuntimeData[index].socNum, Data, &status);

			if (ret_oob == OOB_SUCCESS) {
				m_pprRuntimeData[index].repairResult = status.repair_result;
				UpdatePprResult((int)index, (uint16_t) status.repair_result, m_pprRuntimeData[index].repairType);
				sd_journal_print(LOG_INFO,
					"get_bmc_ras_action_status Repair Index = %d Slot = %d, Repair Result = 0x%x \n",
					index, slot, status.repair_result);
				if ((m_currentRuntimeCnt - slot) == 1)
					m_currentRuntimeCnt = 0;

				break;
			}
			usleep(200 * 1000);
			retryCount--;
			sd_journal_print(LOG_WARNING,
				"Get RAS Action PPR Runtime Status failed. Index = %d  Slot = %d  RetryCount = %d \n",
				index, slot, retryCount);
		}

		sd_journal_print(LOG_INFO,
			"childPprData::updateRuntimeRepairStatus() - End \n");
	}
	else
	{   // Boot Time PPR
		ret_oob = OOB_SUCCESS;
	}
	return ret_oob;
}

