#include "bt_ppr.hpp"
#include "rt_ppr.hpp"
#include "iomanip"
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/error.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/property.hpp>
#include <gpiod.hpp>
#include <filesystem>
#include <linux/types.h>
#include <linux/ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_RETRY           250
#define TSLEEP              3

#define index_file  ("/sys/devices/platform/ahb/ahb:apb/1e7e0000.bmc_dev/bmc-dev-queue2")
#define BMC_DEV ("/dev/bmc-device")

BootTimePprDataHolder* BootTimePprDataHolder::instance = 0;
using namespace std;

// write BMC DOne cmd to HOSt Que 2
void BootTimePprData::WriteHostQueue2()
{
	int fd;
	ssize_t ret;
	char buf[4];

	mode_t mode = O_NONBLOCK ;
	fd = open(index_file, O_WRONLY, mode);

	buf[0] = BT_PPR_Q2_BMC_DONE;
	buf[1] = 0x00;
	buf[2] = 0x00;
	buf[3] = 0x00;
	ret = write(fd, buf, 4);
	sd_journal_print(LOG_INFO, "WriteHostQueue2 data 0x%x ret %d fd %d \n", buf[0], ret, fd);
	close(fd);
}

//Read Shared Memory device
bool BootTimePprData::ReadHostSharedMem()
{
	sd_journal_print(LOG_INFO, " ReadHostSharedMem Start  \n");
	bool retvalue = false;
	base_addr = NULL;
	size_t sdev = (MEMBAR_BUFFER_LENGTH * MEMBAR_BUFFER_LENGTH);
	int mfd;
	struct stat sb;

	try
	{
		if (stat(BMC_DEV, &sb) == 0)
		{
			mfd = open(BMC_DEV, O_RDWR | O_SYNC);
			base_addr = mmap(0, sdev, PROT_READ|PROT_WRITE, MAP_SHARED, mfd, 0);
			if (base_addr == MAP_FAILED)
			{
				sd_journal_print(LOG_ERR, "Buffer map failed  \n");
			}
			else
			{
				base_addr = (UINT8 *)base_addr + PPR_SM_OFFSET;
				retvalue = true;
			}
			close(mfd);
		}
		else
		{
			sd_journal_print(LOG_ERR, "BMC Device does not exist \n");
		}
	}
	catch (std::exception& e)
	{
		sd_journal_print(LOG_ERR, "Error getting membar value for: %s \n", e.what());
	}

	sd_journal_print(LOG_INFO, " ReadHostSharedMem End Return %d Base %p \n", retvalue, base_addr);
	return retvalue;
}

bool BootTimePprData::getBiosInData()
{
	UINT32 Signature;
	UINT16 Command;

	sd_journal_print(LOG_INFO, "getBiosInData Start");
	BiosInCnt = 0;
	if(base_addr != NULL)
	{
		BiosPprHeader *BiosPprHeader_ptr = new BiosPprHeader();
		BiosPprHeader_ptr = (struct BiosPprHeader*)((UINT8 *)base_addr);

		if (BiosPprHeader_ptr !=NULL)
		{
			Signature = BiosPprHeader_ptr->Signature;
			BiosInCnt = BiosPprHeader_ptr->EntryCount;
			Command   = BiosPprHeader_ptr->Command;
			sd_journal_print(LOG_INFO, "getBiosInData Signature 0x%x Cnt 0x%x Cmd 0x%x", Signature, BiosInCnt, Command);
			if (Signature == BOOTTIME_PPR_SIGNATURE)
			{
				if (Command == BT_PPR_CMD_BIOS_CNT)
				{
					//TBD Enable PPR OOB
					if (BiosInCnt > MAX_REPAIR_SLOTS)
					{
						sd_journal_print(LOG_INFO, "getBiosInData BIOS In Cnt is greater than %d, change it to 0", MAX_REPAIR_SLOTS);
						BiosInCnt = 0;
					}
					if (BiosInCnt > 0)
						ReadBootTimePprData(BiosInCnt);
					WriteHostSharedMem();
					WriteHostQueue2();
					return true;
				}
				else if (Command == BT_PPR_CMD_BMC_CNT)
				{
					sd_journal_print(LOG_INFO, "getBiosInData BMC Signature");
				}
				else if (Command == BT_PPR_CMD_DISABLE_OOB)
				{
					//TBD disable PPR OOB
					sd_journal_print(LOG_INFO, "getBiosInData Disable PPR OOB");
					return true;
				}
			}
		}
	}

	return false;
}

