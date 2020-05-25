#include "drv_rtcee.h"
#include "drv_w25qxxx.h"



//rtu地址
static void _sl651_get_rtu_addr(rt_uint8_t *rtu_addr_ptr)
{
	if((rt_uint8_t *)0 == rtu_addr_ptr)
	{
		return;
	}
	if(RTCEE_EEPROM_ERR_NONE == rtcee_eeprom_read(SL651_POS_RTU_ADDR, rtu_addr_ptr, SL651_BYTES_RTU_ADDR))
	{
		return;
	}
	rt_memset((void *)rtu_addr_ptr, 0x88, SL651_BYTES_RTU_ADDR);
}
static void _sl651_set_rtu_addr(rt_uint8_t const *rtu_addr_ptr)
{
	if((rt_uint8_t *)0 == rtu_addr_ptr)
	{
		return;
	}
	
	rtcee_eeprom_write(SL651_POS_RTU_ADDR, rtu_addr_ptr, SL651_BYTES_RTU_ADDR);
}

//pw
static rt_uint16_t _sl651_get_pw(void)
{
	rt_uint16_t pw;
	
	if(RTCEE_EEPROM_ERR_NONE != rtcee_eeprom_read(SL651_POS_PW, (rt_uint8_t *)(&pw), 2))
	{
		pw = SL651_FRAME_DEFAULT_PW;
	}
	
	return pw;
}
static void _sl651_set_pw(rt_uint16_t pw)
{
	rtcee_eeprom_write(SL651_POS_PW, (rt_uint8_t *)(&pw), 2);
}

//sn
static rt_uint16_t _sl651_get_sn(void)
{
	rt_uint16_t sn;
	
	if(RTCEE_EEPROM_ERR_NONE != rtcee_eeprom_read(SL651_POS_SN, (rt_uint8_t *)(&sn), 2))
	{
		return 1;
	}
	
	if(0 == sn)
	{
		sn++;
	}
	
	return sn;
}
static void _sl651_set_sn(rt_uint16_t sn)
{
	if(0 == sn)
	{
		return;
	}
	
	rtcee_eeprom_write(SL651_POS_SN, (rt_uint8_t *)(&sn), 2);
}

//分类码
static rt_uint8_t _sl651_get_rtu_type(void)
{
	rt_uint8_t rtu_type;
	
	if(RTCEE_EEPROM_ERR_NONE != rtcee_eeprom_read(SL651_POS_RTU_TYPE, &rtu_type, 1))
	{
		return SL651_RTU_TYPE_SHUIKU;
	}
	
	switch(rtu_type)
	{
	//降水
	case SL651_RTU_TYPE_JIANGSHUI:
	//河道
	case SL651_RTU_TYPE_HEDAO:
	//水库
	case SL651_RTU_TYPE_SHUIKU:
	//闸坝
	case SL651_RTU_TYPE_ZHABA:
	//泵站
	case SL651_RTU_TYPE_BENGZHAN:
	//潮汐
	case SL651_RTU_TYPE_CHAOXI:
	//墒情
	case SL651_RTU_TYPE_SHANGQING:
	//地下水
	case SL651_RTU_TYPE_DIXIASHUI:
	//水质
	case SL651_RTU_TYPE_SHUIZHI:
	//取水口
	case SL651_RTU_TYPE_QUSHUIKOU:
	//排水口
	case SL651_RTU_TYPE_PAISHUIKOU:
		break;
	default:
		rtu_type = SL651_RTU_TYPE_SHUIKU;
		break;
	}
	
	return rtu_type;
}
static void _sl651_set_rtu_type(rt_uint8_t rtu_type)
{
	switch(rtu_type)
	{
	//降水
	case SL651_RTU_TYPE_JIANGSHUI:
	//河道
	case SL651_RTU_TYPE_HEDAO:
	//水库
	case SL651_RTU_TYPE_SHUIKU:
	//闸坝
	case SL651_RTU_TYPE_ZHABA:
	//泵站
	case SL651_RTU_TYPE_BENGZHAN:
	//潮汐
	case SL651_RTU_TYPE_CHAOXI:
	//墒情
	case SL651_RTU_TYPE_SHANGQING:
	//地下水
	case SL651_RTU_TYPE_DIXIASHUI:
	//水质
	case SL651_RTU_TYPE_SHUIZHI:
	//取水口
	case SL651_RTU_TYPE_QUSHUIKOU:
	//排水口
	case SL651_RTU_TYPE_PAISHUIKOU:
		break;
	default:
		return;
	}
	rtcee_eeprom_write(SL651_POS_RTU_TYPE, &rtu_type, 1);
}

