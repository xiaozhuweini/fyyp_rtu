/*添加一个协议要注意的地方
**_ptcl_heart_encoder	心跳数据编码函数
**_ptcl_data_update		发送数据时更新函数
**_ptcl_data_valid		补报时数据验证函数
*/



#include "ptcl.h"
#include "drv_mempool.h"
#include "dtu.h"
#include "drv_rtcee.h"
#include "myself.h"
#include "smt_cfg.h"
#include "xmodem.h"
#include "smt_manage.h"
#include "sample.h"
#include "drv_debug.h"
#include "drv_lpm.h"
#include "stm32f4xx.h"
#include "string.h"
#include "drv_ads.h"
#include "drv_bluetooth.h"
#include "hj212.h"



//tc缓冲区
static rt_uint8_t				m_ptcl_tc_buf[PTCL_BYTES_TC_BUF];
static rt_uint16_t				m_ptcl_tc_buf_len				= 0;

//自报mailbox
static rt_ubase_t				m_ptcl_msgpool_auto_report[PTCL_NUM_AUTO_REPORT];
static struct rt_mailbox		m_ptcl_mb_auto_report;

//tc mailbox
static rt_ubase_t				m_ptcl_msgpool_tc_data;
static struct rt_mailbox		m_ptcl_mb_tc_data;

//shell mailbox
static rt_ubase_t				m_ptcl_msgpool_shell_data;
static struct rt_mailbox		m_ptcl_mb_shell_data;

//运行相关
static struct rt_event			m_ptcl_event_module;
static struct rt_event			m_ptcl_event_param;
static PTCL_REPLY_DATA			*m_ptcl_reply_list				= (PTCL_REPLY_DATA *)0;				//回执链表
static rt_uint8_t				m_ptcl_run_sta					= PTCL_RUN_STA_NORMAL;				//运行状态

//任务
static struct rt_thread			m_ptcl_thread_auto_report_manage;
static struct rt_thread			m_ptcl_thread_auto_report[PTCL_NUM_CENTER];
static struct rt_thread			m_ptcl_thread_tc_buf_handler;
static struct rt_thread			m_ptcl_thread_dtu_recv_handler;
static struct rt_thread			m_ptcl_thread_tc_recv_handler;
static struct rt_thread			m_ptcl_thread_shell_recv_handler;
static struct rt_thread			m_ptcl_thread_bt_recv_handler;
static rt_uint8_t				m_ptcl_stk_auto_report_manage[PTCL_STK_AUTO_REPORT_MANAGE];
static rt_uint8_t				m_ptcl_stk_auto_report[PTCL_NUM_CENTER][PTCL_STK_AUTO_REPORT];
static rt_uint8_t				m_ptcl_stk_tc_buf_handler[PTCL_STK_TC_BUF_HANDLER];
static rt_uint8_t				m_ptcl_stk_dtu_recv_handler[PTCL_STK_DTU_RECV_HANDLER];
static rt_uint8_t				m_ptcl_stk_tc_recv_handler[PTCL_STK_TC_RECV_HANDLER];
static rt_uint8_t				m_ptcl_stk_shell_recv_handler[PTCL_STK_SHELL_RECV_HANDLER];
static rt_uint8_t				m_ptcl_stk_bt_recv_handler[PTCL_STK_BT_RECV_HANDLER];

//参数集合
static PTCL_PARAM_SET			m_ptcl_param_set;
#define m_ptcl_center_info		m_ptcl_param_set.center_info



