#include "rt_ppr.hpp"

commonPPR* commonPPR::instance = 0;
uint8_t DIMM_ADDR[MAX_DIMM_SLOT_PER_SOC]={7,3,5,1,6,2,4,0,15,11,13,9,14,10,12,8};

uint16_t childPprData::getTotalPprCount()
{
    return (m_pprRuntimeIndex + globalBT->getBTindex() - m_rtToBtCount);
}

int childPprData::GetDimmSerialNum(uint16_t Socket, uint16_t Ch, uint16_t Chip)
{
    int dimm;
    uint8_t dimmAddr;
    uint32_t dimmData;
    oob_status_t ret_oob;

    dimm = (Ch & DIMM_SN_CH_MASK);
    if (dimm >= MAX_DIMM_SLOT_PER_SOC)
    {
        return MAX_DIMM_SLOT;
    }
    dimmAddr = (uint8_t)(DIMM_ADDR[dimm] + DIMM_SN_MODE_1);

    if (Socket == DIMM_SOCKET_0)
    { // Socket 0
        if (Chip >= DIMM_CHIP_2DPC)
        { // 2 DPC
            dimm = dimm + MAX_DIMM_SLOT_PER_SOC;
            dimmAddr = dimmAddr + DIMM_SN_2DPC;
        }
    }
    else
    { // socket 1
        dimm = dimm + MAX_DIMM_SLOT_PER_SOC;
    }

    // Call APML to get the DIMM SN
    sd_journal_print(LOG_INFO, "GetDimmSerialNum: DIMM = %d Addr = 0x%x \n",
                     dimm, dimmAddr);

    ret_oob = get_dimm_serial_num((uint8_t)Socket, dimmAddr, &dimmData);
    if (ret_oob == OOB_SUCCESS)
    {
        DimmSN0[dimm] = (uint16_t)(dimmData & DIMM_SN_LSB_MASK);
        DimmSN1[dimm] =
            (uint16_t)((dimmData & DIMM_SN_MSB_MASK) >> DIMM_SN_MSB_SHIFT);
        sd_journal_print(LOG_INFO,
                         "GetDimmSerialNum: DIMM SN = 0x%x , %d, %d   \n",
                         dimmData, DimmSN0[dimm], DimmSN1[dimm]);
    }
    else
    {
        sd_journal_print(
            LOG_INFO, "GetDimmSerialNum: Error getting DIMM SN for %d %d  \n",
            dimm, dimmAddr);
        DimmSN0[dimm] = 0;
        DimmSN1[dimm] = 0;
    }

    return dimm;
}

void childPprData::SetBTfromRT(int index)
{
    uint16_t Socket;
    uint16_t Ch;
    uint16_t Chip;
    int dimm;
    int btIndex;
    std::vector<uint16_t> payload;

    Socket = ((m_pprRuntimeData[index].payload[DIMM_SOCKET_PL_NUM] &
               DIMM_SOCKET_MASK) >>
              DIMM_SOCKET_SHIFT);
    Ch = ((m_pprRuntimeData[index].payload[DIMM_CH_PL_NUM] & DIMM_CH_MASK) >>
          DIMM_CH_SHIFT);
    Chip =
        ((m_pprRuntimeData[index].payload[DIMM_CHIP_PL_NUM] & DIMM_CHIP_MASK) >>
         DIMM_CHIP_SHIFT);
    dimm = GetDimmSerialNum(Socket, Ch, Chip);
    sd_journal_print(LOG_INFO, "setBTfromRT: DIMM = %d (%d , %d, %d)  \n", dimm,
                     Socket, Ch, Chip);

    if (globalBT->getBtSetToHard())
    {
        sd_journal_print(LOG_INFO,
                         "setBTfromRT: DIMM = Set BT to Hard Repair  \n");
        m_pprRuntimeData[index].payload[PAYLOAD_4] =
            (m_pprRuntimeData[index].payload[PAYLOAD_4] | BT_SET_TO_HARD_MASK);
    }
    // Copy the 1st 6 Payloads
    for (int i = 0; i < PAYLOAD_6; i++)
        payload.push_back(m_pprRuntimeData[index].payload[i]);

    // Payload 7 and 8 are DIMM SN
    if (dimm >= MAX_DIMM_SLOT)
    {
        sd_journal_print(
            LOG_ERR, "setBTfromRT: Bad DIMM # (%d : %d %d %d) in PPR file  \n",
            dimm, Socket, Ch, Chip);
        payload.push_back((uint16_t)0);
        payload.push_back((uint16_t)0);
    }
    else
    {
        payload.push_back(DimmSN0[dimm]);
        payload.push_back(DimmSN1[dimm]);
        sd_journal_print(LOG_INFO, "setBTfromRT: DIMM %d SN = 0x%x 0x%x\n",
                         dimm, DimmSN0[dimm], DimmSN1[dimm]);
    }
    // Payload 9 and 10 are 0 (Currently DIMM SN is only 4 bytes)
    payload.push_back((uint16_t)0);
    payload.push_back((uint16_t)0);

    btIndex = globalBT->getBTindex();
    sd_journal_print(LOG_INFO, "setBTfromRT: Add Boottime Entry Index = %d \n",
                     btIndex);
    if (getTotalPprCount() >= MAX_REPAIR_SLOTS)
    {
        sd_journal_print(
            LOG_ERR, "SetBTfromRT: No free PPR slots available");
        return;
    }
    globalBT->setBTdata(
        true, RUNTIME, m_pprRuntimeData[index].repairEntryNum,
        (m_pprRuntimeData[index].repairType | PPR_TYPE_BOOTTIME_MASK),
        m_pprRuntimeData[index].socNum, PPR_STATUS_REPAIR_NOT_PROCESSED,
        payload);
    m_rtToBtCount++;
}
void childPprData::deleteAll()
{
    sd_journal_print(
        LOG_ERR,
        "Delete Action not permitted for Post Package Repair Entries \n");
}

