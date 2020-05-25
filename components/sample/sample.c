#include "sample.h"
#include "drv_w25qxxx.h"
#include "drv_debug.h"
#include "drv_tevent.h"
#include "drv_uart.h"
#include "drv_mempool.h"
#include "drv_lpm.h"
#include "drv_ads.h"
#include "string.h"



//事件标志组
static struct rt_event			m_sample_event_module;

static struct rt_mailbox		m_sample_mb_query_data_ex;
static rt_ubase_t				m_sample_msgpool_query_data_ex;

//采集控制块编号列表(每个要素对应一个编号)
static rt_uint8_t				m_sample_ctrl_blk_num[SAMPLE_NUM_SENSOR_TYPE];

#ifdef SAMPLE_TASK_STORE_EN
//任务
static struct rt_thread			m_sample_thread_data_store;
static rt_uint8_t				m_sample_stk_data_store[SAMPLE_STK_DATA_STORE];
#endif
static struct rt_thread			m_sample_thread_query_data_ex;
static rt_uint8_t				m_sample_stk_query_data_ex[SAMPLE_STK_QUERY_DATA_EX];

//传感器采集指令
static rt_uint8_t				m_sample_cmd_a[] = {1, 3, 0, 0, 0, 2, 0, 0};

//参数
static SAMPLE_PARAM_SET				m_sample_param_set;
#define m_sample_ctrl_blk			m_sample_param_set.ctrl_blk
#define m_sample_data_pointer		m_sample_param_set.data_pointer
#define m_sample_data_pointer_ex	m_sample_param_set.data_pointer_ex



#include "sample_io.c"



//串口对应关系
static rt_uint8_t _sample_get_uart_periph(rt_uint8_t hw_port, rt_uint8_t *uart_periph)
{
	switch(hw_port)
	{
	default:
		return RT_FALSE;
	case SAMPLE_HW_RS232_1:
		*uart_periph = UART_PERIPH_4;
		break;
	case SAMPLE_HW_RS232_2:
		*uart_periph = UART_PERIPH_5;
		break;
	case SAMPLE_HW_RS232_3:
		*uart_periph = UART_PERIPH_1;
		break;
	case SAMPLE_HW_RS232_4:
		*uart_periph = UART_PERIPH_8;
		break;
	case SAMPLE_HW_RS485_1:
		*uart_periph = UART_PERIPH_2;
		break;
	case SAMPLE_HW_RS485_2:
		*uart_periph = UART_PERIPH_7;
		break;
	}

	return RT_TRUE;
}

//串口接收数据
static SAMPLE_COM_RECV *_sample_com_recv_req(rt_uint16_t bytes_req, rt_int32_t ticks)
{
	SAMPLE_COM_RECV *com_recv_ptr;

	if(0 == bytes_req)
	{
		return (SAMPLE_COM_RECV *)0;
	}
	
	bytes_req += sizeof(SAMPLE_COM_RECV);
	com_recv_ptr = (SAMPLE_COM_RECV *)mempool_req(bytes_req, ticks);
	if((SAMPLE_COM_RECV *)0 != com_recv_ptr)
	{
		com_recv_ptr->pdata = (rt_uint8_t *)(com_recv_ptr + 1);
	}
	
	return com_recv_ptr;
}

//串口接收处理方法
static void _sample_com_recv_hdr(void *args, rt_uint8_t recv_data)
{
	if(((SAMPLE_COM_RECV *)args)->recv_event)
	{
		if(((SAMPLE_COM_RECV *)args)->data_len < SAMPLE_BYTES_COM_RECV)
		{
			((SAMPLE_COM_RECV *)args)->pdata[((SAMPLE_COM_RECV *)args)->data_len++] = recv_data;
		}
		rt_event_send(&m_sample_event_module, ((SAMPLE_COM_RECV *)args)->recv_event);
	}
}

//CRC校验函数
static rt_uint16_t _sample_crc_value(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint16_t	crc_value = 0xffff;
	rt_uint8_t	j;
	
	while(data_len)
	{
		data_len--;
		crc_value ^= *pdata++;
		for(j = 0; j < 8; j++)
		{
			if(crc_value & 0x01)
			{
				crc_value >>= 1;
				crc_value ^= 0xa001;
			}
			else
			{
				crc_value >>= 1;
			}
		}
	}
	
	return crc_value;
}

//串口接收的传感器数据解析
static rt_uint8_t _sample_com_data_decoder(float *pfloat, rt_uint8_t protocol, rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint16_t crc_value;
	
	if(0 == data_len)
	{
		return RT_FALSE;
	}
	
	switch(protocol)
	{
	default:
		return RT_FALSE;
	case SAMPLE_PTCL_A:
		if(9 == data_len)
		{
			data_len = 0;
			if(*(rt_uint8_t *)pfloat != pdata[data_len++])
			{
				return RT_FALSE;
			}
			if(3 != pdata[data_len++])
			{
				return RT_FALSE;
			}
			if(4 != pdata[data_len++])
			{
				return RT_FALSE;
			}
			*((rt_uint8_t *)pfloat + 1) = pdata[data_len++];
			*((rt_uint8_t *)pfloat + 0) = pdata[data_len++];
			*((rt_uint8_t *)pfloat + 3) = pdata[data_len++];
			*((rt_uint8_t *)pfloat + 2) = pdata[data_len++];
			crc_value = _sample_crc_value(pdata, data_len);
			if(crc_value != *(rt_uint16_t *)(pdata + data_len))
			{
				return RT_FALSE;
			}
			
			return RT_TRUE;
		}
		return RT_FALSE;
	}
}

