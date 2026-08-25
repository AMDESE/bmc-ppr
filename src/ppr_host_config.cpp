#include "ppr_host_config.hpp"

#include "bt_ppr.hpp"

#include <cerrno>
#include <fstream>
#include <systemd/sd-journal.h>
#include <nlohmann/json.hpp>
#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>

PprHostConfig g_hostConfig;

static const std::string kConfigFilePath = "/usr/share/amd-ppr/ppr-config.json";

static const std::string kDefaultBmcDev0 = "/dev/bmc-device0";
static const std::string kDefaultBmcDev1 = "/dev/bmc-device1";
static const std::string kDefaultIndexFile0 =
    "/sys/devices/platform/soc@10000000/12110000.bmc-dev/bmc-dev-queue2";
static const std::string kDefaultIndexFile1 =
    "/sys/devices/platform/soc@10000000/12120000.bmc-dev/bmc-dev-queue2";

std::string getRuntimeConfigPath(int hostId)
{
    return "/var/lib/amd-ppr/config" + std::to_string(hostId) + ".json";
}

static void applyHostDefaults(int hostId)
{
    g_hostConfig.hostId = hostId;
    g_hostConfig.btJsonFile = "BtPPRData" + std::to_string(hostId) + ".json";
    g_hostConfig.runtimeConfigFile = getRuntimeConfigPath(hostId);
    // Each host instance owns a bus name; the object path is common, so
    // clients select a host purely by service name (same as com.amd.RAS).
    g_hostConfig.dbusServiceName =
        std::string(kPprServiceBase) + std::to_string(hostId);
    g_hostConfig.dbusObjectPath = kPprObjectPath;

    switch (hostId)
    {
        case 2:
            g_hostConfig.socNum = 1;
            g_hostConfig.bmcDev = kDefaultBmcDev1;
            g_hostConfig.indexFile = kDefaultIndexFile1;
            break;
        case 1:
            g_hostConfig.socNum = 0;
            g_hostConfig.bmcDev = kDefaultBmcDev0;
            g_hostConfig.indexFile = kDefaultIndexFile0;
            break;
        case 0:
        default:
            g_hostConfig.socNum = 0;
            g_hostConfig.bmcDev = kDefaultBmcDev0;
            g_hostConfig.indexFile = kDefaultIndexFile0;
            break;
    }
}

int loadHostConfig(int hostId)
{
    applyHostDefaults(hostId);

    std::ifstream configFile(kConfigFilePath);
    if (!configFile.is_open())
    {
        sd_journal_print(LOG_INFO,
                         "PPR host %d: config file not present, use defaults "
                         "(bmcDev=%s)\n",
                         hostId, g_hostConfig.bmcDev.c_str());
        return 0;
    }

    try
    {
        auto jsonData =
            nlohmann::json::parse(configFile, nullptr, true, true);
        auto pprConfig = jsonData["ppr_configs"];
        const std::string hostKey = std::to_string(hostId);

        if (pprConfig.contains("hosts") &&
            pprConfig["hosts"].contains(hostKey))
        {
            const auto& hostCfg = pprConfig["hosts"][hostKey];
            if (hostCfg.contains("bmcDev"))
            {
                g_hostConfig.bmcDev = hostCfg["bmcDev"].get<std::string>();
            }
            if (hostCfg.contains("indexFile"))
            {
                g_hostConfig.indexFile =
                    hostCfg["indexFile"].get<std::string>();
            }
            if (hostCfg.contains("socNum"))
            {
                g_hostConfig.socNum = hostCfg["socNum"].get<uint16_t>();
            }
            if (hostCfg.contains("btJsonFile"))
            {
                g_hostConfig.btJsonFile =
                    hostCfg["btJsonFile"].get<std::string>();
            }
        }
        else if (hostId == 0)
        {
            if (pprConfig.contains("bmcDev"))
            {
                g_hostConfig.bmcDev = pprConfig["bmcDev"].get<std::string>();
            }
            if (pprConfig.contains("indexFile"))
            {
                g_hostConfig.indexFile =
                    pprConfig["indexFile"].get<std::string>();
            }
        }

        sd_journal_print(LOG_INFO,
                         "PPR host %d: bmcDev=%s socNum=%u btJson=%s\n",
                         hostId, g_hostConfig.bmcDev.c_str(),
                         g_hostConfig.socNum, g_hostConfig.btJsonFile.c_str());
    }
    catch (const std::exception& e)
    {
        sd_journal_print(LOG_ERR, "PPR host %d: config parse failed: %s\n",
                         hostId, e.what());
        return -1;
    }

    return 0;
}

void* initHostSharedMem(const std::string& bmcDev)
{
    void* mapped = nullptr;
    size_t sdev = (MEMBAR_BUFFER_LENGTH * MEMBAR_BUFFER_LENGTH);
    struct stat sb;

    if (stat(bmcDev.c_str(), &sb) != 0)
    {
        sd_journal_print(LOG_ERR,
                         "InitHostSharedMem: BMC device %s does not exist\n",
                         bmcDev.c_str());
        return nullptr;
    }

    int mfd = open(bmcDev.c_str(), O_RDWR | O_SYNC);
    if (mfd < 0)
    {
        sd_journal_print(LOG_ERR,
                         "InitHostSharedMem: failed to open %s errno=%d\n",
                         bmcDev.c_str(), errno);
        return nullptr;
    }

    mapped = mmap(0, sdev, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);
    close(mfd);

    if (mapped == MAP_FAILED)
    {
        sd_journal_print(LOG_ERR, "InitHostSharedMem: mmap failed for %s\n",
                         bmcDev.c_str());
        return nullptr;
    }

    return static_cast<uint8_t*>(mapped) + PPR_SM_OFFSET;
}
