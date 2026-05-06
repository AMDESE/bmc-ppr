// Copyright (c) 2023 AMD Inc.
// SPDX-License-Identifier: Apache-2.0
//
// RtPprManager â€” Runtime Soft PPR for bmc-ppr
//
// Watches RAS_WATCH_DIR for *_rtppr.json files written by amd-bmc-ras,
// sends repair data to the CPU via APML mailbox, polls for the result,
// writes *_rtppr_status.json, and emits a Redfish EventLog entry.

#include "rt_ppr.hpp"

using json = nlohmann::json;

RtPprManager::RtPprManager(const std::string& watchDir,
                            const std::string& configFile) :
    m_watchDir(watchDir)
{
    // Normalise: strip trailing slash
    while (m_watchDir.size() > 1 && m_watchDir.back() == '/')
        m_watchDir.pop_back();
    loadConfig(configFile);
}

bool RtPprManager::loadConfig(const std::string& configFile)
{
    std::ifstream f(configFile);
    if (!f.is_open())
    {
        lg2::warning("RtPprManager: cannot open config {FILE}, using defaults",
                     "FILE", configFile);
        return false;
    }
    try
    {
        json cfg                  = json::parse(f);
        m_maxRetries              = cfg.value("MaxRetries",          m_maxRetries);
        m_retryDelayUs            = cfg.value("RetryDelayUs",        m_retryDelayUs);
        m_statusPollTimeoutMs     = cfg.value("StatusPollTimeoutMs", m_statusPollTimeoutMs);
    }
    catch (const json::exception& e)
    {
        lg2::warning("RtPprManager: failed to parse config {FILE}: {MSG}",
                     "FILE", configFile, "MSG", e.what());
        return false;
    }
    lg2::info("RtPprManager: config loaded MaxRetries={R} RetryDelayUs={D} "
              "StatusPollTimeoutMs={T}",
              "R", m_maxRetries, "D", m_retryDelayUs, "T",
              m_statusPollTimeoutMs);
    return true;
}

void RtPprManager::run()
{
    scanExistingFiles();

    int inotifyFd = inotify_init1(IN_CLOEXEC);
    if (inotifyFd < 0)
    {
        throw std::runtime_error(
            std::string("RtPprManager: inotify_init1 failed: ") + strerror(errno));
    }

    std::error_code ec;
    fs::create_directories(m_watchDir, ec);

    int wd = inotify_add_watch(inotifyFd, m_watchDir.c_str(), IN_CLOSE_WRITE);
    if (wd < 0)
    {
        close(inotifyFd);
        throw std::runtime_error(
            std::string("RtPprManager: inotify_add_watch failed on ") +
            m_watchDir + ": " + strerror(errno));
    }

    lg2::info("RtPprManager: watching {DIR} for *_rtppr.json files",
              "DIR", m_watchDir);

    constexpr size_t BUF_SIZE = 4096;
    char buf[BUF_SIZE]
        __attribute__((aligned(__alignof__(struct inotify_event))));

    while (true)
    {
        ssize_t len = read(inotifyFd, buf, BUF_SIZE);
        if (len < 0)
        {
            if (errno == EINTR)
                continue;
            throw std::runtime_error(
                std::string("RtPprManager: inotify read failed: ") + strerror(errno));
        }

        for (char* p = buf; p < buf + len;)
        {
            auto* ev = reinterpret_cast<struct inotify_event*>(p);
            if ((ev->len > 0) && (ev->mask & IN_CLOSE_WRITE))
            {
                std::string name{ev->name};
                if (name.size() > 11 &&
                    name.compare(name.size() - 11, 11, "_rtppr.json") == 0)
                {
                    processFile(fs::path(m_watchDir) / name);
                }
            }
            p += sizeof(struct inotify_event) + ev->len;
        }
    }
}

void RtPprManager::scanExistingFiles()
{
    std::error_code ec;
    if (!fs::exists(m_watchDir, ec))
    {
        lg2::info("RtPprManager: watch dir {DIR} does not exist yet, skipping scan",
                  "DIR", m_watchDir);
        return;
    }

    for (const auto& entry : fs::directory_iterator(m_watchDir, ec))
    {
        const std::string name = entry.path().filename().string();
        if (name.size() <= 11 ||
            name.compare(name.size() - 11, 11, "_rtppr.json") != 0)
            continue;

        fs::path statusPath =
            entry.path().parent_path() /
            (name.substr(0, name.size() - 5) + "_status.json");

        if (fs::exists(statusPath))
        {
            lg2::info("RtPprManager: skipping already-processed {FILE}",
                      "FILE", name);
            continue;
        }

        lg2::info("RtPprManager: processing unhandled file on startup: {FILE}",
                  "FILE", name);
        processFile(entry.path());
    }
}

