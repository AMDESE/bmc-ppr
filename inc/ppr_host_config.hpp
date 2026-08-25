#pragma once

#include <cstdint>
#include <string>

static constexpr auto kPprServiceBase = "xyz.openbmc_project.PostPackageRepair";
static constexpr auto kPprObjectPath = "/xyz/openbmc_project/PostPackageRepair";

struct PprHostConfig
{
    int hostId = 0;
    uint16_t socNum = 0;
    std::string bmcDev;
    std::string indexFile;
    std::string btJsonFile;
    std::string dbusServiceName;
    std::string dbusObjectPath;
    std::string runtimeConfigFile;
    void* baseAddr = nullptr;
};

extern PprHostConfig g_hostConfig;

int loadHostConfig(int hostId);

void* initHostSharedMem(const std::string& bmcDev);

std::string getRuntimeConfigPath(int hostId);
