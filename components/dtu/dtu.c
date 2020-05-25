#include "dtu.h"
#include "drv_mempool.h"
#include "drv_debug.h"
#include "drv_tevent.h"
#include "string.h"



//数据接收mailbox
static rt_ubase_t				m_dtu_msgpool_recv_data[DTU_NUM_RECV_DATA];
static struct rt_mailbox		m_dtu_mb_recv_data;

//接收缓冲区
static rt_uint8_t				m_dtu_recv_buf[DTU_BYTES_RECV_BUF];
static rt_uint16_t				m_dtu_recv_buf_len							= 0;
static rt_ubase_t				m_dtu_msgpool_at_data[DTU_NUM_AT_DATA];
static struct rt_mailbox		m_dtu_mb_at_data;

//来电号码
static DTU_SMS_ADDR				m_dtu_telephone_nbr, *m_dtu_telephone_nbr_ptr;
static rt_ubase_t				m_dtu_msgpool_telephone_nbr;
static struct rt_mailbox		m_dtu_mb_telephone_nbr;

//拨打电话
static rt_ubase_t				m_dtu_msgpool_dial_phone;
static struct rt_mailbox		m_dtu_mb_dial_phone;

//http数据接收mailbox
static rt_ubase_t				m_dtu_msgpool_http_data[DTU_NUM_HTTP_DATA];
static struct rt_mailbox		m_dtu_mb_http_data;

//运行管理
static struct rt_semaphore		m_dtu_sem_module;
static struct rt_event			m_dtu_event_param;
static struct rt_event			m_dtu_event_module;
static struct rt_event			m_dtu_event_module_ex;
static rt_uint8_t				m_dtu_module_sta							= 0;
static rt_uint8_t				m_dtu_conn_sta								= 0;
static rt_uint16_t				m_dtu_time_sms_idle							= 0;
static rt_uint16_t				m_dtu_time_ip_idle[DTU_NUM_IP_CH];
static rt_uint16_t				m_dtu_time_heart[DTU_NUM_IP_CH];
static rt_uint8_t				m_dtu_hold_ip								= 0;
static rt_uint8_t				m_dtu_hold_sms								= 0;

//运行数据
static rt_uint8_t				m_dtu_csq_value;
static DTU_FUN_HEART_ENCODER	m_dtu_fun_heart_encoder						= (DTU_FUN_HEART_ENCODER)0;

//任务
static struct rt_thread			m_dtu_thread_time_tick;
static struct rt_thread			m_dtu_thread_telephone_handler;
static struct rt_thread			m_dtu_thread_recv_buf_handler;
static struct rt_thread			m_dtu_thread_dial_phone;
static struct rt_thread			m_dtu_thread_at_data_handler;
static rt_uint8_t				m_dtu_stk_time_tick[DTU_STK_TIME_TICK];
static rt_uint8_t				m_dtu_stk_telephone_handler[DTU_STK_TELEPHONE_HANDLER];
static rt_uint8_t				m_dtu_stk_recv_buf_handler[DTU_STK_RECV_BUF_HANDLER];
static rt_uint8_t				m_dtu_stk_dial_phone[DTU_STK_DIAL_PHONE];
static rt_uint8_t				m_dtu_stk_at_data_handler[DTU_STK_AT_DATA_HANDLER];

//参数
static DTU_PARAM_SET			m_dtu_param_set;
#define m_dtu_ip_addr			m_dtu_param_set.ip_addr
#define m_dtu_sms_addr			m_dtu_param_set.sms_addr
#define m_dtu_ip_type			m_dtu_param_set.ip_type
#define m_dtu_heart_period		m_dtu_param_set.heart_period
#define m_dtu_run_mode			m_dtu_param_set.run_mode
#define m_dtu_super_phone		m_dtu_param_set.super_phone
#define m_dtu_ip_ch				m_dtu_param_set.ip_ch
#define m_dtu_sms_ch			m_dtu_param_set.sms_ch
#define m_dtu_apn				m_dtu_param_set.apn



static void _dtu_recv_buf_fill(void *parg, rt_uint8_t recv_data)
{
	if(m_dtu_recv_buf_len < DTU_BYTES_RECV_BUF)
	{
		m_dtu_recv_buf[m_dtu_recv_buf_len++] = recv_data;
	}

	if(m_dtu_recv_buf_len < DTU_BYTES_RECV_BUF)
	{
		rt_event_send(&m_dtu_event_module_ex, DTU_EVENT_RECV_BUF);
	}
	else
	{
		rt_event_send(&m_dtu_event_module_ex, DTU_EVENT_RECV_BUF_FULL);
	}
}



#include "dtu_io.c"

#include "pdu.c"



//比较电话号码
static rt_uint8_t _dtu_compare_phone(DTU_SMS_ADDR const *sms_addr_ptr1, DTU_SMS_ADDR const *sms_addr_ptr2)
{
	rt_uint8_t i, j, k, len;

	k = strlen("+86");
	
	if(sms_addr_ptr1->addr_len < k)
	{
		i = 0;
	}
	else if(0 == memcmp((void *)sms_addr_ptr1->addr_data, (void *)"+86", k))
	{
		i = k;
	}
	else
	{
		i = 0;
	}
	
	if(sms_addr_ptr2->addr_len < k)
	{
		j = 0;
	}
	else if(0 == memcmp((void *)sms_addr_ptr2->addr_data, (void *)"+86", k))
	{
		j = k;
	}
	else
	{
		j = 0;
	}
	
	if((sms_addr_ptr1->addr_len - i) != (sms_addr_ptr2->addr_len - j))
	{
		return RT_FALSE;
	}
	
	len = sms_addr_ptr1->addr_len - i;
	if(0 == memcmp((void *)(sms_addr_ptr1->addr_data + i), (void *)(sms_addr_ptr2->addr_data + j), len))
	{
		return RT_TRUE;
	}

	return RT_FALSE;
}

//接收数据申请
static DTU_RECV_DATA *_dtu_recv_data_req(rt_uint16_t bytes_req, rt_int32_t ticks)
{
	DTU_RECV_DATA *recv_data_ptr;

	if(0 == bytes_req)
	{
		return (DTU_RECV_DATA *)0;
	}
	
	bytes_req += sizeof(DTU_RECV_DATA);
	recv_data_ptr = (DTU_RECV_DATA *)mempool_req(bytes_req, ticks);
	if((DTU_RECV_DATA *)0 != recv_data_ptr)
	{
		recv_data_ptr->pdata = (rt_uint8_t *)(recv_data_ptr + 1);
	}
	
	return recv_data_ptr;
}

//保持在线时间
static rt_uint16_t _dtu_hold_time(rt_uint8_t comm_type, rt_uint8_t ch)
{
	rt_base_t	level;
	rt_uint16_t	time;
	
	switch(comm_type)
	{
	case DTU_COMM_TYPE_SMS:
		level = rt_hw_interrupt_disable();
		if(m_dtu_hold_sms)
		{
			rt_hw_interrupt_enable(level);
			time = DTU_TIME_SMS_IDLE_L;
		}
		else
		{
			rt_hw_interrupt_enable(level);
			time = DTU_TIME_SMS_IDLE_S;
		}
		break;
	case DTU_COMM_TYPE_IP:
		if(ch < DTU_NUM_IP_CH)
		{
			ch = 1 << ch;
			level = rt_hw_interrupt_disable();
			if(m_dtu_hold_ip & ch)
			{
				rt_hw_interrupt_enable(level);
				time = DTU_TIME_IP_IDLE_L;
			}
			else
			{
				rt_hw_interrupt_enable(level);
				time = DTU_TIME_IP_IDLE_S;
			}
		}
		else
		{
			time = 30;
		}
		break;
	default:
		time = 30;
		break;
	}
	
	return time;
}

