/*
**治污设备运行状态管理
*/



#include "rtthread.h"
#include "drv_pin.h"
#include "user_priority.h"



#define EQUIP_NUM_EQUIP				4
#define EQUIP_PIN_SB_1				PIN_INDEX_10
#define EQUIP_PIN_SB_2				PIN_INDEX_11
#define EQUIP_PIN_SB_3				PIN_INDEX_12
#define EQUIP_PIN_SB_4				PIN_INDEX_13

#define EQUIP_EVENT_RUN_STA			0x01
#define EQUIP_EVENT_RUN_TIME		0x100
#define EQUIP_EVENT_INIT_VAL		0xffff

#define EQUIP_STK_TIME_TICK			512
#ifndef EQUIP_PRIO_TIME_TICK
#define EQUIP_PRIO_TIME_TICK		27
#endif



rt_uint32_t equip_get_run_sta(void);
rt_uint32_t equip_get_run_time(rt_uint8_t equip, rt_uint8_t clred);
