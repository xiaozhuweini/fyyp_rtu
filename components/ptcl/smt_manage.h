/*占用的存储空间
**EEPROM		:1300--1399
*/



#ifndef __SMT_MANAGE_H__
#define __SMT_MANAGE_H__



#include "rtthread.h"
#include "user_priority.h"
#include "ptcl.h"



//参数基地址
#define SMTMNG_BASE_ADDR_PARAM					1300

//配置信息
#define SMTMNG_DEFAULT_PERIOD_STA				60
#define SMTMNG_DEFAULT_PERIOD_TIMING			1440
#define SMTMNG_DEFAULT_PERIOD_HEART				15
#define SMTMNG_BYTES_STA_REPORT					100

//帧框架信息
#define SMTMNG_BYTES_FRAME_HEAD					16
#define SMTMNG_BYTES_FRAME_END					2
#define SMTMNG_BYTES_FRAME_UUID_TYPE			1
#define SMTMNG_BYTES_FRAME_UUID_VALUE			12
#define SMTMNG_BYTES_FRAME_ELEMENT_TYPE			2
#define SMTMNG_BYTES_FRAME_ELEMENT_LEN			2
#define SMTMNG_FRAME_HEAD						0x5a
#define SMTMNG_FRAME_END						0xa5
#define SMTMNG_FRAME_CRC_VALUE					0

//uuid type
#define SMTMNG_UUID_TYPE_STM32					1
#define SMTMNG_UUID_TYPE_DS18B20				2

//element type
#define SMTMNG_ELEMENT_TYPE_RTU_ADDR			0
#define SMTMNG_ELEMENT_TYPE_SOFTWARE_EDITION	1
#define SMTMNG_ELEMENT_TYPE_RUN_TIME			513
#define SMTMNG_ELEMENT_TYPE_SUPPLY_VOLT			514
#define SMTMNG_ELEMENT_TYPE_SOLAR_VOLT			515
#define SMTMNG_ELEMENT_TYPE_CHARGE_CURRENT		516
#define SMTMNG_ELEMENT_TYPE_GSM_CSQ				518
#define SMTMNG_ELEMENT_TYPE_HEART_UP			1024
#define SMTMNG_ELEMENT_TYPE_HEART_DOWN			1025
#define SMTMNG_ELEMENT_TYPE_TIMING_UP			1026
#define SMTMNG_ELEMENT_TYPE_TIMING_DOWN			1027
#define SMTMNG_ELEMENT_TYPE_CMD_UP				1030
#define SMTMNG_ELEMENT_TYPE_CMD_DOWN			1031
#define SMTMNG_ELEMENT_TYPE_OTA_UP				1032
#define SMTMNG_ELEMENT_TYPE_OTA_DOWN			1033
#define SMTMNG_ELEMENT_TYPE_SHELL_UP			1034
#define SMTMNG_ELEMENT_TYPE_SHELL_DOWN			1035

//element len
#define SMTMNG_ELEMENT_LEN_RUN_TIME				3
#define SMTMNG_ELEMENT_LEN_SUPPLY_VOLT			2
#define SMTMNG_ELEMENT_LEN_SOLAR_VOLT			2
#define SMTMNG_ELEMENT_LEN_CHARGE_CURRENT		2
#define SMTMNG_ELEMENT_LEN_GSM_CSQ				1
#define SMTMNG_ELEMENT_LEN_HEART_UP				0
#define SMTMNG_ELEMENT_LEN_HEART_DOWN			0
#define SMTMNG_ELEMENT_LEN_TIMING_UP			0
#define SMTMNG_ELEMENT_LEN_TIMING_DOWN			6

//任务
#define SMTMNG_STK_TIME_TICK					640
#ifndef SMTMNG_PRIO_TIME_TICK
#define SMTMNG_PRIO_TIME_TICK					27
#endif

#define SMTMNG_EVENT_PARAM_PERIOD_STA			0x01
#define SMTMNG_EVENT_PARAM_PERIOD_TIMING		0x02
#define SMTMNG_EVENT_PARAM_PERIOD_HEART			0x04
#define SMTMNG_EVENT_PARAM_ALL					0x07

#define SMTMNG_CMD_OTA_START					"<05><cfg>"

//参数集合
typedef struct smtmng_param_set
{
	rt_uint16_t			period_sta;
	rt_uint16_t			period_timing;
	rt_uint16_t			period_heart;
} SMTMNG_PARAM_SET;



rt_uint16_t smtmng_get_sta_period(void);
void smtmng_set_sta_period(rt_uint16_t period);

rt_uint16_t smtmng_get_timing_period(void);
void smtmng_set_timing_period(rt_uint16_t period);

rt_uint16_t smtmng_get_heart_period(void);
void smtmng_set_heart_period(rt_uint16_t period);

void smtmng_param_restore(void);

rt_uint16_t smtmng_heart_encoder(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t ch);

rt_uint8_t smtmng_data_decoder(PTCL_REPORT_DATA *report_data_ptr, PTCL_RECV_DATA const *recv_data_ptr, PTCL_PARAM_INFO **param_info_ptr_ptr);



#endif