//模块互斥
static void _dtu_module_pend(void)
{
	if(RT_EOK != rt_sem_take(&m_dtu_sem_module, RT_WAITING_FOREVER))
	{
		while(1);
	}
}
static void _dtu_module_post(void)
{
	if(RT_EOK != rt_sem_release(&m_dtu_sem_module))
	{
		while(1);
	}
}

//参数互斥
static void _dtu_param_pend(rt_uint32_t param)
{
	if(RT_EOK != rt_event_recv(&m_dtu_event_param, param, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}
}
static void _dtu_param_post(rt_uint32_t param)
{
	if(RT_EOK != rt_event_send(&m_dtu_event_param, param))
	{
		while(1);
	}
}

#ifdef DTU_MODULE_SIM800C
#include "sim800c.c"
#endif

#ifdef DTU_MODULE_EC20
#include "ec20.c"
#endif

#ifdef DTU_MODULE_HT7710
#include "ht7710.c"
#endif

//数据发送
static rt_uint8_t _dtu_send_data(rt_uint8_t comm_type, rt_uint8_t ch, rt_uint8_t const *pdata, rt_uint16_t data_len, rt_uint8_t packet_type, DTU_FUN_CSQ_UPDATE fun_csq_update)
{
	rt_uint8_t boot_times = 0, boot_sta;

	DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]发送[%d]字节,[%s]", data_len, (DTU_PACKET_TYPE_DATA == packet_type) ? "数据" : "心跳"));

	if(0 == data_len)
	{
		return RT_TRUE;
	}

	//判断发送信道是否打开，如是关闭的，直接返回失败
	if(DTU_COMM_TYPE_SMS == comm_type)
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]信道[SMS],通道[%d]", ch));
		
		if(ch < DTU_NUM_SMS_CH)
		{
			_dtu_param_pend(DTU_PARAM_SMS_CH);
			if(m_dtu_sms_ch & (1 << ch))
			{
				_dtu_param_post(DTU_PARAM_SMS_CH);
			}
			else
			{
				_dtu_param_post(DTU_PARAM_SMS_CH);
				return RT_FALSE;
			}
		}
		else if((ch - DTU_NUM_SMS_CH) < DTU_NUM_SUPER_PHONE)
		{
			_dtu_param_pend(DTU_PARAM_SUPER_PHONE);
			if(0 == m_dtu_super_phone[ch - DTU_NUM_SMS_CH].addr_len)
			{
				_dtu_param_post(DTU_PARAM_SUPER_PHONE);
				return RT_FALSE;
			}
			else
			{
				_dtu_param_post(DTU_PARAM_SUPER_PHONE);
			}
		}
		else
		{
			return RT_FALSE;
		}
	}
	else if(DTU_COMM_TYPE_IP == comm_type)
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]信道[IP],通道[%d]", ch));
		
		if(ch < DTU_NUM_IP_CH)
		{
			_dtu_param_pend(DTU_PARAM_IP_CH);
			if(m_dtu_ip_ch & (1 << ch))
			{
				_dtu_param_post(DTU_PARAM_IP_CH);
			}
			else
			{
				_dtu_param_post(DTU_PARAM_IP_CH);
				return RT_FALSE;
			}
		}
		else
		{
			return RT_FALSE;
		}
	}
	else
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]信道[ERROR],通道[%d]", ch));
		
		return RT_FALSE;
	}

	DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]内容:"));
	DEBUG_INFO_OUTPUT_HEX(DEBUG_INFO_TYPE_DTU, (pdata, data_len));
	
	//发送
	while(1)
	{
		while(1)
		{
			//打开硬件端口
			if(DTU_STA_HW_OPEN != (m_dtu_module_sta & DTU_STA_HW_OPEN))
			{
				DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]打开硬件端口"));
				_dtu_open();
				m_dtu_module_sta |= DTU_STA_HW_OPEN;
			}
			//供电
			if(DTU_STA_PWR != (m_dtu_module_sta & DTU_STA_PWR))
			{
				DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]打开电源,延时[%d]秒", DTU_TIME_PWR_ON));
				_dtu_pwr_sta_switch(RT_TRUE);
				m_dtu_module_sta |= DTU_STA_PWR;
				rt_thread_delay(DTU_TIME_PWR_ON * RT_TICK_PER_SECOND);
			}
			//开机
			if(DTU_STA_DTU != (m_dtu_module_sta & DTU_STA_DTU))
			{
				if(RT_TRUE == _dtu_module_sta_switch(RT_TRUE))
				{
					DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]开机,延时[%d]秒", DTU_TIME_STA_ON));
					m_dtu_module_sta |= DTU_STA_DTU;
					rt_thread_delay(DTU_TIME_STA_ON * RT_TICK_PER_SECOND);
				}
				else
				{
					DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]开机失败"));
					break;
				}
			}
			//SMS初始化
			if(DTU_STA_SMS != (m_dtu_module_sta & DTU_STA_SMS))
			{
				if(RT_TRUE == _dtu_at_boot_sms())
				{
					DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]短信初始化成功"));
					m_dtu_module_sta |= DTU_STA_SMS;
				}
				else
				{
					DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]短信初始化失败"));
					break;
				}
			}
			//信号强度更新
			if((DTU_FUN_CSQ_UPDATE)0 != fun_csq_update)
			{
				if(RT_TRUE == _dtu_at_csq_update())
				{
					DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]信号强度查询成功"));
					fun_csq_update((rt_uint8_t *)(void *)pdata, data_len, m_dtu_csq_value);
				}
				else
				{
					DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]信号强度查询失败"));
					break;
				}
			}
			//SMS发送
			if(DTU_COMM_TYPE_SMS == comm_type)
			{
				if(RT_TRUE == _dtu_at_send_sms(ch, pdata, data_len))
				{
					DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]短信通道[%d]发送成功", ch));
					m_dtu_time_sms_idle = 0;
					return RT_TRUE;
				}
				else
				{
					DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]短信通道[%d]发送失败", ch));
					return RT_FALSE;
				}
			}
			//IP发送
			else if(DTU_COMM_TYPE_IP == comm_type)
			{
				//IP初始化
				if(0 == m_dtu_conn_sta)
				{
					boot_sta = _dtu_at_boot_ip();
					if(0 == boot_sta)
					{
						DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]网络初始化成功"));
						m_dtu_module_sta |= DTU_STA_IP;
					}
					else if(1 == boot_sta)
					{
						DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]有异常需要处理，直接返回失败"));
						return RT_FALSE;
					}
					else
					{
						DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]网络初始化失败"));
						break;
					}
				}
				//连接状态
				if(0 == (m_dtu_conn_sta & (1 << ch)))
				{
					rt_event_recv(&m_dtu_event_module_ex, DTU_EVENT_NEED_CONN_CH << ch, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, RT_WAITING_NO, (rt_uint32_t *)0);
					if(RT_TRUE == _dtu_at_connect_ip(ch))
					{
						DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]连接服务器[%d]成功", ch));
						m_dtu_conn_sta |= (1 << ch);
					}
					else
					{
						DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]连接服务器[%d]失败", ch));
						return RT_FALSE;
					}
				}
				else if(RT_EOK == rt_event_recv(&m_dtu_event_module_ex, DTU_EVENT_NEED_CONN_CH << ch, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, RT_WAITING_NO, (rt_uint32_t *)0))
				{
					if(RT_FALSE == _dtu_at_connect_ip(ch))
					{
						DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]连接服务器[%d]失败", ch));
						m_dtu_conn_sta &= (rt_uint8_t)~(rt_uint8_t)(1 << ch);
						return RT_FALSE;
					}
				}
				//发送
				if(RT_TRUE == _dtu_at_send_ip(ch, pdata, data_len))
				{
					DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]网络通道[%d]发送成功", ch));
					if(DTU_PACKET_TYPE_DATA == packet_type)
					{
						m_dtu_time_ip_idle[ch] = 0;
					}
					m_dtu_time_heart[ch] = 0;
					return RT_TRUE;
				}
				else
				{
					DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]网络通道[%d]发送失败", ch));
					m_dtu_conn_sta &= (rt_uint8_t)~(rt_uint8_t)(1 << ch);
					return RT_FALSE;
				}
			}
		}
		//断电
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]断电并延时[%d]秒", DTU_TIME_PWR_OFF));
		_dtu_pwr_sta_switch(RT_FALSE);
		m_dtu_module_sta	= DTU_STA_HW_OPEN;
		m_dtu_conn_sta		= 0;
		rt_thread_delay(DTU_TIME_PWR_OFF * RT_TICK_PER_SECOND);
		boot_times++;
		if(boot_times >= DTU_BOOT_TIMES_MAX)
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]发送尝试已达[%d]次,返回失败", boot_times));
			return RT_FALSE;
		}
	}
}

