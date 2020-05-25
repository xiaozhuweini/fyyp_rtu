#include "drv_rtcee.h"
#include "drv_fyyp.h"
#include "string.h"



static rt_uint8_t _hj212_get_param_set(HJ212_PARAM_SET *param_set_ptr)
{
	return (RTCEE_EEPROM_ERR_NONE == rtcee_eeprom_read(HJ212_BASE_ADDR_PARAM, (rt_uint8_t *)param_set_ptr, sizeof(HJ212_PARAM_SET))) ? RT_TRUE : RT_FALSE;
}

static rt_uint8_t _hj212_set_param_set(HJ212_PARAM_SET const *param_set_ptr)
{
	return (RTCEE_EEPROM_ERR_NONE == rtcee_eeprom_write(HJ212_BASE_ADDR_PARAM, (rt_uint8_t *)param_set_ptr, sizeof(HJ212_PARAM_SET))) ? RT_TRUE : RT_FALSE;
}

static void _hj212_reset_param_set(HJ212_PARAM_SET *param_set_ptr)
{
	//ST
	memcpy((void *)param_set_ptr->st, (void *)HJ212_DEF_VAL_ST, HJ212_BYTES_ST);
	//PW
	memcpy((void *)param_set_ptr->pw, (void *)HJ212_DEF_VAL_PW, HJ212_BYTES_PW);
	//MN
	memcpy((void *)param_set_ptr->mn, (void *)HJ212_DEF_VAL_MN, HJ212_BYTES_MN);
	//over_time
	param_set_ptr->over_time	= HJ212_DEF_VAL_OT;
	//recount
	param_set_ptr->recount		= HJ212_DEF_VAL_RECOUNT;
	//rtd_en
	param_set_ptr->rtd_en		= RT_TRUE;
	//rtd_interval
	param_set_ptr->rtd_interval	= HJ212_DEF_VAL_RTD_INTERVAL;
	//min_interval
	param_set_ptr->min_interval	= HJ212_DEF_VAL_MIN_INTERVAL;
	//rs_en
	param_set_ptr->rs_en		= RT_TRUE;
}

static void _hj212_validate_param_set(HJ212_PARAM_SET *param_set_ptr)
{
	if(!param_set_ptr->over_time)
	{
		param_set_ptr->over_time = HJ212_DEF_VAL_OT;
	}

	if(RT_TRUE != param_set_ptr->rtd_en)
	{
		param_set_ptr->rtd_en = RT_FALSE;
	}

	if(RT_TRUE != param_set_ptr->rs_en)
	{
		param_set_ptr->rs_en = RT_FALSE;
	}
}

static void _hj212_set_st(rt_uint8_t const *pdata)
{
	rt_uint32_t addr = HJ212_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(HJ212_PARAM_SET, st[0]);

	rtcee_eeprom_write(addr, pdata, HJ212_BYTES_ST);
}

static void _hj212_set_pw(rt_uint8_t const *pdata)
{
	rt_uint32_t addr = HJ212_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(HJ212_PARAM_SET, pw[0]);

	rtcee_eeprom_write(addr, pdata, HJ212_BYTES_PW);
}

static void _hj212_set_mn(rt_uint8_t const *pdata)
{
	rt_uint32_t addr = HJ212_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(HJ212_PARAM_SET, mn[0]);

	rtcee_eeprom_write(addr, pdata, HJ212_BYTES_MN);
}

static void _hj212_set_over_time(rt_uint16_t over_time)
{
	rt_uint32_t addr = HJ212_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(HJ212_PARAM_SET, over_time);

	rtcee_eeprom_write(addr, (rt_uint8_t *)&over_time, sizeof(rt_uint16_t));
}

static void _hj212_set_recount(rt_uint8_t recount)
{
	rt_uint32_t addr = HJ212_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(HJ212_PARAM_SET, recount);

	rtcee_eeprom_write(addr, &recount, sizeof(rt_uint8_t));
}

static void _hj212_set_rtd_en(rt_uint8_t en)
{
	rt_uint32_t addr = HJ212_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(HJ212_PARAM_SET, rtd_en);

	rtcee_eeprom_write(addr, &en, sizeof(rt_uint8_t));
}

static void _hj212_set_rtd_interval(rt_uint16_t interval)
{
	rt_uint32_t addr = HJ212_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(HJ212_PARAM_SET, rtd_interval);

	rtcee_eeprom_write(addr, (rt_uint8_t *)&interval, sizeof(rt_uint16_t));
}

static void _hj212_set_min_interval(rt_uint16_t interval)
{
	rt_uint32_t addr = HJ212_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(HJ212_PARAM_SET, min_interval);

	rtcee_eeprom_write(addr, (rt_uint8_t *)&interval, sizeof(rt_uint16_t));
}

static void _hj212_set_rs_en(rt_uint8_t en)
{
	rt_uint32_t addr = HJ212_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(HJ212_PARAM_SET, rs_en);

	rtcee_eeprom_write(addr, &en, sizeof(rt_uint8_t));
}

