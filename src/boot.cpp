#include "boot.hpp"
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

#define MAX_RETRY           25
#define TSLEEP              20

#define index_file  ("/sys/devices/platform/ahb/ahb:apb/1e7e0000.bmc_dev/bmc-dev-queue1")
#define BMC_DEV ("/dev/bmc-device")

BootTimePprDataHolder* BootTimePprDataHolder::instance = 0;
using namespace std;

//Read Membar device
bool BootTimePprData::ReadHostQueue()
{
    sd_journal_print(LOG_INFO, "In membarRead  \n");
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

    return retvalue;
}

//first get the number of PCIe Devices
uint32_t BootTimePprData::getPCIeEntryCount()
{
  deviceEntryCount = 0;
  if(ReadHostQueue() && base_addr != NULL)
  {
     BiosPprHeader *BiosPprHeader_ptr = new BiosPprHeader();
     BiosPprHeader_ptr = (struct BiosPprHeader*)(base_addr);

     if (BiosPprHeader_ptr !=NULL)
     {
        deviceEntryCount = BiosPprHeader_ptr->EntryCount;
     }
  }

  sd_journal_print(LOG_INFO, "deviceEntryCount %d", deviceEntryCount);
  return deviceEntryCount;
}

void BootTimePprData::poolMembar()
{
   uint32_t retry = 0;
   while((0 == getPCIeEntryCount()) && (retry < MAX_RETRY))
   {
     sleep(TSLEEP);
     sd_journal_print(LOG_INFO, "Membar buffer is empty, retrying...%d \n", retry);
     retry++;
   }
}
//entry point
void BootTimePprData::onHostPwrChange()
{
  poolMembar();
  if (deviceEntryCount > 0)
  {
    ReadBootTimePprData(deviceEntryCount);
  }
}

void BootTimePprData::ReadBootTimePprData(uint32_t entryCount)
{
    if (entryCount > 0 )
    {
       UINT8 index = OFfSET_1;
       char buffer[CMD_BUFF_LEN];
       BiosPprData *BiosPprData_ptr = new BiosPprData();

       for (int i = 0; i < (int) entryCount; i++)
       {
           char c = ((unsigned char*)base_addr)[index];
           sprintf(buffer, "%d", c);
           uint8_t type = stoi(buffer);
           if (type == 0)
           {
            BiosPprData_ptr = (struct BiosPprData*)((UINT8 *)base_addr + index);
            sd_journal_print(LOG_INFO, "repair Type %d \n", BiosPprData_ptr->RepairType);
            pprBoottimeDataIN[i].repairEntryNum = BiosPprData_ptr->RepairEntryNumber;
            pprBoottimeDataIN[i].repairType = BiosPprData_ptr->RepairType;
            pprBoottimeDataIN[i].socNum = BiosPprData_ptr->SocNum;
            pprBoottimeDataIN[i].repairResult = BiosPprData_ptr->RepairResult;

            for (int j=0; j < 10; j++) {
              pprBoottimeDataIN[i].payload[j] = BiosPprData_ptr->Payload[j];
            }

              index = index + OFfSET_2;
           }
       }//end of for loop
    }//end of entry count check
}

void BootTimePprData::SyncMainDB()
{

}

void BootTimePprData::WriteHostQueue()
{

}