//心跳
static void _dtu_heart_beat(rt_uint8_t ch)
{
	rt_uint8_t				*pdata;
	rt_uint16_t				data_len;
	DTU_FUN_HEART_ENCODER	fun_heart_encoder;
	rt_base_t				level;

	if(ch >= DTU_NUM_IP_CH)
	{
		return;
	}

	_dtu_param_pend(DTU_PARAM_IP_CH);
	if(m_dtu_ip_ch & (1 << ch))
	{
		_dtu_param_post(DTU_PARAM_IP_CH);
	}
	else
	{
		_dtu_param_post(DTU_PARAM_IP_CH);
		return;
	}
	
	//申请内存
	pdata = (rt_uint8_t *)mempool_req(DTU_BYTES_HEART, RT_WAITING_FOREVER);
	if((rt_uint8_t *)0 == pdata)
	{
		return;
	}
	//心跳包编码函数
	level = rt_hw_interrupt_disable();
	fun_heart_encoder = m_dtu_fun_heart_encoder;
	rt_hw_interrupt_enable(level);
	if((DTU_FUN_HEART_ENCODER)0 == fun_heart_encoder)
	{
		fun_heart_encoder = _dtu_heart_encoder;
	}
	//发送心跳包
	data_len = fun_heart_encoder(pdata, DTU_BYTES_HEART, ch);
	_dtu_send_data(DTU_COMM_TYPE_IP, ch, pdata, data_len, DTU_PACKET_TYPE_HEART, (DTU_FUN_CSQ_UPDATE)0);
	//释放内存
	rt_mp_free((void *)pdata);
}

//计时任务
static void _dtu_task_time_tick(void *parg)
{
	rt_uint8_t		run_mode[DTU_NUM_IP_CH], ch;
	rt_uint16_t		heart_period[DTU_NUM_IP_CH];
	rt_base_t		level;
	
	while(1)
	{
		tevent_pend(TEVENT_EVENT_SEC);
		lpm_cpu_ref(RT_TRUE);
		
		_dtu_module_pend();

		//获取相关param
		_dtu_param_pend(DTU_PARAM_RUN_MODE + DTU_PARAM_HEART_PERIOD);
		for(ch = 0; ch < DTU_NUM_IP_CH; ch++)
		{
			run_mode[ch]		= m_dtu_run_mode[ch];
			heart_period[ch]	= m_dtu_heart_period[ch];
		}
		_dtu_param_post(DTU_PARAM_RUN_MODE + DTU_PARAM_HEART_PERIOD);
		
		//下线时间
		for(ch = 0; ch < DTU_NUM_IP_CH; ch++)
		{
			//实时在线
			if(DTU_RUN_MODE_ONLINE == run_mode[ch])
			{
				m_dtu_time_ip_idle[ch] = 0;
			}
			//未在线
			else if(0 == (m_dtu_conn_sta & (1 << ch)))
			{
				m_dtu_time_ip_idle[ch] = 0;
			}
			//计时
			else
			{
				m_dtu_time_ip_idle[ch]++;
				if(m_dtu_time_ip_idle[ch] < _dtu_hold_time(DTU_COMM_TYPE_IP, ch))
				{
					if(0 == (m_dtu_time_ip_idle[ch] % 60))
					{
						_dtu_heart_beat(ch);
					}
				}
				else
				{
					m_dtu_time_ip_idle[ch] = 0;
					level = rt_hw_interrupt_disable();
					m_dtu_hold_ip &= (rt_uint8_t)~(rt_uint8_t)(1 << ch);
					rt_hw_interrupt_enable(level);
					_dtu_at_close_ip(ch);
					m_dtu_conn_sta &= (rt_uint8_t)~(rt_uint8_t)(1 << ch);
				}
			}
		}
		//全部通道掉线并网络附着，断开网络
		if((0 == m_dtu_conn_sta) && (DTU_STA_IP == (m_dtu_module_sta & DTU_STA_IP)))
		{
			_dtu_at_close_net();
			m_dtu_module_sta &= (rt_uint8_t)~(rt_uint8_t)DTU_STA_IP;
		}
		
		//断电时间
		for(ch = 0; ch < DTU_NUM_IP_CH; ch++)
		{
			if(DTU_RUN_MODE_PWR != run_mode[ch])
			{
				break;
			}
		}
		//有通道不是掉电模式
		if(ch < DTU_NUM_IP_CH)
		{
			m_dtu_time_sms_idle = 0;
		}
		//模块没电
		else if(DTU_STA_PWR != (m_dtu_module_sta & DTU_STA_PWR))
		{
			m_dtu_time_sms_idle = 0;
		}
		//模块附着网络
		else if(DTU_STA_IP == (m_dtu_module_sta & DTU_STA_IP))
		{
			m_dtu_time_sms_idle = 0;
		}
		//计时
		else
		{
			m_dtu_time_sms_idle++;
			if(m_dtu_time_sms_idle > _dtu_hold_time(DTU_COMM_TYPE_SMS, DTU_NUM_SMS_CH))
			{
				m_dtu_time_sms_idle = 0;
				level = rt_hw_interrupt_disable();
				m_dtu_hold_sms = 0;
				rt_hw_interrupt_enable(level);
				_dtu_pwr_sta_switch(RT_FALSE);
				_dtu_close();
				m_dtu_module_sta = 0;
				m_dtu_conn_sta = 0;
				DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]模块断电关闭", ch));
			}
		}
		
		//心跳包时间
		for(ch = 0; ch < DTU_NUM_IP_CH; ch++)
		{
			if(RT_EOK == rt_event_recv(&m_dtu_event_module_ex, DTU_EVENT_SEND_IMMEDIATELY << ch, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, RT_WAITING_NO, (rt_uint32_t *)0))
			{
				m_dtu_time_heart[ch] = 0;
				_dtu_heart_beat(ch);
			}
			//不是在线模式
			else if(DTU_RUN_MODE_ONLINE != run_mode[ch])
			{
				m_dtu_time_heart[ch] = 0;
			}
			//心跳间隔为0
			else if(0 == heart_period[ch])
			{
				m_dtu_time_heart[ch] = 0;
			}
			//计时
			else
			{
				m_dtu_time_heart[ch]++;
				if(m_dtu_time_heart[ch] > heart_period[ch])
				{
					m_dtu_time_heart[ch] = 0;
					_dtu_heart_beat(ch);
				}
			}
		}
		
		_dtu_module_post();

		lpm_cpu_ref(RT_FALSE);
	}
}

