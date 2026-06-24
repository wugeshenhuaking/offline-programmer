/* add user code begin Header */
/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/
/* add user code end Header */

#include "ff.h"      /* Obtains integer types */
#include "diskio.h"    /* Declarations of disk functions */
#include "fatfs_disk.h"

#include <string.h>

/* private includes ----------------------------------------------------------*/
/* add user code begin private includes */

/* add user code end private includes */

/* private typedef -----------------------------------------------------------*/
/* add user code begin private typedef */

/* add user code end private typedef */

/* private define ------------------------------------------------------------*/
/* add user code begin private define */

/* add user code end private define */

/* private macro -------------------------------------------------------------*/
/* add user code begin private macro */

/* add user code end private macro */

/* private variables ---------------------------------------------------------*/
/* add user code begin private variables */

/* add user code end private variables */

/* private function prototypes --------------------------------------------*/
/* add user code begin function prototypes */
static uint8_t fatfs_drive_valid(BYTE pdrv);

/* add user code end function prototypes */

/* add user code begin 0 */
static uint8_t fatfs_drive_valid(BYTE pdrv)
{
  if(pdrv >= fatfs_disk.nbr)
  {
    return 0;
  }

  if(fatfs_disk.disk[pdrv] == 0)
  {
    return 0;
  }

  return 1;
}

/* add user code end 0 */

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status (
  BYTE pdrv    /* Physical drive nmuber to identify the drive */
)
{
/* add user code begin disk_status 0 */
  if(!fatfs_drive_valid(pdrv))
  {
    return STA_NOINIT;
  }

/* add user code end disk_status 0 */

  DSTATUS stat;

/* add user code begin disk_status 1 */

/* add user code end disk_status 1 */

  stat = fatfs_disk.disk[pdrv]->disk_status(fatfs_disk.lun[pdrv]);

/* add user code begin disk_status 2 */

/* add user code end disk_status 2 */

  return stat;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize (
  BYTE pdrv        /* Physical drive nmuber to identify the drive */
)
{
/* add user code begin disk_initialize 0 */
  if(!fatfs_drive_valid(pdrv))
  {
    return STA_NOINIT;
  }

/* add user code end disk_initialize 0 */

  DSTATUS stat = RES_OK;

/* add user code begin disk_initialize 1 */

/* add user code end disk_initialize 1 */

  if(fatfs_disk.is_initialized[pdrv] == 0)
	{
		stat = fatfs_disk.disk[pdrv]->disk_initialize(fatfs_disk.lun[pdrv]);
		if(stat == RES_OK)
		{
			fatfs_disk.is_initialized[pdrv] = 1;
		}
	}

/* add user code begin disk_initialize 2 */

/* add user code end disk_initialize 2 */

  return stat;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
DRESULT disk_read (
  BYTE pdrv,    /* Physical drive nmuber to identify the drive */
  BYTE *buff,    /* Data buffer to store read data */
  LBA_t sector,  /* Start sector in LBA */
  UINT count    /* Number of sectors to read */
)
{
/* add user code begin disk_read 0 */
  if(!fatfs_drive_valid(pdrv))
  {
    return RES_NOTRDY;
  }

  if(buff == NULL || count == 0)
  {
    return RES_PARERR;
  }

/* add user code end disk_read 0 */

  DRESULT res;

/* add user code begin disk_read 1 */

/* add user code end disk_read 1 */

  res = fatfs_disk.disk[pdrv]->disk_read(fatfs_disk.lun[pdrv], buff, sector, count);

/* add user code begin disk_read 2 */

/* add user code end disk_read 2 */

  return res;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/
#if FF_FS_READONLY == 0

DRESULT disk_write (
  BYTE pdrv,      /* Physical drive nmuber to identify the drive */
  const BYTE *buff,  /* Data to be written */
  LBA_t sector,    /* Start sector in LBA */
  UINT count      /* Number of sectors to write */
)
{
/* add user code begin disk_write 0 */
  if(!fatfs_drive_valid(pdrv))
  {
    return RES_NOTRDY;
  }

  if(buff == NULL || count == 0)
  {
    return RES_PARERR;
  }

/* add user code end disk_write 0 */

  DRESULT res;

/* add user code begin disk_write 1 */

/* add user code end disk_write 1 */

  res = fatfs_disk.disk[pdrv]->disk_write(fatfs_disk.lun[pdrv], buff, sector, count);

/* add user code begin disk_write 2 */

/* add user code end disk_write 2 */

  return res;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/
DRESULT disk_ioctl (
  BYTE pdrv,    /* Physical drive nmuber (0..) */
  BYTE cmd,    /* Control code */
  void *buff    /* Buffer to send/receive control data */
)
{
/* add user code begin disk_ioctl 0 */
  if(!fatfs_drive_valid(pdrv))
  {
    return RES_NOTRDY;
  }

/* add user code end disk_ioctl 0 */

  DRESULT res;

/* add user code begin disk_ioctl 1 */

/* add user code end disk_ioctl 1 */

  res = fatfs_disk.disk[pdrv]->disk_ioctl(fatfs_disk.lun[pdrv], cmd, buff);

/* add user code begin disk_ioctl 2 */

/* add user code end disk_ioctl 2 */

  return res;
}

/*-----------------------------------------------------------------------*/
/* get time                                                              */
/*-----------------------------------------------------------------------*/
__WEAK DWORD get_fattime (void)
{
/* add user code begin get_fattime 0 */
  rtc_time_get();

  if(calendar.year < 1980 || calendar.year > 2107 ||
     calendar.month < 1 || calendar.month > 12 ||
     calendar.date < 1 || calendar.date > 31 ||
     calendar.hour > 23 || calendar.min > 59 || calendar.sec > 59)
  {
    return ((DWORD)(2026 - 1980) << 25) | ((DWORD)1 << 21) | ((DWORD)1 << 16);
  }

  return ((DWORD)(calendar.year - 1980) << 25) |
         ((DWORD)calendar.month << 21) |
         ((DWORD)calendar.date << 16) |
         ((DWORD)calendar.hour << 11) |
         ((DWORD)calendar.min << 5) |
         ((DWORD)calendar.sec >> 1);

/* add user code end get_fattime 0 */

  return 0;
}

/* add user code begin 1 */

/* add user code end 1 */