bool childPprData::setPostPackageRepairData(uint16_t repairEntryNum,
                                            uint16_t repairType,
                                            uint16_t socNum,
                                            std::vector<uint16_t> payload)
{
    std::vector<uint16_t>::iterator it;
    int i = 0;
    uint16_t index;

    if ((repairType & PPR_TYPE_BOOTTIME_MASK) == 0)
    {
        if (getTotalPprCount() >= MAX_REPAIR_SLOTS)
        {
            sd_journal_print(
                LOG_ERR,
                "setPPRData: (RT) PPR Max request reached. "
                "RuntimeIndex=%u BTIndex=%u RTtoBTcnt=%u Max=%u",
                m_pprRuntimeIndex, globalBT->getBTindex(), m_rtToBtCount, MAX_REPAIR_SLOTS);
            return false;
        }
        if (m_currentRuntimeCnt == 0)
        {
            m_currentRuntimeIndex = m_pprRuntimeIndex;
        }
        m_currentRuntimeCnt++;
        if (m_currentRuntimeCnt > MAX_CURRENT_Runtime_PPR)
        {
            sd_journal_print(
                LOG_INFO,
                "setPPRData: Reach Max Runtime Curr Cnt = %d , Curr Index = %d",
                m_currentRuntimeCnt, m_currentRuntimeIndex);
            return false;
        }
        sd_journal_print(LOG_INFO,
                         "setPPRData: Runtime Curr Cnt = %d , Curr Index = %d  "
                         "Index = %d Entry Num = %d\n",
                         m_currentRuntimeCnt, m_currentRuntimeIndex,
                         m_pprRuntimeIndex, repairEntryNum);
        m_pprRuntimeData[m_pprRuntimeIndex].repairResult =
            PPR_STATUS_REPAIR_NOT_PROCESSED;
        m_pprRuntimeData[m_pprRuntimeIndex].repairEntryNum = repairEntryNum;
        m_pprRuntimeData[m_pprRuntimeIndex].repairType = repairType;
        m_pprRuntimeData[m_pprRuntimeIndex].socNum = socNum;
        for (it = payload.begin(); it != payload.end(); it++)
        {
            m_pprRuntimeData[m_pprRuntimeIndex].payload[i] = *it;
            i++;
        }
        m_pprRuntimeIndex++;
    }
    else
    {
        index = globalBT->getBTindex();
        if (getTotalPprCount() >= MAX_REPAIR_SLOTS)
        {
            sd_journal_print(
                LOG_ERR,
                "setPPRData: (BT) PPR Max request reached. "
                "RuntimeIndex=%u BTIndex=%u Max=%u",
                m_pprRuntimeIndex, globalBT->getBTindex(), MAX_REPAIR_SLOTS);
            return false;
        }
        sd_journal_print(LOG_INFO,
                         "setPPRData: Boottime Entry Num = %d, Index = %d \n",
                         repairEntryNum, index);
        globalBT->setBTdata(true, BOOTTIME, repairEntryNum, repairType, socNum,
                            PPR_STATUS_REPAIR_NOT_PROCESSED, payload);
    }

    return true;
}

bool childPprData::recordAdd(bool value)
{
    if (value == true)
    {
        PprData::currentRepairEntry(m_pprRuntimeIndex);
        sd_journal_print(LOG_INFO,
                         "Record Added. Runtime Curr Cnt = %d , Curr Index = "
                         "%d  Index = %d \n",
                         m_currentRuntimeCnt, m_currentRuntimeIndex,
                         m_pprRuntimeIndex);
        value = false;
    }
    return PprData::recordAdd(value, false);
}