//数据指针占用和释放
static void _sample_data_pointer_pend(void)
{
	if(RT_EOK != rt_event_recv(&m_sample_event_module, SAMPLE_EVENT_DATA_POINTER, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}
}
static void _sample_data_pointer_post(void)
{
	if(RT_EOK != rt_event_send(&m_sample_event_module, SAMPLE_EVENT_DATA_POINTER))
	{
		while(1);
	}
}
static void _sample_data_pointer_ex_pend(void)
{
	if(RT_EOK != rt_event_recv(&m_sample_event_module, SAMPLE_EVENT_DATA_POINTER_EX, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}
}
static void _sample_data_pointer_ex_post(void)
{
	if(RT_EOK != rt_event_send(&m_sample_event_module, SAMPLE_EVENT_DATA_POINTER_EX))
	{
		while(1);
	}
}

//采集控制块占用和释放
static void _sample_ctrl_blk_pend(rt_uint8_t ctrl_num)
{
	if(RT_EOK != rt_event_recv(&m_sample_event_module, SAMPLE_EVENT_CTRL_BLK << ctrl_num, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}
}
static void _sample_ctrl_blk_post(rt_uint8_t ctrl_num)
{
	if(RT_EOK != rt_event_send(&m_sample_event_module, SAMPLE_EVENT_CTRL_BLK << ctrl_num))
	{
		while(1);
	}
}

//数据存储
static void _sample_data_store(rt_uint8_t const *pdata)
{
	rt_uint32_t addr;
	
	_sample_data_pointer_pend();
	
	addr = m_sample_data_pointer.p;
	addr *= SAMPLE_BYTES_PER_ITEM;
	addr += SAMPLE_BASE_ADDR_DATA;
	if(W25QXXX_ERR_NONE == w25qxxx_write(addr, pdata, SAMPLE_BYTES_PER_ITEM))
	{
		m_sample_data_pointer.p++;
		if(m_sample_data_pointer.p >= SAMPLE_NUM_TOTAL_ITEM)
		{
			m_sample_data_pointer.p		= 0;
			m_sample_data_pointer.sta	= RT_TRUE;
		}
		_sample_set_data_pointer(m_sample_data_pointer);
	}

	_sample_data_pointer_post();
}
static void _sample_data_store_ex(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint32_t addr;
	
	_sample_data_pointer_ex_pend();
	
	addr = m_sample_data_pointer_ex.p;
	addr *= SAMPLE_BYTES_PER_ITEM_EX;
	addr += SAMPLE_BASE_ADDR_DATA_EX;
	if(W25QXXX_ERR_NONE == w25qxxx_write(addr, pdata, data_len))
	{
		m_sample_data_pointer_ex.p++;
		if(m_sample_data_pointer_ex.p >= SAMPLE_NUM_TOTAL_ITEM_EX)
		{
			m_sample_data_pointer_ex.p		= 0;
			m_sample_data_pointer_ex.sta	= RT_TRUE;
		}
		_sample_set_data_pointer_ex(m_sample_data_pointer_ex);
	}

	_sample_data_pointer_ex_post();
}

//查找低指针(在pL和pH范围内查找最接近于unix并低于unix的指针)
static rt_uint32_t _sample_find_pointer_low(rt_uint32_t unix, rt_uint32_t pointer_low, rt_uint32_t pointer_high, rt_uint32_t base_addr, rt_uint16_t item_len)
{
	rt_uint8_t	data[5];
	rt_uint32_t	p, unix_temp;
	struct tm	time;

	while(1)
	{
		p = (pointer_low + pointer_high) / 2;
		if(p == pointer_low)
		{
			break;
		}
		
		unix_temp = p;
		unix_temp *= item_len;
		unix_temp += base_addr;
		if(W25QXXX_ERR_NONE != w25qxxx_read_ex(unix_temp, data, 5))
		{
			return pointer_low;
		}
		unix_temp = 0;
		time.tm_year	= data[unix_temp++] + 2000;
		time.tm_mon		= data[unix_temp++] - 1;
		time.tm_mday	= data[unix_temp++];
		time.tm_hour	= data[unix_temp++];
		time.tm_min		= data[unix_temp];
		time.tm_sec		= 0;
		unix_temp = rtcee_rtc_calendar_to_unix(time);
		if(unix_temp < unix)
		{
			pointer_low = p;
		}
		else
		{
			pointer_high = p;
		}
	}

	return p;
}

//查找高指针(在pL和pH范围内查找最接近于unix并高于unix的指针)
static rt_uint32_t _sample_find_pointer_high(rt_uint32_t unix, rt_uint32_t pointer_low, rt_uint32_t pointer_high, rt_uint32_t base_addr, rt_uint16_t item_len)
{
	rt_uint8_t	data[5];
	rt_uint32_t	p, unix_temp;
	struct tm	time;

	while(1)
	{
		p = (pointer_low + pointer_high) / 2;
		if(p == pointer_low)
		{
			p = pointer_high;
			break;
		}
		
		unix_temp = p;
		unix_temp *= item_len;
		unix_temp += base_addr;
		if(W25QXXX_ERR_NONE != w25qxxx_read_ex(unix_temp, data, 5))
		{
			return pointer_high;
		}
		unix_temp = 0;
		time.tm_year	= data[unix_temp++] + 2000;
		time.tm_mon		= data[unix_temp++] - 1;
		time.tm_mday	= data[unix_temp++];
		time.tm_hour	= data[unix_temp++];
		time.tm_min		= data[unix_temp];
		time.tm_sec		= 0;
		unix_temp = rtcee_rtc_calendar_to_unix(time);
		if(unix_temp > unix)
		{
			pointer_high = p;
		}
		else
		{
			pointer_low = p;
		}
	}

	return p;
}

//查找数据指针范围
static void _sample_find_pointer_range(rt_uint32_t *pointer_low, rt_uint32_t *pointer_high, rt_uint32_t unix_low, rt_uint32_t unix_high)
{
	rt_uint8_t	data[5];
	rt_uint32_t	unix;
	struct tm	time;

	//未覆盖
	if(RT_FALSE == m_sample_data_pointer.sta)
	{
		unix = m_sample_data_pointer.p;
		if(unix)
		{
			unix--;
		}
		*pointer_low	= _sample_find_pointer_low(unix_low, 0, unix, SAMPLE_BASE_ADDR_DATA, SAMPLE_BYTES_PER_ITEM);
		*pointer_high	= _sample_find_pointer_high(unix_high, 0, unix, SAMPLE_BASE_ADDR_DATA, SAMPLE_BYTES_PER_ITEM);
	}
	//刚覆盖一遍，此时数据的时间戳是连续的
	else if(0 == m_sample_data_pointer.p)
	{
		unix = SAMPLE_NUM_TOTAL_ITEM - 1;
		*pointer_low	= _sample_find_pointer_low(unix_low, 0, unix, SAMPLE_BASE_ADDR_DATA, SAMPLE_BYTES_PER_ITEM);
		*pointer_high	= _sample_find_pointer_high(unix_high, 0, unix, SAMPLE_BASE_ADDR_DATA, SAMPLE_BYTES_PER_ITEM);
	}
	//覆盖至中间某处，此时数据的时间戳分成两部分，后半部分时间戳在前，前半部分时间戳在后
	else
	{
		//计算指针0处的时间戳
		if(W25QXXX_ERR_NONE != w25qxxx_read_ex(SAMPLE_BASE_ADDR_DATA, data, 5))
		{
			*pointer_low = 0;
			*pointer_high = 0;
			return;
		}
		unix = 0;
		time.tm_year	= data[unix++] + 2000;
		time.tm_mon		= data[unix++] - 1;
		time.tm_mday	= data[unix++];
		time.tm_hour	= data[unix++];
		time.tm_min		= data[unix];
		time.tm_sec		= 0;
		unix = rtcee_rtc_calendar_to_unix(time);
		//查找低指针
		if(unix < unix_low)
		{
			*pointer_low = _sample_find_pointer_low(unix_low, 0, m_sample_data_pointer.p - 1, SAMPLE_BASE_ADDR_DATA, SAMPLE_BYTES_PER_ITEM);
		}
		else
		{
			*pointer_low = _sample_find_pointer_low(unix_low, m_sample_data_pointer.p, SAMPLE_NUM_TOTAL_ITEM - 1, SAMPLE_BASE_ADDR_DATA, SAMPLE_BYTES_PER_ITEM);
		}
		//查找高指针
		if(unix > unix_high)
		{
			*pointer_high = _sample_find_pointer_high(unix_high, m_sample_data_pointer.p, SAMPLE_NUM_TOTAL_ITEM - 1, SAMPLE_BASE_ADDR_DATA, SAMPLE_BYTES_PER_ITEM);
		}
		else
		{
			*pointer_high = _sample_find_pointer_high(unix_high, 0, m_sample_data_pointer.p - 1, SAMPLE_BASE_ADDR_DATA, SAMPLE_BYTES_PER_ITEM);
		}
	}
}
static void _sample_find_pointer_ex_range(rt_uint32_t *pointer_low, rt_uint32_t *pointer_high, rt_uint32_t unix_low, rt_uint32_t unix_high)
{
	rt_uint8_t	data[5];
	rt_uint32_t	unix;
	struct tm	time;

	//未覆盖
	if(RT_FALSE == m_sample_data_pointer_ex.sta)
	{
		unix = m_sample_data_pointer_ex.p;
		if(unix)
		{
			unix--;
		}
		*pointer_low	= _sample_find_pointer_low(unix_low, 0, unix, SAMPLE_BASE_ADDR_DATA_EX, SAMPLE_BYTES_PER_ITEM_EX);
		*pointer_high	= _sample_find_pointer_high(unix_high, 0, unix, SAMPLE_BASE_ADDR_DATA_EX, SAMPLE_BYTES_PER_ITEM_EX);
	}
	//刚覆盖一遍，此时数据的时间戳是连续的
	else if(0 == m_sample_data_pointer_ex.p)
	{
		unix = SAMPLE_NUM_TOTAL_ITEM_EX - 1;
		*pointer_low	= _sample_find_pointer_low(unix_low, 0, unix, SAMPLE_BASE_ADDR_DATA_EX, SAMPLE_BYTES_PER_ITEM_EX);
		*pointer_high	= _sample_find_pointer_high(unix_high, 0, unix, SAMPLE_BASE_ADDR_DATA_EX, SAMPLE_BYTES_PER_ITEM_EX);
	}
	//覆盖至中间某处，此时数据的时间戳分成两部分，后半部分时间戳在前，前半部分时间戳在后
	else
	{
		//计算指针0处的时间戳
		if(W25QXXX_ERR_NONE != w25qxxx_read_ex(SAMPLE_BASE_ADDR_DATA_EX, data, 5))
		{
			*pointer_low = 0;
			*pointer_high = 0;
			return;
		}
		unix = 0;
		time.tm_year	= data[unix++] + 2000;
		time.tm_mon		= data[unix++] - 1;
		time.tm_mday	= data[unix++];
		time.tm_hour	= data[unix++];
		time.tm_min		= data[unix];
		time.tm_sec		= 0;
		unix = rtcee_rtc_calendar_to_unix(time);
		//查找低指针
		if(unix < unix_low)
		{
			*pointer_low = _sample_find_pointer_low(unix_low, 0, m_sample_data_pointer_ex.p - 1, SAMPLE_BASE_ADDR_DATA_EX, SAMPLE_BYTES_PER_ITEM_EX);
		}
		else
		{
			*pointer_low = _sample_find_pointer_low(unix_low, m_sample_data_pointer_ex.p, SAMPLE_NUM_TOTAL_ITEM_EX - 1, SAMPLE_BASE_ADDR_DATA_EX, SAMPLE_BYTES_PER_ITEM_EX);
		}
		//查找高指针
		if(unix > unix_high)
		{
			*pointer_high = _sample_find_pointer_high(unix_high, m_sample_data_pointer_ex.p, SAMPLE_NUM_TOTAL_ITEM_EX - 1, SAMPLE_BASE_ADDR_DATA_EX, SAMPLE_BYTES_PER_ITEM_EX);
		}
		else
		{
			*pointer_high = _sample_find_pointer_high(unix_high, 0, m_sample_data_pointer_ex.p - 1, SAMPLE_BASE_ADDR_DATA_EX, SAMPLE_BYTES_PER_ITEM_EX);
		}
	}
}

#ifdef SAMPLE_TASK_STORE_EN
//数据存储任务
static void _sample_task_data_store(void *parg)
{
	struct tm			time;
	rt_uint8_t			i, j, store_data[SAMPLE_BYTES_PER_ITEM];
	rt_uint16_t			temp16, period;
	SAMPLE_DATA_TYPE	data_type;

	while(1)
	{
		tevent_pend(TEVENT_EVENT_MIN);
		lpm_cpu_ref(RT_TRUE);

		//计算分钟数
		time = rtcee_rtc_get_calendar();
		time.tm_sec = 0;
		temp16 = time.tm_hour;
		temp16 *= 60;
		temp16 += time.tm_min;
//		if(temp16 < 480)
//		{
//			temp16 += 960;
//		}
//		else
//		{
//			temp16 -= 480;
//		}

		//对所有要素进行存储判断
		for(i = 0; i < SAMPLE_NUM_SENSOR_TYPE; i++)
		{
			//获取要素对应的采集控制块
			if(m_sample_ctrl_blk_num[i] < SAMPLE_NUM_CTRL_BLK)
			{
				//该要素的存储周期
				period = sample_get_store_period(m_sample_ctrl_blk_num[i]);
				//存储周期为0则不存储
				if(period)
				{
					//存储时间到
					if(0 == (temp16 % period))
					{
						//采集数据
						data_type = sample_get_cur_data(i);
						//组包(年、月、日、时、分、类型、状态、数值)
						j = 0;
						store_data[j++] = time.tm_year % 100;
						store_data[j++] = time.tm_mon + 1;
						store_data[j++] = time.tm_mday;
						store_data[j++] = time.tm_hour;
						store_data[j++] = time.tm_min;
						store_data[j++] = i;
						store_data[j++] = data_type.sta;
						memcpy((void *)(store_data + j), (void *)&data_type.value, 4);
						//存储
						_sample_data_store(store_data);
					}
				}
			}
		}

		lpm_cpu_ref(RT_FALSE);
	}
}
#endif

static void _sample_task_query_data_ex(void *parg)
{
	SAMPLE_QUERY_INFO_EX	*pinfo;
	rt_uint8_t				data[SAMPLE_BYTES_ITEM_INFO_EX], *pmem;
	rt_uint16_t				data_pos, data_len;
	rt_uint32_t				pointer_low, pointer_high, unix, addr;
	struct tm				time;

	while(1)
	{
		if(RT_EOK != rt_mb_recv(&m_sample_mb_query_data_ex, (rt_ubase_t *)&pinfo, RT_WAITING_FOREVER))
		{
			while(1);
		}
		lpm_cpu_ref(RT_TRUE);

		if((SAMPLE_QUERY_INFO_EX *)0 != pinfo)
		{
			if(pinfo->unix_low > pinfo->unix_high)
			{
				goto __exit;
			}
			if((rt_mailbox_t)0 == pinfo->pmb)
			{
				goto __exit;
			}

			_sample_data_pointer_ex_pend();
			w25qxxx_open_ex();
			
			//计算指针范围
			_sample_find_pointer_ex_range(&pointer_low, &pointer_high, pinfo->unix_low, pinfo->unix_high);
			
			while(1)
			{
				//计算pL点的时间戳unix
				addr = pointer_low;
				addr *= SAMPLE_BYTES_PER_ITEM_EX;
				addr += SAMPLE_BASE_ADDR_DATA_EX;
				if(W25QXXX_ERR_NONE != w25qxxx_read_ex(addr, data, SAMPLE_BYTES_ITEM_INFO_EX))
				{
					goto __next_pointer;
				}
				data_pos = 0;
				time.tm_year	= data[data_pos++] + 2000;
				time.tm_mon		= data[data_pos++] - 1;
				time.tm_mday	= data[data_pos++];
				time.tm_hour	= data[data_pos++];
				time.tm_min		= data[data_pos++];
				time.tm_sec		= 0;
				unix = rtcee_rtc_calendar_to_unix(time);
				//时间在范围内
				if((unix < pinfo->unix_low) || (unix > pinfo->unix_high))
				{
					goto __next_pointer;
				}
				//CN
				if(pinfo->cn != *(rt_uint16_t *)(data + data_pos))
				{
					goto __next_pointer;
				}
				data_pos += sizeof(rt_uint16_t);
				//数据长度
				data_len = *(rt_uint16_t *)(data + data_pos);
				data_pos += sizeof(rt_uint16_t);
				if((data_len + data_pos) > SAMPLE_BYTES_PER_ITEM_EX)
				{
					goto __next_pointer;
				}
				//数据内容
				pmem = (rt_uint8_t *)mempool_req(data_len + sizeof(rt_uint16_t), RT_WAITING_NO);
				if((rt_uint8_t *)0 == pmem)
				{
					goto __next_pointer;
				}
				*(rt_uint16_t *)pmem = data_len;
				if(W25QXXX_ERR_NONE != w25qxxx_read_ex(addr + SAMPLE_BYTES_ITEM_INFO_EX, pmem + sizeof(rt_uint16_t), data_len))
				{
					rt_mp_free((void *)pmem);
					goto __next_pointer;
				}
				if(RT_EOK != rt_mb_send_wait(pinfo->pmb, (rt_ubase_t)pmem, RT_WAITING_FOREVER))
				{
					while(1);
				}
				
__next_pointer:
				//是否是最后一个点
				if(pointer_low == pointer_high)
				{
					//退出数据下载
					break;
				}
				else
				{
					pointer_low++;
					if(pointer_low >= SAMPLE_NUM_TOTAL_ITEM_EX)
					{
						pointer_low = 0;
					}
				}
			}

			w25qxxx_close_ex();
			_sample_data_pointer_ex_post();

			if(RT_EOK != rt_mb_send_wait(pinfo->pmb, (rt_ubase_t)0, RT_WAITING_FOREVER))
			{
				while(1);
			}

__exit:
			rt_mp_free((void *)pinfo);
		}
		
		lpm_cpu_ref(RT_FALSE);
	}
}

static int _sample_component_init(void)
{
	rt_uint8_t i;
	
	//事件标志组
	if(RT_EOK != rt_event_init(&m_sample_event_module, "sample", RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	if(RT_EOK != rt_event_send(&m_sample_event_module, SAMPLE_EVENT_INIT_VALUE))
	{
		while(1);
	}

	if(RT_EOK != rt_mb_init(&m_sample_mb_query_data_ex, "sample", (void *)&m_sample_msgpool_query_data_ex, 1, RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	
	//参数
	if(RT_TRUE == _sample_get_param_set(&m_sample_param_set))
	{
		_sample_validate_param_set(&m_sample_param_set);
	}
	else
	{
		_sample_reset_param_set(&m_sample_param_set);
	}
	
	//采集控制块编号列表(每个要素对应一个编号)
	memset((void *)m_sample_ctrl_blk_num, SAMPLE_NUM_CTRL_BLK, SAMPLE_NUM_SENSOR_TYPE);
	for(i = 0; i < SAMPLE_NUM_CTRL_BLK; i++)
	{
		if(m_sample_ctrl_blk[i].sensor_type < SAMPLE_NUM_SENSOR_TYPE)
		{
			if(m_sample_ctrl_blk_num[m_sample_ctrl_blk[i].sensor_type] >= SAMPLE_NUM_CTRL_BLK)
			{
				m_sample_ctrl_blk_num[m_sample_ctrl_blk[i].sensor_type] = i;
			}
		}
	}

	return 0;
}
INIT_COMPONENT_EXPORT(_sample_component_init);

static int _sample_app_init(void)
{
#ifdef SAMPLE_TASK_STORE_EN
	//数据存储任务
	if(RT_EOK != rt_thread_init(&m_sample_thread_data_store, "sample", _sample_task_data_store, (void *)0, (void *)m_sample_stk_data_store, SAMPLE_STK_DATA_STORE, SAMPLE_PRIO_DATA_STORE, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_sample_thread_data_store))
	{
		while(1);
	}
#endif

	if(RT_EOK != rt_thread_init(&m_sample_thread_query_data_ex, "sample", _sample_task_query_data_ex, (void *)0, (void *)m_sample_stk_query_data_ex, SAMPLE_STK_QUERY_DATA_EX, SAMPLE_PRIO_QUERY_DATA_EX, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_sample_thread_query_data_ex))
	{
		while(1);
	}


	return 0;
}
INIT_APP_EXPORT(_sample_app_init);



void sample_set_sensor_type(rt_uint8_t ctrl_num, rt_uint8_t sensor_type)
{
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return;
	}
	if(sensor_type > SAMPLE_NUM_SENSOR_TYPE)
	{
		return;
	}

	//这个参数重启生效
	_sample_set_sensor_type(ctrl_num, sensor_type);
}
rt_uint8_t sample_get_sensor_type(rt_uint8_t ctrl_num)
{
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return SAMPLE_NUM_SENSOR_TYPE;
	}

	return m_sample_ctrl_blk[ctrl_num].sensor_type;
}

void sample_set_sensor_addr(rt_uint8_t ctrl_num, rt_uint8_t sensor_addr)
{
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	m_sample_ctrl_blk[ctrl_num].sensor_addr = sensor_addr;
	_sample_set_sensor_addr(ctrl_num, sensor_addr);
	_sample_ctrl_blk_post(ctrl_num);
}
rt_uint8_t sample_get_sensor_addr(rt_uint8_t ctrl_num)
{
	rt_uint8_t sensor_addr;
	
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return 0;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	sensor_addr = m_sample_ctrl_blk[ctrl_num].sensor_addr;
	_sample_ctrl_blk_post(ctrl_num);

	return sensor_addr;
}

void sample_set_hw_port(rt_uint8_t ctrl_num, rt_uint8_t hw_port)
{
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return;
	}
	if(hw_port >= SAMPLE_NUM_HW_PORT)
	{
		return;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	m_sample_ctrl_blk[ctrl_num].hw_port = hw_port;
	_sample_set_hw_port(ctrl_num, hw_port);
	_sample_ctrl_blk_post(ctrl_num);
}
rt_uint8_t sample_get_hw_port(rt_uint8_t ctrl_num)
{
	rt_uint8_t hw_port;
	
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return SAMPLE_NUM_HW_PORT;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	hw_port = m_sample_ctrl_blk[ctrl_num].hw_port;
	_sample_ctrl_blk_post(ctrl_num);

	return hw_port;
}

void sample_set_protocol(rt_uint8_t ctrl_num, rt_uint8_t protocol)
{
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return;
	}
	if(protocol >= SAMPLE_NUM_PTCL)
	{
		return;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	m_sample_ctrl_blk[ctrl_num].protocol = protocol;
	_sample_set_protocol(ctrl_num, protocol);
	_sample_ctrl_blk_post(ctrl_num);
}
rt_uint8_t sample_get_protocol(rt_uint8_t ctrl_num)
{
	rt_uint8_t protocol;
	
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return SAMPLE_NUM_PTCL;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	protocol = m_sample_ctrl_blk[ctrl_num].protocol;
	_sample_ctrl_blk_post(ctrl_num);

	return protocol;
}

void sample_set_hw_rate(rt_uint8_t ctrl_num, rt_uint32_t hw_rate)
{
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	m_sample_ctrl_blk[ctrl_num].hw_rate = hw_rate;
	_sample_set_hw_rate(ctrl_num, hw_rate);
	_sample_ctrl_blk_post(ctrl_num);
}
rt_uint32_t sample_get_hw_rate(rt_uint8_t ctrl_num)
{
	rt_uint32_t hw_rate;
	
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return 0;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	hw_rate = m_sample_ctrl_blk[ctrl_num].hw_rate;
	_sample_ctrl_blk_post(ctrl_num);

	return hw_rate;
}

void sample_set_store_period(rt_uint8_t ctrl_num, rt_uint16_t store_period)
{
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	m_sample_ctrl_blk[ctrl_num].store_period = store_period;
	_sample_set_store_period(ctrl_num, store_period);
	_sample_ctrl_blk_post(ctrl_num);
}
rt_uint16_t sample_get_store_period(rt_uint8_t ctrl_num)
{
	rt_uint16_t period;
	
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return 0;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	period = m_sample_ctrl_blk[ctrl_num].store_period;
	_sample_ctrl_blk_post(ctrl_num);

	return period;
}

void sample_set_kopt(rt_uint8_t ctrl_num, rt_uint8_t kopt)
{
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return;
	}
	if((RT_TRUE != kopt) && (RT_FALSE != kopt))
	{
		return;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	if(m_sample_ctrl_blk[ctrl_num].k_opt != kopt)
	{
		m_sample_ctrl_blk[ctrl_num].k_opt = kopt;
		_sample_set_kopt(ctrl_num, kopt);
	}
	_sample_ctrl_blk_post(ctrl_num);
}
rt_uint8_t sample_get_kopt(rt_uint8_t ctrl_num)
{
	rt_uint8_t kopt;
	
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return RT_FALSE;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	kopt = m_sample_ctrl_blk[ctrl_num].k_opt;
	_sample_ctrl_blk_post(ctrl_num);

	return kopt;
}

void sample_set_k(rt_uint8_t ctrl_num, float value)
{
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	m_sample_ctrl_blk[ctrl_num].k = value;
	_sample_set_k(ctrl_num, value);
	_sample_ctrl_blk_post(ctrl_num);
}
float sample_get_k(rt_uint8_t ctrl_num)
{
	float tmp_float;
	
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return (float)0;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	tmp_float = m_sample_ctrl_blk[ctrl_num].k;
	_sample_ctrl_blk_post(ctrl_num);

	return tmp_float;
}

void sample_set_b(rt_uint8_t ctrl_num, float value)
{
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	m_sample_ctrl_blk[ctrl_num].b = value;
	_sample_set_b(ctrl_num, value);
	_sample_ctrl_blk_post(ctrl_num);
}
float sample_get_b(rt_uint8_t ctrl_num)
{
	float tmp_float;
	
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return (float)0;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	tmp_float = m_sample_ctrl_blk[ctrl_num].b;
	_sample_ctrl_blk_post(ctrl_num);

	return tmp_float;
}

void sample_set_base(rt_uint8_t ctrl_num, float value)
{
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	m_sample_ctrl_blk[ctrl_num].base = value;
	_sample_set_base(ctrl_num, value);
	_sample_ctrl_blk_post(ctrl_num);
}
float sample_get_base(rt_uint8_t ctrl_num)
{
	float tmp_float;
	
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return (float)0;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	tmp_float = m_sample_ctrl_blk[ctrl_num].base;
	_sample_ctrl_blk_post(ctrl_num);

	return tmp_float;
}

void sample_set_offset(rt_uint8_t ctrl_num, float value)
{
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	m_sample_ctrl_blk[ctrl_num].offset = value;
	_sample_set_offset(ctrl_num, value);
	_sample_ctrl_blk_post(ctrl_num);
}
float sample_get_offset(rt_uint8_t ctrl_num)
{
	float tmp_float;
	
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return (float)0;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	tmp_float = m_sample_ctrl_blk[ctrl_num].offset;
	_sample_ctrl_blk_post(ctrl_num);

	return tmp_float;
}

void sample_set_up(rt_uint8_t ctrl_num, float value)
{
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	m_sample_ctrl_blk[ctrl_num].up = value;
	_sample_set_up(ctrl_num, value);
	_sample_ctrl_blk_post(ctrl_num);
}
float sample_get_up(rt_uint8_t ctrl_num)
{
	float tmp_float;
	
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return (float)0;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	tmp_float = m_sample_ctrl_blk[ctrl_num].up;
	_sample_ctrl_blk_post(ctrl_num);

	return tmp_float;
}

void sample_set_down(rt_uint8_t ctrl_num, float value)
{
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	m_sample_ctrl_blk[ctrl_num].down = value;
	_sample_set_down(ctrl_num, value);
	_sample_ctrl_blk_post(ctrl_num);
}
float sample_get_down(rt_uint8_t ctrl_num)
{
	float tmp_float;
	
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return (float)0;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	tmp_float = m_sample_ctrl_blk[ctrl_num].down;
	_sample_ctrl_blk_post(ctrl_num);

	return tmp_float;
}

void sample_set_threshold(rt_uint8_t ctrl_num, float value)
{
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	m_sample_ctrl_blk[ctrl_num].threshold = value;
	_sample_set_threshold(ctrl_num, value);
	_sample_ctrl_blk_post(ctrl_num);
}
float sample_get_threshold(rt_uint8_t ctrl_num)
{
	float tmp_float;
	
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		return (float)0;
	}

	_sample_ctrl_blk_pend(ctrl_num);
	tmp_float = m_sample_ctrl_blk[ctrl_num].threshold;
	_sample_ctrl_blk_post(ctrl_num);

	return tmp_float;
}

//恢复出厂
void sample_param_restore(void)
{
	rt_uint8_t i;

	//采集控制块
	for(i = 0; i < SAMPLE_NUM_CTRL_BLK; i++)
	{
		sample_set_sensor_type(i, SAMPLE_NUM_SENSOR_TYPE);
	}

	//数据指针
	sample_clear_old_data();
}

//是否存在
rt_uint8_t sample_sensor_type_exist(rt_uint8_t sensor_type)
{
	if(sensor_type >= SAMPLE_NUM_SENSOR_TYPE)
	{
		return RT_FALSE;
	}

	return (m_sample_ctrl_blk_num[sensor_type] < SAMPLE_NUM_CTRL_BLK) ? RT_TRUE : RT_FALSE;
}

//得到控制块
rt_uint8_t sample_get_ctrl_num_by_sensor_type(rt_uint8_t sensor_type)
{
	return (sensor_type < SAMPLE_NUM_SENSOR_TYPE) ? m_sample_ctrl_blk_num[sensor_type] : SAMPLE_NUM_CTRL_BLK;
}

//数据采集
SAMPLE_DATA_TYPE sample_get_cur_data(rt_uint8_t sensor_type)
{
	SAMPLE_DATA_TYPE	data_type;
	rt_uint8_t			ctrl_num, tmp, cmd_len, fcb, *pcmd;
	rt_uint16_t			crc_value;
	SAMPLE_COM_RECV		*com_recv_ptr;
	rt_base_t			level;
	rt_uint32_t			hw_event;

	data_type.sta = RT_FALSE;

	//是否存在该要素
	if(sensor_type >= SAMPLE_NUM_SENSOR_TYPE)
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_SAMPLE, ("\r\n[SAMPLE]采集要素[%d]不存在", sensor_type));
		return data_type;
	}
	//获取采集控制块号
	ctrl_num = m_sample_ctrl_blk_num[sensor_type];
	if(ctrl_num >= SAMPLE_NUM_CTRL_BLK)
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_SAMPLE, ("\r\n[SAMPLE]采集要素[%d]无对应控制块", sensor_type));
		return data_type;
	}
	
	//占用采集控制块
	_sample_ctrl_blk_pend(ctrl_num);

	DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_SAMPLE, ("\r\n[SAMPLE]采集要素[%d],控制块[%d],地址[%d],端口[%d],协议[%d],速率[%d]", sensor_type, ctrl_num, m_sample_ctrl_blk[ctrl_num].sensor_addr, m_sample_ctrl_blk[ctrl_num].hw_port, m_sample_ctrl_blk[ctrl_num].protocol, m_sample_ctrl_blk[ctrl_num].hw_rate));
	
	//根据硬件接口进行相应的采集操作
	switch(m_sample_ctrl_blk[ctrl_num].hw_port)
	{
	case SAMPLE_HW_ADC:
		if(RT_FALSE == ads_conv_value(m_sample_ctrl_blk[ctrl_num].sensor_addr, &crc_value))
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_SAMPLE, ("\r\n[SAMPLE]采集要素[%d],ADC转换失败", sensor_type));
			goto __hw_port_adc;
		}
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_SAMPLE, ("\r\n[SAMPLE]采集要素[%d],ADC转换值[%d]", sensor_type, crc_value));
		data_type.sta	= RT_TRUE;
		data_type.value	= (float)crc_value;
