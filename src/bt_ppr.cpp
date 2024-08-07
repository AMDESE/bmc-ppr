#include "bt_ppr.hpp"

#include "rt_ppr.hpp"

#include <fcntl.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <boost/asio.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <filesystem>
#include <gpiod.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/property.hpp>

#include "iomanip"

BootTimePprDataHolder* BootTimePprDataHolder::instance = 0;
using namespace std;

bool BootTimePprData::getBiosInData()
{
    UINT32 Signature;
    UINT16 Command;

    sd_journal_print(LOG_DEBUG, "getBiosInData Start");
    BiosInCnt = 0;
    if (base_addr != NULL)
    {
        BiosPprHeader* BiosPprHeader_ptr = new BiosPprHeader();
        BiosPprHeader_ptr = (struct BiosPprHeader*)((UINT8*)base_addr);

        if (BiosPprHeader_ptr != NULL)
        {
            Signature = BiosPprHeader_ptr->Signature;
            BiosInCnt = BiosPprHeader_ptr->EntryCount;
            Command = BiosPprHeader_ptr->Command;
            if (Signature == BOOTTIME_PPR_SIGNATURE)
            {
                sd_journal_print(
                    LOG_DEBUG, "getBiosInData Signature 0x%x Cnt 0x%x Cmd 0x%x",
                    Signature, BiosInCnt, Command);

                if (Command == BT_PPR_CMD_BIOS_CNT)
                {
                    // Enable OOB PPR
                    sd_journal_print(LOG_INFO, "getBiosInData BIOS Signature");
                    globalBT->updateConfigFile(PPR_ENABLE, true);
                    if (BiosInCnt > MAX_REPAIR_SLOTS)
                    {
                        sd_journal_print(LOG_INFO,
                                         "getBiosInData BIOS In Cnt is greater "
                                         "than %d, change it to 0",
                                         MAX_REPAIR_SLOTS);
                        BiosInCnt = 0;
                    }
                    if (BiosInCnt > 0)
                        ReadBootTimePprData(BiosInCnt);
                    WriteHostSharedMem();
                    return true;
                }
                else if (Command == BT_PPR_CMD_BMC_CNT)
                {
                    sd_journal_print(LOG_DEBUG, "getBiosInData BMC Signature");
                }
                else if (Command == BT_PPR_CMD_DISABLE_OOB)
                {
                    // Disable OOB PPR
                    sd_journal_print(LOG_DEBUG,
                                     "getBiosInData Disable PPR OOB");
                    globalBT->updateConfigFile(PPR_ENABLE, false);
                    return true;
                }
            }
        }
    }

    return false;
}

void BootTimePprData::pollSharedMem()
{
#ifdef BMC_DEV_IRQ
    uint32_t retry = 0;
    if (base_addr != NULL)
    {
        while ((false == getBiosInData()) && (retry < MAX_RETRY))
        {
            retry++;
            sleep(TSLEEP);
        }
    }
#else
    if (base_addr != NULL)
    {
        while (1)
        {
            getBiosInData();
            sleep(TSLEEP);
        }
    }
#endif
    else
        sd_journal_print(LOG_ERR, "BMC_DEV Shared Membar is not available \n");
    sd_journal_print(LOG_DEBUG, "pollSharedMem End \n");
}

void BootTimePprData::ReadBootTimePprData(UINT8 entryCount)
{
    if (entryCount > 0)
    {
        UINT8 index = PPR_BT_HEADER_SIZE;
        char buffer[CMD_BUFF_LEN];
        BiosPprData* BiosPprData_ptr = new BiosPprData();

        for (int i = 0; i < (int)entryCount; i++)
        {
            char c = ((unsigned char*)base_addr)[index];
            sprintf(buffer, "%d", c);
            uint8_t type = stoi(buffer);
            if (type == BOOTTIME_PPR_TYPE)
            {
                BiosPprData_ptr =
                    (struct BiosPprData*)((UINT8*)base_addr + index);
                pprBoottimeDataIn[i].repairEntryNum =
                    BiosPprData_ptr->RepairEntryNumber;
                pprBoottimeDataIn[i].repairType = BiosPprData_ptr->RepairType;
                pprBoottimeDataIn[i].socNum = BiosPprData_ptr->SocNum;
                pprBoottimeDataIn[i].repairResult =
                    BiosPprData_ptr->RepairResult;

                for (int j = 0; j < PAYLOAD_SIZE; j++)
                {
                    pprBoottimeDataIn[i].payload[j] =
                        BiosPprData_ptr->Payload[j];
                }
                sd_journal_print(LOG_INFO,
                                 "BIOS In Data Type 0x%x Entry Num 0x%x SOC "
                                 "0x%x Result 0x%x \n",
                                 BiosPprData_ptr->RepairType,
                                 BiosPprData_ptr->RepairEntryNumber,
                                 BiosPprData_ptr->SocNum,
                                 BiosPprData_ptr->RepairResult);
                index = index + PPR_BT_DATA_SIZE;
            }
        } // end of for loop
    }     // end of entry count check
}

