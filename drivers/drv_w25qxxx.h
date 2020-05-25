#ifndef __DRV_W25QXXX_H__
#define __DRV_W25QXXX_H__



#include "rtthread.h"
#include "drv_pin.h"



#define W25QXXX_128
//#define W25QXXX_256



//接口定义
#define W25QXXX_PIN_CS						PIN_INDEX_51
#define W25QXXX_PIN_SCK						PIN_INDEX_52
#define W25QXXX_PIN_MISO					PIN_INDEX_53
#define W25QXXX_PIN_MOSI					PIN_INDEX_54

//配置信息
#define W25QXXX_BYTES_PER_PAGE				256
#define W25QXXX_BYTES_PER_SECTOR			4096
#define W25QXXX_BYTES_PER_BLOCK				0x10000
#ifdef W25QXXX_128
#define W25QXXX_BYTES_CHIP					0x1000000
#endif
#ifdef W25QXXX_256
#define W25QXXX_BYTES_CHIP					0x2000000
#endif
#define W25QXXX_4K_BLKSIZE					1024
#define W25QXXX_4K_NBLKS					4
#define W25QXXX_TICKS_ENTER_PWR_DOWN		2
#define W25QXXX_TICKS_EXIT_PWR_DOWN			2

//指令
#define W25QXXX_CMD_WRITE_ENABLE			0x06
#define W25QXXX_CMD_EXIT_PWR_DOWN			0xab
#define W25QXXX_CMD_READ_DATA				0x03
#define W25QXXX_CMD_READ_DATA_4B			0x13
#define W25QXXX_CMD_PAGE_PROGRAM			0x02
#define W25QXXX_CMD_PAGE_PROGRAM_4B			0x12
#define W25QXXX_CMD_SECTOR_ERASE			0x20
#define W25QXXX_CMD_SECTOR_ERASE_4B			0x21
#define W25QXXX_CMD_BLOCK_ERASE				0xd8
#define W25QXXX_CMD_BLOCK_ERASE_4B			0xdc
#define W25QXXX_CMD_READ_STATUS_1			0x05
#define W25QXXX_CMD_READ_STATUS_3			0x15
#define W25QXXX_CMD_ENTER_PWR_DOWN			0xb9
#define W25QXXX_CMD_ENTER_4B				0xb7
#define W25QXXX_CMD_EXIT_4B					0xe9
#define W25QXXX_CMD_IS_BUSY					0x01
#define W25QXXX_CMD_IS_4B					0x01

//错误码
#define W25QXXX_ERR_NONE					0
#define W25QXXX_ERR_ARG						1
#define W25QXXX_ERR_MEM						2
#define W25QXXX_ERR_STATUS					3
#define W25QXXX_ERR_DATA_READ				4
#define W25QXXX_ERR_DATA_PROGRAM			5
#define W25QXXX_ERR_SECTOR_ERASE			6



rt_uint8_t w25qxxx_read(rt_uint32_t addr, rt_uint8_t *pdata, rt_uint16_t data_len);
rt_uint8_t w25qxxx_write(rt_uint32_t addr, rt_uint8_t const *pdata, rt_uint16_t data_len);
rt_uint8_t w25qxxx_block_erase(rt_uint32_t addr);
rt_uint8_t w25qxxx_program(rt_uint32_t addr, rt_uint8_t const *pdata, rt_uint16_t data_len);
void w25qxxx_open_ex(void);
void w25qxxx_close_ex(void);
rt_uint8_t w25qxxx_program_ex(rt_uint32_t addr, rt_uint8_t const *pdata, rt_uint16_t data_len);
rt_uint8_t w25qxxx_block_erase_ex(rt_uint32_t addr);
rt_uint8_t w25qxxx_read_ex(rt_uint32_t addr, rt_uint8_t *pdata, rt_uint16_t data_len);



#endif