__hw_port_adc:
		break;
	case SAMPLE_HW_RS232_1:
	case SAMPLE_HW_RS232_2:
	case SAMPLE_HW_RS232_3:
	case SAMPLE_HW_RS232_4:
	case SAMPLE_HW_RS485_1:
	case SAMPLE_HW_RS485_2:
		//发送接收数据
		pcmd			= (rt_uint8_t *)0;
		com_recv_ptr	= (SAMPLE_COM_RECV *)0;
		hw_event		= SAMPLE_EVENT_HW_PORT << m_sample_ctrl_blk[ctrl_num].hw_port;
		//关联uart
		if(RT_FALSE == _sample_get_uart_periph(m_sample_ctrl_blk[ctrl_num].hw_port, &tmp))
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_SAMPLE, ("\r\n[SAMPLE]采集要素[%d],未找到关联uart", sensor_type));
			goto __hw_port_com;
		}
		//发送数据
		switch(m_sample_ctrl_blk[ctrl_num].protocol)
		{
		case SAMPLE_PTCL_A:
			cmd_len = sizeof(m_sample_cmd_a);
			pcmd = (rt_uint8_t *)mempool_req(cmd_len, RT_WAITING_NO);
			if((rt_uint8_t *)0 != pcmd)
			{
				memcpy((void *)pcmd, (void *)m_sample_cmd_a, cmd_len);
				*pcmd = m_sample_ctrl_blk[ctrl_num].sensor_addr;
				cmd_len -= sizeof(rt_uint16_t);
				crc_value = _sample_crc_value(pcmd, cmd_len);
				*(rt_uint16_t *)(pcmd + cmd_len) = crc_value;
				cmd_len += sizeof(rt_uint16_t);
			}
			break;
		}
		if((rt_uint8_t *)0 == pcmd)
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_SAMPLE, ("\r\n[SAMPLE]采集要素[%d],未生成采集协议对应的发送数据", sensor_type));
			goto __hw_port_com;
		}
		//接收数据
		com_recv_ptr = _sample_com_recv_req(SAMPLE_BYTES_COM_RECV, RT_WAITING_NO);
		if((SAMPLE_COM_RECV *)0 == com_recv_ptr)
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_SAMPLE, ("\r\n[SAMPLE]采集要素[%d],未生成采集协议对应的接收数据", sensor_type));
			goto __hw_port_com;
		}
		com_recv_ptr->recv_event = 0;
		//采集
		fcb = SAMPLE_ERROR_TIMES;
		uart_open(tmp, m_sample_ctrl_blk[ctrl_num].hw_rate, _sample_com_recv_hdr, (void *)com_recv_ptr);
		while(fcb--)
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_SAMPLE, ("\r\n[SAMPLE]采集要素[%d],uart发送数据:", sensor_type));
			DEBUG_INFO_OUTPUT_HEX(DEBUG_INFO_TYPE_SAMPLE, (pcmd, cmd_len));

			rt_event_recv(&m_sample_event_module, hw_event, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, RT_WAITING_NO, (rt_uint32_t *)0);
			
			level = rt_hw_interrupt_disable();
			com_recv_ptr->data_len		= 0;
			com_recv_ptr->recv_event	= hw_event;
			rt_hw_interrupt_enable(level);

			if(RT_FALSE == uart_send_data(tmp, pcmd, cmd_len))
			{
				break;
			}

			if(RT_EOK == rt_event_recv(&m_sample_event_module, hw_event, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, SAMPLE_TIME_DATA_NO * RT_TICK_PER_SECOND / 1000, (rt_uint32_t *)0))
			{
				while(RT_EOK == rt_event_recv(&m_sample_event_module, hw_event, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, SAMPLE_TIME_DATA_END * RT_TICK_PER_SECOND / 1000, (rt_uint32_t *)0));
			}

			level = rt_hw_interrupt_disable();
			com_recv_ptr->recv_event = 0;
			rt_hw_interrupt_enable(level);

			if(com_recv_ptr->data_len)
			{
				DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_SAMPLE, ("\r\n[SAMPLE]采集要素[%d],uart接收数据:", sensor_type));
				DEBUG_INFO_OUTPUT_HEX(DEBUG_INFO_TYPE_SAMPLE, (com_recv_ptr->pdata, com_recv_ptr->data_len));

				*(rt_uint8_t *)&data_type.value = m_sample_ctrl_blk[ctrl_num].sensor_addr;
				if(RT_TRUE == _sample_com_data_decoder(&data_type.value, m_sample_ctrl_blk[ctrl_num].protocol, com_recv_ptr->pdata, com_recv_ptr->data_len))
				{
					data_type.sta = RT_TRUE;
					break;
				}
			}
		}
		uart_close(tmp);
