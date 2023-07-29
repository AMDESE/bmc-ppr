#include "ppr_Runtime.hpp"

bool ppr_Runtime::isPPRSupported() {
	// TBD - Need to check model ID via CPU inventory interface
	return true;
}

uint32_t ppr_Runtime::startRuntimeRepair() {

	for (unsigned int i = 0; i < m_pprDataSize; i++) {
		struct set_ras_action_data_in Data;
		uint16_t retryCount = MAX_RETRIES;
		oob_status_t ret_oob;
		uint32_t status;

		Data.payload.repair_entry_num = m_pprDataIn[i].repairEntryNum;
		Data.payload.offset = m_pprDataIn[i].offset;
		Data.payload.pay_load = m_pprDataIn[i].payload;
		Data.ras_act_id = RAS_ACTION_ID_RUNTIME_PPR;

		Data.eom_flag = 0;
		if (i == (m_pprDataSize - 1)) {
			Data.eom_flag = 1;
		}

		ret_oob = OOB_MAILBOX_ERR_END;
		while (retryCount > 0) {
			ret_oob = set_bmc_ras_action_status(m_pprDataIn[i].soc_num, Data,
					&status);

			if (ret_oob == OOB_SUCCESS) {
				sd_journal_print(LOG_INFO,
						"set_bmc_ras_action_status = 0x%x \n", status);
				break;
			}
			usleep(1000 * 1000);
			retryCount--;
			sd_journal_print(LOG_INFO,
					"Set RAS Action PPR Runtime failed. Index = %d, RetryCount = %d \n",
					i, retryCount);
		}
	}

	return 0;
}

uint32_t ppr_Runtime::getRuntimeRepairStatus() {

	for (unsigned int i = 0; i < m_pprDataSize; i++) {
		struct get_ras_action_data_in Data;
		struct ras_action_status status;
		uint16_t retryCount = MAX_RETRIES;
		oob_status_t ret_oob;

		Data.pay_load.repair_entry_num = m_pprDataIn[i].repairEntryNum;
		Data.ras_action_id = RAS_ACTION_ID_RUNTIME_PPR;

		struct PPR_Data_Status pprDataStatus;

		pprDataStatus.offset = m_pprDataIn[i].offset;
		pprDataStatus.payload = m_pprDataIn[i].payload;
		pprDataStatus.soc_num = m_pprDataIn[i].soc_num;

		ret_oob = OOB_MAILBOX_ERR_END;
		while (retryCount > 0) {
			ret_oob = get_bmc_ras_action_status(m_pprDataIn[i].soc_num, Data,
					&status);

			if (ret_oob == OOB_SUCCESS) {

				pprDataStatus.repair_result = status.repair_result;
				pprDataStatus.repairEntryNum = status.repair_entry_num;

				m_pprStatus.push_back(pprDataStatus);
				sd_journal_print(LOG_INFO,
						"get_bmc_ras_action_status Repair Entry = 0x%x, Repair Result = 0x%x \n",
						status.repair_entry_num, status.repair_result);

				break;
			}
			usleep(1000 * 1000);
			retryCount--;
			sd_journal_print(LOG_INFO,
					"Set RAS Action PPR Runtime failed. Index = %d, RetryCount = %d \n",
					i, retryCount);
		}
	}

	return 0;
}

void ppr_Runtime::calculate_time_stamp(ERROR_TIME_STAMP& TimeStamp)
{
    using namespace std;
    using namespace std::chrono;
    typedef duration<int, ratio_multiply<hours::period, ratio<24> >::type> days;

    system_clock::time_point now = system_clock::now();
    system_clock::duration tp = now.time_since_epoch();

    days d = duration_cast<days>(tp);
    tp -= d;
    hours h = duration_cast<hours>(tp);
    tp -= h;
    minutes m = duration_cast<minutes>(tp);
    tp -= m;
    seconds s = duration_cast<seconds>(tp);
    tp -= s;

    time_t tt = system_clock::to_time_t(now);
    tm utc_tm = *gmtime(&tt);

    TimeStamp.Seconds = utc_tm.tm_sec;
    TimeStamp.Minutes = utc_tm.tm_min;
    TimeStamp.Hours   = utc_tm.tm_hour;
    TimeStamp.Flag    = 1;
    TimeStamp.Day     = utc_tm.tm_mday;
    TimeStamp.Month   = utc_tm.tm_mon + 1;
    TimeStamp.Year    = utc_tm.tm_year;
    TimeStamp.Century = 20 + utc_tm.tm_year/100;
    TimeStamp.Year    = TimeStamp.Year % 100;
}

uint32_t ppr_Runtime::updatePPRStatustoDBus() {

	ERROR_TIME_STAMP t;
	char timestamp[30];

	calculate_time_stamp(t);
	sprintf(timestamp, "%d-%d-%dT%d:%d:%dZ", (t.Century - 1) * 100 + t.Year,
			t.Month, t.Day, t.Hours, t.Minutes, t.Seconds);

	statusIface = objServer_.add_interface(pprPath.data(),
			pprStatusInterface.data());
	statusIface->register_property("Entry Id", m_pprIndex);

	statusIface->register_property("Timestamp", std::string { timestamp });
	//statusIface->register_property("PPRDATA_OUT", m_pprStatus);
	statusIface->initialize();

	//statusInterfaces.push_back(m_pprIndex, statusIface);

	return 0;

}

