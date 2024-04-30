#include "bt_ppr.hpp"
#include "rt_ppr.hpp"

#include <thread>

static boost::asio::io_service io;
std::shared_ptr<sdbusplus::asio::connection> conn;
void* base_addr;

void CreateConfigFile()
{

    struct stat buffer;

    // Create PPR Config file
    if (stat(config_file, &buffer) != 0)
    {
        sd_journal_print(LOG_INFO, "New PPR Config file created\n");
        nlohmann::json jsonConfig = {
            {"oobPprEnable", false},
            {"RtToBt", true},
            {"BtSetToHard", false},
        };

        std::ofstream jsonWrite(config_file);
        jsonWrite << jsonConfig;
        jsonWrite.close();
    }
}

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
            sd_journal_print(LOG_INFO, "New PPR directory created\n");
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
                sd_journal_print(
                    LOG_ERR, "InitHostSharedMem: Error Buffer map failed  \n");
            }
            else
            {
                base_addr = (UINT8*)base_addr + PPR_SM_OFFSET;
            }
            close(mfd);
        }
        else
        {
            sd_journal_print(
                LOG_ERR,
                "InitHostSharedMem: Error BMC Device does not exist \n");
        }
    }
    catch (std::exception& e)
    {
        sd_journal_print(
            LOG_ERR, "InitHostSharedMem: Error getting membar value for: %s \n",
            e.what());
    }

    sd_journal_print(LOG_DEBUG, "InitHostSharedMem End with Base Addr = %p \n",
                     base_addr);
    return;
}

void ReadHostQueue2()
{
    BootTimePprData* btPpr = NULL;
#ifdef BMC_DEV_IRQ
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
    if (btPpr == NULL)
    {
        sd_journal_print(LOG_INFO, " ReadHostQueue2: start BT Poll \n");
        btPpr = new BootTimePprData();
    }
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
    CreateConfigFile();

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

    std::thread h2bQ2Thread(ReadHostQueue2);
    h2bQ2Thread.detach();

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