__hw_port_com:
		if((rt_uint8_t *)0 != pcmd)
		{
			rt_mp_free((void *)pcmd);
		}
		if((SAMPLE_COM_RECV *)0 != com_recv_ptr)
		{
			rt_mp_free((void *)com_recv_ptr);
		}
		break;
	}
	if(RT_TRUE == data_type.sta)
	{
		//计算
		if(RT_TRUE == m_sample_ctrl_blk[ctrl_num].k_opt)
		{
			data_type.value	/= m_sample_ctrl_blk[ctrl_num].k;
		}
		else
		{
			data_type.value	*= m_sample_ctrl_blk[ctrl_num].k;
		}
		data_type.value	+= m_sample_ctrl_blk[ctrl_num].b;
		data_type.value	+= m_sample_ctrl_blk[ctrl_num].base;
		data_type.value	+= m_sample_ctrl_blk[ctrl_num].offset;
	}
	
	//释放采集控制块
	_sample_ctrl_blk_post(ctrl_num);

	return data_type;
}

//数据清空
void sample_clear_old_data(void)
{
	_sample_data_pointer_pend();
	
	//数据指针
	m_sample_data_pointer.sta	= RT_FALSE;
	m_sample_data_pointer.p		= 0;
	_sample_set_data_pointer(m_sample_data_pointer);
	w25qxxx_write(SAMPLE_BASE_ADDR_DATA, (rt_uint8_t *)&m_sample_data_pointer.p, 4);

	_sample_data_pointer_post();

	_sample_data_pointer_ex_pend();
	
	//数据指针
	m_sample_data_pointer_ex.sta	= RT_FALSE;
	m_sample_data_pointer_ex.p		= 0;
	_sample_set_data_pointer_ex(m_sample_data_pointer_ex);
	w25qxxx_write(SAMPLE_BASE_ADDR_DATA_EX, (rt_uint8_t *)&m_sample_data_pointer_ex.p, 4);

	_sample_data_pointer_ex_post();
}

