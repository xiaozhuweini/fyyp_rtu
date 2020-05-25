#include "drv_rtcee.h"



static void _sl427_get_rtu_addr(rt_uint8_t *rtu_addr_ptr)
{
	if(RTCEE_EEPROM_ERR_NONE == rtcee_eeprom_read(SL427_POS_RTU_ADDR, rtu_addr_ptr, SL427_BYTES_RTU_ADDR))
	{
		return;
	}
	
	rt_sprintf((char *)rtu_addr_ptr, "smt-h7660");
}
static void _sl427_set_rtu_addr(rt_uint8_t const *rtu_addr_ptr)
{
	rtcee_eeprom_write(SL427_POS_RTU_ADDR, rtu_addr_ptr, SL427_BYTES_RTU_ADDR);
}

static rt_uint16_t _sl427_get_report_period(void)
{
	rt_uint16_t period;
	
	if(RTCEE_EEPROM_ERR_NONE != rtcee_eeprom_read(SL427_POS_REPORT_PERIOD, (rt_uint8_t *)(&period), 2))
	{
		period = 60;
	}
	
	return period;
}
static void _sl427_set_report_period(rt_uint16_t period)
{
	rtcee_eeprom_write(SL427_POS_REPORT_PERIOD, (rt_uint8_t *)(&period), 2);
}

static rt_uint8_t _sl427_get_sms_warn_receipt(void)
{
	rt_uint8_t en;

	if(RTCEE_EEPROM_ERR_NONE != rtcee_eeprom_read(SL427_POS_SMS_WARN_RECEIPT, &en, 1))
	{
		return RT_TRUE;
	}
	
	return (RT_FALSE == en) ? RT_FALSE : RT_TRUE;
}
static void _sl427_set_sms_warn_receipt(rt_uint8_t en)
{
	if((RT_TRUE != en) && (RT_FALSE != en))
	{
		return;
	}
	
	rtcee_eeprom_write(SL427_POS_SMS_WARN_RECEIPT, &en, 1);
}