uint16_t childPprData::GetRuntimeIndex(void)
{
    return m_pprRuntimeIndex;
}

std::tuple<uint16_t, uint16_t, uint16_t, uint16_t, std::vector<uint16_t>>
    childPprData::getRuntimeData(uint16_t index, uint16_t slot)
{

    std::tuple<uint16_t, uint16_t, uint16_t, uint16_t, std::vector<uint16_t>>
        tup;

    sd_journal_print(LOG_INFO,
                     "getRuntimeData: - Begin , Index = %d, Slot = %d\n", index,
                     slot);
    std::vector<uint16_t> vec;

    if ((m_pprRuntimeData[index].repairResult ==
         PPR_STATUS_REPAIR_NOT_PROCESSED) &&
        (slot < MAX_REPAIR_SLOTS))
        updateRuntimeRepairStatus(index, slot);

    for (int i = 0; i < PAYLOAD_SIZE; i++)
        vec.push_back(m_pprRuntimeData[index].payload[i]);

    sd_journal_print(LOG_INFO,
                     "getRuntimeData(): Index = %d, repairEntryNum = %d, "
                     "repairType = %d, socNum = %d, repairResult = 0x%x\n",
                     index, m_pprRuntimeData[index].repairEntryNum,
                     m_pprRuntimeData[index].repairType,
                     m_pprRuntimeData[index].socNum,
                     m_pprRuntimeData[index].repairResult);

    tup = std::make_tuple(m_pprRuntimeData[index].repairEntryNum,
                          m_pprRuntimeData[index].repairType,
                          m_pprRuntimeData[index].socNum,
                          m_pprRuntimeData[index].repairResult, vec);

    sd_journal_print(LOG_INFO, "getRuntimeData: - End \n");

    return tup;
}

std::vector<
    std::tuple<uint16_t, uint16_t, uint16_t, uint16_t, std::vector<uint16_t>>>
    childPprData::getPostPackageRepairStatus()
{

    std::vector<std::tuple<uint16_t, uint16_t, uint16_t, uint16_t,
                           std::vector<uint16_t>>>
        Finalvec;
    uint16_t slot = 0;
    uint16_t BTindex;
    sd_journal_print(LOG_INFO,
                     "childPprData::getPostPackageRepairStatus() - Begin \n");

    for (uint16_t index = 0; index < PprData::currentRepairEntry(); index++)
    {
        if (m_pprRuntimeData[index].repairResult ==
            PPR_STATUS_REPAIR_NOT_PROCESSED)
        {
            Finalvec.push_back(getRuntimeData(index, slot));
            slot++;
        }
        else
            Finalvec.push_back(getRuntimeData(index, MAX_REPAIR_SLOTS));
    }
    BTindex = globalBT->getBTindex();
    for (uint16_t index = 0; index < BTindex; index++)
    {
        Finalvec.push_back(globalBT->getBTdata(index));
    }

    return Finalvec;
}

bool childPprData::getConfigParam(uint16_t index)
{
    bool data = false;
    switch (index)
    {
        case OOB_PPR_ENABLE_INDEX:
            data = globalBT->getPprEnableStatus();
            PprData::oobPprEnable(data, false);
            break;
        case RT_TO_BT_INDEX:
            data = globalBT->getRtToBt();
            PprData::rtToBt(data, false);
            break;
        case BT_SET_TO_HARD_INDEX:
            data = globalBT->getBtSetToHard();
            PprData::btSetToHard(data, false);
            break;
        default:
            break;
    }
    return data;
}
std::vector<uint16_t> childPprData::getPostPackageRepairConfig()
{
    std::vector<uint16_t> FinalData = {0, 0, 0};
    bool data;
    uint16_t i;

    sd_journal_print(LOG_INFO,
                     "childPprData::getPostPackageRepairConfig() - Begin \n");

    for (i = 0; i < MAX_PPR_CONFIG_INDEX; i++)
    {
        data = getConfigParam(i);
        if (data)
            FinalData[i] = PPR_CONFIG_TRUE;
    }

    return FinalData;
}

bool childPprData::setPostPackageRepairConfig(uint16_t flag, bool data)
{
    bool ret = true;

    sd_journal_print(LOG_INFO,
                     "setPostPackageRepairConfig() - flag 0x%x data 0x%x \n",
                     flag, data);
    if (flag & RT_TO_BT_MASK)
    {
        globalBT->updateConfigFile(RT_TO_BT, data);
        PprData::rtToBt(data, false);
    }
    else if (flag & BT_SET_TO_HARD_MASK)
    {
        globalBT->updateConfigFile(BT_SET_TO_HARD, data);
        PprData::btSetToHard(data, false);
    }
    else
    {
        ret = false;
    }

    return ret;
}