//数据查询
rt_uint16_t sample_query_data(rt_uint8_t *pdata, rt_uint8_t sensor_type, rt_uint32_t unix_low, rt_uint32_t unix_high, rt_uint32_t unix_step, SAMPLE_FUN_DATA_FORMAT fun_data_format)
{
	static rt_uint8_t	data[SAMPLE_BYTES_PER_ITEM];
	rt_uint8_t			data_pos;
	rt_uint16_t			data_len = 0;
	rt_uint32_t			pointer_low, pointer_high, unix, unix_match;
	struct tm			time;

	if((rt_uint8_t *)0 == pdata)
	{
		return 0;
	}
	if(sensor_type >= SAMPLE_NUM_SENSOR_TYPE)
	{
		return 0;
	}
	if(unix_low > unix_high)
	{
		return 0;
	}
	if(0 == unix_step)
	{
		return 0;
	}
	if((SAMPLE_FUN_DATA_FORMAT)0 == fun_data_format)
	{
		return 0;
	}

	unix_match = unix_low;
	
	_sample_data_pointer_pend();
	w25qxxx_open_ex();
	
	//计算指针范围
	_sample_find_pointer_range(&pointer_low, &pointer_high, unix_low, unix_high);
	
	while(1)
	{
		//计算pL点的时间戳unix
		unix = pointer_low;
		unix *= SAMPLE_BYTES_PER_ITEM;
		unix += SAMPLE_BASE_ADDR_DATA;
		if(W25QXXX_ERR_NONE != w25qxxx_read_ex(unix, data, SAMPLE_BYTES_PER_ITEM))
		{
			goto __next_pointer;
		}
		data_pos = 0;
		time.tm_year	= data[data_pos++] + 2000;
		time.tm_mon		= data[data_pos++] - 1;
		time.tm_mday	= data[data_pos++];
		time.tm_hour	= data[data_pos++];
		time.tm_min		= data[data_pos++];
		time.tm_sec		= 0;
		unix = rtcee_rtc_calendar_to_unix(time);
		//时间在范围内
		if((unix >= unix_match) && (unix <= unix_high))
		{
			//时间在步子上
			if(0 == ((unix - unix_match) % unix_step))
			{
				//传感器类型
				if(data[data_pos++] == sensor_type)
				{
					//数据填充至这个点
					while(unix_match <= unix)
					{
						//时间匹配
						if(unix_match == unix)
						{
							if(RT_TRUE == data[data_pos++])
							{
								data_len += fun_data_format(pdata + data_len, sensor_type, RT_TRUE, *((float *)(data + data_pos)));
							}
							else
							{
								data_len += fun_data_format(pdata + data_len, sensor_type, RT_FALSE, (float)0);
							}
						}
						else
						{
							data_len += fun_data_format(pdata + data_len, sensor_type, RT_FALSE, (float)0);
						}
						unix_match += unix_step;
					}
				}
			}
		}
__next_pointer:
		//是否是最后一个点
		if(pointer_low == pointer_high)
		{
			//数据填充完整
			while(unix_match <= unix_high)
			{
				data_len += fun_data_format(pdata + data_len, sensor_type, RT_FALSE, (float)0);
				unix_match += unix_step;
			}
			//退出数据下载
			break;
		}
		else
		{
			pointer_low++;
			if(pointer_low >= SAMPLE_NUM_TOTAL_ITEM)
			{
				pointer_low = 0;
			}
		}
	}

	w25qxxx_close_ex();
	_sample_data_pointer_post();

	return data_len;
}
rt_uint8_t sample_query_data_ex(rt_uint16_t cn, rt_uint32_t unix_low, rt_uint32_t unix_high, rt_mailbox_t pmb)
{
	SAMPLE_QUERY_INFO_EX *pinfo;

	if(unix_low > unix_high)
	{
		return RT_FALSE;
	}
	if((rt_mailbox_t)0 == pmb)
	{
		return RT_FALSE;
	}

	pinfo = (SAMPLE_QUERY_INFO_EX *)mempool_req(sizeof(SAMPLE_QUERY_INFO_EX), RT_WAITING_NO);
	if((SAMPLE_QUERY_INFO_EX *)0 == pinfo)
	{
		return RT_FALSE;
	}

	pinfo->cn			= cn;
	pinfo->unix_low		= unix_low;
	pinfo->unix_high	= unix_high;
	pinfo->pmb			= pmb;

	if(RT_EOK == rt_mb_send_wait(&m_sample_mb_query_data_ex, (rt_ubase_t)pinfo, RT_WAITING_NO))
	{
		return RT_TRUE;
	}

	rt_mp_free((void *)pinfo);
	return RT_FALSE;
}