//电话处理任务
static void _dtu_task_telephone_handler(void *parg)
{
	DTU_SMS_ADDR	*sms_addr_ptr;
	rt_uint8_t		ch;
	rt_base_t		level;
	
	while(1)
	{
		//来电号码
		if(RT_EOK != rt_mb_recv(&m_dtu_mb_telephone_nbr, (rt_ubase_t *)&sms_addr_ptr, RT_WAITING_FOREVER))
		{
			while(1);
		}
		lpm_cpu_ref(RT_TRUE);

		if((DTU_SMS_ADDR *)0 != sms_addr_ptr)
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]有来电,号码:"));
			DEBUG_INFO_OUTPUT(DEBUG_INFO_TYPE_DTU, (sms_addr_ptr->addr_data, sms_addr_ptr->addr_len));

			//下发通知：有来电需要处理
			_dtu_at_event_incoming_phone(RT_TRUE);
			//占用模块
			_dtu_module_pend();
			//清除通知：有来电需要处理
			_dtu_at_event_incoming_phone(RT_FALSE);
			//挂电话
			_dtu_at_hang_up();
			//释放模块
			_dtu_module_post();
			
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]来电已拒接"));

			//比较电话号码
			while(1)
			{
				_dtu_param_pend(DTU_PARAM_SUPER_PHONE);
				for(ch = 0; ch < DTU_NUM_SUPER_PHONE; ch++)
				{
					if(m_dtu_super_phone[ch].addr_len)
					{
						if(RT_TRUE == _dtu_compare_phone(sms_addr_ptr, m_dtu_super_phone + ch))
						{
							break;
						}
					}
				}
				_dtu_param_post(DTU_PARAM_SUPER_PHONE);

				if(ch < DTU_NUM_SUPER_PHONE)
				{
					ch += DTU_NUM_SMS_CH;
					break;
				}

				for(ch = 0; ch < DTU_NUM_SMS_CH; ch++)
				{
					_dtu_param_pend(DTU_PARAM_SMS_CH + (DTU_PARAM_SMS_ADDR << ch));
					if(m_dtu_sms_ch & (1 << ch))
					{
						if(RT_TRUE == _dtu_compare_phone(sms_addr_ptr, m_dtu_sms_addr + ch))
						{
							_dtu_param_post(DTU_PARAM_SMS_CH + (DTU_PARAM_SMS_ADDR << ch));
							break;
						}
					}
					_dtu_param_post(DTU_PARAM_SMS_CH + (DTU_PARAM_SMS_ADDR << ch));
				}

				if(ch >= DTU_NUM_SMS_CH)
				{
					ch += DTU_NUM_SUPER_PHONE;
				}

				break;
			}

			if(ch < DTU_NUM_SMS_CH)
			{
				dtu_ip_send_immediately(ch);
			}
			else if(ch < (DTU_NUM_SMS_CH + DTU_NUM_SUPER_PHONE))
			{
				for(ch = 0; ch < DTU_NUM_IP_CH; ch++)
				{
					dtu_ip_send_immediately(ch);
				}
			}

			//释放来电号码
			level = rt_hw_interrupt_disable();
			m_dtu_telephone_nbr_ptr = sms_addr_ptr;
			rt_hw_interrupt_enable(level);
		}
		
		lpm_cpu_ref(RT_FALSE);
	}
}

//接收缓冲区处理任务
static void _dtu_task_recv_buf_handler(void *parg)
{
	rt_base_t	level;
	rt_uint8_t	*pmem;
	rt_uint32_t	event_recv;

	while(1)
	{
		if(RT_EOK != rt_event_recv(&m_dtu_event_module_ex, DTU_EVENT_RECV_BUF, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
		{
			while(1);
		}
		lpm_cpu_ref(RT_TRUE);

		do
		{
			if(RT_EOK != rt_event_recv(&m_dtu_event_module_ex, DTU_EVENT_RECV_BUF + DTU_EVENT_RECV_BUF_FULL, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, DTU_RECV_BUF_TIMEOUT * RT_TICK_PER_SECOND / 1000, &event_recv))
			{
				break;
			}
		} while(event_recv & DTU_EVENT_RECV_BUF);

		level = rt_hw_interrupt_disable();
		if(m_dtu_recv_buf_len)
		{
			pmem = (rt_uint8_t *)mempool_req(m_dtu_recv_buf_len + sizeof(rt_uint16_t), RT_WAITING_NO);
			if((rt_uint8_t *)0 != pmem)
			{
				*(rt_uint16_t *)pmem = m_dtu_recv_buf_len;
				memcpy(pmem + sizeof(rt_uint16_t), m_dtu_recv_buf, m_dtu_recv_buf_len);
			}
			m_dtu_recv_buf_len = 0;
		}
		else
		{
			pmem = (rt_uint8_t *)0;
		}
		rt_hw_interrupt_enable(level);

		if((rt_uint8_t *)0 != pmem)
		{
			if(RT_EOK != rt_mb_send_wait(&m_dtu_mb_at_data, (rt_ubase_t)pmem, RT_WAITING_NO))
			{
				rt_mp_free((void *)pmem);
			}
		}

		lpm_cpu_ref(RT_FALSE);
	}
}

//拨打电话任务
static void _dtu_task_dial_phone(void *parg)
{
	DTU_SMS_ADDR *sms_addr_ptr;

	while(1)
	{
		if(RT_EOK != rt_mb_recv(&m_dtu_mb_dial_phone, (rt_ubase_t *)&sms_addr_ptr, RT_WAITING_FOREVER))
		{
			while(1);
		}

		lpm_cpu_ref(RT_TRUE);

		if((DTU_SMS_ADDR *)0 != sms_addr_ptr)
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]拨打电话:"));
			DEBUG_INFO_OUTPUT(DEBUG_INFO_TYPE_DTU, (sms_addr_ptr->addr_data, sms_addr_ptr->addr_len));
			
			_dtu_module_pend();
			while(1)
			{
				//硬件未打开
				if(DTU_STA_HW_OPEN != (m_dtu_module_sta & DTU_STA_HW_OPEN))
				{
					DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]硬件未打开,放弃打电话"));
					break;
				}
				//模块没电
				if(DTU_STA_PWR != (m_dtu_module_sta & DTU_STA_PWR))
				{
					DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]未供电,放弃打电话"));
					break;
				}
				//模块没开机
				if(DTU_STA_DTU != (m_dtu_module_sta & DTU_STA_DTU))
				{
					DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]未开机,放弃打电话"));
					break;
				}
				//拨打电话
				if(RT_TRUE == _dtu_at_dial_phone(sms_addr_ptr))
				{
					DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]拨打电话成功，延时[%d]秒后挂断", DTU_TIME_DIAL_PHONE));
					rt_thread_delay(DTU_TIME_DIAL_PHONE * RT_TICK_PER_SECOND);
				}
				//挂断电话
				DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]挂断电话"));
				_dtu_at_hang_up();

				break;
			}
			_dtu_module_post();
			
			rt_mp_free((void *)sms_addr_ptr);
		}

		lpm_cpu_ref(RT_FALSE);
	}
}

