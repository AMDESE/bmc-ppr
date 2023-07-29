#include "ppr_Runtime.hpp"

int main() {
	int index;
	std::vector<struct PPR_Data_Status> pprDataOut;

	boost::asio::io_context ioc;
	boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);

	signals.async_wait([&ioc](const boost::system::error_code&, const int&) {
		ioc.stop();
	});

	auto bus = std::make_shared<sdbusplus::asio::connection>(ioc);
	auto objServer = std::make_unique<sdbusplus::asio::object_server>(bus);

	bus->request_name(pprService.data());

	ppr_Runtime *ppr_runtimeObj = new ppr_Runtime(ioc, *bus, *objServer);

	//blocking call
	//Wait for PPR Data In
	index = 0;
	ppr_runtimeObj->syncIndex(index);

	ppr_runtimeObj->startRuntimeRepair();
	ppr_runtimeObj->getRuntimeRepairStatus();
	ppr_runtimeObj->updatePPRStatustoDBus();

	ioc.run();
}