//供电电压，0.01V
rt_uint16_t sample_supply_volt(rt_uint16_t adc_pin)
{
	ADC_HandleTypeDef		hadc;
	ADC_ChannelConfTypeDef	ch_conf;
	float					volt = 0;
	
	if((SAMPLE_PIN_ADC_BATT != adc_pin) && (SAMPLE_PIN_ADC_VIN != adc_pin))
	{
		return 0;
	}

	if(RT_EOK != rt_event_recv(&m_sample_event_module, SAMPLE_EVENT_SUPPLY_VOLT, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}

	//配置GPIO
	pin_clock_enable(adc_pin, RT_TRUE);
	pin_mode(adc_pin, PIN_MODE_ANALOG);
	//打开ADC时钟
	__HAL_RCC_ADC1_CLK_ENABLE();
	//配置ADC
	hadc.Instance = ADC1;
	if(HAL_OK != HAL_ADC_DeInit(&hadc))
	{
		while(1);
	}
	hadc.Init.ClockPrescaler		= ADC_CLOCK_SYNC_PCLK_DIV4;
	hadc.Init.Resolution			= ADC_RESOLUTION_12B;
	hadc.Init.DataAlign				= ADC_DATAALIGN_RIGHT;
	hadc.Init.ScanConvMode			= DISABLE;
	hadc.Init.EOCSelection			= ADC_EOC_SINGLE_CONV;
	hadc.Init.ContinuousConvMode	= DISABLE;
	hadc.Init.NbrOfConversion		= 1;
	hadc.Init.DiscontinuousConvMode	= DISABLE;
	hadc.Init.NbrOfDiscConversion	= 1;
	hadc.Init.ExternalTrigConv		= ADC_SOFTWARE_START;
	hadc.Init.ExternalTrigConvEdge	= ADC_EXTERNALTRIGCONVEDGE_RISING;
	hadc.Init.DMAContinuousRequests	= DISABLE;
	if(HAL_OK != HAL_ADC_Init(&hadc))
	{
		while(1);
	}
	//配置通道
	ch_conf.Channel					= (SAMPLE_PIN_ADC_BATT == adc_pin) ? ADC_CHANNEL_8 : ADC_CHANNEL_9;
	ch_conf.Rank					= 1;
	ch_conf.SamplingTime			= ADC_SAMPLETIME_480CYCLES;
	ch_conf.Offset					= 0;
	if(HAL_OK != HAL_ADC_ConfigChannel(&hadc, &ch_conf))
	{
		while(1);
	}
	//启动转换
	if(HAL_OK != HAL_ADC_Start(&hadc))
	{
		while(1);
	}
	rt_thread_delay(RT_TICK_PER_SECOND / 10);
	//判断EOC
	if(HAL_OK != HAL_ADC_PollForConversion(&hadc, 0))
	{
		goto __exit;
	}
	//计算结果
	volt = (float)HAL_ADC_GetValue(&hadc);
	volt *= 330;
	if(SAMPLE_PIN_ADC_VIN == adc_pin)
	{
		volt /= 20;
		volt *= 120;
	}
	else
	{
		volt /= 86.6;
		volt *= 597.6;
	}
	volt /= 4096;

__exit:
	HAL_ADC_Stop(&hadc);
	__HAL_RCC_ADC_FORCE_RESET();
	__HAL_RCC_ADC_RELEASE_RESET();
	__HAL_RCC_ADC1_CLK_DISABLE();
	pin_clock_enable(adc_pin, RT_FALSE);

	if(RT_EOK != rt_event_send(&m_sample_event_module, SAMPLE_EVENT_SUPPLY_VOLT))
	{
		while(1);
	}

	return (rt_uint16_t)volt;
}

void sample_store_data(struct tm const *ptime, rt_uint8_t sensor_type, SAMPLE_DATA_TYPE const *pdata)
{
	rt_uint8_t	*pmem;
	rt_uint32_t	i;

	pmem = (rt_uint8_t *)mempool_req(SAMPLE_BYTES_PER_ITEM, RT_WAITING_NO);
	if((rt_uint8_t *)0 == pmem)
	{
		return;
	}

	if(RT_TRUE == pdata->sta)
	{
		i = (rt_uint32_t)(pdata->value * 100);
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_SAMPLE, ("\r\n[sample]数据存储%02d-%02d-%02d %02d:%02d %02x %d.%02d", ptime->tm_year % 100, ptime->tm_mon + 1, ptime->tm_mday, ptime->tm_hour, ptime->tm_min, sensor_type, i / 100, i % 100));
	}
	else
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_SAMPLE, ("\r\n[sample]数据存储%02d-%02d-%02d %02d:%02d %02x 无效值", ptime->tm_year % 100, ptime->tm_mon + 1, ptime->tm_mday, ptime->tm_hour, ptime->tm_min, sensor_type));
	}

	//组包(年、月、日、时、分、类型、状态、数值)
	i = 0;
	pmem[i++] = ptime->tm_year % 100;
	pmem[i++] = ptime->tm_mon + 1;
	pmem[i++] = ptime->tm_mday;
	pmem[i++] = ptime->tm_hour;
	pmem[i++] = ptime->tm_min;
	pmem[i++] = sensor_type;
	pmem[i++] = pdata->sta;
	memcpy((void *)(pmem + i), (void *)&pdata->value, 4);
	//存储
	_sample_data_store(pmem);
	rt_mp_free((void *)pmem);
}