//监测要素
static void _sl651_get_element(rt_uint8_t *pelement)
{
	if((rt_uint8_t *)0 == pelement)
	{
		return;
	}
	
	if(RTCEE_EEPROM_ERR_NONE == rtcee_eeprom_read(SL651_POS_ELEMENT, pelement, SL651_BYTES_ELEMENT))
	{
		return;
	}
	
	rt_memset((void *)pelement, 0, SL651_BYTES_ELEMENT);
}
static void _sl651_set_element(rt_uint8_t const *pelement)
{
	if((rt_uint8_t *)0 == pelement)
	{
		return;
	}
	
	rtcee_eeprom_write(SL651_POS_ELEMENT, pelement, SL651_BYTES_ELEMENT);
}

//工作模式
static rt_uint8_t _sl651_get_work_mode(void)
{
	rt_uint8_t work_mode;
	
	if(RTCEE_EEPROM_ERR_NONE != rtcee_eeprom_read(SL651_POS_WORK_MODE, &work_mode, 1))
	{
		return SL651_MODE_REPLY;
	}
	
	switch(work_mode)
	{
	//自报模式
	case SL651_MODE_REPORT:
	//自报确认模式
	case SL651_MODE_REPLY:
	//查询应答模式
	case SL651_MODE_QA:
	//调试模式
	case SL651_MODE_DEBUG:
		break;
	default:
		work_mode = SL651_MODE_REPLY;
		break;
	}
	
	return work_mode;
}
static void _sl651_set_work_mode(rt_uint8_t work_mode)
{
	switch(work_mode)
	{
	//自报模式
	case SL651_MODE_REPORT:
	//自报确认模式
	case SL651_MODE_REPLY:
	//查询应答模式
	case SL651_MODE_QA:
	//调试模式
	case SL651_MODE_DEBUG:
		break;
	default:
		return;
	}
	
	rtcee_eeprom_write(SL651_POS_WORK_MODE, &work_mode, 1);
}

//定时报间隔
static rt_uint8_t _sl651_get_report_hour(void)
{
	rt_uint8_t period;
	
	if(RTCEE_EEPROM_ERR_NONE != rtcee_eeprom_read(SL651_POS_REPORT_PERIOD_HOUR, &period, 1))
	{
		return 0;
	}
	
	if(period > 24)
	{
		period = 0;
	}
	
	return period;
}
static void _sl651_set_report_hour(rt_uint8_t period)
{
	if(period > 24)
	{
		return;
	}
	
	rtcee_eeprom_write(SL651_POS_REPORT_PERIOD_HOUR, &period, 1);
}

//加报间隔
static rt_uint8_t _sl651_get_report_min(void)
{
	rt_uint8_t period;
	
	if(RTCEE_EEPROM_ERR_NONE != rtcee_eeprom_read(SL651_POS_REPORT_PERIOD_MIN, &period, 1))
	{
		return 0;
	}
	
	if(period >= 60)
	{
		period = 0;
	}
	
	return period;
}
static void _sl651_set_report_min(rt_uint8_t period)
{
	if(period >= 60)
	{
		return;
	}
	
	rtcee_eeprom_write(SL651_POS_REPORT_PERIOD_MIN, &period, 1);
}