void RtPprManager::processFile(const fs::path& rtpprPath)
{
    lg2::info("[RtPPR] BEGIN processFile: {FILE}",
              "FILE", rtpprPath.filename().string());

    std::vector<RtPprEntry> entries;
    if (!parseRtPprJson(rtpprPath, entries))
    {
        lg2::error("[RtPPR] Failed to parse {FILE}, skipping",
                   "FILE", rtpprPath.string());
        return;
    }

    if (!sendRepairData(entries))
    {
        lg2::error("[RtPPR] sendRepairData failed for {FILE} â€” writing status anyway",
                   "FILE", rtpprPath.filename().string());
    }

    pollRepairStatus(entries);
    writeStatusFile(rtpprPath, entries);

    lg2::info("[RtPPR] END processFile: {FILE}",
              "FILE", rtpprPath.filename().string());
}

bool RtPprManager::parseRtPprJson(const fs::path& path,
                                   std::vector<RtPprEntry>& entries)
{
    std::ifstream f(path);
    if (!f.is_open())
    {
        lg2::error("RtPprManager: cannot open {FILE}", "FILE", path.string());
        return false;
    }

    json j;
    try { j = json::parse(f); }
    catch (const json::exception& e)
    {
        lg2::error("RtPprManager: JSON parse error in {FILE}: {MSG}",
                   "FILE", path.string(), "MSG", e.what());
        return false;
    }

    if (!j.contains("pprDataIn") || !j["pprDataIn"].is_array())
    {
        lg2::error("RtPprManager: no 'pprDataIn' array in {FILE}",
                   "FILE", path.string());
        return false;
    }

    for (const auto& item : j["pprDataIn"])
    {
        RtPprEntry e;
        e.repairEntryNum = item.value("RepairEntryNum", 0u);
        e.repairType     = item.value("RepairType",     0u);
        e.socNum         = item.value("SocNum",         0u);

        if (!item.contains("Payload") || !item["Payload"].is_array())
        {
            lg2::error("RtPprManager: entry missing Payload in {FILE} â€” skipping",
                       "FILE", path.string());
            continue;
        }
        const auto& pl = item["Payload"];
        for (int i = 0; i < PAYLOAD_SIZE && i < static_cast<int>(pl.size()); ++i)
            e.payload[i] = static_cast<uint16_t>(pl[i].get<uint32_t>());

        entries.push_back(e);
    }

    lg2::info("RtPprManager: parsed {N} PPR entries from {FILE}",
              "N", entries.size(), "FILE", path.filename().string());
    return !entries.empty();
}

bool RtPprManager::sendRepairData(std::vector<RtPprEntry>& entries)
{
    const size_t totalEntries = entries.size();

    lg2::info("[RtPPR] sendRepairData: {N} entries", "N", totalEntries);

    for (size_t ei = 0; ei < totalEntries; ++ei)
    {
        RtPprEntry& entry  = entries[ei];
        bool        isLast = (ei == totalEntries - 1);

        lg2::info("[RtPPR] Entry[{EI}] soc={S} repairType={T} "
                  "payload=[{P0},{P1},{P2},{P3},{P4},{P5},{P6},{P7},{P8},{P9}]",
                  "EI", ei, "S", entry.socNum, "T", entry.repairType,
                  "P0", entry.payload[0], "P1", entry.payload[1],
                  "P2", entry.payload[2], "P3", entry.payload[3],
                  "P4", entry.payload[4], "P5", entry.payload[5],
                  "P6", entry.payload[6], "P7", entry.payload[7],
                  "P8", entry.payload[8], "P9", entry.payload[9]);

        for (int offset = 0; offset < PAYLOAD_SIZE; ++offset)
        {
            struct set_ras_action_data_in Data{};
            Data.payload.repair_entry_num =
                static_cast<uint8_t>(entry.repairEntryNum);
            Data.payload.offset   = static_cast<uint8_t>(offset * 2);
            Data.payload.pay_load = entry.payload[offset];
            Data.ras_act_id       = RAS_ACTION_ID_RUNTIME_PPR;
            Data.eom_flag = (isLast && (offset == PAYLOAD_SIZE - 1)) ? 1 : 0;

            uint32_t     status  = 0;
            oob_status_t ret     = OOB_MAILBOX_ERR_END;
            int          retries = m_maxRetries;

            while (retries > 0)
            {
                ret = set_bmc_ras_action_status(
                    static_cast<uint8_t>(entry.socNum), Data, &status);
                if (ret == OOB_SUCCESS)
                {
                    lg2::info("[RtPPR] set_bmc_ras_action_status: soc={S} "
                              "entry={E} offset={O} status=0x{ST}",
                              "S", entry.socNum, "E", entry.repairEntryNum,
                              "O", offset, "ST", lg2::hex, status);
                    break;
                }
                --retries;
                lg2::warning("[RtPPR] set_bmc_ras_action_status failed: "
                             "soc={S} entry={E} offset={O} ret={R} left={RL}",
                             "S", entry.socNum, "E", entry.repairEntryNum,
                             "O", offset, "R", ret, "RL", retries);
                usleep(static_cast<useconds_t>(m_retryDelayUs));
            }

            if (ret != OOB_SUCCESS)
            {
                lg2::error("[RtPPR] set_bmc_ras_action_status exhausted retries: "
                           "soc={S} entry={E} offset={O}",
                           "S", entry.socNum, "E", entry.repairEntryNum,
                           "O", offset);
                return false;
            }
        }
    }

    lg2::info("[RtPPR] sendRepairData: all {N} entries sent", "N", totalEntries);
    return true;
}