void sample_store_data_ex(struct tm const *ptime, rt_uint16_t cn, rt_uint16_t data_len, rt_uint8_t const *pdata)
{
	rt_uint8_t	*pmem;
	rt_uint16_t	i;

	if((data_len + SAMPLE_BYTES_ITEM_INFO_EX) > SAMPLE_BYTES_PER_ITEM_EX)
	{
		return;
	}

	pmem = (rt_uint8_t *)mempool_req(data_len + SAMPLE_BYTES_ITEM_INFO_EX, RT_WAITING_NO);
	if((rt_uint8_t *)0 == pmem)
	{
		return;
	}

	//组包(年、月、日、时、分、CN、LEN、数据)
	i = 0;
	pmem[i++]					= ptime->tm_year % 100;
	pmem[i++]					= ptime->tm_mon + 1;
	pmem[i++]					= ptime->tm_mday;
	pmem[i++]					= ptime->tm_hour;
	pmem[i++]					= ptime->tm_min;
	*(rt_uint16_t *)(pmem + i)	= cn;
	i += sizeof(rt_uint16_t);
	*(rt_uint16_t *)(pmem + i)	= data_len;
	i += sizeof(rt_uint16_t);
	memcpy((void *)(pmem + i), (void *)pdata, data_len);
	i += data_len;

	DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_TEST, ("\r\n[sample]扩展数据信息:"));
	DEBUG_INFO_OUTPUT_HEX(DEBUG_INFO_TYPE_TEST, (pmem, SAMPLE_BYTES_ITEM_INFO_EX));
	DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_TEST, ("\r\n[sample]扩展数据内容:"));
	DEBUG_INFO_OUTPUT(DEBUG_INFO_TYPE_TEST, (pdata, data_len));

//	DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_SAMPLE, ("\r\n[sample]扩展数据信息:"));
//	DEBUG_INFO_OUTPUT_HEX(DEBUG_INFO_TYPE_SAMPLE, (pmem, SAMPLE_BYTES_ITEM_INFO_EX));
//	DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_SAMPLE, ("\r\n[sample]扩展数据内容:"));
//	DEBUG_INFO_OUTPUT(DEBUG_INFO_TYPE_SAMPLE, (pdata, data_len));
	
	//存储
	_sample_data_store_ex(pmem, i);
	rt_mp_free((void *)pmem);
}

void sample_query_min(rt_uint8_t sensor_type, rt_uint32_t unix_low, rt_uint32_t unix_high, rt_uint32_t unix_step, SAMPLE_DATA_TYPE *pval)
{
	rt_uint8_t			data_pos, *pdata;
	rt_uint32_t			pointer_low, pointer_high, unix;
	struct tm			time;

	pval->sta = RT_FALSE;
	
	if(!SAMPLE_IS_SENSOR_TYPE(sensor_type))
	{
		return;
	}
	if(unix_low > unix_high)
	{
		return;
	}
	if(0 == unix_step)
	{
		return;
	}

	pdata = (rt_uint8_t *)mempool_req(SAMPLE_BYTES_PER_ITEM, RT_WAITING_NO);
	if((rt_uint8_t *)0 == pdata)
	{
		return;
	}

	_sample_data_pointer_pend();
	w25qxxx_open_ex();
	
	//计算指针范围
	_sample_find_pointer_range(&pointer_low, &pointer_high, unix_low, unix_high);
	
	while(1)
	{
		//计算pL点的时间戳unix
		unix = pointer_low;
		unix *= SAMPLE_BYTES_PER_ITEM;
		unix += SAMPLE_BASE_ADDR_DATA;
		if(W25QXXX_ERR_NONE != w25qxxx_read_ex(unix, pdata, SAMPLE_BYTES_PER_ITEM))
		{
			goto __next_pointer;
		}
		data_pos = 0;
		time.tm_year	= pdata[data_pos++] + 2000;
		time.tm_mon		= pdata[data_pos++] - 1;
		time.tm_mday	= pdata[data_pos++];
		time.tm_hour	= pdata[data_pos++];
		time.tm_min		= pdata[data_pos++];
		time.tm_sec		= 0;
		unix = rtcee_rtc_calendar_to_unix(time);
		//时间在范围内
		if((unix < unix_low) || (unix > unix_high))
		{
			goto __next_pointer;
		}
		//时间在步子上
		if((unix - unix_low) % unix_step)
		{
			goto __next_pointer;
		}
		//传感器类型
		if(pdata[data_pos++] != sensor_type)
		{
			goto __next_pointer;
		}
		//数据有效
		if(RT_TRUE != pdata[data_pos++])
		{
			goto __next_pointer;
		}
		//计算
		if(RT_FALSE == pval->sta)
		{
			pval->sta	= RT_TRUE;
			pval->value	= *(float *)(pdata + data_pos);
		}
		else if(pval->value > *(float *)(pdata + data_pos))
		{
			pval->value	= *(float *)(pdata + data_pos);
		}
__next_pointer:
		//是否是最后一个点
		if(pointer_low == pointer_high)
		{
			break;
		}
		else
		{
			pointer_low++;
			if(pointer_low >= SAMPLE_NUM_TOTAL_ITEM)
			{
				pointer_low = 0;
			}
		}
	}

	w25qxxx_close_ex();
	_sample_data_pointer_post();

	rt_mp_free((void *)pdata);
}

