#include "drv_rtcee.h"
#include "drv_fyyp.h"



//参数集
static rt_uint8_t _sample_get_param_set(SAMPLE_PARAM_SET *param_set_ptr)
{
	return (RTCEE_EEPROM_ERR_NONE == rtcee_eeprom_read(SAMPLE_BASE_ADDR_PARAM, (rt_uint8_t *)param_set_ptr, sizeof(SAMPLE_PARAM_SET))) ? RT_TRUE : RT_FALSE;
}
static rt_uint8_t _sample_set_param_set(SAMPLE_PARAM_SET const *param_set_ptr)
{
	return (RTCEE_EEPROM_ERR_NONE == rtcee_eeprom_write(SAMPLE_BASE_ADDR_PARAM, (rt_uint8_t *)param_set_ptr, sizeof(SAMPLE_PARAM_SET))) ? RT_TRUE : RT_FALSE;
}
static void _sample_reset_param_set(SAMPLE_PARAM_SET *param_set_ptr)
{
	rt_uint8_t i;

	for(i = 0; i < SAMPLE_NUM_CTRL_BLK; i++)
	{
		param_set_ptr->ctrl_blk[i].sensor_type = SAMPLE_NUM_SENSOR_TYPE;
	}
	param_set_ptr->data_pointer.sta		= RT_FALSE;
	param_set_ptr->data_pointer.p		= 0;
	param_set_ptr->data_pointer_ex.sta	= RT_FALSE;
	param_set_ptr->data_pointer_ex.p	= 0;
}
static void _sample_validate_param_set(SAMPLE_PARAM_SET *param_set_ptr)
{
	rt_uint8_t i;

	for(i = 0; i < SAMPLE_NUM_CTRL_BLK; i++)
	{
		if(param_set_ptr->ctrl_blk[i].sensor_type > SAMPLE_NUM_SENSOR_TYPE)
		{
			param_set_ptr->ctrl_blk[i].sensor_type = SAMPLE_NUM_SENSOR_TYPE;
		}
	}

	if(((RT_FALSE != param_set_ptr->data_pointer.sta) && (RT_TRUE != param_set_ptr->data_pointer.sta)) || (param_set_ptr->data_pointer.p >= SAMPLE_NUM_TOTAL_ITEM))
	{
		param_set_ptr->data_pointer.sta		= RT_FALSE;
		param_set_ptr->data_pointer.p		= 0;
	}

	if(((RT_FALSE != param_set_ptr->data_pointer_ex.sta) && (RT_TRUE != param_set_ptr->data_pointer_ex.sta)) || (param_set_ptr->data_pointer_ex.p >= SAMPLE_NUM_TOTAL_ITEM_EX))
	{
		param_set_ptr->data_pointer_ex.sta	= RT_FALSE;
		param_set_ptr->data_pointer_ex.p	= 0;
	}
}

//传感器类型
static void _sample_set_sensor_type(rt_uint8_t ctrl_num, rt_uint8_t value)
{
	rt_uint32_t addr = SAMPLE_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_PARAM_SET, ctrl_blk[ctrl_num]);
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_CTRL_BLK, sensor_type);
	
	rtcee_eeprom_write(addr, &value, 1);
}

//传感器地址
static void _sample_set_sensor_addr(rt_uint8_t ctrl_num, rt_uint8_t value)
{
	rt_uint32_t addr = SAMPLE_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_PARAM_SET, ctrl_blk[ctrl_num]);
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_CTRL_BLK, sensor_addr);
	
	rtcee_eeprom_write(addr, &value, 1);
}

//硬件接口
static void _sample_set_hw_port(rt_uint8_t ctrl_num, rt_uint8_t value)
{
	rt_uint32_t addr = SAMPLE_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_PARAM_SET, ctrl_blk[ctrl_num]);
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_CTRL_BLK, hw_port);
	
	rtcee_eeprom_write(addr, &value, 1);
}

//协议
static void _sample_set_protocol(rt_uint8_t ctrl_num, rt_uint8_t value)
{
	rt_uint32_t addr = SAMPLE_BASE_ADDR_PARAM;
	
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_PARAM_SET, ctrl_blk[ctrl_num]);
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_CTRL_BLK, protocol);
	
	rtcee_eeprom_write(addr, &value, 1);
}

