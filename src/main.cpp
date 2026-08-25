#include "bt_ppr.hpp"
#include "ppr_host_config.hpp"
#include "rt_ppr.hpp"

#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include <thread>
#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>

static void createRuntimeConfigFile(int hostId)
{
    struct stat buffer;
    const std::string path = getRuntimeConfigPath(hostId);

    if (stat(path.c_str(), &buffer) != 0)
    {
        sd_journal_print(LOG_INFO, "PPR host %d: created runtime config %s\n",
                         hostId, path.c_str());
        nlohmann::json jsonConfig = {{"oobPprEnable", false},
                                     {"RtToBt", true},
                                     {"BtSetToHard", false}};

        std::ofstream jsonWrite(path);
        jsonWrite << jsonConfig;
    }
}

static void createPprDir()
{
    struct stat buffer;

    if (stat(kPprDir.data(), &buffer) != 0)
    {
        if (mkdir(kPprDir.data(), 0777) != 0)
        {
            sd_journal_print(LOG_ERR, "PPR directory not created\n");
        }
        else
        {
            sd_journal_print(LOG_INFO, "New PPR directory created\n");
        }
    }
}

static void readHostQueue2(void* hostBaseAddr, const std::string& queue2Path)
{
    BootTimePprData* btPpr = nullptr;
#ifdef BMC_DEV_IRQ
    int fd = -1;
    char buff[Q2_READ_CNT];
    ssize_t ret;

    while (1)
    {
        fd = open(queue2Path.c_str(), O_RDWR, 0);
        if (fd < 0)
        {
            sd_journal_print(LOG_ERR,
                             "ReadHostQueue2 host %d: failed to open %s\n",
                             g_hostConfig.hostId, queue2Path.c_str());
            sleep(TSLEEP);
            continue;
        }

        ret = read(fd, buff, Q2_READ_CNT);
        if (ret >= 0 && (buff[0] == Q2_BIOS_SIG) && (buff[1] == Q2_BIOS_SIG) &&
            (buff[2] == Q2_BIOS_SIG) && (buff[3] == Q2_BIOS_SIG))
        {
            if (btPpr != nullptr)
            {
                delete btPpr;
            }
            btPpr = new BootTimePprData(hostBaseAddr);
        }
        close(fd);
        sleep(1);
    }
#else
    (void)queue2Path;
    if (btPpr == nullptr)
    {
        sd_journal_print(LOG_INFO, "ReadHostQueue2 host %d: start BT poll\n",
                         g_hostConfig.hostId);
        btPpr = new BootTimePprData(hostBaseAddr);
    }
#endif
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        sd_journal_print(LOG_ERR, "amd-bmc-ppr: host instance id required\n");
        return EXIT_FAILURE;
    }

    const int hostId = std::atoi(argv[1]);
    if (hostId < 0 || hostId > 2)
    {
        sd_journal_print(LOG_ERR, "amd-bmc-ppr: invalid host id %d\n", hostId);
        return EXIT_FAILURE;
    }

    if (loadHostConfig(hostId) < 0)
    {
        return EXIT_FAILURE;
    }

    commonPPR* globalBT = commonPPR::getInstance();
    globalBT->initForHost(hostId, g_hostConfig.socNum, g_hostConfig.btJsonFile,
                          g_hostConfig.runtimeConfigFile);

    createPprDir();
    createRuntimeConfigFile(hostId);

    g_hostConfig.baseAddr = initHostSharedMem(g_hostConfig.bmcDev);
    sd_journal_print(LOG_INFO, "PPR host %d: shared mem base %p\n", hostId,
                     g_hostConfig.baseAddr);

    sd_event* event = nullptr;
    int ret = sd_event_default(&event);
    if (ret < 0)
    {
        sd_journal_print(LOG_ERR, "Error creating sd_event handler");
        return ret;
    }

    EventPtr eventP{event};
    event = nullptr;

    sdbusplus::bus::bus bus = sdbusplus::bus::new_default();
    sdbusplus::server::manager_t m{bus, g_hostConfig.dbusObjectPath.c_str()};

    bus.request_name(g_hostConfig.dbusServiceName.c_str());

    childPprData rtPprData{bus, g_hostConfig.dbusObjectPath.c_str(), eventP};

    sd_journal_print(LOG_INFO, "PPR host %d: D-Bus %s %s\n", hostId,
                     g_hostConfig.dbusServiceName.c_str(),
                     g_hostConfig.dbusObjectPath.c_str());

    std::thread h2bQ2Thread(readHostQueue2, g_hostConfig.baseAddr,
                            g_hostConfig.indexFile);
    h2bQ2Thread.detach();

    try
    {
        bus.attach_event(eventP.get(), SD_EVENT_PRIORITY_NORMAL);
        ret = sd_event_loop(eventP.get());
        if (ret < 0)
        {
            sd_journal_print(LOG_ERR,
                             "Error occurred during sd_event_loop, RET=%d",
                             ret);
        }
    }
    catch (const std::exception& e)
    {
        sd_journal_print(LOG_ERR, "Error:%s", e.what());
        return EXIT_FAILURE;
    }

    return 0;
}
