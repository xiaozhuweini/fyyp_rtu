#include "drv_rtcee.h"
#include "drv_w25qxxx.h"
#include "drv_fyyp.h"
#include "usbd_cdc_interface.h"



//参数集
static rt_uint8_t _ptcl_get_param_set(PTCL_PARAM_SET *param_set_ptr)
{
	return (RTCEE_EEPROM_ERR_NONE == rtcee_eeprom_read(PTCL_BASE_ADDR_PARAM, (rt_uint8_t *)param_set_ptr, sizeof(PTCL_PARAM_SET))) ? RT_TRUE : RT_FALSE;
}
static rt_uint8_t _ptcl_set_param_set(PTCL_PARAM_SET const *param_set_ptr)
{
	return (RTCEE_EEPROM_ERR_NONE == rtcee_eeprom_write(PTCL_BASE_ADDR_PARAM, (rt_uint8_t *)param_set_ptr, sizeof(PTCL_PARAM_SET))) ? RT_TRUE : RT_FALSE;
}
static void _ptcl_validate_param_set(PTCL_PARAM_SET *param_set_ptr)
{
	rt_uint8_t i, j;

	for(i = 0; i < PTCL_NUM_CENTER; i++)
	{
		if(param_set_ptr->center_info[i].ptcl_type >= PTCL_NUM_PTCL_TYPE)
		{
			param_set_ptr->center_info[i].ptcl_type = PTCL_PTCL_TYPE_HJ212;
		}
		
		for(j = 0; j < PTCL_NUM_COMM_PRIO; j++)
		{
			if(param_set_ptr->center_info[i].comm_type[j] >= PTCL_NUM_COMM_TYPE)
			{
				param_set_ptr->center_info[i].comm_type[j] = PTCL_COMM_TYPE_NONE;
			}
		}

		if((param_set_ptr->center_info[i].ram_ptr.in >= PTCL_NUM_CENTER_RAM) || (param_set_ptr->center_info[i].ram_ptr.out >= PTCL_NUM_CENTER_RAM))
		{
			param_set_ptr->center_info[i].ram_ptr.in	= 0;
			param_set_ptr->center_info[i].ram_ptr.out	= 0;
		}
	}
}
static void _ptcl_reset_param_set(PTCL_PARAM_SET *param_set_ptr)
{
	rt_uint8_t i, j;

	for(i = 0; i < PTCL_NUM_CENTER; i++)
	{
		param_set_ptr->center_info[i].center_addr	= 0;
		param_set_ptr->center_info[i].ptcl_type		= PTCL_PTCL_TYPE_HJ212;
		
		for(j = 0; j < PTCL_NUM_COMM_PRIO; j++)
		{
			param_set_ptr->center_info[i].comm_type[j] = PTCL_COMM_TYPE_NONE;
		}

		param_set_ptr->center_info[i].ram_ptr.in	= 0;
		param_set_ptr->center_info[i].ram_ptr.out	= 0;
	}
}

//中心站地址
static void _ptcl_set_center_addr(rt_uint8_t center_addr, rt_uint8_t center)
{
	rt_uint32_t addr = PTCL_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(PTCL_PARAM_SET, center_info[center]);
	addr += FYYP_MEMBER_ADDR_OFFSET(PTCL_CENTER_INFO, center_addr);

	rtcee_eeprom_write(addr, &center_addr, sizeof(rt_uint8_t));
}

//协议类型
static void _ptcl_set_ptcl_type(rt_uint8_t ptcl_type, rt_uint8_t center)
{
	rt_uint32_t addr = PTCL_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(PTCL_PARAM_SET, center_info[center]);
	addr += FYYP_MEMBER_ADDR_OFFSET(PTCL_CENTER_INFO, ptcl_type);

	rtcee_eeprom_write(addr, &ptcl_type, sizeof(rt_uint8_t));
}