//硬件速率
static void _sample_set_hw_rate(rt_uint8_t ctrl_num, rt_uint32_t value)
{
	rt_uint32_t addr = SAMPLE_BASE_ADDR_PARAM;
	
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_PARAM_SET, ctrl_blk[ctrl_num]);
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_CTRL_BLK, hw_rate);
	
	rtcee_eeprom_write(addr, (rt_uint8_t *)&value, 4);
}

//存储间隔
static void _sample_set_store_period(rt_uint8_t ctrl_num, rt_uint16_t value)
{
	rt_uint32_t addr = SAMPLE_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_PARAM_SET, ctrl_blk[ctrl_num]);
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_CTRL_BLK, store_period);
	
	rtcee_eeprom_write(addr, (rt_uint8_t *)&value, 2);
}

//k_opt
static void _sample_set_kopt(rt_uint8_t ctrl_num, rt_uint8_t value)
{
	rt_uint32_t addr = SAMPLE_BASE_ADDR_PARAM;
	
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_PARAM_SET, ctrl_blk[ctrl_num]);
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_CTRL_BLK, k_opt);
	
	rtcee_eeprom_write(addr, &value, 1);
}

//k
static void _sample_set_k(rt_uint8_t ctrl_num, float value)
{
	rt_uint32_t addr = SAMPLE_BASE_ADDR_PARAM;
	
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_PARAM_SET, ctrl_blk[ctrl_num]);
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_CTRL_BLK, k);

	rtcee_eeprom_write(addr, (rt_uint8_t *)&value, 4);
}

//b
static void _sample_set_b(rt_uint8_t ctrl_num, float value)
{
	rt_uint32_t addr = SAMPLE_BASE_ADDR_PARAM;
	
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_PARAM_SET, ctrl_blk[ctrl_num]);
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_CTRL_BLK, b);

	rtcee_eeprom_write(addr, (rt_uint8_t *)&value, 4);
}

//基值
static void _sample_set_base(rt_uint8_t ctrl_num, float value)
{
	rt_uint32_t addr = SAMPLE_BASE_ADDR_PARAM;
	
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_PARAM_SET, ctrl_blk[ctrl_num]);
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_CTRL_BLK, base);

	rtcee_eeprom_write(addr, (rt_uint8_t *)&value, 4);
}

//修正值
static void _sample_set_offset(rt_uint8_t ctrl_num, float value)
{
	rt_uint32_t addr = SAMPLE_BASE_ADDR_PARAM;
	
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_PARAM_SET, ctrl_blk[ctrl_num]);
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_CTRL_BLK, offset);

	rtcee_eeprom_write(addr, (rt_uint8_t *)&value, 4);
}

//上限
static void _sample_set_up(rt_uint8_t ctrl_num, float value)
{
	rt_uint32_t addr = SAMPLE_BASE_ADDR_PARAM;
	
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_PARAM_SET, ctrl_blk[ctrl_num]);
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_CTRL_BLK, up);

	rtcee_eeprom_write(addr, (rt_uint8_t *)&value, 4);
}

//下限
static void _sample_set_down(rt_uint8_t ctrl_num, float value)
{
	rt_uint32_t addr = SAMPLE_BASE_ADDR_PARAM;
	
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_PARAM_SET, ctrl_blk[ctrl_num]);
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_CTRL_BLK, down);

	rtcee_eeprom_write(addr, (rt_uint8_t *)&value, 4);
}

//阈值
static void _sample_set_threshold(rt_uint8_t ctrl_num, float value)
{
	rt_uint32_t addr = SAMPLE_BASE_ADDR_PARAM;
	
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_PARAM_SET, ctrl_blk[ctrl_num]);
	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_CTRL_BLK, threshold);

	rtcee_eeprom_write(addr, (rt_uint8_t *)&value, 4);
}

//数据指针
static void _sample_set_data_pointer(SAMPLE_DATA_POINTER pointer)
{
	rt_uint32_t addr = SAMPLE_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_PARAM_SET, data_pointer);

	rtcee_eeprom_write(addr, (rt_uint8_t *)&pointer, sizeof(SAMPLE_DATA_POINTER));
}

//扩展数据指针
static void _sample_set_data_pointer_ex(SAMPLE_DATA_POINTER pointer)
{
	rt_uint32_t addr = SAMPLE_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(SAMPLE_PARAM_SET, data_pointer_ex);

	rtcee_eeprom_write(addr, (rt_uint8_t *)&pointer, sizeof(SAMPLE_DATA_POINTER));
}