bool RtPprManager::pollRepairStatus(std::vector<RtPprEntry>& entries)
{
    using clock = std::chrono::steady_clock;
    const auto deadline =
        clock::now() + std::chrono::milliseconds(m_statusPollTimeoutMs);

    for (auto& entry : entries)
    {
        struct get_ras_action_data_in Data{};
        Data.pay_load.repair_entry_num =
            static_cast<uint8_t>(entry.repairEntryNum);
        Data.ras_action_id = RAS_ACTION_ID_RUNTIME_PPR;

        bool done        = false;
        int  pollAttempt = 0;

        lg2::info("[RtPPR] pollRepairStatus: soc={S} entry={E} timeout={T}ms",
                  "S", entry.socNum, "E", entry.repairEntryNum,
                  "T", m_statusPollTimeoutMs);

        while (clock::now() < deadline)
        {
            struct ras_action_status status{};
            oob_status_t ret = get_bmc_ras_action_status(
                static_cast<uint8_t>(entry.socNum), Data, &status);
            ++pollAttempt;

            if (ret != OOB_SUCCESS)
            {
                lg2::warning("[RtPPR] get_bmc_ras_action_status failed: "
                             "soc={S} entry={E} ret={R} attempt={A}",
                             "S", entry.socNum, "E", entry.repairEntryNum,
                             "R", ret, "A", pollAttempt);
                usleep(static_cast<useconds_t>(m_retryDelayUs));
                continue;
            }

            lg2::info("[RtPPR] poll attempt={A}: soc={S} entry={E} "
                      "raw_result=0x{R}",
                      "A", pollAttempt, "S", entry.socNum,
                      "E", entry.repairEntryNum,
                      "R", lg2::hex,
                      static_cast<uint32_t>(status.repair_result));

            if (status.repair_result !=
                static_cast<uint16_t>(PPR_STATUS_REPAIR_NOT_PROCESSED))
            {
                entry.repairResult = status.repair_result;
                lg2::info("[RtPPR] result after {A} poll(s): soc={S} "
                          "entry={E} result=0x{R}",
                          "A", pollAttempt, "S", entry.socNum,
                          "E", entry.repairEntryNum,
                          "R", lg2::hex,
                          static_cast<uint32_t>(status.repair_result));
                done = true;
                break;
            }

            usleep(static_cast<useconds_t>(m_retryDelayUs));
        }

        if (!done)
        {
            lg2::error("[RtPPR] timed out waiting for PPR status: "
                       "soc={S} entry={E}",
                       "S", entry.socNum, "E", entry.repairEntryNum);
        }
    }

    return true;
}

void RtPprManager::writeStatusFile(const fs::path&              rtpprPath,
                                    const std::vector<RtPprEntry>& entries)
{
    const std::string origName  = rtpprPath.filename().string();
    const std::string statusName =
        origName.substr(0, origName.size() - 5) + "_status.json";
    const fs::path statusPath = rtpprPath.parent_path() / statusName;

    json j;
    j["pprStatusOut"] = json::array();

    for (const auto& e : entries)
    {
        json item;
        item["RepairEntryNum"]  = e.repairEntryNum;
        item["RepairType"]      = e.repairType;
        item["SocNum"]          = e.socNum;
        item["RepairResult"]    = e.repairResult;
        item["RepairResultStr"] =
            (e.repairResult == PPR_STATUS_REPAIR_PASS)           ? "PASS"
            : (e.repairResult == PPR_STATUS_REPAIR_FAIL)          ? "FAIL"
            : (e.repairResult ==
               static_cast<uint16_t>(PPR_STATUS_REPAIR_NOT_PROCESSED)) ? "NOT_PROCESSED"
            : "UNKNOWN";
        j["pprStatusOut"].push_back(item);
    }

    std::ofstream f(statusPath);
    if (!f.is_open())
    {
        lg2::error("RtPprManager: cannot write status file {FILE}",
                   "FILE", statusPath.string());
        return;
    }
    f << j.dump(4) << '\n';
    lg2::info("RtPprManager: wrote status file {FILE}",
              "FILE", statusPath.string());
}