UINT8 BootTimePprData::setBiosOutData()
{
    UINT8 ret = 0;
    int i, j;
    uint16_t index;
    std::tuple<uint16_t, uint16_t, uint16_t, uint16_t, std::vector<uint16_t>>
        tup;
    std::vector<uint16_t> vec;

    index = globalBT->getBTindex();
    sd_journal_print(LOG_INFO, "BIOS Out Data: start , BT Index = %d \n",
                     index);
    for (i = 0; i < index; i++)
    {
        tup = globalBT->getBTdata(i);

        pprBoottimeDataOut[i].repairEntryNum = std::get<0>(tup);
        pprBoottimeDataOut[i].repairType = std::get<1>(tup);
        pprBoottimeDataOut[i].socNum = std::get<2>(tup);
        pprBoottimeDataOut[i].repairResult = std::get<3>(tup);
        vec = std::get<std::vector<uint16_t>>(tup);
        for (j = 0; j < PAYLOAD_SIZE; j++)
        {
            pprBoottimeDataOut[i].payload[j] = vec[j];
        }
        sd_journal_print(LOG_INFO,
                         "BIOS Out Data: Index %d  Type 0x%x Entry Num 0x%x "
                         "SOC 0x%x Payload %d \n",
                         i, pprBoottimeDataOut[i].repairType,
                         pprBoottimeDataOut[i].repairEntryNum,
                         pprBoottimeDataOut[i].socNum,
                         pprBoottimeDataOut[i].payload[0]);
        ret++;
    }
    return ret;
}

UINT8 BootTimePprData::compareBiosData()
{
    int i, j, k;
    bool match;
    UINT8 matchCnt = 0;
    UINT8 ret = 0;
    UINT16 payloadMask[PAYLOAD_SIZE] = {0xFFFF, 0xFFFF, 0xFFFF, 0xE3FF, 0x0007,
                                        0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};

    for (i = 0; i < BiosOutCnt; i++)
    {
        MatchIndex[i] = BT_NOT_MATCH;
        for (j = 0; j < BiosInCnt; j++)
        {
            match = true;
            for (k = 0; k < PAYLOAD_SIZE; k++)
            {
                if ((pprBoottimeDataOut[i].payload[k] & payloadMask[k]) !=
                    (pprBoottimeDataIn[j].payload[k] & payloadMask[k]))
                {
                    sd_journal_print(LOG_INFO,
                                     "compareBiosData i %d j %d k %d , data "
                                     "are not the same 0x%x 0x%x \n",
                                     i, j, k, pprBoottimeDataOut[i].payload[k],
                                     pprBoottimeDataIn[j].payload[k]);
                    // break;
                    match = false;
                }
            } // end of for k
            if (match)
            {
                pprBoottimeDataOut[i].repairResult =
                    pprBoottimeDataIn[j].repairResult;
                globalBT->UpdateBtResult(i, pprBoottimeDataIn[j].repairResult);
                matchCnt++;
                MatchIndex[i] = BT_MATCH;
                break;
            }
        } // end of for j
    }     // end of for i
    if (BiosOutCnt >= matchCnt)
        ret = BiosOutCnt - matchCnt;
    sd_journal_print(LOG_INFO,
                     "compareBiosData ret(Out Cnt) = %d , match Cnt = %d\n",
                     ret, matchCnt);
    return ret;
}

void BootTimePprData::WriteHostSharedMem()
{
    UINT8 index = PPR_BT_HEADER_SIZE;
    UINT8 outCnt = 0;
    BiosPprHeader* BiosPprHeader_ptr = new BiosPprHeader();
    BiosPprData* BiosPprData_ptr = new BiosPprData();

    // Get BT PPR Data
    BiosOutCnt = setBiosOutData();
    sd_journal_print(LOG_INFO,
                     "WriteHostSharedMem start , BT count Out = %d , In = %d ",
                     BiosOutCnt, BiosInCnt);
    if (BiosInCnt > 0)
        outCnt = compareBiosData();
    else
        outCnt = BiosOutCnt;
    sd_journal_print(
        LOG_INFO,
        "WriteHostSharedMem start , BT count Out = %d (%d) , In = %d ", outCnt,
        BiosOutCnt, BiosInCnt);

    // Header
    BiosPprHeader_ptr = (struct BiosPprHeader*)((UINT8*)base_addr);

    BiosPprHeader_ptr->Signature = BOOTTIME_PPR_SIGNATURE;
    BiosPprHeader_ptr->Version = BOOTTIME_PPR_VERSION;
    BiosPprHeader_ptr->EntryCount = outCnt;
    BiosPprHeader_ptr->ReportSize =
        PPR_BT_HEADER_SIZE + (PPR_BT_DATA_SIZE * outCnt);
    BiosPprHeader_ptr->Command = BT_PPR_CMD_BMC_CNT;

    // PPR Entry
    if (outCnt > 0)
    {
        for (int i = 0; i < (int)BiosOutCnt; i++)
        {
            if (MatchIndex[i] == BT_NOT_MATCH)
            {
                BiosPprData_ptr =
                    (struct BiosPprData*)((UINT8*)base_addr + index);
                BiosPprData_ptr->Type = BOOTTIME_PPR_TYPE;
                BiosPprData_ptr->Version = BOOTTIME_PPR_VERSION;
                BiosPprData_ptr->Length = PPR_BT_DATA_SIZE;
                BiosPprData_ptr->RepairEntryNumber =
                    pprBoottimeDataOut[i].repairEntryNum;
                BiosPprData_ptr->RepairType = pprBoottimeDataOut[i].repairType;
                BiosPprData_ptr->SocNum = pprBoottimeDataOut[i].socNum;
                for (int j = 0; j < PAYLOAD_SIZE; j++)
                {
                    BiosPprData_ptr->Payload[j] =
                        pprBoottimeDataOut[i].payload[j];
                }

                sd_journal_print(
                    LOG_INFO,
                    "BIOS Out Data Entry Num %d Type %d SOC %d PL0 %d\n",
                    BiosPprData_ptr->RepairEntryNumber,
                    BiosPprData_ptr->RepairType, BiosPprData_ptr->SocNum,
                    BiosPprData_ptr->Payload[0]);
                index = index + PPR_BT_DATA_SIZE;
            }
        } // end of for i
    }     // end of if BiosOutCnt
}