uint32_t childPprData::startRuntimeRepair(uint16_t repairSlot)
{
    uint8_t offset = 0;
    oob_status_t ret_oob = OOB_MAILBOX_ERR_END;
    struct set_ras_action_data_in Data;
    uint16_t retryCount = MAX_RETRIES;
    uint32_t status;
    uint16_t slot;
    uint16_t soc;

    sd_journal_print(LOG_INFO,
                     "startRuntimeRepair: Slot = %d Runtime Curr Cnt = %d\n",
                     repairSlot, m_currentRuntimeCnt);
    if (repairSlot < m_currentRuntimeCnt)
    {
        slot = repairSlot + m_currentRuntimeIndex;
        sd_journal_print(
            LOG_INFO,
            "startRuntimeRepair: Begin Runtime Repair for Slot = %d\n", slot);
        for (offset = 0; offset < PAYLOAD_SIZE; offset++)
        {
            if (globalBT->getMultiHostState())
                soc = m_pprRuntimeData[slot].socNum;
            else
                soc = 0;
            Data.payload.repair_entry_num = repairSlot;
            Data.payload.offset = offset * 2;
            Data.payload.pay_load = m_pprRuntimeData[slot].payload[offset];
            Data.ras_act_id = RAS_ACTION_ID_RUNTIME_PPR;

            Data.eom_flag = 0;
            if ((offset == (PAYLOAD_SIZE - 1)) &&
                ((m_currentRuntimeCnt - repairSlot) == 1))
                Data.eom_flag = 1;

            ret_oob = OOB_MAILBOX_ERR_END;
            while (retryCount > 0)
            {
                ret_oob = set_bmc_ras_action_status(
                    soc, Data, &status);

                if (ret_oob == OOB_SUCCESS)
                {
                    sd_journal_print(LOG_INFO,
                                     "set_bmc_ras_action_status = 0x%x \n",
                                     status);
                    break;
                }

                retryCount--;
                sd_journal_print(LOG_WARNING,
                                 "Set RAS Action PPR Runtime failed. Repair "
                                 "Slot=%d, RetryCount=%d \n",
                                 repairSlot, retryCount);
            }
        }

        sd_journal_print(LOG_INFO,
                         "childPprData::startRuntimeRepair() - End \n");
    }
    else
    {
        // Boottime PPR
        ret_oob = OOB_SUCCESS;
    }
    return ret_oob;
}

uint32_t childPprData::updateRuntimeRepairStatus(uint16_t index, uint16_t slot)
{

    struct get_ras_action_data_in Data;
    struct ras_action_status status;
    uint16_t retryCount = MAX_RETRIES;
    uint16_t soc;
    oob_status_t ret_oob;

    sd_journal_print(
        LOG_INFO,
        "updateRuntimeRepairStatus: Index = %d Slot = %d Curr Cnt = %d\n",
        index, slot, m_currentRuntimeCnt);
    if (slot < m_currentRuntimeCnt)
    {
        sd_journal_print(
            LOG_INFO, "updateRuntimeRepairStatus: Begin Runtime Slot = %d \n",
            slot);
        Data.pay_load.repair_entry_num = slot;
        Data.ras_action_id = RAS_ACTION_ID_RUNTIME_PPR;

        ret_oob = OOB_MAILBOX_ERR_END;
        if (globalBT->getMultiHostState())
            soc = m_pprRuntimeData[index].socNum;
        else
            soc = 0;
        while (retryCount > 0)
        {
            ret_oob = get_bmc_ras_action_status(soc,
                                                Data, &status);

            if (ret_oob == OOB_SUCCESS)
            {
                m_pprRuntimeData[index].repairResult = status.repair_result;
                sd_journal_print(LOG_INFO,
                                 "get_bmc_ras_action_status Repair Index = %d "
                                 "Slot = %d, Repair Result = 0x%x \n",
                                 index, slot, status.repair_result);
                if ((m_currentRuntimeCnt - slot) == 1)
                    m_currentRuntimeCnt = 0;

                break;
            }
            usleep(200 * 1000);
            retryCount--;
            sd_journal_print(LOG_WARNING,
                             "Get RAS Action PPR Runtime Status failed. Index "
                             "= %d  Slot = %d  RetryCount = %d \n",
                             index, slot, retryCount);
        }

        if (m_pprRuntimeData[index].repairResult == PPR_STATUS_REPAIR_PASS)
        {
            if (globalBT->getRtToBt())
            { // Set BT from RT is enabled
                SetBTfromRT((int)index);
            }
        }

        sd_journal_print(LOG_INFO,
                         "childPprData::updateRuntimeRepairStatus() - End \n");
    }
    else
    { // Boot Time PPR
        ret_oob = OOB_SUCCESS;
    }
    return ret_oob;
}