//参数互斥
static void _ptcl_param_pend(rt_uint32_t param)
{
	if(RT_EOK != rt_event_recv(&m_ptcl_event_param, param, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}
}
static void _ptcl_param_post(rt_uint32_t param)
{
	if(RT_EOK != rt_event_send(&m_ptcl_event_param, param))
	{
		while(1);
	}
}

static void _ptcl_reply_list_pend(void)
{
	if(RT_EOK != rt_event_recv(&m_ptcl_event_module, PTCL_EVENT_REPLY_LIST_PEND, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}
}
static void _ptcl_reply_list_post(void)
{
	if(RT_EOK != rt_event_send(&m_ptcl_event_module, PTCL_EVENT_REPLY_LIST_PEND))
	{
		while(1);
	}
}

//回执链表增加结点
static void _ptcl_reply_list_add(PTCL_REPLY_DATA *reply_data_ptr)
{
	_ptcl_reply_list_pend();
	
	reply_data_ptr->prev	= (PTCL_REPLY_DATA *)0;
	reply_data_ptr->next	= m_ptcl_reply_list;
	if((PTCL_REPLY_DATA *)0 != m_ptcl_reply_list)
	{
		m_ptcl_reply_list->prev = reply_data_ptr;
	}
	m_ptcl_reply_list = reply_data_ptr;
	
	_ptcl_reply_list_post();
}

//回执链表移除结点
static void _ptcl_reply_list_remove(PTCL_REPLY_DATA *reply_data_ptr)
{
	_ptcl_reply_list_pend();
	
	if((PTCL_REPLY_DATA *)0 == reply_data_ptr->prev)
	{
		m_ptcl_reply_list = reply_data_ptr->next;
		if((PTCL_REPLY_DATA *)0 != m_ptcl_reply_list)
		{
			m_ptcl_reply_list->prev = (PTCL_REPLY_DATA *)0;
		}
	}
	else
	{
		reply_data_ptr->prev->next = reply_data_ptr->next;
		if((PTCL_REPLY_DATA *)0 != reply_data_ptr->next)
		{
			reply_data_ptr->next->prev = reply_data_ptr->prev;
		}
	}
	
	_ptcl_reply_list_post();
}

//回执链表查找结点
static rt_uint32_t _ptcl_reply_list_find(rt_uint8_t comm_type, rt_uint8_t ch, rt_uint16_t data_id, rt_uint16_t reply_info)
{
	rt_uint32_t			event = 0;
	PTCL_REPLY_DATA		*reply_data_ptr;
	
	_ptcl_reply_list_pend();
	
	reply_data_ptr = m_ptcl_reply_list;
	while((PTCL_REPLY_DATA *)0 != reply_data_ptr)
	{
		if((comm_type == reply_data_ptr->comm_type) && (ch == reply_data_ptr->ch) && (data_id == reply_data_ptr->data_id))
		{
			reply_data_ptr->comm_type	= PTCL_COMM_TYPE_NONE;
			reply_data_ptr->data_id		= reply_info;
			event = reply_data_ptr->event;
			break;
		}
		else
		{
			reply_data_ptr = reply_data_ptr->next;
		}
	}
	
	_ptcl_reply_list_post();
	
	return event;
}

//回执等待
static rt_uint8_t _ptcl_reply_pend(rt_uint8_t comm_type, rt_uint8_t ch, rt_uint16_t data_id, rt_uint32_t event, rt_uint16_t *reply_info_ptr, rt_uint8_t timeout)
{
	PTCL_REPLY_DATA		reply_data;
	rt_uint32_t			events_recv;
	
	reply_data.comm_type	= comm_type;
	reply_data.ch			= ch;
	reply_data.data_id		= data_id;
	reply_data.event		= event;
	
	rt_event_recv(&m_ptcl_event_module, event, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, RT_WAITING_NO, (rt_uint32_t *)0);
	
	_ptcl_reply_list_add(&reply_data);

	events_recv = 0;
	rt_event_recv(&m_ptcl_event_module, event, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, timeout * RT_TICK_PER_SECOND, &events_recv);
	
	_ptcl_reply_list_remove(&reply_data);
	
	if(events_recv == event)
	{
		if((rt_uint16_t *)0 != reply_info_ptr)
		{
			*reply_info_ptr = reply_data.data_id;
		}
		return RT_TRUE;
	}
	else
	{
		return RT_FALSE;
	}
}

//内存是否为空
static rt_uint8_t _ptcl_ram_is_empty(rt_uint8_t center)
{
	_ptcl_param_pend(PTCL_PARAM_RAM_PTR << center);
	if(m_ptcl_center_info[center].ram_ptr.in == m_ptcl_center_info[center].ram_ptr.out)
	{
		_ptcl_param_post(PTCL_PARAM_RAM_PTR << center);
		return RT_TRUE;
	}
	else
	{
		_ptcl_param_post(PTCL_PARAM_RAM_PTR << center);
		return RT_FALSE;
	}
}

//数据验证
static rt_uint8_t _ptcl_data_valid(rt_uint8_t const *pdata, rt_uint16_t data_len, rt_uint16_t data_id, rt_uint8_t need_reply, rt_uint8_t fcb_value, rt_uint8_t ptcl_type)
{
	switch(ptcl_type)
	{
	default:
		return RT_FALSE;
	case PTCL_PTCL_TYPE_MYSELF:
		return RT_TRUE;
	}
}

//数据更新
static void _ptcl_data_update(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t center_addr, rt_uint8_t fcb_value, struct tm time, rt_uint8_t ptcl_type)
{
	switch(ptcl_type)
	{
	default:
		return;
	case PTCL_PTCL_TYPE_MYSELF:
		return;
	}
}

static rt_uint16_t _ptcl_heart_encoder(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t ch)
{
	if(ch < PTCL_NUM_CENTER)
	{
		switch(ptcl_get_ptcl_type(ch))
		{
		default:
			return 0;
		case PTCL_PTCL_TYPE_SL427:
			return 0;
		case PTCL_PTCL_TYPE_SL651:
			return 0;
		case PTCL_PTCL_TYPE_HJ212:
			return hj212_heart_encoder(pdata, data_len, ch);
		}
	}
	else
	{
		return smtmng_heart_encoder(pdata, data_len, ch);
	}
}

//tc信道缓冲区数据接收
static void _ptcl_tc_buf_fill(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	if((m_ptcl_tc_buf_len + data_len) <= PTCL_BYTES_TC_BUF)
	{
		memcpy((void *)(m_ptcl_tc_buf + m_ptcl_tc_buf_len), (void *)pdata, data_len);
		m_ptcl_tc_buf_len += data_len;
	}

	rt_event_send(&m_ptcl_event_module, PTCL_EVENT_TC_RECV);
}



#include "ptcl_io.c"



//参数设置信息处理
static void _ptcl_param_info_handler(PTCL_PARAM_INFO **param_info_ptr_ptr, rt_uint8_t set)
{
	PTCL_PARAM_INFO *ptr;

	ptr = *param_info_ptr_ptr;
	while((PTCL_PARAM_INFO *)0 != ptr)
	{
		if(RT_TRUE == set)
		{
			switch(ptr->param_type)
			{
			case PTCL_PARAM_INFO_TT:
				rtcee_rtc_set_calendar(*(struct tm *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_CENTER_ADDR:
				ptcl_set_center_addr(*(ptr->pdata), ptr->data_len);
				break;
			case PTCL_PARAM_INFO_CENTER_ADDR_ALL:
				while(ptr->data_len)
				{
					ptr->data_len--;
					ptcl_set_center_addr(*(ptr->pdata + ptr->data_len), ptr->data_len);
				}
				break;
			case PTCL_PARAM_INFO_COMM_TYPE:
				ptcl_set_comm_type(*(ptr->pdata), ptr->data_len / PTCL_NUM_COMM_PRIO, ptr->data_len % PTCL_NUM_COMM_PRIO);
				break;
			case PTCL_PARAM_INFO_PTCL_TYPE:
				ptcl_set_ptcl_type(*(ptr->pdata), ptr->data_len);
				break;
			case PTCL_PARAM_INFO_IP_ADDR:
				dtu_set_ip_addr((DTU_IP_ADDR *)(ptr->pdata), ptr->data_len);
				break;
			case PTCL_PARAM_INFO_SMS_ADDR:
				dtu_set_sms_addr((DTU_SMS_ADDR *)(ptr->pdata), ptr->data_len);
				break;
			case PTCL_PARAM_INFO_IP_TYPE:
				dtu_set_ip_type(*(ptr->pdata), ptr->data_len);
				break;
			case PTCL_PARAM_INFO_IP_CH:
				dtu_set_ip_ch(*(ptr->pdata), ptr->data_len);
				break;
			case PTCL_PARAM_INFO_SMS_CH:
				dtu_set_sms_ch(*(ptr->pdata), ptr->data_len);
				break;
			case PTCL_PARAM_INFO_DTU_APN:
				dtu_set_apn((DTU_APN *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_HEART_PERIOD:
				dtu_set_heart_period(*(rt_uint16_t *)(ptr->pdata), ptr->data_len);
				break;
			case PTCL_PARAM_INFO_DTU_MODE:
				dtu_set_run_mode(*(ptr->pdata), ptr->data_len);
				break;
			case PTCL_PARAM_INFO_ADD_SUPER_PHONE:
				dtu_del_super_phone((DTU_SMS_ADDR *)(ptr->pdata));
				dtu_add_super_phone((DTU_SMS_ADDR *)(ptr->pdata), ptr->data_len);
				break;
			case PTCL_PARAM_INFO_DEL_SUPER_PHONE:
				dtu_del_super_phone((DTU_SMS_ADDR *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_SMTMNG_STA_PERIOD:
				smtmng_set_sta_period(*(rt_uint16_t *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_SMTMNG_TIMING_PERIOD:
				smtmng_set_timing_period(*(rt_uint16_t *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_SMTMNG_HEART_PERIOD:
				smtmng_set_heart_period(*(rt_uint16_t *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_SAMPLE_SENSOR_TYPE:
				sample_set_sensor_type(ptr->data_len, *(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_SAMPLE_SENSOR_ADDR:
				sample_set_sensor_addr(ptr->data_len, *(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_SAMPLE_HW_PORT:
				sample_set_hw_port(ptr->data_len, *(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_SAMPLE_PROTOCOL:
				sample_set_protocol(ptr->data_len, *(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_SAMPLE_HW_RATE:
				sample_set_hw_rate(ptr->data_len, *(rt_uint32_t *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_SAMPLE_KOPT:
				sample_set_kopt(ptr->data_len, *(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_SAMPLE_K:
				sample_set_k(ptr->data_len, *(float *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_SAMPLE_B:
				sample_set_b(ptr->data_len, *(float *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_SAMPLE_BASE:
				sample_set_base(ptr->data_len, *(float *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_SAMPLE_OFFSET:
				sample_set_offset(ptr->data_len, *(float *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_SAMPLE_UP:
				sample_set_up(ptr->data_len, *(float *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_SAMPLE_DOWN:
				sample_set_down(ptr->data_len, *(float *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_SAMPLE_THRESHOLD:
				sample_set_threshold(ptr->data_len, *(float *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_SAMPLE_STORE_PERIOD:
				sample_set_store_period(ptr->data_len, *(rt_uint16_t *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_AD_CAL_VAL:
				ads_set_cal_val(ptr->data_len, *(rt_int32_t *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_HJ212_ST:
				hj212_set_st(ptr->pdata);
				break;
			case PTCL_PARAM_INFO_HJ212_PW:
				hj212_set_pw(ptr->pdata);
				break;
			case PTCL_PARAM_INFO_HJ212_MN:
				hj212_set_mn(ptr->pdata);
				break;
			case PTCL_PARAM_INFO_HJ212_RTD_EN:
				hj212_set_rtd_en(*ptr->pdata);
				break;
			case PTCL_PARAM_INFO_HJ212_RECOUNT:
				hj212_set_recount(*ptr->pdata);
				break;
			case PTCL_PARAM_INFO_HJ212_MIN_INTERVAL:
				hj212_set_min_interval(*(rt_uint16_t *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_HJ212_RTD_INTERVAL:
				hj212_set_rtd_interval(*(rt_uint16_t *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_HJ212_OVER_TIME:
				hj212_set_over_time(*(rt_uint16_t *)(ptr->pdata));
				break;
			case PTCL_PARAM_INFO_HJ212_RS_EN:
				hj212_set_rs_en(*ptr->pdata);
				break;
//			case PTCL_PARAM_INFO_SL651_RTU_ADDR:
//				sl651_set_rtu_addr(ptr->pdata);
//				break;
//			case PTCL_PARAM_INFO_SL651_PW:
//				sl651_set_pw(*(rt_uint16_t *)(ptr->pdata));
//				break;
//			case PTCL_PARAM_INFO_SL651_SN:
//				sl651_set_sn(*(rt_uint16_t *)(ptr->pdata));
//				break;
//			case PTCL_PARAM_INFO_SL651_RTU_TYPE:
//				sl651_set_rtu_type(*(ptr->pdata));
//				break;
//			case PTCL_PARAM_INFO_SL651_REPORT_HOUR:
//				sl651_set_report_hour(*(ptr->pdata));
//				break;
//			case PTCL_PARAM_INFO_SL651_REPORT_MIN:
//				sl651_set_report_min(*(ptr->pdata));
//				break;
//			case PTCL_PARAM_INFO_SL651_RANDOM_PERIOD:
//				sl651_set_random_period(*(rt_uint16_t *)(ptr->pdata));
//				break;
//			case PTCL_PARAM_INFO_SL651_CENTER_COMM:
//				sl651_set_center_comm((ptr->data_len >> 8) / 2, (ptr->data_len >> 8) % 2, ptr->pdata, (rt_uint8_t)ptr->data_len);
//				break;
			case PTCL_PARAM_INFO_DEBUG_INFO_TYPE_STA:
				debug_set_info_type_sta(ptr->data_len, *(ptr->pdata));
				break;
			}
		}
		*param_info_ptr_ptr = ptr->next;
		rt_mp_free((void *)ptr);
		ptr = *param_info_ptr_ptr;
	}
}

//上报一条数据(上报过程中，只有pdata会由于数据更新函数可能发生改变，其余数据域都不会发生变化，所以互斥是针对pdata)
static rt_uint8_t _ptcl_report_data_send(PTCL_REPORT_DATA *report_data_ptr,//上报的数据
						rt_uint32_t event_pend,//对上报数据的互斥，m_ptcl_event_module中的事件，自报时为PTCL_EVENT_AUTO_REPORT_PEND，其他为0
						rt_uint8_t comm_type,//上报的信道类型
						rt_uint8_t ch,//上报的通道
						rt_uint8_t center_addr,//上报通道的中心地址
						rt_uint32_t event_reply,//判断回执用的事件，是m_ptcl_event_module中的事件
						rt_uint16_t *reply_info_ptr)//接收回执信息
{
	rt_uint8_t fcb_value, send_sta;

	if(0 == report_data_ptr->data_len)
	{
		//此处一定要返回发送成功
		return RT_TRUE;
	}
	fcb_value = report_data_ptr->fcb_value;
	//根据信道上报
	if(PTCL_COMM_TYPE_IP == comm_type)
	{
		while(1)
		{
			if(event_pend)
			{
				if(RT_EOK != rt_event_recv(&m_ptcl_event_module, event_pend, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
				{
					while(1);
				}
			}
			//数据更新
			_ptcl_data_update(report_data_ptr->pdata, report_data_ptr->data_len, center_addr, fcb_value, rtcee_rtc_get_calendar(), report_data_ptr->ptcl_type);
			//数据发送
			send_sta = dtu_send_data(DTU_COMM_TYPE_IP, ch, report_data_ptr->pdata, report_data_ptr->data_len, (DTU_FUN_CSQ_UPDATE)report_data_ptr->fun_csq_update);
			if(event_pend)
			{
				if(RT_EOK != rt_event_send(&m_ptcl_event_module, event_pend))
				{
					while(1);
				}
			}
			//发送失败
			if(RT_FALSE == send_sta)
			{
				return RT_FALSE;
			}
			//不需要回执
			else if(RT_FALSE == report_data_ptr->need_reply)
			{
				return RT_TRUE;
			}
			//收到回执
			else if(RT_TRUE == _ptcl_reply_pend(PTCL_COMM_TYPE_IP, ch, report_data_ptr->data_id, event_reply, reply_info_ptr, PTCL_REPLY_TIME_IP))
			{
				return RT_TRUE;
			}
			//未收到回执
			else
			{
				if(fcb_value)
				{
					fcb_value--;
				}
				if(0 == fcb_value)
				{
					return RT_FALSE;
				}
			}
		}
	}
	else if(PTCL_COMM_TYPE_SMS == comm_type)
	{
		if(event_pend)
		{
			if(RT_EOK != rt_event_recv(&m_ptcl_event_module, event_pend, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
			{
				while(1);
			}
		}
		//数据更新
		_ptcl_data_update(report_data_ptr->pdata, report_data_ptr->data_len, center_addr, fcb_value, rtcee_rtc_get_calendar(), report_data_ptr->ptcl_type);
		//数据发送
		send_sta = dtu_send_data(DTU_COMM_TYPE_SMS, ch, report_data_ptr->pdata, report_data_ptr->data_len, (DTU_FUN_CSQ_UPDATE)report_data_ptr->fun_csq_update);
		if(event_pend)
		{
			if(RT_EOK != rt_event_send(&m_ptcl_event_module, event_pend))
			{
				while(1);
			}
		}
		
		return send_sta;
	}
	else if(PTCL_COMM_TYPE_TC == comm_type)
	{
		while(1)
		{
			if(event_pend)
			{
				if(RT_EOK != rt_event_recv(&m_ptcl_event_module, event_pend, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
				{
					while(1);
				}
			}
			//数据更新
			_ptcl_data_update(report_data_ptr->pdata, report_data_ptr->data_len, center_addr, fcb_value, rtcee_rtc_get_calendar(), report_data_ptr->ptcl_type);
			//数据发送
			send_sta = _ptcl_tc_send(report_data_ptr->pdata, report_data_ptr->data_len);
			if(event_pend)
			{
				if(RT_EOK != rt_event_send(&m_ptcl_event_module, event_pend))
				{
					while(1);
				}
			}
			//发送失败
			if(RT_FALSE == send_sta)
			{
				return RT_FALSE;
			}
			//不需要回执
			else if(RT_FALSE == report_data_ptr->need_reply)
			{
				return RT_TRUE;
			}
			//收到回执
			else if(RT_TRUE == _ptcl_reply_pend(PTCL_COMM_TYPE_TC, 0, report_data_ptr->data_id, event_reply, reply_info_ptr, PTCL_REPLY_TIME_TC))
			{
				return RT_TRUE;
			}
			//未收到回执
			else
			{
				if(fcb_value)
				{
					fcb_value--;
				}
				if(0 == fcb_value)
				{
					return RT_FALSE;
				}
			}
		}
	}
	else if(PTCL_COMM_TYPE_BT == comm_type)
	{
		while(1)
		{
			if(event_pend)
			{
				if(RT_EOK != rt_event_recv(&m_ptcl_event_module, event_pend, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
				{
					while(1);
				}
			}
			//数据更新
			_ptcl_data_update(report_data_ptr->pdata, report_data_ptr->data_len, center_addr, fcb_value, rtcee_rtc_get_calendar(), report_data_ptr->ptcl_type);
			//数据发送
			send_sta = bt_send_data(report_data_ptr->pdata, report_data_ptr->data_len);
			if(event_pend)
			{
				if(RT_EOK != rt_event_send(&m_ptcl_event_module, event_pend))
				{
					while(1);
				}
			}
			//发送失败
			if(RT_FALSE == send_sta)
			{
				return RT_FALSE;
			}
			//不需要回执
			else if(RT_FALSE == report_data_ptr->need_reply)
			{
				return RT_TRUE;
			}
			//收到回执
			else if(RT_TRUE == _ptcl_reply_pend(PTCL_COMM_TYPE_BT, 0, report_data_ptr->data_id, event_reply, reply_info_ptr, PTCL_REPLY_TIME_BT))
			{
				return RT_TRUE;
			}
			//未收到回执
			else
			{
				if(fcb_value)
				{
					fcb_value--;
				}
				if(0 == fcb_value)
				{
					return RT_FALSE;
				}
			}
		}
	}
	
	return RT_FALSE;
}

//自报任务
static void _ptcl_task_auto_report(void *parg)
{
	PTCL_REPORT_DATA	*auto_report_ptr;
	struct rt_thread	*thread_self_ptr;
	rt_uint8_t			center, center_addr, comm_type, comm_prio = 0, send_sta;
	
	//获得上报的数据和中心站
	auto_report_ptr = (PTCL_REPORT_DATA *)parg;
	thread_self_ptr = rt_thread_self();
	for(center = 0; center < PTCL_NUM_CENTER; center++)
	{
		if(thread_self_ptr == (m_ptcl_thread_auto_report + center))
		{
			break;
		}
	}
	//上报
	if(((PTCL_REPORT_DATA *)0 != auto_report_ptr) && (center < PTCL_NUM_CENTER))
	{
		while(1)
		{
			//判断中心站地址
			_ptcl_param_pend((PTCL_PARAM_CENTER_ADDR << center) + (PTCL_PARAM_COMM_TYPE << center) + (PTCL_PARAM_PTCL_TYPE << center));
			if(m_ptcl_center_info[center].center_addr && (m_ptcl_center_info[center].ptcl_type == auto_report_ptr->ptcl_type))
			{
				center_addr	= m_ptcl_center_info[center].center_addr;
				comm_type	= m_ptcl_center_info[center].comm_type[comm_prio];
				_ptcl_param_post((PTCL_PARAM_CENTER_ADDR << center) + (PTCL_PARAM_COMM_TYPE << center) + (PTCL_PARAM_PTCL_TYPE << center));
			}
			else
			{
				_ptcl_param_post((PTCL_PARAM_CENTER_ADDR << center) + (PTCL_PARAM_COMM_TYPE << center) + (PTCL_PARAM_PTCL_TYPE << center));
				break;
			}
			//上报
			if((PTCL_REPORT_DATA *)parg == auto_report_ptr)
			{
				send_sta = _ptcl_report_data_send(auto_report_ptr, PTCL_EVENT_AUTO_REPORT_PEND, comm_type, center, center_addr, PTCL_EVENT_REPLY_AUTO_REPORT << center, (rt_uint16_t *)0);
			}
			else
			{
				send_sta = _ptcl_report_data_send(auto_report_ptr, 0, comm_type, center, center_addr, PTCL_EVENT_REPLY_AUTO_REPORT << center, (rt_uint16_t *)0);
			}
			//发送失败
			if(RT_FALSE == send_sta)
			{
				comm_prio++;
				if(comm_prio >= PTCL_NUM_COMM_PRIO)
				{
					if((PTCL_REPORT_DATA *)parg == auto_report_ptr)
					{
						_ptcl_ram_data_push(auto_report_ptr, PTCL_EVENT_AUTO_REPORT_PEND, center);
					}
					else
					{
						_ptcl_ram_data_push(auto_report_ptr, 0, center);
					}
					break;
				}
			}
			//发送成功且无内存数据
			else if(RT_TRUE == _ptcl_ram_is_empty(center))
			{
				break;
			}
			//有内存数据
			else
			{
				//申请数据空间
				if((PTCL_REPORT_DATA *)parg == auto_report_ptr)
				{
					auto_report_ptr = ptcl_report_data_req(PTCL_BYTES_RAM_DATA - PTCL_BYTES_RAM_FEATURE, RT_WAITING_FOREVER);
					if((PTCL_REPORT_DATA *)0 == auto_report_ptr)
					{
						break;
					}
				}
				//取内存数据失败
				if(RT_FALSE == _ptcl_ram_data_pop(auto_report_ptr, center))
				{
					break;
				}
				//内存数据为空
				else if(0 == auto_report_ptr->data_len)
				{
					break;
				}
				//从最高优先级信道开始发送
				else
				{
					comm_prio = 0;
				}
			}
		}
		//释放补报数据内存空间
		if(((PTCL_REPORT_DATA *)parg != auto_report_ptr) && ((PTCL_REPORT_DATA *)0 != auto_report_ptr))
		{
			rt_mp_free((void *)auto_report_ptr);
		}
	}
}

//自报管理任务
static void _ptcl_task_auto_report_manage(void *parg)
{
	rt_uint8_t			i, del_sta, *stk_ptr;
	rt_base_t			level;
	PTCL_REPORT_DATA	*auto_report_ptr, *report_data_ptr;
	
	while(1)
	{
		if(RT_EOK != rt_mb_recv(&m_ptcl_mb_auto_report, (rt_ubase_t *)&report_data_ptr, RT_WAITING_FOREVER))
		{
			while(1);
		}

		lpm_cpu_ref(RT_TRUE);

		if((PTCL_REPORT_DATA *)0 != report_data_ptr)
		{
#ifdef PTCL_AUTO_REPORT_ENCRYPT
			auto_report_ptr = tsm_data_encrypt(report_data_ptr);
			//此处直接释放数据，可以修改为(加密成功释放，失败就塞进另一个消息队列)
			rt_mp_free((void *)report_data_ptr);
#else
			auto_report_ptr = report_data_ptr;
#endif
			if((PTCL_REPORT_DATA *)0 != auto_report_ptr)
			{
				//创建上报任务
				del_sta = 0;
				stk_ptr = *m_ptcl_stk_auto_report;
				for(i = 0; i < PTCL_NUM_CENTER; i++)
				{
					if(RT_EOK != rt_thread_init(m_ptcl_thread_auto_report + i, "autorep", _ptcl_task_auto_report, (void *)auto_report_ptr, stk_ptr, PTCL_STK_AUTO_REPORT, PTCL_PRIO_AUTO_REPORT, 2))
					{
						while(1);
					}
					if(RT_EOK != rt_thread_startup(m_ptcl_thread_auto_report + i))
					{
						while(1);
					}
					
					del_sta += (1 << i);
					stk_ptr += PTCL_STK_AUTO_REPORT;
				}
				//判断上报任务删除情况
				while(del_sta)
				{
					rt_thread_delay(RT_TICK_PER_SECOND);
					for(i = 0; i < PTCL_NUM_CENTER; i++)
					{
						if(del_sta & (1 << i))
						{
							level = rt_hw_interrupt_disable();
							if(RT_THREAD_CLOSE == m_ptcl_thread_auto_report[i].stat)
							{
								rt_hw_interrupt_enable(level);
								del_sta &= (rt_uint8_t)~(rt_uint8_t)(1 << i);
							}
							else
							{
								rt_hw_interrupt_enable(level);
							}
						}
					}
				}
				
				rt_mp_free((void *)auto_report_ptr);
			}
		}

		lpm_cpu_ref(RT_FALSE);
	}
}

//dtu应答任务
static void _ptcl_task_dtu_recv_handler(void *parg)
{
	DTU_RECV_DATA		*dtu_data_ptr;
	PTCL_RECV_DATA		*recv_data_ptr;
	PTCL_REPORT_DATA	*report_data_ptr;
	rt_base_t			level;
	PTCL_PARAM_INFO		*param_info_ptr = (PTCL_PARAM_INFO *)0;
	rt_uint8_t			send_sta;
	
	while(1)
	{
		dtu_data_ptr = dtu_recv_data_pend();
		lpm_cpu_ref(RT_TRUE);
		
		if((DTU_RECV_DATA *)0 != dtu_data_ptr)
		{
			//处理
			while(1)
			{
				if(0 == dtu_data_ptr->data_len)
				{
					break;
				}
				
				recv_data_ptr = ptcl_recv_data_req(dtu_data_ptr->data_len, RT_WAITING_FOREVER);
				if((PTCL_RECV_DATA *)0 != recv_data_ptr)
				{
					report_data_ptr = ptcl_report_data_req(PTCL_BYTES_ACK_REPORT, RT_WAITING_FOREVER);
					if((PTCL_REPORT_DATA *)0 != report_data_ptr)
					{
						if(DTU_COMM_TYPE_SMS == dtu_data_ptr->comm_type)
						{
							recv_data_ptr->comm_type = PTCL_COMM_TYPE_SMS;
						}
						else
						{
							recv_data_ptr->comm_type = PTCL_COMM_TYPE_IP;
						}
						recv_data_ptr->ch = dtu_data_ptr->ch;
						memcpy(recv_data_ptr->pdata, dtu_data_ptr->pdata, dtu_data_ptr->data_len);
						recv_data_ptr->data_len = dtu_data_ptr->data_len;

						while(1)
						{
							if(RT_TRUE == my_data_decoder(report_data_ptr, recv_data_ptr, &param_info_ptr))
							{
								send_sta = _ptcl_report_data_send(report_data_ptr, 0, recv_data_ptr->comm_type, recv_data_ptr->ch, 0, PTCL_EVENT_REPLY_ACK_IP, (rt_uint16_t *)0);
								_ptcl_param_info_handler(&param_info_ptr, send_sta);
								break;
							}

							if(RT_TRUE == xm_data_decoder(recv_data_ptr))
							{
								break;
							}

							if(RT_TRUE == smtmng_data_decoder(report_data_ptr, recv_data_ptr, &param_info_ptr))
							{
								send_sta = _ptcl_report_data_send(report_data_ptr, 0, recv_data_ptr->comm_type, recv_data_ptr->ch, 0, PTCL_EVENT_REPLY_ACK_IP, (rt_uint16_t *)0);
								_ptcl_param_info_handler(&param_info_ptr, send_sta);
								break;
							}

							if(RT_TRUE == hj212_data_decoder(report_data_ptr, recv_data_ptr, &param_info_ptr))
							{
								_ptcl_report_data_send(report_data_ptr, 0, recv_data_ptr->comm_type, recv_data_ptr->ch, 0, PTCL_EVENT_REPLY_ACK_IP, (rt_uint16_t *)0);
								_ptcl_param_info_handler(&param_info_ptr, RT_TRUE);
								break;
							}

//							if(RT_TRUE == sl651_data_decoder(report_data_ptr, recv_data_ptr, &param_info_ptr))
//							{
//								send_sta = _ptcl_report_data_send(report_data_ptr, 0, recv_data_ptr->comm_type, recv_data_ptr->ch, 0, PTCL_EVENT_REPLY_ACK_IP, (rt_uint16_t *)0);
//								_ptcl_param_info_handler(&param_info_ptr, send_sta);
//								break;
//							}

							break;
						}

						rt_mp_free((void *)report_data_ptr);
					}
					
					rt_mp_free((void *)recv_data_ptr);
				}

				break;
			}
			
			rt_mp_free(dtu_data_ptr);
			
			level = rt_hw_interrupt_disable();
			if(PTCL_RUN_STA_NORMAL != m_ptcl_run_sta)
			{
				if(PTCL_RUN_STA_RESTORE == m_ptcl_run_sta)
				{
					rt_hw_interrupt_enable(level);
					dtu_param_restore();
					ptcl_param_restore();
					smtmng_param_restore();
					sample_param_restore();
					ads_param_restore();
					hj212_param_restore();
//					sl651_param_restore();
				}
				else
				{
					rt_hw_interrupt_enable(level);
				}
				HAL_NVIC_SystemReset();
			}
			else
			{
				rt_hw_interrupt_enable(level);
			}
		}

		lpm_cpu_ref(RT_FALSE);
	}
}

//tc信道接收处理任务
static void _ptcl_task_tc_recv_handler(void *parg)
{
	PTCL_RECV_DATA		*recv_data_ptr;
	PTCL_REPORT_DATA	*report_data_ptr;
	rt_base_t			level;
	PTCL_PARAM_INFO		*param_info_ptr = (PTCL_PARAM_INFO *)0;
	rt_uint8_t			send_sta;
	
	while(1)
	{
		if(RT_EOK != rt_mb_recv(&m_ptcl_mb_tc_data, (rt_ubase_t *)&recv_data_ptr, RT_WAITING_FOREVER))
		{
			while(1);
		}

		lpm_cpu_ref(RT_TRUE);
		
		if((PTCL_RECV_DATA *)0 != recv_data_ptr)
		{
			report_data_ptr = ptcl_report_data_req(PTCL_BYTES_ACK_REPORT, RT_WAITING_FOREVER);
			if((PTCL_REPORT_DATA *)0 != report_data_ptr)
			{
				while(1)
				{
					if(RT_TRUE == my_data_decoder(report_data_ptr, recv_data_ptr, &param_info_ptr))
					{
						send_sta = _ptcl_report_data_send(report_data_ptr, 0, PTCL_COMM_TYPE_TC, 0, 0, PTCL_EVENT_REPLY_ACK_TC, (rt_uint16_t *)0);
						_ptcl_param_info_handler(&param_info_ptr, send_sta);
						break;
					}

					if(RT_TRUE == smtcfg_data_decoder(report_data_ptr, recv_data_ptr, &param_info_ptr))
					{
						send_sta = _ptcl_report_data_send(report_data_ptr, 0, PTCL_COMM_TYPE_TC, 0, 0, PTCL_EVENT_REPLY_ACK_TC, (rt_uint16_t *)0);
						_ptcl_param_info_handler(&param_info_ptr, send_sta);
						break;
					}

					if(RT_TRUE == xm_data_decoder(recv_data_ptr))
					{
						break;
					}

					if(RT_TRUE == hj212_data_decoder(report_data_ptr, recv_data_ptr, &param_info_ptr))
					{
						_ptcl_report_data_send(report_data_ptr, 0, PTCL_COMM_TYPE_TC, 0, 0, PTCL_EVENT_REPLY_ACK_TC, (rt_uint16_t *)0);
						_ptcl_param_info_handler(&param_info_ptr, RT_TRUE);
						break;
					}

//					if(RT_TRUE == sl651_data_decoder(report_data_ptr, recv_data_ptr, &param_info_ptr))
//					{
//						send_sta = _ptcl_report_data_send(report_data_ptr, 0, PTCL_COMM_TYPE_TC, 0, 0, PTCL_EVENT_REPLY_ACK_TC, (rt_uint16_t *)0);
//						_ptcl_param_info_handler(&param_info_ptr, send_sta);
//						break;
//					}

					break;
				}
				
				rt_mp_free((void *)report_data_ptr);
			}
			
			rt_mp_free((void *)recv_data_ptr);
			
			level = rt_hw_interrupt_disable();
			if(PTCL_RUN_STA_NORMAL != m_ptcl_run_sta)
			{
				if(PTCL_RUN_STA_RESTORE == m_ptcl_run_sta)
				{
					rt_hw_interrupt_enable(level);
					dtu_param_restore();
					ptcl_param_restore();
					smtmng_param_restore();
					sample_param_restore();
					ads_param_restore();
					hj212_param_restore();
//					sl651_param_restore();
				}
				else
				{
					rt_hw_interrupt_enable(level);
				}
				HAL_NVIC_SystemReset();
			}
			else
			{
				rt_hw_interrupt_enable(level);
			}
		}

		lpm_cpu_ref(RT_FALSE);
	}
}

//tc缓冲区处理任务
static void _ptcl_task_tc_buf_handler(void *parg)
{
	rt_base_t		level;
	PTCL_RECV_DATA	*recv_data_ptr;

	while(1)
	{
		if(RT_EOK != rt_event_recv(&m_ptcl_event_module, PTCL_EVENT_TC_RECV, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
		{
			while(1);
		}

		lpm_cpu_ref(RT_TRUE);
		
		while(RT_EOK == rt_event_recv(&m_ptcl_event_module, PTCL_EVENT_TC_RECV, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, PTCL_TC_BUF_TIMEOUT * RT_TICK_PER_SECOND / 1000, (rt_uint32_t *)0));

		level = rt_hw_interrupt_disable();
		if(m_ptcl_tc_buf_len)
		{
			recv_data_ptr = ptcl_recv_data_req(m_ptcl_tc_buf_len, RT_WAITING_NO);
			if((PTCL_RECV_DATA *)0 != recv_data_ptr)
			{
				recv_data_ptr->data_len = m_ptcl_tc_buf_len;
				memcpy((void *)recv_data_ptr->pdata, (void *)m_ptcl_tc_buf, m_ptcl_tc_buf_len);
			}
			m_ptcl_tc_buf_len = 0;
		}
		else
		{
			recv_data_ptr = (PTCL_RECV_DATA *)0;
		}
		rt_hw_interrupt_enable(level);

		if((PTCL_RECV_DATA *)0 != recv_data_ptr)
		{
			recv_data_ptr->comm_type	= PTCL_COMM_TYPE_TC;
			recv_data_ptr->ch			= 0;

			if(RT_EOK != rt_mb_send_wait(&m_ptcl_mb_tc_data, (rt_ubase_t)recv_data_ptr, RT_WAITING_NO))
			{
				rt_mp_free((void *)recv_data_ptr);
			}
		}

		lpm_cpu_ref(RT_FALSE);
	}
}

//shell接收处理任务
static void _ptcl_task_shell_recv_handler(void *parg)
{
	PTCL_RECV_DATA		*recv_data_ptr;
	PTCL_REPORT_DATA	*report_data_ptr;
	rt_base_t			level;
	PTCL_PARAM_INFO		*param_info_ptr = (PTCL_PARAM_INFO *)0;
	rt_uint8_t			send_sta;
	void (*fun_shell_encoder)(PTCL_REPORT_DATA **report_data_ptr);
	
	while(1)
	{
		if(RT_EOK != rt_mb_recv(&m_ptcl_mb_shell_data, (rt_ubase_t *)&recv_data_ptr, RT_WAITING_FOREVER))
		{
			while(1);
		}

		lpm_cpu_ref(RT_TRUE);
		
		if((PTCL_RECV_DATA *)0 != recv_data_ptr)
		{
			*(void **)&fun_shell_encoder = *(void **)(recv_data_ptr->pdata + recv_data_ptr->data_len);
			if((void *)0 != fun_shell_encoder)
			{
				report_data_ptr = ptcl_report_data_req(PTCL_BYTES_ACK_REPORT, RT_WAITING_FOREVER);
				if((PTCL_REPORT_DATA *)0 != report_data_ptr)
				{
					while(1)
					{
						if(RT_TRUE == smtcfg_data_decoder(report_data_ptr, recv_data_ptr, &param_info_ptr))
						{
							fun_shell_encoder(&report_data_ptr);
							send_sta = _ptcl_report_data_send(report_data_ptr, 0, recv_data_ptr->comm_type, recv_data_ptr->ch, 0, PTCL_EVENT_REPLY_ACK_SHELL, (rt_uint16_t *)0);
							_ptcl_param_info_handler(&param_info_ptr, send_sta);
							break;
						}

						if(RT_TRUE == xm_data_decoder(recv_data_ptr))
						{
							break;
						}

						break;
					}
					
					rt_mp_free((void *)report_data_ptr);
				}
			}
			
			rt_mp_free((void *)recv_data_ptr);
			
			level = rt_hw_interrupt_disable();
			if(PTCL_RUN_STA_NORMAL != m_ptcl_run_sta)
			{
				if(PTCL_RUN_STA_RESTORE == m_ptcl_run_sta)
				{
					rt_hw_interrupt_enable(level);
					dtu_param_restore();
					ptcl_param_restore();
					smtmng_param_restore();
					sample_param_restore();
					ads_param_restore();
					hj212_param_restore();
//					sl651_param_restore();
				}
				else
				{
					rt_hw_interrupt_enable(level);
				}
				HAL_NVIC_SystemReset();
			}
			else
			{
				rt_hw_interrupt_enable(level);
			}
		}

		lpm_cpu_ref(RT_FALSE);
	}
}

//bt信道接收处理任务
static void _ptcl_task_bt_recv_handler(void *parg)
{
	PTCL_RECV_DATA		*recv_data_ptr;
	PTCL_REPORT_DATA	*report_data_ptr;
	rt_base_t			level;
	PTCL_PARAM_INFO		*param_info_ptr = (PTCL_PARAM_INFO *)0;
	rt_uint8_t			send_sta;
	
	while(1)
	{
		parg = (void *)bt_recv_data_pend();
		lpm_cpu_ref(RT_TRUE);
		
		if((void *)0 != parg)
		{
			if(0 == *(rt_uint16_t *)parg)
			{
				goto __exit_parg;
			}
			
			recv_data_ptr = ptcl_recv_data_req(*(rt_uint16_t *)parg, RT_WAITING_NO);
			if((PTCL_RECV_DATA *)0 != recv_data_ptr)
			{
				report_data_ptr = ptcl_report_data_req(PTCL_BYTES_ACK_REPORT, RT_WAITING_NO);
				if((PTCL_REPORT_DATA *)0 != report_data_ptr)
				{
					recv_data_ptr->comm_type	= PTCL_COMM_TYPE_BT;
					recv_data_ptr->ch			= 0;
					recv_data_ptr->data_len		= *(rt_uint16_t *)parg;
					memcpy((void *)recv_data_ptr->pdata, (void *)((rt_uint8_t *)parg + sizeof(rt_uint16_t)), *(rt_uint16_t *)parg);

					if(RT_TRUE == my_data_decoder(report_data_ptr, recv_data_ptr, &param_info_ptr))
					{
						send_sta = _ptcl_report_data_send(report_data_ptr, 0, recv_data_ptr->comm_type, recv_data_ptr->ch, 0, PTCL_EVENT_REPLY_ACK_BT, (rt_uint16_t *)0);
						_ptcl_param_info_handler(&param_info_ptr, send_sta);
						goto __exit_recv_data;
					}

					if(RT_TRUE == smtcfg_data_decoder(report_data_ptr, recv_data_ptr, &param_info_ptr))
					{
						send_sta = _ptcl_report_data_send(report_data_ptr, 0, recv_data_ptr->comm_type, recv_data_ptr->ch, 0, PTCL_EVENT_REPLY_ACK_BT, (rt_uint16_t *)0);
						_ptcl_param_info_handler(&param_info_ptr, send_sta);
						goto __exit_recv_data;
					}

					if(RT_TRUE == xm_data_decoder(recv_data_ptr))
					{
						goto __exit_recv_data;
					}

//					if(RT_TRUE == sl651_data_decoder(report_data_ptr, recv_data_ptr, &param_info_ptr))
//					{
//						send_sta = _ptcl_report_data_send(report_data_ptr, 0, recv_data_ptr->comm_type, recv_data_ptr->ch, 0, PTCL_EVENT_REPLY_ACK_BT, (rt_uint16_t *)0);
//						_ptcl_param_info_handler(&param_info_ptr, send_sta);
//						goto __exit_recv_data;
//					}

__exit_recv_data:
					rt_mp_free((void *)report_data_ptr);
				}
				
				rt_mp_free((void *)recv_data_ptr);
			}

__exit_parg:
			rt_mp_free(parg);
			
			level = rt_hw_interrupt_disable();
			if(PTCL_RUN_STA_NORMAL != m_ptcl_run_sta)
			{
				if(PTCL_RUN_STA_RESTORE == m_ptcl_run_sta)
				{
					rt_hw_interrupt_enable(level);
					dtu_param_restore();
					ptcl_param_restore();
					smtmng_param_restore();
					sample_param_restore();
					ads_param_restore();
					hj212_param_restore();
//					sl651_param_restore();
				}
				else
				{
					rt_hw_interrupt_enable(level);
				}
				HAL_NVIC_SystemReset();
			}
			else
			{
				rt_hw_interrupt_enable(level);
			}
		}

		lpm_cpu_ref(RT_FALSE);
	}
}

//初始化
static int _ptcl_component_init(void)
{
	//自报mailbox
	if(RT_EOK != rt_mb_init(&m_ptcl_mb_auto_report, "ptcl", (void *)m_ptcl_msgpool_auto_report, PTCL_NUM_AUTO_REPORT, RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	
	//tc接收mailbox
	if(RT_EOK != rt_mb_init(&m_ptcl_mb_tc_data, "ptcl", (void *)&m_ptcl_msgpool_tc_data, 1, RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	
	//shell接收mailbox
	if(RT_EOK != rt_mb_init(&m_ptcl_mb_shell_data, "ptcl", (void *)&m_ptcl_msgpool_shell_data, 1, RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	
	//运行相关
	if(RT_EOK != rt_event_init(&m_ptcl_event_module, "ptcl", RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	if(RT_EOK != rt_event_send(&m_ptcl_event_module, PTCL_EVENT_AUTO_REPORT_PEND + PTCL_EVENT_REPLY_LIST_PEND))
	{
		while(1);
	}
	if(RT_EOK != rt_event_init(&m_ptcl_event_param, "ptcl", RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	if(RT_EOK != rt_event_send(&m_ptcl_event_param, PTCL_PARAM_ALL))
	{
		while(1);
	}
	
	//参数集合
	if(RT_TRUE == _ptcl_get_param_set(&m_ptcl_param_set))
	{
		_ptcl_validate_param_set(&m_ptcl_param_set);
	}
	else
	{
		_ptcl_reset_param_set(&m_ptcl_param_set);
	}

	usbd_set_recv_hdr(_ptcl_tc_buf_fill);
	dtu_fun_mount(_ptcl_heart_encoder);

	return 0;
}
INIT_COMPONENT_EXPORT(_ptcl_component_init);

static int _ptcl_app_init(void)
{
	//自报管理任务
	if(RT_EOK != rt_thread_init(&m_ptcl_thread_auto_report_manage, "ptcl", _ptcl_task_auto_report_manage, (void *)0, (void *)m_ptcl_stk_auto_report_manage, PTCL_STK_AUTO_REPORT_MANAGE, PTCL_PRIO_AUTO_REPORT_MANAGE, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_ptcl_thread_auto_report_manage))
	{
		while(1);
	}
	//dtu接收处理任务
	if(RT_EOK != rt_thread_init(&m_ptcl_thread_dtu_recv_handler, "ptcl", _ptcl_task_dtu_recv_handler, (void *)0, (void *)m_ptcl_stk_dtu_recv_handler, PTCL_STK_DTU_RECV_HANDLER, PTCL_PRIO_DTU_RECV_HANDLER, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_ptcl_thread_dtu_recv_handler))
	{
		while(1);
	}
	//tc接收处理任务
	if(RT_EOK != rt_thread_init(&m_ptcl_thread_tc_recv_handler, "ptcl", _ptcl_task_tc_recv_handler, (void *)0, (void *)m_ptcl_stk_tc_recv_handler, PTCL_STK_TC_RECV_HANDLER, PTCL_PRIO_TC_RECV_HANDLER, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_ptcl_thread_tc_recv_handler))
	{
		while(1);
	}
	//tc缓冲区处理任务
	if(RT_EOK != rt_thread_init(&m_ptcl_thread_tc_buf_handler, "ptcl", _ptcl_task_tc_buf_handler, (void *)0, (void *)m_ptcl_stk_tc_buf_handler, PTCL_STK_TC_BUF_HANDLER, PTCL_PRIO_TC_BUF_HANDLER, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_ptcl_thread_tc_buf_handler))
	{
		while(1);
	}
	//shell接收处理任务
	if(RT_EOK != rt_thread_init(&m_ptcl_thread_shell_recv_handler, "ptcl", _ptcl_task_shell_recv_handler, (void *)0, (void *)m_ptcl_stk_shell_recv_handler, PTCL_STK_SHELL_RECV_HANDLER, PTCL_PRIO_SHELL_RECV_HANDLER, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_ptcl_thread_shell_recv_handler))
	{
		while(1);
	}
	//bt接收处理任务
	if(RT_EOK != rt_thread_init(&m_ptcl_thread_bt_recv_handler, "ptcl", _ptcl_task_bt_recv_handler, (void *)0, (void *)m_ptcl_stk_bt_recv_handler, PTCL_STK_BT_RECV_HANDLER, PTCL_PRIO_BT_RECV_HANDLER, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_ptcl_thread_bt_recv_handler))
	{
		while(1);
	}

	return 0;
}
INIT_APP_EXPORT(_ptcl_app_init);



//参数读写
void ptcl_set_center_addr(rt_uint8_t center_addr, rt_uint8_t center)
{
	if(center >= PTCL_NUM_CENTER)
	{
		return;
	}
	
	_ptcl_param_pend(PTCL_PARAM_CENTER_ADDR << center);
	if(m_ptcl_center_info[center].center_addr != center_addr)
	{
		m_ptcl_center_info[center].center_addr = center_addr;
		_ptcl_set_center_addr(center_addr, center);
	}
	_ptcl_param_post(PTCL_PARAM_CENTER_ADDR << center);
}
rt_uint8_t ptcl_get_center_addr(rt_uint8_t center)
{
	rt_uint8_t center_addr;

	if(center >= PTCL_NUM_CENTER)
	{
		return 0;
	}
	
	_ptcl_param_pend(PTCL_PARAM_CENTER_ADDR << center);
	center_addr = m_ptcl_center_info[center].center_addr;
	_ptcl_param_post(PTCL_PARAM_CENTER_ADDR << center);

	return center_addr;
}

void ptcl_set_comm_type(rt_uint8_t comm_type, rt_uint8_t center, rt_uint8_t prio)
{
	if(comm_type >= PTCL_NUM_COMM_TYPE)
	{
		return;
	}
	if(center >= PTCL_NUM_CENTER)
	{
		return;
	}
	if(prio >= PTCL_NUM_COMM_PRIO)
	{
		return;
	}
	
	_ptcl_param_pend(PTCL_PARAM_COMM_TYPE << center);
	if(m_ptcl_center_info[center].comm_type[prio] != comm_type)
	{
		m_ptcl_center_info[center].comm_type[prio] = comm_type;
		_ptcl_set_comm_type(comm_type, center, prio);
	}
	_ptcl_param_post(PTCL_PARAM_COMM_TYPE << center);
}
rt_uint8_t ptcl_get_comm_type(rt_uint8_t center, rt_uint8_t prio)
{
	rt_uint8_t comm_type;

	if(center >= PTCL_NUM_CENTER)
	{
		return PTCL_COMM_TYPE_NONE;
	}
	if(prio >= PTCL_NUM_COMM_PRIO)
	{
		return PTCL_COMM_TYPE_NONE;
	}
	
	_ptcl_param_pend(PTCL_PARAM_COMM_TYPE << center);
	comm_type = m_ptcl_center_info[center].comm_type[prio];
	_ptcl_param_post(PTCL_PARAM_COMM_TYPE << center);

	return comm_type;
}

void ptcl_set_ram_ptr(PTCL_RAM_PTR const *ram_ptr, rt_uint8_t center)
{
	if((PTCL_RAM_PTR *)0 == ram_ptr)
	{
		return;
	}
	if((ram_ptr->in >= PTCL_NUM_CENTER_RAM) || (ram_ptr->out >= PTCL_NUM_CENTER_RAM))
	{
		return;
	}
	if(center >= PTCL_NUM_CENTER)
	{
		return;
	}
	
	_ptcl_param_pend(PTCL_PARAM_RAM_PTR << center);
	m_ptcl_center_info[center].ram_ptr = *ram_ptr;
	_ptcl_set_ram_ptr(ram_ptr, center);
	_ptcl_param_post(PTCL_PARAM_RAM_PTR << center);
}
void ptcl_get_ram_ptr(PTCL_RAM_PTR *ram_ptr, rt_uint8_t center)
{
	if((PTCL_RAM_PTR *)0 == ram_ptr)
	{
		return;
	}
	if(center >= PTCL_NUM_CENTER)
	{
		ram_ptr->in		= PTCL_NUM_CENTER_RAM;
		ram_ptr->out	= PTCL_NUM_CENTER_RAM;
		return;
	}
	
	_ptcl_param_pend(PTCL_PARAM_RAM_PTR << center);
	*ram_ptr = m_ptcl_center_info[center].ram_ptr;
	_ptcl_param_post(PTCL_PARAM_RAM_PTR << center);
}

void ptcl_set_ptcl_type(rt_uint8_t ptcl_type, rt_uint8_t center)
{
	if(ptcl_type >= PTCL_NUM_PTCL_TYPE)
	{
		return;
	}
	if(center >= PTCL_NUM_CENTER)
	{
		return;
	}

	_ptcl_param_pend(PTCL_PARAM_PTCL_TYPE << center);
	if(m_ptcl_center_info[center].ptcl_type != ptcl_type)
	{
		m_ptcl_center_info[center].ptcl_type = ptcl_type;
		_ptcl_set_ptcl_type(ptcl_type, center);
	}
	_ptcl_param_post(PTCL_PARAM_PTCL_TYPE << center);
}
rt_uint8_t ptcl_get_ptcl_type(rt_uint8_t center)
{
	rt_uint8_t ptcl_type;
	
	if(center >= PTCL_NUM_CENTER)
	{
		return PTCL_PTCL_TYPE_SL427;
	}

	_ptcl_param_pend(PTCL_PARAM_PTCL_TYPE << center);
	ptcl_type = m_ptcl_center_info[center].ptcl_type;
	_ptcl_param_post(PTCL_PARAM_PTCL_TYPE << center);

	return ptcl_type;
}

//参数恢复出厂
void ptcl_param_restore(void)
{
	rt_uint8_t i, j;
	
	_ptcl_param_pend(PTCL_PARAM_ALL);
	
	//中心站信息
	for(i = 0; i < PTCL_NUM_CENTER; i++)
	{
		m_ptcl_center_info[i].center_addr	= 0;
		
		for(j = 0; j < PTCL_NUM_COMM_PRIO; j++)
		{
			m_ptcl_center_info[i].comm_type[j] = PTCL_COMM_TYPE_NONE;
		}
		
		m_ptcl_center_info[i].ram_ptr.in	= 0;
		m_ptcl_center_info[i].ram_ptr.out	= 0;

		m_ptcl_center_info[i].ptcl_type		= PTCL_PTCL_TYPE_HJ212;
	}

	_ptcl_set_param_set(&m_ptcl_param_set);
	
	_ptcl_param_post(PTCL_PARAM_ALL);
}

//上报数据申请
PTCL_REPORT_DATA *ptcl_report_data_req(rt_uint16_t bytes_req, rt_int32_t ticks)
{
	PTCL_REPORT_DATA *report_data_ptr;
	
	if(0 == bytes_req)
	{
		return (PTCL_REPORT_DATA *)0;
	}
	
	bytes_req += sizeof(PTCL_REPORT_DATA);
	report_data_ptr = (PTCL_REPORT_DATA *)mempool_req(bytes_req, ticks);
	if((PTCL_REPORT_DATA *)0 != report_data_ptr)
	{
		report_data_ptr->pdata = (rt_uint8_t *)(report_data_ptr + 1);
	}

	return report_data_ptr;
}

//接收数据申请
PTCL_RECV_DATA *ptcl_recv_data_req(rt_uint16_t bytes_req, rt_int32_t ticks)
{
	PTCL_RECV_DATA *recv_data_ptr;
	
	if(0 == bytes_req)
	{
		return (PTCL_RECV_DATA *)0;
	}
	
	bytes_req += sizeof(PTCL_RECV_DATA);
	recv_data_ptr = (PTCL_RECV_DATA *)mempool_req(bytes_req, ticks);
	if((PTCL_RECV_DATA *)0 != recv_data_ptr)
	{
		recv_data_ptr->pdata = (rt_uint8_t *)(recv_data_ptr + 1);
	}
	
	return recv_data_ptr;
}

//参数信息申请
PTCL_PARAM_INFO *ptcl_param_info_req(rt_uint16_t bytes_req, rt_int32_t ticks)
{
	PTCL_PARAM_INFO *param_info_ptr;
	
	if(0 == bytes_req)
	{
		return (PTCL_PARAM_INFO *)0;
	}
	
	bytes_req += sizeof(PTCL_PARAM_INFO);
	param_info_ptr = (PTCL_PARAM_INFO *)mempool_req(bytes_req, ticks);
	if((PTCL_PARAM_INFO *)0 != param_info_ptr)
	{
		param_info_ptr->pdata = (rt_uint8_t *)(param_info_ptr + 1);
	}
	
	return param_info_ptr;
}

//参数信息添加
void ptcl_param_info_add(PTCL_PARAM_INFO **param_info_ptr_ptr, PTCL_PARAM_INFO *param_info_ptr)
{
	if((PTCL_PARAM_INFO **)0 == param_info_ptr_ptr)
	{
		return;
	}
	if((PTCL_PARAM_INFO *)0 == param_info_ptr)
	{
		return;
	}

	param_info_ptr->next = *param_info_ptr_ptr;
	*param_info_ptr_ptr = param_info_ptr;
}

//上报数据
rt_uint8_t ptcl_report_data_send(PTCL_REPORT_DATA *report_data_ptr, rt_uint8_t comm_type, rt_uint8_t ch, rt_uint8_t center_addr, rt_uint32_t event_reply, rt_uint16_t *reply_info_ptr)
{
	if((PTCL_REPORT_DATA *)0 == report_data_ptr)
	{
		return RT_FALSE;
	}

	return _ptcl_report_data_send(report_data_ptr, 0, comm_type, ch, center_addr, event_reply, reply_info_ptr);
}

//回执释放
void ptcl_reply_post(rt_uint8_t comm_type, rt_uint8_t ch, rt_uint16_t data_id, rt_uint16_t reply_info)
{
	rt_uint32_t	event_reply;
	rt_uint8_t	i = 0;

	while(1)
	{
		event_reply = _ptcl_reply_list_find(comm_type, ch, data_id, reply_info);
		if(0 == event_reply)
		{
			i++;
			if(i < PTCL_REPLY_VALID_TIMEOUT)
			{
				rt_thread_delay(RT_TICK_PER_SECOND);
			}
			else
			{
				return;
			}
		}
		else
		{
			if(RT_EOK != rt_event_send(&m_ptcl_event_module, event_reply))
			{
				while(1);
			}
			return;
		}
	}
}

//自报数据POST
rt_uint8_t ptcl_auto_report_post(PTCL_REPORT_DATA *report_data_ptr)
{
	if((PTCL_REPORT_DATA *)0 == report_data_ptr)
	{
		return RT_FALSE;
	}

	if(RT_EOK == rt_mb_send_wait(&m_ptcl_mb_auto_report, (rt_ubase_t)report_data_ptr, RT_WAITING_NO))
	{
		return RT_TRUE;
	}
	else
	{
		return RT_FALSE;
	}
}

//运行状态
rt_uint8_t ptcl_run_sta_set(rt_uint8_t sta)
{
	rt_base_t level;

	if((PTCL_RUN_STA_RESET != sta) && (PTCL_RUN_STA_RESTORE != sta))
	{
		return RT_FALSE;
	}

	level = rt_hw_interrupt_disable();
	if(PTCL_RUN_STA_NORMAL == m_ptcl_run_sta)
	{
		m_ptcl_run_sta = sta;
		rt_hw_interrupt_enable(level);
		return RT_TRUE;
	}
	else
	{
		rt_hw_interrupt_enable(level);
		return RT_FALSE;
	}
}

//是否有壳子
rt_uint8_t ptcl_have_shell(void)
{
	if(&m_ptcl_thread_shell_recv_handler == rt_thread_self())
	{
		return RT_TRUE;
	}

	return RT_FALSE;
}

rt_uint8_t ptcl_shell_data_post(PTCL_RECV_DATA *shell_data_ptr)
{
	if((PTCL_RECV_DATA *)0 == shell_data_ptr)
	{
		return RT_FALSE;
	}

	if(RT_EOK == rt_mb_send_wait(&m_ptcl_mb_shell_data, (rt_ubase_t)shell_data_ptr, RT_WAITING_NO))
	{
		return RT_TRUE;
	}
	else
	{
		return RT_FALSE;
	}
}