//信道类型
static void _ptcl_set_comm_type(rt_uint8_t comm_type, rt_uint8_t center, rt_uint8_t prio)
{
	rt_uint32_t addr = PTCL_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(PTCL_PARAM_SET, center_info[center]);
	addr += FYYP_MEMBER_ADDR_OFFSET(PTCL_CENTER_INFO, comm_type[prio]);

	rtcee_eeprom_write(addr, &comm_type, sizeof(rt_uint8_t));
}

//内存指针
static void _ptcl_set_ram_ptr(PTCL_RAM_PTR const *ram_ptr, rt_uint8_t center)
{
	rt_uint32_t addr = PTCL_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(PTCL_PARAM_SET, center_info[center]);
	addr += FYYP_MEMBER_ADDR_OFFSET(PTCL_CENTER_INFO, ram_ptr);

	rtcee_eeprom_write(addr, (rt_uint8_t *)ram_ptr, sizeof(PTCL_RAM_PTR));
}



//透传信道发送数据
static rt_uint8_t _ptcl_tc_send(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	return usbd_send(pdata, data_len);
}



//内存数据存储(只对report_data_ptr->pdata进行互斥)
static void _ptcl_ram_data_push(PTCL_REPORT_DATA const *report_data_ptr, rt_uint32_t event, rt_uint8_t center)
{
	rt_uint8_t	*pmem;
	rt_uint32_t	ram_addr;

	//判断长度
	if((report_data_ptr->data_len + PTCL_BYTES_RAM_FEATURE) > PTCL_BYTES_RAM_DATA)
	{
		return;
	}
	//申请内存
	pmem = mempool_req(PTCL_BYTES_RAM_DATA, RT_WAITING_FOREVER);
	if((rt_uint8_t *)0 == pmem)
	{
		return;
	}
	//拷贝数据
	if(event)
	{
		if(RT_EOK != rt_event_recv(&m_ptcl_event_module, event, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
		{
			while(1);
		}
	}
	memcpy((void *)pmem, (void *)report_data_ptr->pdata, report_data_ptr->data_len);
	if(event)
	{
		if(RT_EOK != rt_event_send(&m_ptcl_event_module, event))
		{
			while(1);
		}
	}
	//数据属性
	ram_addr = PTCL_BYTES_RAM_DATA - PTCL_BYTES_RAM_FEATURE;
	*(rt_uint16_t *)(pmem + ram_addr) 	= report_data_ptr->data_len;
	ram_addr += 2;
	*(rt_uint16_t *)(pmem + ram_addr) 	= report_data_ptr->data_id;
	ram_addr += 2;
	pmem[ram_addr++]					= report_data_ptr->need_reply;
	pmem[ram_addr++]					= report_data_ptr->fcb_value;
	pmem[ram_addr]						= report_data_ptr->ptcl_type;
	//计算存储地址
	ram_addr = center;
	ram_addr *= PTCL_NUM_CENTER_RAM;
	_ptcl_param_pend(PTCL_PARAM_RAM_PTR << center);
	ram_addr += m_ptcl_center_info[center].ram_ptr.in;
	ram_addr *= PTCL_BYTES_RAM_DATA;
	ram_addr += PTCL_BASE_ADDR_RAM;
	if(W25QXXX_ERR_NONE == w25qxxx_write(ram_addr, pmem, PTCL_BYTES_RAM_DATA))
	{
		m_ptcl_center_info[center].ram_ptr.in++;
		if(m_ptcl_center_info[center].ram_ptr.in >= PTCL_NUM_CENTER_RAM)
		{
			m_ptcl_center_info[center].ram_ptr.in = 0;
		}
		if(m_ptcl_center_info[center].ram_ptr.in == m_ptcl_center_info[center].ram_ptr.out)
		{
			m_ptcl_center_info[center].ram_ptr.out++;
			if(m_ptcl_center_info[center].ram_ptr.out >= PTCL_NUM_CENTER_RAM)
			{
				m_ptcl_center_info[center].ram_ptr.out = 0;
			}
		}
		_ptcl_set_ram_ptr(&m_ptcl_center_info[center].ram_ptr, center);
	}
	_ptcl_param_post(PTCL_PARAM_RAM_PTR << center);
	//释放内存
	rt_mp_free((void *)pmem);
}

//内存数据读取
static rt_uint8_t _ptcl_ram_data_pop(PTCL_REPORT_DATA *report_data_ptr, rt_uint8_t center)
{
	rt_uint8_t		*pmem;
	rt_uint32_t		ram_addr;

	_ptcl_param_pend(PTCL_PARAM_RAM_PTR << center);
	//内存是否有数据
	if(m_ptcl_center_info[center].ram_ptr.in == m_ptcl_center_info[center].ram_ptr.out)
	{
		_ptcl_param_post(PTCL_PARAM_RAM_PTR << center);
		return RT_FALSE;
	}
	//申请内存
	pmem = mempool_req(PTCL_BYTES_RAM_DATA, RT_WAITING_FOREVER);
	if((rt_uint8_t *)0 == pmem)
	{
		_ptcl_param_post(PTCL_PARAM_RAM_PTR << center);
		return RT_FALSE;
	}
	//计算数据地址
	ram_addr = center;
	ram_addr *= PTCL_NUM_CENTER_RAM;
	ram_addr += m_ptcl_center_info[center].ram_ptr.out;
	ram_addr *= PTCL_BYTES_RAM_DATA;
	ram_addr += PTCL_BASE_ADDR_RAM;
	if(W25QXXX_ERR_NONE == w25qxxx_read(ram_addr, pmem, PTCL_BYTES_RAM_DATA))
	{
		m_ptcl_center_info[center].ram_ptr.out++;
		if(m_ptcl_center_info[center].ram_ptr.out >= PTCL_NUM_CENTER_RAM)
		{
			m_ptcl_center_info[center].ram_ptr.out = 0;
		}
		_ptcl_set_ram_ptr(&m_ptcl_center_info[center].ram_ptr, center);
		_ptcl_param_post(PTCL_PARAM_RAM_PTR << center);
	}
	else
	{
		_ptcl_param_post(PTCL_PARAM_RAM_PTR << center);
		rt_mp_free((void *)pmem);
		return RT_FALSE;
	}
	//数据属性
	ram_addr = PTCL_BYTES_RAM_DATA - PTCL_BYTES_RAM_FEATURE;
	report_data_ptr->data_len			= *(rt_uint16_t *)(pmem + ram_addr);
	ram_addr += 2;
	if((report_data_ptr->data_len + PTCL_BYTES_RAM_FEATURE) > PTCL_BYTES_RAM_DATA)
	{
		report_data_ptr->data_len = 0;
		rt_mp_free((void *)pmem);
		return RT_TRUE;
	}
	report_data_ptr->data_id			= *(rt_uint16_t *)(pmem + ram_addr);
	ram_addr += 2;
	report_data_ptr->need_reply			= pmem[ram_addr++];
	report_data_ptr->fcb_value			= pmem[ram_addr++];
	report_data_ptr->ptcl_type			= pmem[ram_addr];
	report_data_ptr->fun_csq_update		= (void *)0;
	//数据验证
	if(RT_FALSE == _ptcl_data_valid(pmem, report_data_ptr->data_len, report_data_ptr->data_id, report_data_ptr->need_reply, report_data_ptr->fcb_value, report_data_ptr->ptcl_type))
	{
		report_data_ptr->data_len = 0;
		rt_mp_free((void *)pmem);
		return RT_TRUE;
	}
	//拷贝数据
	memcpy((void *)report_data_ptr->pdata, (void *)pmem, report_data_ptr->data_len);
	//释放内存
	rt_mp_free((void *)pmem);
	
	return RT_TRUE;
}