void BootTimePprData::poolSharedMem()
{
	uint32_t retry = 0;
	bool     ret;
	ret = ReadHostSharedMem();
	if(ret && (base_addr != NULL))
	{
		while((false == getBiosInData()) && (retry < MAX_RETRY))
		{
			sleep(TSLEEP);
			sd_journal_print(LOG_INFO, "Shared Membar is empty, retrying...%d \n", retry);
			retry++;
		}
	}
	else
		sd_journal_print(LOG_ERR, "BMC_DEV Shared Membar is not available \n");
}

//entry point
void BootTimePprData::onHostPwrChange()
{
	poolSharedMem();
}

void BootTimePprData::ReadBootTimePprData(UINT8 entryCount)
{
	if (entryCount > 0 )
	{
		UINT8 index = PPR_BT_HEADER_SIZE;
		char buffer[CMD_BUFF_LEN];
		BiosPprData *BiosPprData_ptr = new BiosPprData();

		for (int i = 0; i < (int) entryCount; i++)
		{
			char c = ((unsigned char*)base_addr)[index];
			sprintf(buffer, "%d", c);
			uint8_t type = stoi(buffer);
			if (type == BOOTTIME_PPR_TYPE)
			{
				BiosPprData_ptr = (struct BiosPprData*)((UINT8 *)base_addr + index);
				pprBoottimeDataIn[i].repairEntryNum = BiosPprData_ptr->RepairEntryNumber;
				pprBoottimeDataIn[i].repairType     = BiosPprData_ptr->RepairType;
				pprBoottimeDataIn[i].socNum         = BiosPprData_ptr->SocNum;
				pprBoottimeDataIn[i].repairResult   = BiosPprData_ptr->RepairResult;

				for (int j=0; j < PAYLOAD_SIZE; j++)
				{
					pprBoottimeDataIn[i].payload[j] = BiosPprData_ptr->Payload[j];
				}
				sd_journal_print(LOG_INFO, "BIOS In Data Type 0x%x Entry Num 0x%x SOC 0x%x Result 0x%x \n",
						BiosPprData_ptr->RepairType, BiosPprData_ptr->RepairEntryNumber,
						BiosPprData_ptr->SocNum, BiosPprData_ptr->RepairResult);
				index = index + PPR_BT_DATA_SIZE;
			}
		}//end of for loop
	}//end of entry count check
}

UINT8 BootTimePprData::setBiosOutData()
{
	UINT8 ret = 0;
	int i, j;
	for (i=0; i < g_pprBoottimeIndex; i++)
	{
		pprBoottimeDataOut[i].repairEntryNum = g_pprBoottimeData[i].repairEntryNum;
		pprBoottimeDataOut[i].repairType     = g_pprBoottimeData[i].repairType;
		pprBoottimeDataOut[i].socNum         = g_pprBoottimeData[i].socNum;
		pprBoottimeDataOut[i].repairResult   = g_pprBoottimeData[i].repairResult;
		for (j=0; j < PAYLOAD_SIZE; j++)
		{
			pprBoottimeDataOut[i].payload[j] = g_pprBoottimeData[i].payload[j];
		}
		sd_journal_print(LOG_INFO, "BIOS Out Data Type 0x%x Entry Num 0x%x SOC 0x%x Payload %d \n",
				pprBoottimeDataOut[i].repairType, pprBoottimeDataOut[i].repairEntryNum,
				pprBoottimeDataOut[i].socNum, pprBoottimeDataOut[i].payload[0]);
		ret++;
	}
	return ret;
}

