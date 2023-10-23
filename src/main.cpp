#include "rt_ppr.hpp"
#include "bt_ppr.hpp"

void CreatePprDir()
{
        int dir;
        struct stat buffer;

        if (stat(kPprDir.data(), &buffer) != 0) {
                //PPR Dir does not exist, create it
                dir = mkdir(kPprDir.data(), 0777);
                if(dir != 0) {
                        sd_journal_print(LOG_ERR, "PPR directory not created\n");
                }
                else {
                        sd_journal_print(LOG_ERR, "New PPR directory was created\n");
                }
        }
}

int main() {

	//event
	BootTimePprDataHolder* bootTimePprDataHolderObj =
			bootTimePprDataHolderObj->getInstance();

	int ret = 0;

	sd_event *event = nullptr;

	CreatePprDir();

	ret = sd_event_default(&event);
	if (ret < 0) {
		sd_journal_print(LOG_ERR, "Error creating a default sd_event handler");
		return ret;
	}

	EventPtr eventQ { event };
	EventPtr1 eventP { event };
	event = nullptr;


	sdbusplus::bus::bus bus = sdbusplus::bus::new_default();
	sdbusplus::server::manager_t m { bus, DBUS_OBJECT_NAME };

	bus.request_name(DBUS_SERVICE_NAME);

	childPprData PprData { bus, DBUS_OBJECT_NAME, eventQ };
	BootTimePprData IodeviceInfo{bus, DBUS_OBJECT_NAME, eventP};

	sd_journal_print(LOG_DEBUG, "Created PPR Data Object \n");

	try {
		bus.attach_event(eventP.get(), SD_EVENT_PRIORITY_NORMAL);
		ret = sd_event_loop(eventP.get());
		if (ret < 0) {
			sd_journal_print(LOG_ERR,
					"Error occurred during the sd_event_loop, RET=%d", ret);
		}
		bus.attach_event(eventQ.get(), SD_EVENT_PRIORITY_NORMAL);
		ret = sd_event_loop(eventQ.get());
		if (ret < 0) {
			sd_journal_print(LOG_ERR,
					"Error occurred during the sd_event_loop, RET=%d", ret);
		}
	} catch (std::exception &e) {
		sd_journal_print(LOG_ERR, "Error:%s", e.what());
		return -1;
	}
	return 0;

}