static void _dtu_task_at_data_handler(void *parg)
{
	rt_uint16_t	i, mem_len;
	rt_uint8_t	*pmem;

	while(1)
	{
		if(RT_EOK != rt_mb_recv(&m_dtu_mb_at_data, (rt_ubase_t *)&pmem, RT_WAITING_FOREVER))
		{
			while(1);
		}

		lpm_cpu_ref(RT_TRUE);
		
		if((rt_uint8_t *)0 != pmem)
		{
			mem_len = *(rt_uint16_t *)pmem;
			mem_len += sizeof(rt_uint16_t);

			DEBUG_INFO_OUTPUT(DEBUG_INFO_TYPE_AT_DATA, (pmem + sizeof(rt_uint16_t), mem_len - sizeof(rt_uint16_t)));
			
			for(i = sizeof(rt_uint16_t); i < mem_len; i++)
			{
				_dtu_at_cmd_handler(pmem[i]);
			}
			
			rt_mp_free((void *)pmem);
		}

		lpm_cpu_ref(RT_FALSE);
	}
}

static int _dtu_component_init(void)
{
	//数据接收mailbox
	if(RT_EOK != rt_mb_init(&m_dtu_mb_recv_data, "dtu", (void *)m_dtu_msgpool_recv_data, DTU_NUM_RECV_DATA, RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	
	//at数据接收mailbox
	if(RT_EOK != rt_mb_init(&m_dtu_mb_at_data, "dtu", (void *)m_dtu_msgpool_at_data, DTU_NUM_AT_DATA, RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	
	//来电号码
	m_dtu_telephone_nbr_ptr = &m_dtu_telephone_nbr;
	if(RT_EOK != rt_mb_init(&m_dtu_mb_telephone_nbr, "dtu", (void *)&m_dtu_msgpool_telephone_nbr, 1, RT_IPC_FLAG_PRIO))
	{
		while(1);
	}

	//拨打电话
	if(RT_EOK != rt_mb_init(&m_dtu_mb_dial_phone, "dtu", (void *)&m_dtu_msgpool_dial_phone, 1, RT_IPC_FLAG_PRIO))
	{
		while(1);
	}

	//http数据接收mailbox
	if(RT_EOK != rt_mb_init(&m_dtu_mb_http_data, "dtu", (void *)m_dtu_msgpool_http_data, DTU_NUM_HTTP_DATA, RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	
	//运行管理
	if(RT_EOK != rt_sem_init(&m_dtu_sem_module, "dtu", 1, RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	if(RT_EOK != rt_event_init(&m_dtu_event_param, "dtu", RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	if(RT_EOK != rt_event_send(&m_dtu_event_param, DTU_PARAM_ALL))
	{
		while(1);
	}
	if(RT_EOK != rt_event_init(&m_dtu_event_module, "dtu", RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	if(RT_EOK != rt_event_init(&m_dtu_event_module_ex, "dtu", RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	
	//参数
	if(RT_TRUE == _dtu_get_param_set(&m_dtu_param_set))
	{
		_dtu_validate_param_set(&m_dtu_param_set);
	}
	else
	{
		_dtu_reset_param_set(&m_dtu_param_set);
	}

	return 0;
}
INIT_COMPONENT_EXPORT(_dtu_component_init);

static int _dtu_app_init(void)
{
	if(RT_EOK != rt_thread_init(&m_dtu_thread_time_tick, "dtu", _dtu_task_time_tick, (void *)0, (void *)m_dtu_stk_time_tick, DTU_STK_TIME_TICK, DTU_PRIO_TIME_TICK, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_dtu_thread_time_tick))
	{
		while(1);
	}
	
	if(RT_EOK != rt_thread_init(&m_dtu_thread_telephone_handler, "dtu", _dtu_task_telephone_handler, (void *)0, (void *)m_dtu_stk_telephone_handler, DTU_STK_TELEPHONE_HANDLER, DTU_PRIO_TELEPHONE_HANDLER, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_dtu_thread_telephone_handler))
	{
		while(1);
	}
	
	if(RT_EOK != rt_thread_init(&m_dtu_thread_recv_buf_handler, "dtu", _dtu_task_recv_buf_handler, (void *)0, (void *)m_dtu_stk_recv_buf_handler, DTU_STK_RECV_BUF_HANDLER, DTU_PRIO_RECV_BUF_HANDLER, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_dtu_thread_recv_buf_handler))
	{
		while(1);
	}
	
	if(RT_EOK != rt_thread_init(&m_dtu_thread_dial_phone, "dtu", _dtu_task_dial_phone, (void *)0, (void *)m_dtu_stk_dial_phone, DTU_STK_DIAL_PHONE, DTU_PRIO_DIAL_PHONE, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_dtu_thread_dial_phone))
	{
		while(1);
	}
	
	if(RT_EOK != rt_thread_init(&m_dtu_thread_at_data_handler, "dtu", _dtu_task_at_data_handler, (void *)0, (void *)m_dtu_stk_at_data_handler, DTU_STK_AT_DATA_HANDLER, DTU_PRIO_AT_DATA_HANDLER, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_dtu_thread_at_data_handler))
	{
		while(1);
	}

	return 0;
}
INIT_APP_EXPORT(_dtu_app_init);



//参数设置读取
void dtu_set_ip_addr(DTU_IP_ADDR const *ip_addr_ptr, rt_uint8_t ch)
{
	if((DTU_IP_ADDR *)0 == ip_addr_ptr)
	{
		return;
	}
	if(ip_addr_ptr->addr_len > DTU_BYTES_IP_ADDR)
	{
		return;
	}
	if(ch >= DTU_NUM_IP_CH)
	{
		return;
	}

	_dtu_param_pend(DTU_PARAM_IP_CH + (DTU_PARAM_IP_ADDR << ch));
	m_dtu_ip_addr[ch] = *ip_addr_ptr;
	_dtu_set_ip_addr(ip_addr_ptr, ch);
	if(0 == (m_dtu_ip_ch & (1 << ch)))
	{
		m_dtu_ip_ch |= (1 << ch);
		_dtu_set_ip_ch(m_dtu_ip_ch);
	}
	_dtu_param_post(DTU_PARAM_IP_CH + (DTU_PARAM_IP_ADDR << ch));
	
	if(RT_EOK != rt_event_send(&m_dtu_event_module_ex, DTU_EVENT_NEED_CONN_CH << ch))
	{
		while(1);
	}
}
void dtu_get_ip_addr(DTU_IP_ADDR *ip_addr_ptr, rt_uint8_t ch)
{
	if((DTU_IP_ADDR *)0 == ip_addr_ptr)
	{
		return;
	}
	if(ch >= DTU_NUM_IP_CH)
	{
		ip_addr_ptr->addr_len = 0;
		return;
	}

	_dtu_param_pend(DTU_PARAM_IP_ADDR << ch);
	*ip_addr_ptr = m_dtu_ip_addr[ch];
	_dtu_param_post(DTU_PARAM_IP_ADDR << ch);
}

void dtu_set_sms_addr(DTU_SMS_ADDR const *sms_addr_ptr, rt_uint8_t ch)
{
	if((DTU_SMS_ADDR *)0 == sms_addr_ptr)
	{
		return;
	}
	if(sms_addr_ptr->addr_len > DTU_BYTES_SMS_ADDR)
	{
		return;
	}
	if(ch >= DTU_NUM_SMS_CH)
	{
		return;
	}

	_dtu_param_pend(DTU_PARAM_SMS_CH + (DTU_PARAM_SMS_ADDR << ch));
	m_dtu_sms_addr[ch] = *sms_addr_ptr;
	_dtu_set_sms_addr(sms_addr_ptr, ch);
	if(0 == (m_dtu_sms_ch & (1 << ch)))
	{
		m_dtu_sms_ch |= (1 << ch);
		_dtu_set_sms_ch(m_dtu_sms_ch);
	}
	_dtu_param_post(DTU_PARAM_SMS_CH + (DTU_PARAM_SMS_ADDR << ch));
}
void dtu_get_sms_addr(DTU_SMS_ADDR *sms_addr_ptr, rt_uint8_t ch)
{
	if((DTU_SMS_ADDR *)0 == sms_addr_ptr)
	{
		return;
	}
	if(ch >= DTU_NUM_SMS_CH)
	{
		sms_addr_ptr->addr_len = 0;
		return;
	}

	_dtu_param_pend(DTU_PARAM_SMS_ADDR << ch);
	*sms_addr_ptr = m_dtu_sms_addr[ch];
	_dtu_param_post(DTU_PARAM_SMS_ADDR << ch);
}

void dtu_set_ip_type(rt_uint8_t ip_type, rt_uint8_t ch)
{
	if((RT_FALSE != ip_type) && (RT_TRUE != ip_type))
	{
		return;
	}
	if(ch >= DTU_NUM_IP_CH)
	{
		return;
	}

	_dtu_param_pend(DTU_PARAM_IP_TYPE);
	if(ip_type != ((m_dtu_ip_type & (1 << ch)) ? RT_TRUE : RT_FALSE))
	{
		if(RT_TRUE == ip_type)
		{
			m_dtu_ip_type |= (1 << ch);
		}
		else
		{
			m_dtu_ip_type &= (rt_uint8_t)~(rt_uint8_t)(1 << ch);
		}
		_dtu_set_ip_type(m_dtu_ip_type);
		_dtu_param_post(DTU_PARAM_IP_TYPE);
		if(RT_EOK != rt_event_send(&m_dtu_event_module_ex, DTU_EVENT_NEED_CONN_CH << ch))
		{
			while(1);
		}
	}
	else
	{
		_dtu_param_post(DTU_PARAM_IP_TYPE);
	}
}
rt_uint8_t dtu_get_ip_type(rt_uint8_t ch)
{
	if(ch >= DTU_NUM_IP_CH)
	{
		return RT_FALSE;
	}

	_dtu_param_pend(DTU_PARAM_IP_TYPE);
	if(m_dtu_ip_type & (1 << ch))
	{
		_dtu_param_post(DTU_PARAM_IP_TYPE);
		return RT_TRUE;
	}
	else
	{
		_dtu_param_post(DTU_PARAM_IP_TYPE);
		return RT_FALSE;
	}
}

void dtu_set_heart_period(rt_uint16_t heart_period, rt_uint8_t ch)
{
	if(ch >= DTU_NUM_IP_CH)
	{
		return;
	}
	
	_dtu_param_pend(DTU_PARAM_HEART_PERIOD);
	if(heart_period != m_dtu_heart_period[ch])
	{
		m_dtu_heart_period[ch] = heart_period;
		_dtu_set_heart_period(heart_period, ch);
	}
	_dtu_param_post(DTU_PARAM_HEART_PERIOD);
}
rt_uint16_t dtu_get_heart_period(rt_uint8_t ch)
{
	rt_uint16_t heart_period;

	if(ch >= DTU_NUM_IP_CH)
	{
		return 0;
	}

	_dtu_param_pend(DTU_PARAM_HEART_PERIOD);
	heart_period = m_dtu_heart_period[ch];
	_dtu_param_post(DTU_PARAM_HEART_PERIOD);

	return heart_period;
}

void dtu_set_run_mode(rt_uint8_t run_mode, rt_uint8_t ch)
{
	if(run_mode >= DTU_NUM_RUN_MODE)
	{
		return;
	}
	if(ch >= DTU_NUM_IP_CH)
	{
		return;
	}
	
	_dtu_param_pend(DTU_PARAM_RUN_MODE);
	if(run_mode != m_dtu_run_mode[ch])
	{
		m_dtu_run_mode[ch] = run_mode;
		_dtu_set_run_mode(run_mode, ch);
	}
	_dtu_param_post(DTU_PARAM_RUN_MODE);
}
rt_uint8_t dtu_get_run_mode(rt_uint8_t ch)
{
	rt_uint8_t run_mode;

	if(ch >= DTU_NUM_IP_CH)
	{
		return DTU_RUN_MODE_PWR;
	}

	_dtu_param_pend(DTU_PARAM_RUN_MODE);
	run_mode = m_dtu_run_mode[ch];
	_dtu_param_post(DTU_PARAM_RUN_MODE);

	return run_mode;
}

void dtu_set_super_phone(DTU_SMS_ADDR const *sms_addr_ptr, rt_uint8_t ch)
{
	if((DTU_SMS_ADDR *)0 == sms_addr_ptr)
	{
		return;
	}
	if(sms_addr_ptr->addr_len > DTU_BYTES_SMS_ADDR)
	{
		return;
	}
	if(ch >= DTU_NUM_SUPER_PHONE)
	{
		return;
	}

	_dtu_param_pend(DTU_PARAM_SUPER_PHONE);
	m_dtu_super_phone[ch] = *sms_addr_ptr;
	_dtu_set_super_phone(sms_addr_ptr, ch);
	_dtu_param_post(DTU_PARAM_SUPER_PHONE);
}
void dtu_get_super_phone(DTU_SMS_ADDR *sms_addr_ptr, rt_uint8_t ch)
{
	if((DTU_SMS_ADDR *)0 == sms_addr_ptr)
	{
		return;
	}
	if(ch >= DTU_NUM_SUPER_PHONE)
	{
		sms_addr_ptr->addr_len = 0;
		return;
	}

	_dtu_param_pend(DTU_PARAM_SUPER_PHONE);
	*sms_addr_ptr = m_dtu_super_phone[ch];
	_dtu_param_post(DTU_PARAM_SUPER_PHONE);
}
void dtu_del_super_phone(DTU_SMS_ADDR const *sms_addr_ptr)
{
	rt_uint8_t ch;
	
	if((DTU_SMS_ADDR *)0 == sms_addr_ptr)
	{
		return;
	}
	if(sms_addr_ptr->addr_len > DTU_BYTES_SMS_ADDR)
	{
		return;
	}

	_dtu_param_pend(DTU_PARAM_SUPER_PHONE);
	for(ch = 0; ch < DTU_NUM_SUPER_PHONE; ch++)
	{
		if(0 != m_dtu_super_phone[ch].addr_len)
		{
			if(RT_TRUE == _dtu_compare_phone(m_dtu_super_phone + ch, sms_addr_ptr))
			{
				m_dtu_super_phone[ch].addr_len = 0;
				_dtu_set_super_phone(m_dtu_super_phone + ch, ch);
			}
		}
	}
	_dtu_param_post(DTU_PARAM_SUPER_PHONE);
}
rt_uint8_t dtu_add_super_phone(DTU_SMS_ADDR const *sms_addr_ptr, rt_uint8_t previlige)
{
	rt_uint8_t ch;

	if((DTU_SMS_ADDR *)0 == sms_addr_ptr)
	{
		return RT_FALSE;
	}
	if(sms_addr_ptr->addr_len > DTU_BYTES_SMS_ADDR)
	{
		return RT_FALSE;
	}

	if(RT_TRUE == previlige)
	{
		ch = 0;
		previlige = DTU_NUM_PREVILIGE_PHONE;
	}
	else
	{
		ch = DTU_NUM_PREVILIGE_PHONE;
		previlige = DTU_NUM_SUPER_PHONE;
	}
	_dtu_param_pend(DTU_PARAM_SUPER_PHONE);
	for(; ch < previlige; ch++)
	{
		if(0 == m_dtu_super_phone[ch].addr_len)
		{
			m_dtu_super_phone[ch] = *sms_addr_ptr;
			_dtu_set_super_phone(m_dtu_super_phone + ch, ch);
			break;
		}
	}
	_dtu_param_post(DTU_PARAM_SUPER_PHONE);
	if(ch < previlige)
	{
		return RT_TRUE;
	}

	return RT_FALSE;
}
rt_uint8_t dtu_is_previlige_phone(rt_uint8_t ch)
{
	if(ch < (DTU_NUM_SMS_CH + DTU_NUM_PREVILIGE_PHONE))
	{
		return RT_TRUE;
	}

	return RT_FALSE;
}

void dtu_set_ip_ch(rt_uint8_t sta, rt_uint8_t ch)
{
	if((RT_FALSE != sta) && (RT_TRUE != sta))
	{
		return;
	}
	if(ch >= DTU_NUM_IP_CH)
	{
		return;
	}

	_dtu_param_pend(DTU_PARAM_IP_CH);
	if(sta != ((m_dtu_ip_ch & (1 << ch)) ? RT_TRUE : RT_FALSE))
	{
		if(RT_TRUE == sta)
		{
			m_dtu_ip_ch |= (1 << ch);
		}
		else
		{
			m_dtu_ip_ch &= (rt_uint8_t)~(rt_uint8_t)(1 << ch);
		}
		_dtu_set_ip_ch(m_dtu_ip_ch);
	}
	_dtu_param_post(DTU_PARAM_IP_CH);
}
rt_uint8_t dtu_get_ip_ch(rt_uint8_t ch)
{
	if(ch >= DTU_NUM_IP_CH)
	{
		return RT_FALSE;
	}

	_dtu_param_pend(DTU_PARAM_IP_CH);
	if(m_dtu_ip_ch & (1 << ch))
	{
		_dtu_param_post(DTU_PARAM_IP_CH);
		return RT_TRUE;
	}
	else
	{
		_dtu_param_post(DTU_PARAM_IP_CH);
		return RT_FALSE;
	}
}

void dtu_set_sms_ch(rt_uint8_t sta, rt_uint8_t ch)
{
	if((RT_FALSE != sta) && (RT_TRUE != sta))
	{
		return;
	}
	if(ch >= DTU_NUM_SMS_CH)
	{
		return;
	}

	_dtu_param_pend(DTU_PARAM_SMS_CH);
	if(sta != ((m_dtu_sms_ch & (1 << ch)) ? RT_TRUE : RT_FALSE))
	{
		if(RT_TRUE == sta)
		{
			m_dtu_sms_ch |= (1 << ch);
		}
		else
		{
			m_dtu_sms_ch &= (rt_uint8_t)~(rt_uint8_t)(1 << ch);
		}
		_dtu_set_sms_ch(m_dtu_sms_ch);
	}
	_dtu_param_post(DTU_PARAM_SMS_CH);
}
rt_uint8_t dtu_get_sms_ch(rt_uint8_t ch)
{
	if(ch >= DTU_NUM_SMS_CH)
	{
		return RT_FALSE;
	}

	_dtu_param_pend(DTU_PARAM_SMS_CH);
	if(m_dtu_sms_ch & (1 << ch))
	{
		_dtu_param_post(DTU_PARAM_SMS_CH);
		return RT_TRUE;
	}
	else
	{
		_dtu_param_post(DTU_PARAM_SMS_CH);
		return RT_FALSE;
	}
}

void dtu_set_apn(DTU_APN const *apn_ptr)
{
	rt_uint8_t ch;
	
	if((DTU_APN *)0 == apn_ptr)
	{
		return;
	}
	if(apn_ptr->apn_len > DTU_BYTES_APN)
	{
		return;
	}

	_dtu_param_pend(DTU_PARAM_APN);
	m_dtu_apn = *apn_ptr;
	_dtu_set_apn(apn_ptr);
	_dtu_param_post(DTU_PARAM_APN);
	for(ch = 0; ch < DTU_NUM_IP_CH; ch++)
	{
		if(RT_EOK != rt_event_send(&m_dtu_event_module_ex, DTU_EVENT_NEED_CONN_CH << ch))
		{
			while(1);
		}
	}
}
void dtu_get_apn(DTU_APN *apn_ptr)
{
	if((DTU_APN *)0 == apn_ptr)
	{
		return;
	}

	_dtu_param_pend(DTU_PARAM_APN);
	*apn_ptr = m_dtu_apn;
	_dtu_param_post(DTU_PARAM_APN);
}

//参数重置
void dtu_param_restore(void)
{
	rt_uint8_t i;
	
	_dtu_param_pend(DTU_PARAM_ALL);
	
	//ip地址、运行模式、心跳间隔
	for(i = 0; i < DTU_NUM_IP_CH; i++)
	{
		if((i + 1) == DTU_NUM_IP_CH)
		{
			m_dtu_ip_addr[i].addr_len = sprintf((char *)m_dtu_ip_addr[i].addr_data, "114.116.41.117:12010");
		}
		else
		{
			m_dtu_ip_addr[i].addr_len = 0;
		}
		
		m_dtu_run_mode[i] = DTU_RUN_MODE_PWR;
		
		m_dtu_heart_period[i] = 40;
	}
	
	//sms地址
	for(i = 0; i < DTU_NUM_SMS_CH; i++)
	{
		m_dtu_sms_addr[i].addr_len = 0;
	}
	
	//ip_type(0-tcp、1-udp)
	m_dtu_ip_type = 0;
	
	//权限手机
	for(i = 0; i < DTU_NUM_SUPER_PHONE; i++)
	{
		if(0 == i)
		{
			m_dtu_super_phone[i].addr_len = sprintf((char *)m_dtu_super_phone[i].addr_data, "18629257314");
		}
		else
		{
			m_dtu_super_phone[i].addr_len = 0;
		}
	}
	
	//IP通道
	m_dtu_ip_ch = 0x10;
	
	//SMS通道
	m_dtu_sms_ch = 0;
	
	//APN
	m_dtu_apn.apn_len = sprintf((char *)m_dtu_apn.apn_data, "CMNET");

	_dtu_set_param_set(&m_dtu_param_set);
	
	_dtu_param_post(DTU_PARAM_ALL);
}

//发送数据
rt_uint8_t dtu_send_data(rt_uint8_t comm_type, rt_uint8_t ch, rt_uint8_t const *pdata, rt_uint16_t data_len, DTU_FUN_CSQ_UPDATE fun_csq_update)
{
	rt_uint8_t sta;

	if(DTU_COMM_TYPE_IP == comm_type)
	{
		if(ch >= DTU_NUM_IP_CH)
		{
			return RT_FALSE;
		}
	}
	else if(DTU_COMM_TYPE_SMS == comm_type)
	{
		if(ch >= (DTU_NUM_SMS_CH + DTU_NUM_SUPER_PHONE))
		{
			return RT_FALSE;
		}
	}
	else
	{
		return RT_FALSE;
	}
	if((rt_uint8_t *)0 == pdata)
	{
		return RT_FALSE;
	}
	if(0 == data_len)
	{
		return RT_TRUE;
	}

	_dtu_module_pend();
	
	sta = _dtu_send_data(comm_type, ch, pdata, data_len, DTU_PACKET_TYPE_DATA, fun_csq_update);
	
	_dtu_module_post();
	
	return sta;
}

//接收数据
DTU_RECV_DATA *dtu_recv_data_pend(void)
{
	DTU_RECV_DATA *recv_data_ptr;

	if(RT_EOK != rt_mb_recv(&m_dtu_mb_recv_data, (rt_ubase_t *)&recv_data_ptr, RT_WAITING_FOREVER))
	{
		while(1);
	}

	lpm_cpu_ref(RT_TRUE);
	
	if((DTU_RECV_DATA *)0 != recv_data_ptr)
	{
		if(DTU_COMM_TYPE_SMS == recv_data_ptr->comm_type)
		{
			_dtu_sms_data_conv(recv_data_ptr);
		}

		if(DTU_COMM_TYPE_SMS == recv_data_ptr->comm_type)
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]接收[%d]字节,信道[SMS],通道[%d]\r\n", recv_data_ptr->data_len, recv_data_ptr->ch));
		}
		else if(DTU_COMM_TYPE_IP == recv_data_ptr->comm_type)
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]接收[%d]字节,信道[IP],通道[%d]\r\n", recv_data_ptr->data_len, recv_data_ptr->ch));
		}
		else
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]接收[%d]字节,信道[ERROR],通道[%d]\r\n", recv_data_ptr->data_len, recv_data_ptr->ch));
		}
		if(recv_data_ptr->data_len)
		{
			DEBUG_INFO_OUTPUT_HEX(DEBUG_INFO_TYPE_DTU, (recv_data_ptr->pdata, recv_data_ptr->data_len));
		}
	}

	lpm_cpu_ref(RT_FALSE);
	
	return recv_data_ptr;
}

DTU_RECV_DATA *dtu_http_data_pend(rt_int32_t timeout)
{
	DTU_RECV_DATA *recv_data_ptr;

	if(RT_EOK != rt_mb_recv(&m_dtu_mb_http_data, (rt_ubase_t *)&recv_data_ptr, timeout))
	{
		return (DTU_RECV_DATA *)0;
	}

	if((DTU_RECV_DATA *)0 != recv_data_ptr)
	{
		if(DTU_COMM_TYPE_SMS == recv_data_ptr->comm_type)
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]接收[%d]字节,信道[SMS],通道[%d]", recv_data_ptr->data_len, recv_data_ptr->ch));
		}
		else if(DTU_COMM_TYPE_IP == recv_data_ptr->comm_type)
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]接收[%d]字节,信道[IP],通道[%d]", recv_data_ptr->data_len, recv_data_ptr->ch));
		}
		else
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n[dtu]接收[%d]字节,信道[ERROR],通道[%d]", recv_data_ptr->data_len, recv_data_ptr->ch));
		}
	}

	return recv_data_ptr;
}

void dtu_http_data_clear(void)
{
	DTU_RECV_DATA *recv_data_ptr;
	
	while(RT_EOK == rt_mb_recv(&m_dtu_mb_http_data, (rt_ubase_t *)&recv_data_ptr, RT_WAITING_NO))
	{
		rt_mp_free((void *)recv_data_ptr);
	}
}

//保持在线
void dtu_hold_enable(rt_uint8_t comm_type, rt_uint8_t ch, rt_uint8_t en)
{
	rt_base_t level;
	
	switch(comm_type)
	{
	case DTU_COMM_TYPE_SMS:
		if(ch >= DTU_NUM_SMS_CH)
		{
			return;
		}
		ch = 1 << ch;
		if(RT_TRUE == en)
		{
			level = rt_hw_interrupt_disable();
			m_dtu_hold_sms |= ch;
			rt_hw_interrupt_enable(level);
		}
		else
		{
			level = rt_hw_interrupt_disable();
			m_dtu_hold_sms &= ~ch;
			rt_hw_interrupt_enable(level);
		}
		break;
	case DTU_COMM_TYPE_IP:
		if(ch >= DTU_NUM_IP_CH)
		{
			return;
		}
		ch = 1 << ch;
		if(RT_TRUE == en)
		{
			level = rt_hw_interrupt_disable();
			m_dtu_hold_ip |= ch;
			rt_hw_interrupt_enable(level);
		}
		else
		{
			level = rt_hw_interrupt_disable();
			m_dtu_hold_ip &= ~ch;
			rt_hw_interrupt_enable(level);
		}
		break;
	}
}

//函数挂载
void dtu_fun_mount(DTU_FUN_HEART_ENCODER fun_heart_encoder)
{
	rt_base_t level;

	level = rt_hw_interrupt_disable();
	m_dtu_fun_heart_encoder = fun_heart_encoder;
	rt_hw_interrupt_enable(level);
}

rt_uint8_t dtu_get_conn_sta(rt_uint8_t ch)
{
	if(ch >= DTU_NUM_IP_CH)
	{
		return RT_FALSE;
	}

	_dtu_module_pend();
	if(m_dtu_conn_sta & (1 << ch))
	{
		_dtu_module_post();
		return RT_TRUE;
	}
	else
	{
		_dtu_module_post();
		return RT_FALSE;
	}
}

//拨打电话
rt_uint8_t dtu_dial_phone(DTU_SMS_ADDR *sms_addr_ptr)
{
	if((DTU_SMS_ADDR *)0 == sms_addr_ptr)
	{
		return RT_FALSE;
	}

	if(RT_EOK == rt_mb_send_wait(&m_dtu_mb_dial_phone, (rt_ubase_t)sms_addr_ptr, RT_WAITING_NO))
	{
		return RT_TRUE;
	}
	else
	{
		return RT_FALSE;
	}
}

//立即发送
void dtu_ip_send_immediately(rt_uint8_t ch)
{
	if(ch >= DTU_NUM_IP_CH)
	{
		return;
	}

	if(RT_EOK != rt_event_send(&m_dtu_event_module_ex, DTU_EVENT_SEND_IMMEDIATELY << ch))
	{
		while(1);
	}
}

rt_uint8_t dtu_get_csq_value(void)
{
	rt_uint8_t csq_value;

	if(RT_EOK != rt_sem_take(&m_dtu_sem_module, RT_WAITING_NO))
	{
		return 0;
	}

	while(1)
	{
		//硬件打开
		if(DTU_STA_HW_OPEN != (m_dtu_module_sta & DTU_STA_HW_OPEN))
		{
			csq_value = 0;
			break;
		}
		//供电
		if(DTU_STA_PWR != (m_dtu_module_sta & DTU_STA_PWR))
		{
			csq_value = 0;
			break;
		}
		//开机
		if(DTU_STA_DTU != (m_dtu_module_sta & DTU_STA_DTU))
		{
			csq_value = 0;
			break;
		}

		if(RT_TRUE == _dtu_at_csq_update())
		{
			csq_value = m_dtu_csq_value;
		}
		else
		{
			csq_value = 0;
		}

		break;
	}
	
	_dtu_module_post();

	return csq_value;
}

rt_uint8_t dtu_http_send(DTU_IP_ADDR const *ip_addr_ptr, rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint8_t sta;
	
	if((DTU_IP_ADDR *)0 == ip_addr_ptr)
	{
		return RT_FALSE;
	}
	if((rt_uint8_t *)0 == pdata)
	{
		return RT_FALSE;
	}
	if(0 == data_len)
	{
		return RT_FALSE;
	}

	_dtu_module_pend();
	
	//网络状况
	if(DTU_STA_IP != (m_dtu_module_sta & DTU_STA_IP))
	{
		sta = RT_FALSE;
		goto __exit;
	}
	//连接HTTP文件服务器
	if(RT_FALSE == _dtu_at_connect_http(ip_addr_ptr))
	{
		sta = RT_FALSE;
		goto __exit;
	}
	//发送数据
	sta = _dtu_at_send_http(pdata, data_len);

__exit:
	_dtu_module_post();

	return sta;
}

