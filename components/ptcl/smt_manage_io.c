#include "drv_rtcee.h"
#include "drv_fyyp.h"



//²ÎÊý¼¯
static rt_uint8_t _smtmng_get_param_set(SMTMNG_PARAM_SET *param_set_ptr)
{
	return (RTCEE_EEPROM_ERR_NONE == rtcee_eeprom_read(SMTMNG_BASE_ADDR_PARAM, (rt_uint8_t *)param_set_ptr, sizeof(SMTMNG_PARAM_SET))) ? RT_TRUE : RT_FALSE;
}
static rt_uint8_t _smtmng_set_param_set(SMTMNG_PARAM_SET const *param_set_ptr)
{
	return (RTCEE_EEPROM_ERR_NONE == rtcee_eeprom_write(SMTMNG_BASE_ADDR_PARAM, (rt_uint8_t *)param_set_ptr, sizeof(SMTMNG_PARAM_SET))) ? RT_TRUE : RT_FALSE;
}
static void _smtmng_reset_param_set(SMTMNG_PARAM_SET *param_set_ptr)
{
	param_set_ptr->period_heart		= SMTMNG_DEFAULT_PERIOD_HEART;
	param_set_ptr->period_sta		= SMTMNG_DEFAULT_PERIOD_STA;
	param_set_ptr->period_timing	= SMTMNG_DEFAULT_PERIOD_TIMING;
}

static void _smtmng_set_sta_period(rt_uint16_t period)
{
	rt_uint32_t addr = SMTMNG_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(SMTMNG_PARAM_SET, period_sta);

	rtcee_eeprom_write(addr, (rt_uint8_t *)&period, sizeof(rt_uint16_t));
}

static void _smtmng_set_timing_period(rt_uint16_t period)
{
	rt_uint32_t addr = SMTMNG_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(SMTMNG_PARAM_SET, period_timing);

	rtcee_eeprom_write(addr, (rt_uint8_t *)&period, sizeof(rt_uint16_t));
}

static void _smtmng_set_heart_period(rt_uint16_t period)
{
	rt_uint32_t addr = SMTMNG_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(SMTMNG_PARAM_SET, period_heart);

	rtcee_eeprom_write(addr, (rt_uint8_t *)&period, sizeof(rt_uint16_t));
}
