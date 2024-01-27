#include "bt_ppr.hpp"
#include "rt_ppr.hpp"

#include <thread>

static boost::asio::io_service io;
std::shared_ptr<sdbusplus::asio::connection> conn;
void* base_addr;

void CreatePprDir()
{
    int dir;
    struct stat buffer;

    if (stat(kPprDir.data(), &buffer) != 0)
    {
        // PPR Dir does not exist, create it
        dir = mkdir(kPprDir.data(), 0777);
        if (dir != 0)
        {
            sd_journal_print(LOG_ERR, "PPR directory not created\n");
        }
        else
        {
            sd_journal_print(LOG_ERR, "New PPR directory was created\n");
        }
    }
}

// Initialize Shared Memory device
void InitHostSharedMem()
{
    sd_journal_print(LOG_DEBUG, "InitHostSharedMem Start  \n");
    base_addr = NULL;
    size_t sdev = (MEMBAR_BUFFER_LENGTH * MEMBAR_BUFFER_LENGTH);
    int mfd;
    struct stat sb;

    try
    {
        if (stat(BMC_DEV, &sb) == 0)
        {
            mfd = open(BMC_DEV, O_RDWR | O_SYNC);
            base_addr =
                mmap(0, sdev, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);
            if (base_addr == MAP_FAILED)
            {
                sd_journal_print(LOG_ERR, "InitHostSharedMem: Error Buffer map failed  \n");
            }
            else
            {
                base_addr = (UINT8*)base_addr + PPR_SM_OFFSET;
            }
            close(mfd);
        }
        else
        {
            sd_journal_print(LOG_ERR, "InitHostSharedMem: Error BMC Device does not exist \n");
        }
    }
    catch (std::exception& e)
    {
        sd_journal_print(LOG_ERR, "InitHostSharedMem: Error getting membar value for: %s \n",
                         e.what());
    }

    sd_journal_print(LOG_INFO, "InitHostSharedMem End with Base Addr = %p \n",
                     base_addr);
    return;
}

template <typename T>
T GetProperty(sdbusplus::bus::bus& bus, const char* service, const char* path,
              const char* interface, const char* propertyName)
{
    std::string ret = "ERROR";
    auto method = bus.new_method_call(service, path,
                                      "org.freedesktop.DBus.Properties", "Get");
    method.append(interface, propertyName);
    std::variant<T> value{};
    try
    {
        auto reply = bus.call(method);
        reply.read(value);
        ret = std::get<T>(value);
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        sd_journal_print(LOG_ERR, "GetProperty call failed \n");
    }
    return ret;
}

void ReadHostQueue2()
{
    BootTimePprData* btPpr = NULL;
#ifdef BMC_DEV_DOORBELL
    int fd = -1;
    char buff[Q2_READ_CNT];
    ssize_t ret;

    while (1)
    {
        sd_journal_print(LOG_INFO, " ReadHostQueue2: Start \n");
        fd = open(index_file, O_RDWR, 0);
        if (fd < 0)
        {
            sd_journal_print(LOG_ERR,
                             " ReadHostQueue2: Failed to Open Que 2\n");
            close(fd);
            exit(-1);
        }
        ret = read(fd, buff, Q2_READ_CNT);
        if (ret < 0)
        {
            sd_journal_print(
                LOG_ERR, " ReadHostQueue2: Read Failed with return %d\n", ret);
        }
        else
        {
            sd_journal_print(LOG_INFO,
                             " ReadHostQueue2:Read return (%d) Data 0x%x "
                             "0x%x 0x%x 0x%x \n",
                             ret, buff[0], buff[1], buff[2], buff[3]);
            if ((buff[0] == Q2_BIOS_SIG) && (buff[1] == Q2_BIOS_SIG) &&
                (buff[2] == Q2_BIOS_SIG) && (buff[3] == Q2_BIOS_SIG))
            {
                if (btPpr != NULL)
                    delete btPpr;
                btPpr = new BootTimePprData();
            }
        }
        close(fd);
        sleep(1);
    } // end of while
#else
    std::string CurrentHostState;
    sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    while (1)
    {
        CurrentHostState = GetProperty<std::string>(
            bus, "xyz.openbmc_project.State.Host",
            "/xyz/openbmc_project/state/host0",
            "xyz.openbmc_project.State.Host", "CurrentHostState");

        if (CurrentHostState.compare("ERROR") != 0)
        {
            if (CurrentHostState.compare(
                    "xyz.openbmc_project.State.Host.HostState.Off") == 0)
                sd_journal_print(LOG_DEBUG,
                                 " ReadHostQueue2: System State is Off \n");
            else
            {
                sd_journal_print(
                    LOG_INFO,
                    " ReadHostQueue2: System State is On, start BT Poll \n");
                if (btPpr != NULL)
                    delete btPpr;
                btPpr = new BootTimePprData();
            }
        }
        // sleep for 30 sec
        sleep(OFF_STATE_SLEEP_SEC);
    } // end of while
#endif
}

int main()
{
    // connect to dbus
    conn = std::make_shared<sdbusplus::asio::connection>(io);

    commonPPR* globalBT = globalBT->getInstance();

    int ret = 0;

    sd_event* event = nullptr;

    CreatePprDir();

    InitHostSharedMem();

    ret = sd_event_default(&event);
    if (ret < 0)
    {
        sd_journal_print(LOG_ERR, "Error creating a default sd_event handler");
        return ret;
    }

    // events
    EventPtr eventP{event};
    event = nullptr;

    sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    sdbusplus::server::manager_t m{bus, DBUS_OBJECT_NAME};

    bus.request_name(DBUS_SERVICE_NAME);

    childPprData RtPprData{bus, DBUS_OBJECT_NAME, eventP};

    sd_journal_print(LOG_DEBUG, "Created PPR Data Object \n");

    std::thread h2bQ1Thread(ReadHostQueue2);
    h2bQ1Thread.detach();

    try
    {
        bus.attach_event(eventP.get(), SD_EVENT_PRIORITY_NORMAL);
        ret = sd_event_loop(eventP.get());
        if (ret < 0)
        {
            sd_journal_print(LOG_ERR,
                             "Error occurred during the sd_event_loop, RET=%d",
                             ret);
        }
    }
    catch (std::exception& e)
    {
        sd_journal_print(LOG_ERR, "Error:%s", e.what());
        return -1;
    }
    return 0;
}
