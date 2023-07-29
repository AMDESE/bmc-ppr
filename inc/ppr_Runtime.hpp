#pragma once

#include "ppr.hpp"

/** @class ppr_Runtime
 *  @brief Responsible for receiving post package repair data and sending
 *  it via APML.
 */
class ppr_Runtime {
public:
	ppr_Runtime();
	ppr_Runtime(boost::asio::io_context &ioc, sdbusplus::asio::connection &bus,
			sdbusplus::asio::object_server &objServer) :
				ioc_(ioc), bus_(bus), objServer_(objServer) {
		m_pprIndex = 0;
		m_pprDataSize = 0;
		m_pprSupported = isPPRSupported();


		fileIface = objServer_.add_interface(pprPath.data(),
				pprFileInterface.data());

		fileIface->register_property("pprIndex", m_pprIndex);
		/*fileIface->register_property("pprDataIn", m_pprDataPtr,
				[](const std::vector<PPR_Data_In> requested,
						std::vector<PPR_Data_In> resp) {
			resp = requested;
			m_pprDataPtr = resp;
			m_pprDataSize = m_pprDataIn.size();
			// run for loop from 0 to vecSize
			for (unsigned int i = 0; i < m_pprDataSize; i++) {
				sd_journal_print(LOG_INFO,
						"RepairType = 0x%x, RepairEntryNum = 0x%x, SOCNum = 0x%x, Offset = 0x%x, Payload = 0x%x \n",
						m_pprDataIn[i].repairType,
						m_pprDataIn[i].repairEntryNum,
						m_pprDataIn[i].soc_num,
						m_pprDataIn[i].offset,
						m_pprDataIn[i].payload);
			}

			m_pprIndex++;
			syncIndex(m_pprIndex);
			return 1;
		});*/
		fileIface->initialize();

	}

	void syncIndex(int value) {
		m_pprIndex = value;
		fileIface->signal_property("pprIndex");
	}
	~ppr_Runtime() = default;

	uint32_t startRuntimeRepair();
	uint32_t getRuntimeRepairStatus();
	uint32_t updatePPRStatustoDBus();

private:
	boost::asio::io_context &ioc_;
	sdbusplus::asio::connection &bus_;
	sdbusplus::asio::object_server &objServer_;

	std::shared_ptr<sdbusplus::asio::dbus_interface> fileIface;
	std::shared_ptr<sdbusplus::asio::dbus_interface> statusIface;
	//std::vector<std::pair<unsigned int, std::shared_ptr<sdbusplus::asio::dbus_interface>> > statusInterfaces;
	/** @brief CPU supports PPR?  */
	bool m_pprSupported;
	std::shared_ptr<PPR_Data_In> m_pprDataPtr;
	std::vector<PPR_Data_In> m_pprDataIn;
	unsigned int m_pprDataSize;
	unsigned int m_pprIndex;
	std::vector<struct PPR_Data_Status> m_pprStatus;

	bool isPPRSupported();
	void calculate_time_stamp(ERROR_TIME_STAMP& TimeStamp);

};