void sample_query_max(rt_uint8_t sensor_type, rt_uint32_t unix_low, rt_uint32_t unix_high, rt_uint32_t unix_step, SAMPLE_DATA_TYPE *pval)
{
	rt_uint8_t			data_pos, *pdata;
	rt_uint32_t			pointer_low, pointer_high, unix;
	struct tm			time;

	pval->sta = RT_FALSE;
	
	if(!SAMPLE_IS_SENSOR_TYPE(sensor_type))
	{
		return;
	}
	if(unix_low > unix_high)
	{
		return;
	}
	if(0 == unix_step)
	{
		return;
	}

	pdata = (rt_uint8_t *)mempool_req(SAMPLE_BYTES_PER_ITEM, RT_WAITING_NO);
	if((rt_uint8_t *)0 == pdata)
	{
		return;
	}

	_sample_data_pointer_pend();
	w25qxxx_open_ex();
	
	//计算指针范围
	_sample_find_pointer_range(&pointer_low, &pointer_high, unix_low, unix_high);
	
	while(1)
	{
		//计算pL点的时间戳unix
		unix = pointer_low;
		unix *= SAMPLE_BYTES_PER_ITEM;
		unix += SAMPLE_BASE_ADDR_DATA;
		if(W25QXXX_ERR_NONE != w25qxxx_read_ex(unix, pdata, SAMPLE_BYTES_PER_ITEM))
		{
			goto __next_pointer;
		}
		data_pos = 0;
		time.tm_year	= pdata[data_pos++] + 2000;
		time.tm_mon		= pdata[data_pos++] - 1;
		time.tm_mday	= pdata[data_pos++];
		time.tm_hour	= pdata[data_pos++];
		time.tm_min		= pdata[data_pos++];
		time.tm_sec		= 0;
		unix = rtcee_rtc_calendar_to_unix(time);
		//时间在范围内
		if((unix < unix_low) || (unix > unix_high))
		{
			goto __next_pointer;
		}
		//时间在步子上
		if((unix - unix_low) % unix_step)
		{
			goto __next_pointer;
		}
		//传感器类型
		if(pdata[data_pos++] != sensor_type)
		{
			goto __next_pointer;
		}
		//数据有效
		if(RT_TRUE != pdata[data_pos++])
		{
			goto __next_pointer;
		}
		//计算
		if(RT_FALSE == pval->sta)
		{
			pval->sta	= RT_TRUE;
			pval->value	= *(float *)(pdata + data_pos);
		}
		else if(pval->value < *(float *)(pdata + data_pos))
		{
			pval->value	= *(float *)(pdata + data_pos);
		}
__next_pointer:
		//是否是最后一个点
		if(pointer_low == pointer_high)
		{
			break;
		}
		else
		{
			pointer_low++;
			if(pointer_low >= SAMPLE_NUM_TOTAL_ITEM)
			{
				pointer_low = 0;
			}
		}
	}

	w25qxxx_close_ex();
	_sample_data_pointer_post();

	rt_mp_free((void *)pdata);
}

void sample_query_avg(rt_uint8_t sensor_type, rt_uint32_t unix_low, rt_uint32_t unix_high, rt_uint32_t unix_step, SAMPLE_DATA_TYPE *pval)
{
	rt_uint8_t			data_pos, *pdata;
	rt_uint32_t			pointer_low, pointer_high, unix, num = 0;
	struct tm			time;

	pval->sta = RT_FALSE;
	
	if(!SAMPLE_IS_SENSOR_TYPE(sensor_type))
	{
		return;
	}
	if(unix_low > unix_high)
	{
		return;
	}
	if(0 == unix_step)
	{
		return;
	}

	pdata = (rt_uint8_t *)mempool_req(SAMPLE_BYTES_PER_ITEM, RT_WAITING_NO);
	if((rt_uint8_t *)0 == pdata)
	{
		return;
	}

	_sample_data_pointer_pend();
	w25qxxx_open_ex();
	
	//计算指针范围
	_sample_find_pointer_range(&pointer_low, &pointer_high, unix_low, unix_high);
	
	while(1)
	{
		//计算pL点的时间戳unix
		unix = pointer_low;
		unix *= SAMPLE_BYTES_PER_ITEM;
		unix += SAMPLE_BASE_ADDR_DATA;
		if(W25QXXX_ERR_NONE != w25qxxx_read_ex(unix, pdata, SAMPLE_BYTES_PER_ITEM))
		{
			goto __next_pointer;
		}
		data_pos = 0;
		time.tm_year	= pdata[data_pos++] + 2000;
		time.tm_mon		= pdata[data_pos++] - 1;
		time.tm_mday	= pdata[data_pos++];
		time.tm_hour	= pdata[data_pos++];
		time.tm_min		= pdata[data_pos++];
		time.tm_sec		= 0;
		unix = rtcee_rtc_calendar_to_unix(time);
		//时间在范围内
		if((unix < unix_low) || (unix > unix_high))
		{
			goto __next_pointer;
		}
		//时间在步子上
		if((unix - unix_low) % unix_step)
		{
			goto __next_pointer;
		}
		//传感器类型
		if(pdata[data_pos++] != sensor_type)
		{
			goto __next_pointer;
		}
		//数据有效
		if(RT_TRUE != pdata[data_pos++])
		{
			goto __next_pointer;
		}
		//计算
		num++;
		if(RT_FALSE == pval->sta)
		{
			pval->sta	= RT_TRUE;
			pval->value	= *(float *)(pdata + data_pos);
		}
		else
		{
			pval->value += *(float *)(pdata + data_pos);
		}
__next_pointer:
		//是否是最后一个点
		if(pointer_low == pointer_high)
		{
			break;
		}
		else
		{
			pointer_low++;
			if(pointer_low >= SAMPLE_NUM_TOTAL_ITEM)
			{
				pointer_low = 0;
			}
		}
	}

	w25qxxx_close_ex();
	_sample_data_pointer_post();

	rt_mp_free((void *)pdata);

	if((RT_TRUE == pval->sta) && (num > 1))
	{
		pval->value /= num;
	}
}

void sample_query_cou(rt_uint8_t sensor_type, rt_uint32_t unix_low, rt_uint32_t unix_high, rt_uint32_t unix_step, SAMPLE_DATA_TYPE *pval)
{
	rt_uint8_t			data_pos, *pdata;
	rt_uint32_t			pointer_low, pointer_high, unix;
	struct tm			time;

	pval->sta = RT_FALSE;
	
	if(!SAMPLE_IS_SENSOR_TYPE(sensor_type))
	{
		return;
	}
	if(unix_low > unix_high)
	{
		return;
	}
	if(0 == unix_step)
	{
		return;
	}

	pdata = (rt_uint8_t *)mempool_req(SAMPLE_BYTES_PER_ITEM, RT_WAITING_NO);
	if((rt_uint8_t *)0 == pdata)
	{
		return;
	}

	_sample_data_pointer_pend();
	w25qxxx_open_ex();
	
	//计算指针范围
	_sample_find_pointer_range(&pointer_low, &pointer_high, unix_low, unix_high);
	
	while(1)
	{
		//计算pL点的时间戳unix
		unix = pointer_low;
		unix *= SAMPLE_BYTES_PER_ITEM;
		unix += SAMPLE_BASE_ADDR_DATA;
		if(W25QXXX_ERR_NONE != w25qxxx_read_ex(unix, pdata, SAMPLE_BYTES_PER_ITEM))
		{
			goto __next_pointer;
		}
		data_pos = 0;
		time.tm_year	= pdata[data_pos++] + 2000;
		time.tm_mon		= pdata[data_pos++] - 1;
		time.tm_mday	= pdata[data_pos++];
		time.tm_hour	= pdata[data_pos++];
		time.tm_min		= pdata[data_pos++];
		time.tm_sec		= 0;
		unix = rtcee_rtc_calendar_to_unix(time);
		//时间在范围内
		if((unix < unix_low) || (unix > unix_high))
		{
			goto __next_pointer;
		}
		//时间在步子上
		if((unix - unix_low) % unix_step)
		{
			goto __next_pointer;
		}
		//传感器类型
		if(pdata[data_pos++] != sensor_type)
		{
			goto __next_pointer;
		}
		//数据有效
		if(RT_TRUE != pdata[data_pos++])
		{
			goto __next_pointer;
		}
		//计算
		if(RT_FALSE == pval->sta)
		{
			pval->sta	= RT_TRUE;
			pval->value	= *(float *)(pdata + data_pos);
		}
		else
		{
			pval->value += *(float *)(pdata + data_pos);
		}
__next_pointer:
		//是否是最后一个点
		if(pointer_low == pointer_high)
		{
			break;
		}
		else
		{
			pointer_low++;
			if(pointer_low >= SAMPLE_NUM_TOTAL_ITEM)
			{
				pointer_low = 0;
			}
		}
	}

	w25qxxx_close_ex();
	_sample_data_pointer_post();

	rt_mp_free((void *)pdata);
}

