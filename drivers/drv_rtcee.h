#ifndef __DRV_RTCEE_H__
#define __DRV_RTCEE_H__



#include "rtthread.h"
#include "drv_pin.h"



#define RTCEE_RTC
#define RTCEE_EEPROM

#define RTCEE_PIN_EE_WP						0
#define RTCEE_PIN_SCL						PIN_INDEX_1
#define RTCEE_PIN_SDA						PIN_INDEX_2
#define RTCEE_PIN_RTC_INT					PIN_INDEX_17

#define RTCEE_EVENT_MODULE_PEND				0x01
#define RTCEE_EVENT_TIME_UPDATE				0x02
#define RTCEE_EVENT_TIME_CONV				0x04



#ifdef RTCEE_RTC

#include "time.h"
#include "rthw.h"



//配置信息
#define RTCEE_RTC_CTRL_WR					0x64
#define RTCEE_RTC_CTRL_RD					0x65

//寄存器信息
#define RTCEE_RTC_NUM_TIME_REG				7
#define RTCEE_RTC_REG_SEC					0
#define RTCEE_RTC_REG_MIN					1
#define RTCEE_RTC_REG_HOUR					2
#define RTCEE_RTC_REG_WDAY					3
#define RTCEE_RTC_REG_MDAY					4
#define RTCEE_RTC_REG_MONTH					5
#define RTCEE_RTC_REG_YEAR					6
#define RTCEE_RTC_REG_RAM					7
#define RTCEE_RTC_REG_ALARM_MIN				8
#define RTCEE_RTC_REG_ALARM_HOUR			9
#define RTCEE_RTC_REG_ALARM_DAY				10
#define RTCEE_RTC_REG_TIME_CNT1				11
#define RTCEE_RTC_REG_TIME_CNT2				12
#define RTCEE_RTC_REG_EXTENSION				13
#define RTCEE_RTC_REG_FLAG					14
#define RTCEE_RTC_REG_CTRL					15
#define RTCEE_RTC_NUM_REG					16

//返回值
#define RTCEE_RTC_ERR_NONE					0
#define RTCEE_RTC_ERR_ARG					1
#define RTCEE_RTC_ERR_ACK					2



struct tm rtcee_rtc_get_calendar(void);
void rtcee_rtc_set_calendar(struct tm calendar_time);
time_t rtcee_rtc_calendar_to_unix(struct tm calendar_time);
struct tm rtcee_rtc_unix_to_calendar(time_t unix);
void rtcee_rtc_sec_pend(void);

#endif

#ifdef RTCEE_EEPROM

//配置信息
#define RTCEE_EEPROM_BYTES_SEPARATOR		2048
#define RTCEE_EEPROM_BYTES_CHIP				4096
#define RTCEE_EEPROM_BYTES_PAGE				32
#define RTCEE_EEPROM_CTRL_WR				0xa0
#define RTCEE_EEPROM_CTRL_RD				0xa1

//返回值
#define RTCEE_EEPROM_ERR_NONE				0
#define RTCEE_EEPROM_ERR_ARG				1
#define RTCEE_EEPROM_ERR_ACK				2
#define RTCEE_EEPROM_ERR_WR_PAGE			3



rt_uint8_t rtcee_eeprom_read(rt_uint32_t addr, rt_uint8_t *pdata, rt_uint16_t data_len);
rt_uint8_t rtcee_eeprom_write(rt_uint32_t addr, rt_uint8_t const *pdata, rt_uint16_t data_len);

#endif



#endif