//事件记录
static rt_uint16_t _sl651_get_erc(rt_uint8_t erc_type)
{
	rt_uint16_t eeprom, record;
	
	if(erc_type >= SL651_NUM_ERC_TYPE)
	{
		return 0;
	}
	
	eeprom = erc_type;
	eeprom *= 2;
	eeprom += SL651_POS_ERC;
	
	if(RTCEE_EEPROM_ERR_NONE != rtcee_eeprom_read(eeprom, (rt_uint8_t *)(&record), 2))
	{
		record = 0;
	}
	
	return record;
}
static void _sl651_set_erc(rt_uint16_t record, rt_uint8_t erc_type)
{
	rt_uint16_t eeprom;
	
	if(erc_type >= SL651_NUM_ERC_TYPE)
	{
		return;
	}
	
	eeprom = erc_type;
	eeprom *= 2;
	eeprom += SL651_POS_ERC;
	
	rtcee_eeprom_write(eeprom, (rt_uint8_t *)(&record), 2);
}

//设备识别号
static rt_uint8_t _sl651_get_device_id(rt_uint8_t *pid)
{
	rt_uint16_t	eeprom;
	rt_uint8_t	id_len;
	
	if((rt_uint8_t *)0 == pid)
	{
		return 0;
	}
	
	eeprom = SL651_POS_DEVICE_ID;
	
	if(RTCEE_EEPROM_ERR_NONE != rtcee_eeprom_read(eeprom++, &id_len, 1))
	{
		return 0;
	}
	if(id_len > SL651_BYTES_DEVICE_ID)
	{
		return 0;
	}
	if(RTCEE_EEPROM_ERR_NONE != rtcee_eeprom_read(eeprom, pid, id_len))
	{
		return 0;
	}

	return id_len;
}
static void _sl651_set_device_id(rt_uint8_t const *pid, rt_uint8_t id_len)
{
	rt_uint16_t eeprom;
	
	if((rt_uint8_t *)0 == pid)
	{
		return;
	}
	
	if(id_len > SL651_BYTES_DEVICE_ID)
	{
		return;
	}
	
	eeprom = SL651_POS_DEVICE_ID;
	
	if(RTCEE_EEPROM_ERR_NONE != rtcee_eeprom_write(eeprom++, &id_len, 1))
	{
		return;
	}
	rtcee_eeprom_write(eeprom, pid, id_len);
}

//随机采集间隔
static rt_uint16_t _sl651_get_random_period(void)
{
	rt_uint16_t period;
	
	if(RTCEE_EEPROM_ERR_NONE != rtcee_eeprom_read(SL651_POS_RANDOM_SAMPLE_PERIOD, (rt_uint8_t *)(&period), 2))
	{
		return 0;
	}
	
	return period;
}
static void _sl651_set_random_period(rt_uint16_t period)
{
	rtcee_eeprom_write(SL651_POS_RANDOM_SAMPLE_PERIOD, (rt_uint8_t *)(&period), 2);
}

//告警信息
static void _sl651_get_warn_info(SL651_WARN_INFO *warn_info_ptr, rt_uint8_t warn_level)
{
	if((SL651_WARN_INFO *)0 == warn_info_ptr)
	{
		return;
	}
	if(warn_level >= SL651_NUM_WARN_INFO)
	{
		warn_info_ptr->valid = 0;
		return;
	}

	if(W25QXXX_ERR_NONE != w25qxxx_read(SL651_WARN_INFO_ADDR + warn_level * SL651_BYTES_WARN_INFO_IN_FLASH, (rt_uint8_t *)warn_info_ptr, sizeof(SL651_WARN_INFO)))
	{
		warn_info_ptr->valid = 0;
		return;
	}

	if(SL651_WARN_INFO_VALID == warn_info_ptr->valid)
	{
		if(warn_info_ptr->data_len > SL651_BYTES_WARN_INFO_MAX)
		{
			warn_info_ptr->valid = 0;
		}
	}
}
static void _sl651_set_warn_info(SL651_WARN_INFO const *warn_info_ptr, rt_uint8_t warn_level)
{
	if((SL651_WARN_INFO *)0 == warn_info_ptr)
	{
		return;
	}
	if(warn_level >= SL651_NUM_WARN_INFO)
	{
		return;
	}

	w25qxxx_write(SL651_WARN_INFO_ADDR + warn_level * SL651_BYTES_WARN_INFO_IN_FLASH, (rt_uint8_t *)warn_info_ptr, sizeof(SL651_WARN_INFO));
}