void BootTimePprData::compareBiosData()
{
	int i, j, k;
	bool match;
	UINT8 matchCnt = 0;
	UINT16 payloadMask[PAYLOAD_SIZE]= {0xFFFF, 0xFFFF, 0xFFFF, 0xE3FF, 0x0007, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};

	for (i=0; i < BiosOutCnt; i++)
	{
		for (j=0; j < BiosInCnt; j++)
		{
			match = true;
			for(k=0; k < PAYLOAD_SIZE; k++)
			{
				if ((pprBoottimeDataOut[i].payload[k] & payloadMask[k]) != (pprBoottimeDataIn[j].payload[k] & payloadMask[k]))
				{
					sd_journal_print(LOG_INFO, "compareBiosData i %d j %d k %d , data are not the same 0x%x 0x%x \n",
							i, j, k, pprBoottimeDataOut[i].payload[k], pprBoottimeDataIn[j].payload[k]);
					//break;
					match = false;
				}
			} // end of for k
			if (match)
			{
				pprBoottimeDataOut[i].repairResult = pprBoottimeDataIn[j].repairResult;
				//TBD Update Result in the data structure
				//g_pprBoottimeData[i].repairResult = pprBoottimeDataIn[j].repairResult;
				matchCnt++;
			}
		} // end of for j
	} // end of for i
	if (BiosOutCnt >= matchCnt)
		BiosOutCnt = BiosOutCnt - matchCnt;
	else
		BiosOutCnt = 0;
	sd_journal_print(LOG_INFO, "compareBiosData BiosOutCnt = %d , match Cnt = %d\n", BiosOutCnt, matchCnt);
}

void BootTimePprData::WriteHostSharedMem()
{
	UINT8 index = PPR_BT_HEADER_SIZE;
	BiosPprHeader *BiosPprHeader_ptr = new BiosPprHeader();
	BiosPprData *BiosPprData_ptr = new BiosPprData();

	// Get BT PPR Data
	BiosOutCnt = setBiosOutData();
	sd_journal_print(LOG_INFO, "WriteHostSharedMem start , BT count Out = %d , In = %d ", BiosOutCnt , BiosInCnt);
	if(BiosInCnt > 0)
		compareBiosData();

	//Header
	BiosPprHeader_ptr = (struct BiosPprHeader*)((UINT8 *)base_addr);

	BiosPprHeader_ptr->Signature  = BOOTTIME_PPR_SIGNATURE;
	BiosPprHeader_ptr->Version    = BOOTTIME_PPR_VERSION;
	BiosPprHeader_ptr->EntryCount = BiosOutCnt;
	BiosPprHeader_ptr->ReportSize = PPR_BT_HEADER_SIZE + (PPR_BT_DATA_SIZE * BiosOutCnt);
	BiosPprHeader_ptr->Command    = BT_PPR_CMD_BMC_CNT;

	// PPR Entry
	if (BiosOutCnt > 0)
	{
		for (int i = 0; i < (int) BiosOutCnt; i++)
		{
			BiosPprData_ptr = (struct BiosPprData*)((UINT8 *)base_addr + index);
			BiosPprData_ptr->Type              = BOOTTIME_PPR_TYPE;
			BiosPprData_ptr->Version           = BOOTTIME_PPR_VERSION;
			BiosPprData_ptr->Length            = PPR_BT_DATA_SIZE;
			BiosPprData_ptr->RepairEntryNumber = pprBoottimeDataOut[i].repairEntryNum;
			BiosPprData_ptr->RepairType        = pprBoottimeDataOut[i].repairType;
			BiosPprData_ptr->SocNum            = pprBoottimeDataOut[i].socNum;
			for (int j=0; j < PAYLOAD_SIZE; j++)
			{
				BiosPprData_ptr->Payload[j] = pprBoottimeDataOut[i].payload[j];
			}

			sd_journal_print(LOG_INFO, "BIOS Out Data Entry Num %d Type %d SOC %d \n",
					BiosPprData_ptr->RepairEntryNumber, BiosPprData_ptr->RepairType,
					BiosPprData_ptr->SocNum);
			index = index + PPR_BT_DATA_SIZE;
		} // end of for i
	} // end of if BiosOutCnt
}
