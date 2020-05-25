#include "sl651.h"
#include "sample.h"
#include "drv_fyyp.h"
#include "dtu.h"
#include "pwr_manage.h"
#include "drv_mempool.h"
#include "warn_manage.h"
#include "bx5k.h"



//事件标志
static struct rt_event			m_sl651_event_module;

//历史数据下载
static SL651_DATA_DOWNLOAD		m_sl651_data_download, *m_sl651_data_download_ptr;
static rt_ubase_t				m_sl651_msgpool_data_download;
static struct rt_mailbox		m_sl651_mb_data_download;

//任务
static struct rt_thread			m_sl651_thread_report_time;
static struct rt_thread			m_sl651_thread_random_sample;
static struct rt_thread			m_sl651_thread_data_download;
static rt_uint8_t				m_sl651_stk_report_time[SL651_STK_REPORT_TIME];
static rt_uint8_t				m_sl651_stk_random_sample[SL651_STK_RANDOM_SAMPLE];
static rt_uint8_t				m_sl651_stk_data_download[SL651_STK_DATA_DOWNLOAD];

//参数
static rt_uint8_t				m_sl651_rtu_addr[SL651_BYTES_RTU_ADDR];		//地址
static rt_uint16_t				m_sl651_pw;									//密码
static rt_uint16_t				m_sl651_sn;									//流水号
static rt_uint8_t				m_sl651_rtu_type;							//分类码
static rt_uint8_t				m_sl651_report_hour;						//定时报间隔
static rt_uint8_t				m_sl651_report_min;							//加报间隔
static rt_uint8_t				m_sl651_random_period;						//随机采集间隔



#include "sl651_io.c"



//CRC校验函数
static rt_uint16_t _sl651_crc_value(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint16_t	crc_value = 0xffff;
	rt_uint8_t	i;
	
	while(data_len)
	{
		data_len--;
		crc_value ^= *pdata++;
		for(i = 0; i < 8; i++)
		{
			if(crc_value & 1)
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

//参数互斥
static void _sl651_param_pend(rt_uint32_t param)
{
	if(RT_EOK != rt_event_recv(&m_sl651_event_module, param, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}
}
static void _sl651_param_post(rt_uint32_t param)
{
	if(RT_EOK != rt_event_send(&m_sl651_event_module, param))
	{
		while(1);
	}
}

//帧头编码
static rt_uint16_t _sl651_bao_tou(rt_uint8_t *pdata, rt_uint8_t center_addr, rt_uint8_t afn, rt_uint8_t start_symbol)
{
	rt_uint16_t i = 0;
	
	if((rt_uint8_t *)0 == pdata)
	{
		return 0;
	}
	
	//帧标志
	pdata[i++] = SL651_FRAME_SYMBOL;
	pdata[i++] = SL651_FRAME_SYMBOL;
	//中心站地址
	pdata[i++] = center_addr;
	//RTU地址
	_sl651_param_pend(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_PW);
	rt_memcpy((void *)(pdata + i), (void *)m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
	i += SL651_BYTES_RTU_ADDR;
	//密码
	pdata[i++] = (rt_uint8_t)(m_sl651_pw >> 8);
	pdata[i++] = (rt_uint8_t)m_sl651_pw;
	_sl651_param_post(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_PW);
	//功能码
	pdata[i++] = afn;
	//上下行标识及长度
	pdata[i++] = 0;
	pdata[i++] = 0;
	//正文起始符
	pdata[i++] = start_symbol;
	
	return i;
}

//流水号
static rt_uint16_t _sl651_sn_value(rt_uint8_t valid)
{
	rt_uint16_t sn;
	
	_sl651_param_pend(SL651_EVENT_PARAM_SN);
	sn = m_sl651_sn;
	if(RT_TRUE == valid)
	{
		if(0xffff == m_sl651_sn)
		{
			m_sl651_sn = 1;
		}
		else
		{
			m_sl651_sn++;
		}
		_sl651_set_sn(m_sl651_sn);
		_sl651_param_post(SL651_EVENT_PARAM_SN);
	}
	else
	{
		_sl651_param_post(SL651_EVENT_PARAM_SN);
		if(sn)
		{
			sn--;
		}
		if(0 == sn)
		{
			sn = 0xffff;
		}
	}
	
	return sn;
}

#if 0
//要素监测状态
static rt_uint8_t _sl651_element_is_active(rt_uint8_t element)
{
	if(element >= SL651_NUM_ELEMENT)
	{
		return RT_FALSE;
	}
	
	_sl651_param_pend(SL651_EVENT_PARAM_ELEMENT);
	if(m_sl651_element[element >> 3] & (1 << (element & 7)))
	{
		_sl651_param_post(SL651_EVENT_PARAM_ELEMENT);
		return RT_TRUE;
	}
	else
	{
		_sl651_param_post(SL651_EVENT_PARAM_ELEMENT);
		return RT_FALSE;
	}
}
#endif

//数据格式化(带标识符)
static rt_uint16_t _sl651_data_format_with_symbol(rt_uint8_t *pdata, rt_uint8_t param, rt_uint8_t sta, float value)
{
	rt_uint16_t	data_len = 0;
	rt_uint32_t	temp32;
	
	if((rt_uint8_t *)0 == pdata)
	{
		return 0;
	}

	switch(param)
	{
	case SAMPLE_SENSOR_RAIN:
		pdata[data_len++] = SL651_BOOT_RAIN_TOTAL;
		if(RT_FALSE == sta)
		{
			pdata[data_len++] = SL651_LEN_RAIN_TOTAL;
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
		}
		else
		{
			if(value < 0)
			{
				temp32 = (rt_uint32_t)(0 - value);
				pdata[data_len++] = SL651_LEN_RAIN_TOTAL + 8;
				pdata[data_len++] = SL651_FRAME_SIGN_NEGATIVE;
			}
			else
			{
				temp32 = (rt_uint32_t)value;
				pdata[data_len++] = SL651_LEN_RAIN_TOTAL;
			}
			pdata[data_len++] = fyyp_hex_to_bcd(temp32 / 10000 % 100);
			pdata[data_len++] = fyyp_hex_to_bcd(temp32 / 100 % 100);
			pdata[data_len++] = fyyp_hex_to_bcd(temp32 % 100);
		}
		break;
	case SAMPLE_SENSOR_SHUIWEI_1:
		pdata[data_len++] = SL651_BOOT_WATER_CUR;
		if(RT_FALSE == sta)
		{
			pdata[data_len++] = SL651_LEN_WATER_CUR;
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
		}
		else
		{
			if(value < 0)
			{
				temp32 = (rt_uint32_t)(0 - value);
				pdata[data_len++] = SL651_LEN_WATER_CUR + 8;
				pdata[data_len++] = SL651_FRAME_SIGN_NEGATIVE;
			}
			else
			{
				temp32 = (rt_uint32_t)value;
				pdata[data_len++] = SL651_LEN_WATER_CUR;
			}
			pdata[data_len++] = fyyp_hex_to_bcd(temp32 / 1000000 % 100);
			pdata[data_len++] = fyyp_hex_to_bcd(temp32 / 10000 % 100);
			pdata[data_len++] = fyyp_hex_to_bcd(temp32 / 100 % 100);
			pdata[data_len++] = fyyp_hex_to_bcd(temp32 % 100);
		}
		break;
	case SAMPLE_SENSOR_SHANGQING_10:
	case SAMPLE_SENSOR_SHANGQING_20:
	case SAMPLE_SENSOR_SHANGQING_30:
	case SAMPLE_SENSOR_SHANGQING_40:
		pdata[data_len++] = SL651_BOOT_SHANGQING_10CM + param - SAMPLE_SENSOR_SHANGQING_10;
		if(RT_FALSE == sta)
		{
			pdata[data_len++] = SL651_LEN_SHANGQING;
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
		}
		else
		{
			if(value < 0)
			{
				temp32 = (rt_uint32_t)(0 - value);
				pdata[data_len++] = SL651_LEN_SHANGQING + 8;
				pdata[data_len++] = SL651_FRAME_SIGN_NEGATIVE;
			}
			else
			{
				temp32 = (rt_uint32_t)value;
				pdata[data_len++] = SL651_LEN_SHANGQING;
			}
			pdata[data_len++]		= fyyp_hex_to_bcd(temp32 / 100 % 100);
			pdata[data_len++]		= fyyp_hex_to_bcd(temp32 % 100);
		}
		break;
	}

	return data_len;
}
static rt_uint16_t _sl651_data_format_without_symbol(rt_uint8_t *pdata, rt_uint8_t param, rt_uint8_t sta, float value)
{
	rt_uint16_t	data_len = 0;
	rt_uint32_t	temp32;
	
	if((rt_uint8_t *)0 == pdata)
	{
		return 0;
	}

	switch(param)
	{
	case SAMPLE_SENSOR_RAIN:
		if(RT_FALSE == sta)
		{
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
		}
		else
		{
			if(value < 0)
			{
				temp32 = (rt_uint32_t)(0 - value);
				pdata[data_len++] = SL651_FRAME_SIGN_NEGATIVE;
			}
			else
			{
				temp32 = (rt_uint32_t)value;
			}
			pdata[data_len++] = fyyp_hex_to_bcd(temp32 / 10000 % 100);
			pdata[data_len++] = fyyp_hex_to_bcd(temp32 / 100 % 100);
			pdata[data_len++] = fyyp_hex_to_bcd(temp32 % 100);
		}
		break;
	case SAMPLE_SENSOR_SHUIWEI_1:
		if(RT_FALSE == sta)
		{
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
		}
		else
		{
			if(value < 0)
			{
				temp32 = (rt_uint32_t)(0 - value);
				pdata[data_len++] = SL651_FRAME_SIGN_NEGATIVE;
			}
			else
			{
				temp32 = (rt_uint32_t)value;
			}
			pdata[data_len++] = fyyp_hex_to_bcd(temp32 / 1000000 % 100);
			pdata[data_len++] = fyyp_hex_to_bcd(temp32 / 10000 % 100);
			pdata[data_len++] = fyyp_hex_to_bcd(temp32 / 100 % 100);
			pdata[data_len++] = fyyp_hex_to_bcd(temp32 % 100);
		}
		break;
	case SAMPLE_SENSOR_SHANGQING_10:
	case SAMPLE_SENSOR_SHANGQING_20:
	case SAMPLE_SENSOR_SHANGQING_30:
	case SAMPLE_SENSOR_SHANGQING_40:
		if(RT_FALSE == sta)
		{
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
			pdata[data_len++] = SL651_FRAME_INVALID_DATA;
		}
		else
		{
			if(value < 0)
			{
				temp32 = (rt_uint32_t)(0 - value);
				pdata[data_len++] = SL651_FRAME_SIGN_NEGATIVE;
			}
			else
			{
				temp32 = (rt_uint32_t)value;
			}
			pdata[data_len++] = fyyp_hex_to_bcd(temp32 / 100 % 100);
			pdata[data_len++] = fyyp_hex_to_bcd(temp32 % 100);
		}
		break;
	}

	return data_len;
}

//信道更新，如某中心的主备信道均未配置短信信道，但是相应的短信信道是开启状态，则要将短信信道关闭
static void _sl651_comm_update(rt_uint8_t center)
{
	rt_uint8_t	i;
	rt_uint16_t	comm_type = 0;

	if(center >= PTCL_NUM_CENTER)
	{
		return;
	}
	
	for(i = 0; i < PTCL_NUM_COMM_PRIO; i++)
	{
		comm_type |= (1 << ptcl_get_comm_type(center, i));
	}
	
	if(center < DTU_NUM_IP_CH)
	{
		if((RT_TRUE == dtu_get_ip_ch(center)) && (0 == (comm_type & (1 << PTCL_COMM_TYPE_IP))))
		{
			dtu_set_ip_ch(RT_FALSE, center);
		}
	}
	
	if(center < DTU_NUM_SMS_CH)
	{
		if((RT_TRUE == dtu_get_sms_ch(center)) && (0 == (comm_type & (1 << PTCL_COMM_TYPE_SMS))))
		{
			dtu_set_sms_ch(RT_FALSE, center);
		}
	}
}

//多包编码(packet_cur从1开始)
static PTCL_REPORT_DATA *_sl651_multi_encoder(rt_uint8_t const *pdata, rt_uint16_t data_len, rt_uint16_t packet_total, rt_uint16_t packet_cur, rt_uint8_t center_addr)
{
	PTCL_REPORT_DATA	*report_data_ptr;
	rt_uint16_t			temp16, pos;
	struct tm			time;

	if((rt_uint8_t *)0 == pdata)
	{
		return (PTCL_REPORT_DATA *)0;
	}
	if(0 == data_len)
	{
		return (PTCL_REPORT_DATA *)0;
	}
	if(packet_cur > packet_total)
	{
		return (PTCL_REPORT_DATA *)0;
	}

	if(1 == packet_cur)
	{
		temp16 = SL651_BYTES_FRAME_HEAD;
		temp16 += SL651_BYTES_FRAME_PACKET_INFO;
		temp16 += SL651_BYTES_SN;
		temp16 += SL651_BYTES_SEND_TIME;
		temp16 += SL651_BYTES_FRAME_RTU_ADDR;
		temp16 += SL651_BYTES_RTU_TYPE;
		temp16 += SL651_BYTES_FRAME_OBSERVE_TIME;
		temp16 += (2 + data_len);
		temp16 += SL651_BYTES_FRAME_END;
	}
	else
	{
		temp16 = SL651_BYTES_FRAME_HEAD;
		temp16 += SL651_BYTES_FRAME_PACKET_INFO;
		temp16 += data_len;
		temp16 += SL651_BYTES_FRAME_END;
	}
	report_data_ptr = ptcl_report_data_req(temp16, RT_WAITING_FOREVER);
	if((PTCL_REPORT_DATA *)0 == report_data_ptr)
	{
		return (PTCL_REPORT_DATA *)0;
	}
	//帧头
	report_data_ptr->data_len = _sl651_bao_tou(report_data_ptr->pdata, center_addr, SL651_AFN_QUERY_PHOTO, SL651_FRAME_SYMBOL_SYN);
	//包信息
	report_data_ptr->pdata[report_data_ptr->data_len++] = (packet_total >> 4);
	report_data_ptr->pdata[report_data_ptr->data_len++] = ((packet_total & 0x0f) << 4) + (packet_cur >> 8);
	report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)packet_cur;
	if(1 == packet_cur)
	{
		//流水号
		temp16 = _sl651_sn_value(RT_TRUE);
		report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(temp16 >> 8);
		report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)temp16;
		//发报时间
		time = rtcee_rtc_get_calendar();
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_year % 100);
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mon + 1);
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mday);
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_hour);
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_min);
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_sec);
		//遥测站地址、测站分类码
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_RTU_CODE;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_RTU_CODE;
		_sl651_param_pend(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_RTU_TYPE);
		rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
		report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
		report_data_ptr->pdata[report_data_ptr->data_len++] = m_sl651_rtu_type;
		_sl651_param_post(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_RTU_TYPE);
		//观测时间
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_OBSERVE_TIME;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_OBSERVE_TIME;
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_year % 100);
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mon + 1);
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mday);
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_hour);
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_min);
		//图片数据标识符
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_PHOTO_INFO;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_PHOTO_INFO;
	}
	//图片数据
	rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)pdata, data_len);
	report_data_ptr->data_len += data_len;
	//上下行标识及长度
	pos = SL651_BYTES_FRAME_HEAD - SL651_BYTES_START_SYMBOL - SL651_BYTES_UP_DOWN_LEN;
	temp16 = report_data_ptr->data_len - SL651_BYTES_FRAME_HEAD;
	report_data_ptr->pdata[pos]		= (rt_uint8_t)(temp16 >> 8);
	report_data_ptr->pdata[pos++]	+= (rt_uint8_t)(SL651_FRAME_DIR_UP << 4);
	report_data_ptr->pdata[pos]		= (rt_uint8_t)temp16;
	//帧尾
	if(packet_cur == packet_total)
	{
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_FRAME_SYMBOL_ETX;
	}
	else
	{
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_FRAME_SYMBOL_ETB;
	}
	temp16 = _sl651_crc_value(report_data_ptr->pdata, report_data_ptr->data_len);
	report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(temp16 >> 8);
	report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)temp16;

	report_data_ptr->fun_csq_update	= (void *)0;
	report_data_ptr->data_id		= packet_total;
	report_data_ptr->need_reply		= RT_FALSE;
	report_data_ptr->fcb_value		= SL651_FRAME_DEFAULT_FCB;
	report_data_ptr->ptcl_type		= PTCL_PTCL_TYPE_SL651;

	return report_data_ptr;
}

static void _sl651_csq_update(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t csq_value)
{
	rt_uint16_t i;

	if((rt_uint8_t *)0 == pdata)
	{
		return;
	}

	i = SL651_BYTES_FRAME_HEAD + SL651_BYTES_SN + SL651_BYTES_SEND_TIME + SL651_BYTES_FRAME_RTU_ADDR + SL651_BYTES_RTU_TYPE + SL651_BYTES_FRAME_OBSERVE_TIME;
	while((i + SL651_BYTES_FRAME_CSQ_EX + SL651_BYTES_FRAME_END) <= data_len)
	{
		if((rt_uint8_t)(SL651_BOOT_CSQ_EX >> 8) == pdata[i])
		{
			if((rt_uint8_t)SL651_BOOT_CSQ_EX == pdata[i + 1])
			{
				if(SL651_LEN_CSQ_EX == pdata[i + 2])
				{
					pdata[i + 3] = fyyp_hex_to_bcd(csq_value);
					data_len -= SL651_BYTES_CRC_VALUE;
					i = _sl651_crc_value(pdata, data_len);
					pdata[data_len++]	= (rt_uint8_t)(i >> 8);
					pdata[data_len]		= (rt_uint8_t)i;
					break;
				}
			}
		}
		i++;
	}
}

//数据下载任务
static void _sl651_task_data_download(void *parg)
{
	rt_uint8_t				sensor_type;
	SL651_DATA_DOWNLOAD		*pdownload;
	rt_base_t				level;
	rt_uint16_t				packet_cur, packet_total, crc_value, sn, pos;
	rt_uint32_t				unix_step, unix_low, unix_high;
	PTCL_REPORT_DATA		*report_data_ptr;
	struct tm				time;

	while(1)
	{
		if(RT_EOK != rt_mb_recv(&m_sl651_mb_data_download, (rt_ubase_t *)&pdownload, RT_WAITING_FOREVER))
		{
			while(1);
		}

		if((SL651_DATA_DOWNLOAD *)0 != pdownload)
		{
			//得到要下载的要素
			sensor_type = SAMPLE_NUM_SENSOR_TYPE;
			switch(pdownload->element_boot)
			{
			case SL651_BOOT_RAIN_TOTAL:
				sensor_type = SAMPLE_SENSOR_RAIN;
				unix_step = pdownload->unix_step;
				packet_cur = 3;
				break;
			case SL651_BOOT_WATER_CUR:
				sensor_type = SAMPLE_SENSOR_SHUIWEI_1;
				unix_step = pdownload->unix_step;
				packet_cur = 4;
				break;
			}
			//可识别的要素
			if(sensor_type < SAMPLE_NUM_SENSOR_TYPE)
			{
				//计算数据点数
				packet_total = (pdownload->unix_high - pdownload->unix_low) / unix_step;
				packet_total++;
				//计算并申请数据空间
				if(packet_total > SL651_DATA_POINT_PER_PACKET)
				{
					packet_cur *= SL651_DATA_POINT_PER_PACKET;
					packet_cur += SL651_BYTES_FRAME_PACKET_INFO;
				}
				else
				{
					packet_cur *= packet_total;
				}
				packet_cur += SL651_BYTES_FRAME_HEAD;				//帧头
				packet_cur += SL651_BYTES_SN;						//流水号
				packet_cur += SL651_BYTES_SEND_TIME;				//发报时间
				packet_cur += SL651_BYTES_FRAME_RTU_ADDR;			//测站编码
				packet_cur += SL651_BYTES_RTU_TYPE;					//测站类型
				packet_cur += SL651_BYTES_FRAME_OBSERVE_TIME;		//观测时间
				packet_cur += SL651_BYTES_FRAME_TIME_STEP;			//时间步长
				packet_cur += SL651_BYTES_FRAME_ELEMENT_SYMBOL;		//要素标识符
				packet_cur += SL651_BYTES_FRAME_END;				//帧尾
				report_data_ptr = ptcl_report_data_req(packet_cur, RT_WAITING_FOREVER);
				if((PTCL_REPORT_DATA *)0 != report_data_ptr)
				{
					//包总数和包序号
					if(packet_total % SL651_DATA_POINT_PER_PACKET)
					{
						packet_total /= SL651_DATA_POINT_PER_PACKET;
						packet_total++;
					}
					else
					{
						packet_total /= SL651_DATA_POINT_PER_PACKET;
					}
					packet_cur = 1;
					
					//帧头
					if(packet_total > 1)
					{
						report_data_ptr->data_len = _sl651_bao_tou(report_data_ptr->pdata, pdownload->center_addr, SL651_AFN_QUERY_OLD_DATA, SL651_FRAME_SYMBOL_SYN);
						report_data_ptr->pdata[report_data_ptr->data_len++] = (packet_total >> 4);
						report_data_ptr->pdata[report_data_ptr->data_len++] = ((packet_total & 0x0f) << 4) + (packet_cur >> 8);
						report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)packet_cur;
					}
					else
					{
						report_data_ptr->data_len = _sl651_bao_tou(report_data_ptr->pdata, pdownload->center_addr, SL651_AFN_QUERY_OLD_DATA, SL651_FRAME_SYMBOL_STX);
					}
					//流水号
					sn = _sl651_sn_value(RT_TRUE);
					report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(sn >> 8);
					report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)sn;
					if(packet_total > 1)
					{
						report_data_ptr->data_id = packet_total;
					}
					else
					{
						report_data_ptr->data_id = sn;
					}
					//发报时间
					time = rtcee_rtc_get_calendar();
					report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_year % 100);
					report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mon + 1);
					report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mday);
					report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_hour);
					report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_min);
					report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_sec);
					//遥测站地址、测站分类码
					report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_RTU_CODE;
					report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_RTU_CODE;
					_sl651_param_pend(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_RTU_TYPE);
					rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
					report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
					report_data_ptr->pdata[report_data_ptr->data_len++] = m_sl651_rtu_type;
					_sl651_param_post(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_RTU_TYPE);
					//观测时间
					time = rtcee_rtc_unix_to_calendar(pdownload->unix_low);
					report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_OBSERVE_TIME;
					report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_OBSERVE_TIME;
					report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_year % 100);
					report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mon + 1);
					report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mday);
					report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_hour);
					report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_min);
					//时间步长
					report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_TIME_STEP;
					report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_TIME_STEP;
					report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(pdownload->unix_step / 86400);
					report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(pdownload->unix_step % 86400 / 3600);
					report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(pdownload->unix_step % 3600 / 60);
					//要素标识符
					report_data_ptr->pdata[report_data_ptr->data_len] = (rt_uint8_t)(pdownload->element_boot >> 8);
					if(SL651_FRAME_SYMBOL_ELEMENT_EX == report_data_ptr->pdata[report_data_ptr->data_len])
					{
						report_data_ptr->data_len++;
					}
					report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)pdownload->element_boot;
					report_data_ptr->pdata[report_data_ptr->data_len++] = pdownload->element_len;
					
					//数据下载发送
					unix_low = pdownload->unix_low;
					while(unix_low <= pdownload->unix_high)
					{
						//本包数据结束的时间点
						unix_high = unix_step;
						unix_high *= (SL651_DATA_POINT_PER_PACKET - 1);
						unix_high += unix_low;
						if(unix_high > pdownload->unix_high)
						{
							unix_high = pdownload->unix_high;
						}
						//填充数据
						report_data_ptr->data_len += sample_query_data(report_data_ptr->pdata + report_data_ptr->data_len, sensor_type, unix_low, unix_high, unix_step, _sl651_data_format_without_symbol);
						//上下行标识及长度
						pos = SL651_BYTES_FRAME_HEAD - SL651_BYTES_START_SYMBOL - SL651_BYTES_UP_DOWN_LEN;
						crc_value = report_data_ptr->data_len - SL651_BYTES_FRAME_HEAD;
						report_data_ptr->pdata[pos]		= (rt_uint8_t)(crc_value >> 8);
						report_data_ptr->pdata[pos++]	+= (rt_uint8_t)(SL651_FRAME_DIR_UP << 4);
						report_data_ptr->pdata[pos]		= (rt_uint8_t)crc_value;
						//帧尾
						if(packet_cur == packet_total)
						{
							report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_FRAME_SYMBOL_ETX;
						}
						else
						{
							report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_FRAME_SYMBOL_ETB;
						}
						crc_value = _sl651_crc_value(report_data_ptr->pdata, report_data_ptr->data_len);
						report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(crc_value >> 8);
						report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)crc_value;

						report_data_ptr->fun_csq_update	= (void *)0;
						if(packet_cur == packet_total)
						{
							report_data_ptr->need_reply	= RT_TRUE;
						}
						else
						{
							report_data_ptr->need_reply	= RT_FALSE;
						}
						report_data_ptr->fcb_value		= SL651_FRAME_DEFAULT_FCB;
						report_data_ptr->ptcl_type		= PTCL_PTCL_TYPE_SL651;
						//发送
						crc_value = 0;
						ptcl_report_data_send(report_data_ptr, pdownload->comm_type, pdownload->ch, 0, PTCL_EVENT_REPLY_SL651, &crc_value);
						//后续处理
						unix_low = unix_high;
						unix_low += unix_step;
						packet_cur++;
						report_data_ptr->data_len = SL651_BYTES_FRAME_HEAD;
						report_data_ptr->pdata[report_data_ptr->data_len++] = (packet_total >> 4);
						report_data_ptr->pdata[report_data_ptr->data_len++] = ((packet_total & 0x0f) << 4) + (packet_cur >> 8);
						report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)packet_cur;
					}
					//补包
					while(crc_value)
					{
						if(crc_value >= packet_total)
						{
							crc_value = 0;
						}
						else
						{
							//计算要补的这一包的时间范围
							unix_low = crc_value - 1;
							unix_low *= SL651_DATA_POINT_PER_PACKET;
							unix_low *= unix_step;
							unix_low += pdownload->unix_low;
							unix_high = (SL651_DATA_POINT_PER_PACKET - 1);
							unix_high *= unix_step;
							unix_high += unix_low;
							if(unix_high > pdownload->unix_high)
							{
								unix_high = pdownload->unix_high;
							}
							//包总数及包序号
							report_data_ptr->data_len = SL651_BYTES_FRAME_HEAD;
							report_data_ptr->pdata[report_data_ptr->data_len++] = (packet_total >> 4);
							report_data_ptr->pdata[report_data_ptr->data_len++] = ((packet_total & 0x0f) << 4) + (crc_value >> 8);
							report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)crc_value;
							//如果是第1包
							if(1 == crc_value)
							{
								//流水号
								report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(sn >> 8);
								report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)sn;
								//发报时间
								time = rtcee_rtc_get_calendar();
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_year % 100);
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mon + 1);
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mday);
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_hour);
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_min);
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_sec);
								//遥测站地址、测站分类码
								report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_RTU_CODE;
								report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_RTU_CODE;
								_sl651_param_pend(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_RTU_TYPE);
								rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
								report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
								report_data_ptr->pdata[report_data_ptr->data_len++] = m_sl651_rtu_type;
								_sl651_param_post(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_RTU_TYPE);
								//观测时间
								time = rtcee_rtc_unix_to_calendar(pdownload->unix_low);
								report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_OBSERVE_TIME;
								report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_OBSERVE_TIME;
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_year % 100);
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mon + 1);
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mday);
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_hour);
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_min);
								//时间步长
								report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_TIME_STEP;
								report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_TIME_STEP;
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(pdownload->unix_step / 86400);
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(pdownload->unix_step % 86400 / 3600);
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(pdownload->unix_step % 3600 / 60);
								//要素标识符
								report_data_ptr->pdata[report_data_ptr->data_len] = (rt_uint8_t)(pdownload->element_boot >> 8);
								if(SL651_FRAME_SYMBOL_ELEMENT_EX == report_data_ptr->pdata[report_data_ptr->data_len])
								{
									report_data_ptr->data_len++;
								}
								report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)pdownload->element_boot;
								report_data_ptr->pdata[report_data_ptr->data_len++] = pdownload->element_len;
							}
							//数据
							report_data_ptr->data_len += sample_query_data(report_data_ptr->pdata + report_data_ptr->data_len, sensor_type, unix_low, unix_high, unix_step, _sl651_data_format_without_symbol);
							//上下行标识及长度
							pos = SL651_BYTES_FRAME_HEAD - SL651_BYTES_START_SYMBOL - SL651_BYTES_UP_DOWN_LEN;
							crc_value = report_data_ptr->data_len - SL651_BYTES_FRAME_HEAD;
							report_data_ptr->pdata[pos]		= (rt_uint8_t)(crc_value >> 8);
							report_data_ptr->pdata[pos++]	+= (rt_uint8_t)(SL651_FRAME_DIR_UP << 4);
							report_data_ptr->pdata[pos]		= (rt_uint8_t)crc_value;
							//帧尾
							report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_FRAME_SYMBOL_ETX;
							crc_value = _sl651_crc_value(report_data_ptr->pdata, report_data_ptr->data_len);
							report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(crc_value >> 8);
							report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)crc_value;

							report_data_ptr->fun_csq_update	= (void *)0;
							report_data_ptr->need_reply		= RT_TRUE;
							report_data_ptr->fcb_value		= SL651_FRAME_DEFAULT_FCB;
							report_data_ptr->ptcl_type		= PTCL_PTCL_TYPE_SL651;
							//发送
							crc_value = 0;
							ptcl_report_data_send(report_data_ptr, pdownload->comm_type, pdownload->ch, 0, PTCL_EVENT_REPLY_SL651, &crc_value);
						}
					}
					
					//释放数据空间
					rt_mp_free((void *)report_data_ptr);
				}
			}

			//释放下载工具
			level = rt_hw_interrupt_disable();
			m_sl651_data_download_ptr = pdownload;
			rt_hw_interrupt_enable(level);
		}
	}
}

//定时报任务
static void _sl651_task_report_time(void *parg)
{
	PTCL_REPORT_DATA	*report_data_ptr;
	PWRM_PWRSYS_INFO	*pwrsys_info_ptr;
	struct tm			time;
	rt_uint8_t			period_hour, period_min, report_en;
	rt_uint16_t			i, crc_value;
	SAMPLE_DATA_TYPE	data_type;

	while(1)
	{
		ptcl_event_min_pend();

		_sl651_param_pend(SL651_EVENT_PARAM_REPORT_HOUR + SL651_EVENT_PARAM_REPORT_MIN);
		period_hour	= m_sl651_report_hour;
		period_min	= m_sl651_report_min;
		_sl651_param_post(SL651_EVENT_PARAM_REPORT_HOUR + SL651_EVENT_PARAM_REPORT_MIN);

		if(period_hour || period_min)
		{
			time = rtcee_rtc_get_calendar();

			report_en = RT_FALSE;
			if(period_hour)
			{
				if((0 == time.tm_min) && (0 == time.tm_hour % period_hour))
				{
					report_en = RT_TRUE;
				}
			}
			if((RT_FALSE == report_en) && period_min)
			{
				if(0 == time.tm_min % period_min)
				{
					report_en = RT_TRUE;
				}
			}

			if(RT_TRUE == report_en)
			{
				report_data_ptr = ptcl_report_data_req(SL651_BYTES_REPORT_DATA, RT_WAITING_FOREVER);
				if((PTCL_REPORT_DATA *)0 != report_data_ptr)
				{
					while(1)
					{
						report_data_ptr->data_len = 0;
						//帧头
						if((report_data_ptr->data_len + SL651_BYTES_FRAME_HEAD) > SL651_BYTES_REPORT_DATA)
						{
							break;
						}
						report_data_ptr->data_len += _sl651_bao_tou(report_data_ptr->pdata + report_data_ptr->data_len, 0, SL651_AFN_REPORT_TIMING, SL651_FRAME_SYMBOL_STX);
						//流水号、发报时间
						if((report_data_ptr->data_len + SL651_BYTES_SN + SL651_BYTES_SEND_TIME) > SL651_BYTES_REPORT_DATA)
						{
							break;
						}
						report_data_ptr->data_len += (SL651_BYTES_SN + SL651_BYTES_SEND_TIME);
						//遥测站地址、分类码、观测时间
						if((report_data_ptr->data_len + SL651_BYTES_FRAME_RTU_ADDR + SL651_BYTES_RTU_TYPE + SL651_BYTES_FRAME_OBSERVE_TIME) > SL651_BYTES_REPORT_DATA)
						{
							break;
						}
						report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_RTU_CODE;
						report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_RTU_CODE;
						_sl651_param_pend(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_RTU_TYPE);
						rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
						report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
						report_data_ptr->pdata[report_data_ptr->data_len++] = m_sl651_rtu_type;
						_sl651_param_post(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_RTU_TYPE);
						report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_OBSERVE_TIME;
						report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_OBSERVE_TIME;
						report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_year % 100);
						report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mon + 1);
						report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mday);
						report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_hour);
						report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_min);
						//数据
						report_en = 0;
						//累计雨量
						if((report_data_ptr->data_len + SL651_BYTES_FRAME_RAIN + SL651_BYTES_FRAME_END) <= SL651_BYTES_REPORT_DATA)
						{
							if(RT_TRUE == sample_sensor_type_exist(SAMPLE_SENSOR_RAIN))
							{
								sample_get_cur_data(&data_type, SAMPLE_SENSOR_RAIN, RT_TRUE);
								report_data_ptr->data_len += _sl651_data_format_with_symbol(report_data_ptr->pdata + report_data_ptr->data_len, SAMPLE_SENSOR_RAIN, data_type.sta, data_type.value);
								report_en++;
							}
						}
						//瞬时水位
						if((report_data_ptr->data_len + SL651_BYTES_FRAME_WATER_CUR + SL651_BYTES_FRAME_END) <= SL651_BYTES_REPORT_DATA)
						{
							if(RT_TRUE == sample_sensor_type_exist(SAMPLE_SENSOR_SHUIWEI_1))
							{
								sample_get_cur_data(&data_type, SAMPLE_SENSOR_SHUIWEI_1, RT_TRUE);
								report_data_ptr->data_len += _sl651_data_format_with_symbol(report_data_ptr->pdata + report_data_ptr->data_len, SAMPLE_SENSOR_SHUIWEI_1, data_type.sta, data_type.value);
								report_en++;
							}
						}
						//信号强度
						if((report_data_ptr->data_len + SL651_BYTES_FRAME_CSQ_EX + SL651_BYTES_FRAME_END) <= SL651_BYTES_REPORT_DATA)
						{
							report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(SL651_BOOT_CSQ_EX >> 8);
							report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)SL651_BOOT_CSQ_EX;
							report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_CSQ_EX;
							report_data_ptr->data_len++;
							report_en++;
						}
						//错误码
						if((report_data_ptr->data_len + SL651_BYTES_FRAME_ERR_CODE_EX + SL651_BYTES_FRAME_END) <= SL651_BYTES_REPORT_DATA)
						{
							report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(SL651_BOOT_ERR_CODE_EX >> 8);
							report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)SL651_BOOT_ERR_CODE_EX;
							report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_ERR_CODE_EX;
							report_data_ptr->pdata[report_data_ptr->data_len++] = 0;
							report_data_ptr->pdata[report_data_ptr->data_len++] = 0;
							report_data_ptr->pdata[report_data_ptr->data_len++] = 0;
							report_en++;
						}
						//电池电压、充电电压
						if((report_data_ptr->data_len + SL651_BYTES_FRAME_VOLTAGE + SL651_BYTES_FRAME_VOLTAGE_EX + SL651_BYTES_FRAME_END) <= SL651_BYTES_REPORT_DATA)
						{
							pwrsys_info_ptr = (PWRM_PWRSYS_INFO *)mempool_req(sizeof(PWRM_PWRSYS_INFO), RT_WAITING_NO);
							if((PWRM_PWRSYS_INFO *)0 != pwrsys_info_ptr)
							{
								pwrm_get_pwrsys_info(pwrsys_info_ptr);

								report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_PWR_VOLTAGE;
								report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_PWR_VOLTAGE;
								crc_value = (pwrsys_info_ptr->batt_value > pwrsys_info_ptr->acdc_value) ? pwrsys_info_ptr->batt_value : pwrsys_info_ptr->acdc_value;
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(crc_value / 100 % 100);
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(crc_value % 100);
								report_en++;

								report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(SL651_BOOT_PWR_VOLTAGE_EX >> 8);
								report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)SL651_BOOT_PWR_VOLTAGE_EX;
								report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_PWR_VOLTAGE_EX;
								crc_value = (pwrsys_info_ptr->solar_value > pwrsys_info_ptr->acdc_value) ? pwrsys_info_ptr->solar_value : pwrsys_info_ptr->acdc_value;
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(crc_value / 100 % 100);
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(crc_value % 100);
								report_en++;

								rt_mp_free((void *)pwrsys_info_ptr);
							}
						}
						
						//帧尾
						if(report_en)
						{
							i = SL651_BYTES_FRAME_HEAD - SL651_BYTES_START_SYMBOL - SL651_BYTES_UP_DOWN_LEN;
							crc_value = report_data_ptr->data_len - SL651_BYTES_FRAME_HEAD;
							report_data_ptr->pdata[i]	= (rt_uint8_t)(crc_value >> 8);
							report_data_ptr->pdata[i++]	+= (rt_uint8_t)(SL651_FRAME_DIR_UP << 4);
							report_data_ptr->pdata[i]	= (rt_uint8_t)crc_value;
							//流水号
							i = SL651_BYTES_FRAME_HEAD;
							crc_value = _sl651_sn_value(RT_TRUE);
							report_data_ptr->pdata[i++]	= (rt_uint8_t)(crc_value >> 8);
							report_data_ptr->pdata[i]	= (rt_uint8_t)crc_value;
							//帧尾
							report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_FRAME_SYMBOL_ETX;
							report_data_ptr->data_len += SL651_BYTES_CRC_VALUE;
							
							report_data_ptr->fun_csq_update	= (void *)_sl651_csq_update;
							report_data_ptr->data_id		= crc_value;
							report_data_ptr->need_reply		= RT_TRUE;
							report_data_ptr->fcb_value		= SL651_FRAME_DEFAULT_FCB;
							report_data_ptr->ptcl_type		= PTCL_PTCL_TYPE_SL651;

							if(RT_TRUE == ptcl_auto_report_post(report_data_ptr))
							{
								report_data_ptr = (PTCL_REPORT_DATA *)0;
							}
						}

						break;
					}
					
					if((PTCL_REPORT_DATA *)0 != report_data_ptr)
					{
						rt_mp_free((void *)report_data_ptr);
					}
				}
			}
		}
	}
}

//随机采集任务
static void _sl651_task_random_sample(void *parg)
{
	PTCL_REPORT_DATA	*report_data_ptr;
	SAMPLE_DATA_TYPE	data_cur, data_ex;
	rt_uint8_t			water_level = 0, ctrl_num, report_type;
	struct tm			time;
	rt_uint16_t			i, crc_value;
	void				*pmem;
	
	while(1)
	{
		i = sl651_get_random_period();
		if(i)
		{
			rt_thread_delay(i * RT_TICK_PER_SECOND);
			i = RT_TRUE;
		}
		else
		{
			rt_thread_delay(RT_TICK_PER_SECOND);
			i = RT_FALSE;
		}

		if(RT_TRUE == i)
		{
			//水位1
			if(RT_TRUE == sample_sensor_type_exist(SAMPLE_SENSOR_SHUIWEI_1))
			{
				sample_get_cur_data(&data_cur, SAMPLE_SENSOR_SHUIWEI_1, RT_FALSE);
				if(RT_FALSE == data_cur.sta)
				{
					water_level = 0;
				}
				else
				{
					report_type = 0;
					ctrl_num = sample_get_ctrl_num_by_sensor_type(SAMPLE_SENSOR_SHUIWEI_1);
					//阈值
					sample_get_ex_data(ctrl_num, &data_ex);
					if(RT_TRUE == data_ex.sta)
					{
						if(data_cur.value > data_ex.value)
						{
							data_ex.value = data_cur.value - data_ex.value;
						}
						else
						{
							data_ex.value = data_ex.value - data_cur.value;
						}
						if(data_ex.value > sample_get_threshold_level(ctrl_num))
						{
							report_type++;
						}
					}
					//超限
					if(data_cur.value < sample_get_down(ctrl_num))
					{
						if(water_level)
						{
							report_type++;
							water_level = 0;
						}
					}
					else if(data_cur.value < sample_get_up(ctrl_num))
					{
						if(1 != water_level)
						{
							report_type++;
							water_level = 1;
						}
					}
					else
					{
						if(2 != water_level)
						{
							report_type++;
							water_level = 2;
						}
					}
					//阈值报或者超限报
					if(report_type)
					{
						sample_set_ex_data(ctrl_num, &data_cur);

						report_data_ptr = ptcl_report_data_req(SL651_BYTES_REPORT_DATA, RT_WAITING_FOREVER);
						if((PTCL_REPORT_DATA *)0 != report_data_ptr)
						{
							while(1)
							{
								report_data_ptr->data_len = 0;
								//帧头
								if((report_data_ptr->data_len + SL651_BYTES_FRAME_HEAD) > SL651_BYTES_REPORT_DATA)
								{
									break;
								}
								report_data_ptr->data_len += _sl651_bao_tou(report_data_ptr->pdata + report_data_ptr->data_len, 0, SL651_AFN_REPORT_ADD, SL651_FRAME_SYMBOL_STX);
								//流水号、发报时间
								if((report_data_ptr->data_len + SL651_BYTES_SN + SL651_BYTES_SEND_TIME) > SL651_BYTES_REPORT_DATA)
								{
									break;
								}
								report_data_ptr->data_len += (SL651_BYTES_SN + SL651_BYTES_SEND_TIME);
								//遥测站地址、分类码、观测时间
								if((report_data_ptr->data_len + SL651_BYTES_FRAME_RTU_ADDR + SL651_BYTES_RTU_TYPE + SL651_BYTES_FRAME_OBSERVE_TIME) > SL651_BYTES_REPORT_DATA)
								{
									break;
								}
								time = rtcee_rtc_get_calendar();
								report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_RTU_CODE;
								report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_RTU_CODE;
								_sl651_param_pend(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_RTU_TYPE);
								rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
								report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
								report_data_ptr->pdata[report_data_ptr->data_len++] = m_sl651_rtu_type;
								_sl651_param_post(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_RTU_TYPE);
								report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_OBSERVE_TIME;
								report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_OBSERVE_TIME;
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_year % 100);
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mon + 1);
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mday);
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_hour);
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_min);
								//数据
								report_type = 0;
								//瞬时水位
								if((report_data_ptr->data_len + SL651_BYTES_FRAME_WATER_CUR + SL651_BYTES_FRAME_END) <= SL651_BYTES_REPORT_DATA)
								{
									report_data_ptr->data_len += _sl651_data_format_with_symbol(report_data_ptr->pdata + report_data_ptr->data_len, SAMPLE_SENSOR_SHUIWEI_1, data_cur.sta, data_cur.value);
									report_type++;
								}
								//一级加报触发值
								if((report_data_ptr->data_len + SL651_BYTES_FRAME_WATER_CUR + SL651_BYTES_FRAME_END) <= SL651_BYTES_REPORT_DATA)
								{
									data_ex.value = sample_get_down(ctrl_num);
									report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(SL651_BOOT_WATER_LIMIT_1_EX >> 8);
									report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)SL651_BOOT_WATER_LIMIT_1_EX;
									if(data_ex.value < (float)0)
									{
										report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_WATER_CUR + 8;
									}
									else
									{
										report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_WATER_CUR;
									}
									report_data_ptr->data_len += _sl651_data_format_without_symbol(report_data_ptr->pdata + report_data_ptr->data_len, SAMPLE_SENSOR_SHUIWEI_1, RT_TRUE, data_ex.value);
								}
								//二级加报触发值
								if((report_data_ptr->data_len + SL651_BYTES_FRAME_WATER_CUR + SL651_BYTES_FRAME_END) <= SL651_BYTES_REPORT_DATA)
								{
									data_ex.value = sample_get_up(ctrl_num);
									report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(SL651_BOOT_WATER_LIMIT_2_EX >> 8);
									report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)SL651_BOOT_WATER_LIMIT_2_EX;
									if(data_ex.value < (float)0)
									{
										report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_WATER_CUR + 8;
									}
									else
									{
										report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_WATER_CUR;
									}
									report_data_ptr->data_len += _sl651_data_format_without_symbol(report_data_ptr->pdata + report_data_ptr->data_len, SAMPLE_SENSOR_SHUIWEI_1, RT_TRUE, data_ex.value);
								}
								//水位状态
								if((report_data_ptr->data_len + SL651_BYTES_FRAME_WATER_STA_EX + SL651_BYTES_FRAME_END) <= SL651_BYTES_REPORT_DATA)
								{
									report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(SL651_BOOT_WATER_STA_EX >> 8);
									report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)SL651_BOOT_WATER_STA_EX;
									report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_WATER_STA_EX;
									report_data_ptr->pdata[report_data_ptr->data_len++] = water_level;
								}
								
								//帧尾
								if(report_type)
								{
									i = SL651_BYTES_FRAME_HEAD - SL651_BYTES_START_SYMBOL - SL651_BYTES_UP_DOWN_LEN;
									crc_value = report_data_ptr->data_len - SL651_BYTES_FRAME_HEAD;
									report_data_ptr->pdata[i]	= (rt_uint8_t)(crc_value >> 8);
									report_data_ptr->pdata[i++]	+= (rt_uint8_t)(SL651_FRAME_DIR_UP << 4);
									report_data_ptr->pdata[i]	= (rt_uint8_t)crc_value;
									//流水号
									i = SL651_BYTES_FRAME_HEAD;
									crc_value = _sl651_sn_value(RT_TRUE);
									report_data_ptr->pdata[i++]	= (rt_uint8_t)(crc_value >> 8);
									report_data_ptr->pdata[i]	= (rt_uint8_t)crc_value;
									//帧尾
									report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_FRAME_SYMBOL_ETX;
									report_data_ptr->data_len += SL651_BYTES_CRC_VALUE;
									
									report_data_ptr->fun_csq_update	= (void *)_sl651_csq_update;
									report_data_ptr->data_id		= crc_value;
									report_data_ptr->need_reply		= RT_TRUE;
									report_data_ptr->fcb_value		= SL651_FRAME_DEFAULT_FCB;
									report_data_ptr->ptcl_type		= PTCL_PTCL_TYPE_SL651;

									ptcl_report_data_send(report_data_ptr, PTCL_COMM_TYPE_LORA, 0, 0, 0, (rt_uint16_t *)0);

									if(RT_TRUE == ptcl_auto_report_post(report_data_ptr))
									{
										report_data_ptr = (PTCL_REPORT_DATA *)0;
									}
								}

								break;
							}
							
							if((PTCL_REPORT_DATA *)0 != report_data_ptr)
							{
								rt_mp_free((void *)report_data_ptr);
							}
						}

						//报警
						pmem = mempool_req(SL651_BYTES_WARN_INFO_MAX, RT_WAITING_FOREVER);
						if((void *)0 != pmem)
						{
							crc_value = sl651_get_warn_info((rt_uint8_t *)pmem, water_level);
							if(crc_value)
							{
								wm_warn_tts_trigger(WM_WARN_TYPE_GPRS, SL651_DEF_WARN_TIMES, DTU_TTS_DATA_TYPE_GBK, (rt_uint8_t *)pmem, crc_value, RT_FALSE);
								bx5k_set_screen_txt((rt_uint8_t *)pmem, crc_value);
							}
							
							rt_mp_free(pmem);
						}

						bx5k_set_screen_data((rt_uint32_t)data_cur.value);
					}
				}
			}
		}
	}
}

static int _sl651_component_init(void)
{
	if(RT_EOK != rt_event_init(&m_sl651_event_module, "sl651", RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	if(RT_EOK != rt_event_send(&m_sl651_event_module, SL651_EVENT_PARAM_ALL))
	{
		while(1);
	}

	m_sl651_data_download_ptr = &m_sl651_data_download;
	if(RT_EOK != rt_mb_init(&m_sl651_mb_data_download, "sl651", (void *)&m_sl651_msgpool_data_download, 1, RT_IPC_FLAG_PRIO))
	{
		while(1);
	}

	_sl651_get_rtu_addr(m_sl651_rtu_addr);
	m_sl651_pw					= _sl651_get_pw();
	m_sl651_sn					= _sl651_get_sn();
	m_sl651_rtu_type			= _sl651_get_rtu_type();
	m_sl651_report_hour			= _sl651_get_report_hour();
	m_sl651_report_min			= _sl651_get_report_min();
	m_sl651_random_period		= _sl651_get_random_period();
	
	return 0;
}
INIT_COMPONENT_EXPORT(_sl651_component_init);

static int _sl651_app_init(void)
{
	if(RT_EOK != rt_thread_init(&m_sl651_thread_report_time, "sl651", _sl651_task_report_time, (void *)0, (void *)m_sl651_stk_report_time, SL651_STK_REPORT_TIME, SL651_PRIO_REPORT_TIME, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_sl651_thread_report_time))
	{
		while(1);
	}

	if(RT_EOK != rt_thread_init(&m_sl651_thread_random_sample, "sl651", _sl651_task_random_sample, (void *)0, (void *)m_sl651_stk_random_sample, SL651_STK_RANDOM_SAMPLE, SL651_PRIO_RANDOM_SAMPLE, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_sl651_thread_random_sample))
	{
		while(1);
	}

	if(RT_EOK != rt_thread_init(&m_sl651_thread_data_download, "sl651", _sl651_task_data_download, (void *)0, (void *)m_sl651_stk_data_download, SL651_STK_DATA_DOWNLOAD, SL651_PRIO_DATA_DOWNLOAD, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_sl651_thread_data_download))
	{
		while(1);
	}
	
	return 0;
}
INIT_APP_EXPORT(_sl651_app_init);



void sl651_get_rtu_addr(rt_uint8_t * rtu_addr_ptr)
{
	if((rt_uint8_t *)0 == rtu_addr_ptr)
	{
		return;
	}

	_sl651_param_pend(SL651_EVENT_PARAM_RTU_ADDR);
	rt_memcpy((void *)rtu_addr_ptr, (void *)m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
	_sl651_param_post(SL651_EVENT_PARAM_RTU_ADDR);
}
void sl651_set_rtu_addr(rt_uint8_t const * rtu_addr_ptr)
{
	if((rt_uint8_t *)0 == rtu_addr_ptr)
	{
		return;
	}

	_sl651_param_pend(SL651_EVENT_PARAM_RTU_ADDR);
	rt_memcpy((void *)m_sl651_rtu_addr, (void *)rtu_addr_ptr, SL651_BYTES_RTU_ADDR);
	_sl651_set_rtu_addr(rtu_addr_ptr);
	_sl651_param_post(SL651_EVENT_PARAM_RTU_ADDR);
}

rt_uint16_t sl651_get_pw(void)
{
	rt_uint16_t pw;

	_sl651_param_pend(SL651_EVENT_PARAM_PW);
	pw = m_sl651_pw;
	_sl651_param_post(SL651_EVENT_PARAM_PW);

	return pw;
}
void sl651_set_pw(rt_uint16_t pw)
{
	_sl651_param_pend(SL651_EVENT_PARAM_PW);
	if(pw != m_sl651_pw)
	{
		m_sl651_pw = pw;
		_sl651_set_pw(pw);
	}
	_sl651_param_post(SL651_EVENT_PARAM_PW);
}

rt_uint16_t sl651_get_sn(void)
{
	rt_uint16_t sn;

	_sl651_param_pend(SL651_EVENT_PARAM_SN);
	sn = m_sl651_sn;
	_sl651_param_post(SL651_EVENT_PARAM_SN);

	return sn;
}
void sl651_set_sn(rt_uint16_t sn)
{
	if(0 == sn)
	{
		return;
	}
	
	_sl651_param_pend(SL651_EVENT_PARAM_SN);
	if(sn != m_sl651_sn)
	{
		m_sl651_sn = sn;
		_sl651_set_sn(sn);
	}
	_sl651_param_post(SL651_EVENT_PARAM_SN);
}

rt_uint8_t sl651_get_rtu_type(void)
{
	rt_uint8_t rtu_type;

	_sl651_param_pend(SL651_EVENT_PARAM_RTU_TYPE);
	rtu_type = m_sl651_rtu_type;
	_sl651_param_post(SL651_EVENT_PARAM_RTU_TYPE);

	return rtu_type;
}
void sl651_set_rtu_type(rt_uint8_t rtu_type)
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

	_sl651_param_pend(SL651_EVENT_PARAM_RTU_TYPE);
	if(rtu_type != m_sl651_rtu_type)
	{
		m_sl651_rtu_type = rtu_type;
		_sl651_set_rtu_type(rtu_type);
	}
	_sl651_param_post(SL651_EVENT_PARAM_RTU_TYPE);
}

#if 0
void sl651_get_element(rt_uint8_t *pelement);
void sl651_set_element(rt_uint8_t const *pelement);

rt_uint8_t sl651_get_work_mode(void);
void sl651_set_work_mode(rt_uint8_t work_mode);

rt_uint8_t sl651_get_device_id(rt_uint8_t *pid);
void sl651_set_device_id(rt_uint8_t const *pid, rt_uint8_t id_len);
#endif

rt_uint8_t sl651_get_report_hour(void)
{
	rt_uint8_t period;

	_sl651_param_pend(SL651_EVENT_PARAM_REPORT_HOUR);
	period = m_sl651_report_hour;
	_sl651_param_post(SL651_EVENT_PARAM_REPORT_HOUR);

	return period;
}
void sl651_set_report_hour(rt_uint8_t period)
{
	if(period > 24)
	{
		return;
	}

	_sl651_param_pend(SL651_EVENT_PARAM_REPORT_HOUR);
	if(period != m_sl651_report_hour)
	{
		m_sl651_report_hour = period;
		_sl651_set_report_hour(period);
	}
	_sl651_param_post(SL651_EVENT_PARAM_REPORT_HOUR);
}

rt_uint8_t sl651_get_report_min(void)
{
	rt_uint8_t period;

	_sl651_param_pend(SL651_EVENT_PARAM_REPORT_MIN);
	period = m_sl651_report_min;
	_sl651_param_post(SL651_EVENT_PARAM_REPORT_MIN);

	return period;
}
void sl651_set_report_min(rt_uint8_t period)
{
	if(period >= 60)
	{
		return;
	}

	_sl651_param_pend(SL651_EVENT_PARAM_REPORT_MIN);
	if(period != m_sl651_report_min)
	{
		m_sl651_report_min = period;
		_sl651_set_report_min(period);
	}
	_sl651_param_post(SL651_EVENT_PARAM_REPORT_MIN);
}

rt_uint16_t sl651_get_random_period(void)
{
	rt_uint16_t period;

	_sl651_param_pend(SL651_EVENT_PARAM_RANDOM_PERIOD);
	period = m_sl651_random_period;
	_sl651_param_post(SL651_EVENT_PARAM_RANDOM_PERIOD);

	return period;
}
void sl651_set_random_period(rt_uint16_t period)
{
	_sl651_param_pend(SL651_EVENT_PARAM_RANDOM_PERIOD);
	if(period != m_sl651_random_period)
	{
		m_sl651_random_period = period;
		_sl651_set_random_period(period);
	}
	_sl651_param_post(SL651_EVENT_PARAM_RANDOM_PERIOD);
}

void sl651_set_center_comm(rt_uint8_t center, rt_uint8_t prio, rt_uint8_t const *pdata, rt_uint8_t data_len)
{
	rt_uint8_t	pos = 0;
	rt_uint16_t	port;
	void		*pmem;

	if(center >= PTCL_NUM_CENTER)
	{
		return;
	}
	if(prio >= PTCL_NUM_COMM_PRIO)
	{
		return;
	}
	if((rt_uint8_t *)0 == pdata)
	{
		return;
	}
	if(0 == data_len)
	{
		return;
	}

	switch(pdata[pos++])
	{
	case PTCL_COMM_TYPE_NONE:
		if(pos == data_len)
		{
			ptcl_set_comm_type(PTCL_COMM_TYPE_NONE, center, prio);
			_sl651_comm_update(center);
		}
		break;
	case PTCL_COMM_TYPE_IP:
		if(((pos + 9) == data_len) && (center < DTU_NUM_IP_CH))
		{
			if(RT_TRUE == fyyp_is_bcdcode(pdata + pos, data_len - pos))
			{
				pmem = mempool_req(sizeof(DTU_IP_ADDR), RT_WAITING_FOREVER);
				if((void *)0 != pmem)
				{
					port = fyyp_bcd_to_hex(pdata[pos++]);
					port *= 10;
					port += pdata[pos] >> 4;
					((DTU_IP_ADDR *)pmem)->addr_len = rt_sprintf((char *)(((DTU_IP_ADDR *)pmem)->addr_data), "%d.", port);
					
					port = pdata[pos++] & 0x0f;
					port *= 100;
					port += fyyp_bcd_to_hex(pdata[pos++]);
					((DTU_IP_ADDR *)pmem)->addr_len += rt_sprintf((char *)(((DTU_IP_ADDR *)pmem)->addr_data + ((DTU_IP_ADDR *)pmem)->addr_len), "%d.", port);
					
					port = fyyp_bcd_to_hex(pdata[pos++]);
					port *= 10;
					port += pdata[pos] >> 4;
					((DTU_IP_ADDR *)pmem)->addr_len += sprintf((char *)(((DTU_IP_ADDR *)pmem)->addr_data + ((DTU_IP_ADDR *)pmem)->addr_len), "%d.", port);
					
					port = pdata[pos++] & 0x0f;
					port *= 100;
					port += fyyp_bcd_to_hex(pdata[pos++]);
					((DTU_IP_ADDR *)pmem)->addr_len += sprintf((char *)(((DTU_IP_ADDR *)pmem)->addr_data + ((DTU_IP_ADDR *)pmem)->addr_len), "%d:", port);
					
					port = fyyp_bcd_to_hex(pdata[pos++]);
					port *= 100;
					port += fyyp_bcd_to_hex(pdata[pos++]);
					port *= 100;
					port += fyyp_bcd_to_hex(pdata[pos]);
					((DTU_IP_ADDR *)pmem)->addr_len += sprintf((char *)(((DTU_IP_ADDR *)pmem)->addr_data + ((DTU_IP_ADDR *)pmem)->addr_len), "%d", port);
					
					ptcl_set_comm_type(PTCL_COMM_TYPE_IP, center, prio);
					dtu_set_ip_addr((DTU_IP_ADDR *)pmem, center);
					_sl651_comm_update(center);
					rt_mp_free(pmem);
				}
			}
		}
		break;
	case PTCL_COMM_TYPE_SMS:
		port = data_len - pos;
		port *= 2;
		if((port < DTU_BYTES_SMS_ADDR) && (center < DTU_NUM_SMS_CH))
		{
			pmem = mempool_req(sizeof(DTU_SMS_ADDR), RT_WAITING_FOREVER);
			if((void *)0 != pmem)
			{
				((DTU_SMS_ADDR *)pmem)->addr_len = 0;
				for(; pos < data_len; pos++)
				{
					port = pdata[pos] >> 4;
					if(port < 10)
					{
						((DTU_SMS_ADDR *)pmem)->addr_data[((DTU_SMS_ADDR *)pmem)->addr_len++] = port + '0';
					}
					port = pdata[pos] & 0x0f;
					if(port < 10)
					{
						((DTU_SMS_ADDR *)pmem)->addr_data[((DTU_SMS_ADDR *)pmem)->addr_len++] = port + '0';
					}
				}
				
				ptcl_set_comm_type(PTCL_COMM_TYPE_SMS, center, prio);
				dtu_set_sms_addr((DTU_SMS_ADDR *)pmem, center);
				_sl651_comm_update(center);
				rt_mp_free(pmem);
			}
		}
		break;
	}
}

rt_uint16_t sl651_get_warn_info(rt_uint8_t *pdata, rt_uint8_t warn_level)
{
	void		*pmem;
	rt_uint16_t	data_len;
	
	if((rt_uint8_t *)0 == pdata)
	{
		return 0;
	}
	if(warn_level >= SL651_NUM_WARN_INFO)
	{
		return 0;
	}

	pmem = mempool_req(sizeof(SL651_WARN_INFO), RT_WAITING_NO);
	if((void *)0 == pmem)
	{
		return 0;
	}
	_sl651_get_warn_info((SL651_WARN_INFO *)pmem, warn_level);
	if(SL651_WARN_INFO_VALID == ((SL651_WARN_INFO *)pmem)->valid)
	{
		data_len = ((SL651_WARN_INFO *)pmem)->data_len;
		rt_memcpy((void *)pdata, (void *)(((SL651_WARN_INFO *)pmem)->data), data_len);
	}
	else
	{
		data_len = 0;
	}
	rt_mp_free(pmem);

	return data_len;
}
void sl651_set_warn_info(rt_uint8_t const *pdata, rt_uint16_t data_len, rt_uint8_t warn_level)
{
	void *pmem;
	
	if((rt_uint8_t *)0 == pdata)
	{
		return;
	}
	if((0 == data_len) || (data_len > SL651_BYTES_WARN_INFO_MAX))
	{
		return;
	}
	if(warn_level >= SL651_NUM_WARN_INFO)
	{
		return;
	}

	pmem = mempool_req(sizeof(SL651_WARN_INFO), RT_WAITING_NO);
	if((void *)0 == pmem)
	{
		return;
	}
	((SL651_WARN_INFO *)pmem)->valid	= SL651_WARN_INFO_VALID;
	((SL651_WARN_INFO *)pmem)->data_len	= data_len;
	rt_memcpy((void *)(((SL651_WARN_INFO *)pmem)->data), (void *)pdata, data_len);
	_sl651_set_warn_info((SL651_WARN_INFO *)pmem, warn_level);
	rt_mp_free(pmem);
}

//参数重置
void sl651_param_restore(void)
{
	_sl651_param_pend(SL651_EVENT_PARAM_ALL);

	//rtu地址
	rt_memset((void *)m_sl651_rtu_addr, 0x88, SL651_BYTES_RTU_ADDR);
	_sl651_set_rtu_addr(m_sl651_rtu_addr);
	
	//密码
	m_sl651_pw = SL651_FRAME_DEFAULT_PW;
	_sl651_set_pw(m_sl651_pw);
	
	//RTU流水号
	m_sl651_sn = 1;
	_sl651_set_sn(m_sl651_sn);
	
	//分类码
	m_sl651_rtu_type = SL651_RTU_TYPE_SHUIKU;
	_sl651_set_rtu_type(m_sl651_rtu_type);
	
	//定时报间隔
	m_sl651_report_hour = 1;
	_sl651_set_report_hour(m_sl651_report_hour);
	
	//加报间隔
	m_sl651_report_min = 0;
	_sl651_set_report_min(m_sl651_report_min);
	
	//随机采集间隔
	m_sl651_random_period = 20;
	_sl651_set_random_period(m_sl651_random_period);

	_sl651_param_post(SL651_EVENT_PARAM_ALL);

	//告警信息
	sl651_set_warn_info("预警解除", rt_strlen("预警解除"), 0);
	sl651_set_warn_info("一级预警准备撤离", rt_strlen("一级预警准备撤离"), 1);
	sl651_set_warn_info("二级预警立即撤离", rt_strlen("二级预警立即撤离"), 2);
	sl651_set_warn_info("一键预警马上撤离", rt_strlen("一键预警马上撤离"), 3);
}

//数据合法性检测函数
rt_uint8_t sl651_data_valid(rt_uint8_t const *pdata, rt_uint16_t data_len, rt_uint16_t data_id, rt_uint8_t need_reply, rt_uint8_t fcb_value)
{
	rt_uint16_t i = 0, j;
	
	if((rt_uint8_t *)0 == pdata)
	{
		return RT_FALSE;
	}
	if(0 == data_len)
	{
		return RT_FALSE;
	}
	if(0 == data_id)
	{
		return RT_FALSE;
	}
	if((RT_FALSE != need_reply) && (RT_TRUE != need_reply))
	{
		return RT_FALSE;
	}
	if(SL651_FRAME_DEFAULT_FCB != fcb_value)
	{
		return RT_FALSE;
	}
	
	//帧头
	if((i + SL651_BYTES_FRAME_HEAD) > data_len)
	{
		return RT_FALSE;
	}
	//7E7E
	if(SL651_FRAME_SYMBOL != pdata[i++])
	{
		return RT_FALSE;
	}
	if(SL651_FRAME_SYMBOL != pdata[i++])
	{
		return RT_FALSE;
	}
	//中心站地址
	i += SL651_BYTES_CENTER_ADDR;
	//RTU地址
	_sl651_param_pend(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_PW);
	for(j = 0; j < SL651_BYTES_RTU_ADDR; j++)
	{
		if(m_sl651_rtu_addr[j] != pdata[i++])
		{
			_sl651_param_post(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_PW);
			return RT_FALSE;
		}
	}
	//密码
	j = pdata[i++];
	j <<= 8;
	j += pdata[i++];
	if(m_sl651_pw != j)
	{
		_sl651_param_post(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_PW);
		return RT_FALSE;
	}
	_sl651_param_post(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_PW);
	//功能码
	i += SL651_BYTES_AFN;
	//上下行标识及长度
	if(SL651_FRAME_DIR_UP != (pdata[i] >> 4))
	{
		return RT_FALSE;
	}
	j = (pdata[i++] & 0x0f);
	j <<= 8;
	j += (pdata[i++] + SL651_BYTES_FRAME_HEAD + SL651_BYTES_FRAME_END);
	if(data_len != j)
	{
		return RT_FALSE;
	}
	//起始符
	if(SL651_FRAME_SYMBOL_STX != pdata[i++])
	{
		return RT_FALSE;
	}
	//流水号
	if((i + SL651_BYTES_SN) > data_len)
	{
		return RT_FALSE;
	}
	j = pdata[i++];
	j <<= 8;
	j += pdata[i++];
	if(data_id != j)
	{
		return RT_FALSE;
	}
	//结束符
	if(data_len < SL651_BYTES_FRAME_END)
	{
		return RT_FALSE;
	}
	if(SL651_FRAME_SYMBOL_ETX != pdata[data_len - SL651_BYTES_FRAME_END])
	{
		return RT_FALSE;
	}
	
	return RT_TRUE;
}

//数据更新函数
rt_uint8_t sl651_data_update(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t center_addr, struct tm time)
{
	rt_uint16_t i = 0;
	
	if((rt_uint8_t *)0 == pdata)
	{
		return RT_FALSE;
	}

	if((i + SL651_BYTES_FRAME_HEAD + SL651_BYTES_SN + SL651_BYTES_SEND_TIME + SL651_BYTES_FRAME_END) > data_len)
	{
		return RT_FALSE;
	}
	if(SL651_FRAME_SYMBOL != pdata[i++])
	{
		return RT_FALSE;
	}
	if(SL651_FRAME_SYMBOL != pdata[i++])
	{
		return RT_FALSE;
	}

	//多包不更新
	if(SL651_FRAME_SYMBOL_SYN == pdata[SL651_BYTES_FRAME_HEAD - 1])
	{
		return RT_TRUE;
	}
	//中心站地址
	if(center_addr)
	{
		pdata[i] = center_addr;
	}
	//发报时间
	i = SL651_BYTES_FRAME_HEAD + SL651_BYTES_SN;
	pdata[i++]	= fyyp_hex_to_bcd(time.tm_year % 100);
	pdata[i++]	= fyyp_hex_to_bcd(time.tm_mon + 1);
	pdata[i++]	= fyyp_hex_to_bcd(time.tm_mday);
	pdata[i++]	= fyyp_hex_to_bcd(time.tm_hour);
	pdata[i++]	= fyyp_hex_to_bcd(time.tm_min);
	pdata[i]	= fyyp_hex_to_bcd(time.tm_sec);
	//CRC校验码
	data_len -= SL651_BYTES_CRC_VALUE;
	i = _sl651_crc_value(pdata, data_len);
	pdata[data_len++]	= (rt_uint8_t)(i >> 8);
	pdata[data_len]		= (rt_uint8_t)i;

	return RT_TRUE;
}

//心跳包编码函数
rt_uint16_t sl651_heart_encoder(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t ch)
{
	rt_uint16_t	i = 0;
	struct tm	time;
	
	if((rt_uint8_t *)0 == pdata)
	{
		return 0;
	}
	if((SL651_BYTES_FRAME_HEAD + SL651_BYTES_SN + SL651_BYTES_SEND_TIME + SL651_BYTES_FRAME_END) > data_len)
	{
		return 0;
	}
	
	//帧头
	i += _sl651_bao_tou(pdata, ptcl_get_center_addr(ch), SL651_AFN_REPORT_HEART, SL651_FRAME_SYMBOL_STX);
	//流水号
	data_len = _sl651_sn_value(RT_FALSE);
	pdata[i++] = (rt_uint8_t)(data_len >> 8);
	pdata[i++] = (rt_uint8_t)data_len;
	//发报时间
	time = rtcee_rtc_get_calendar();
	pdata[i++] = fyyp_hex_to_bcd(time.tm_year % 100);
	pdata[i++] = fyyp_hex_to_bcd(time.tm_mon + 1);
	pdata[i++] = fyyp_hex_to_bcd(time.tm_mday);
	pdata[i++] = fyyp_hex_to_bcd(time.tm_hour);
	pdata[i++] = fyyp_hex_to_bcd(time.tm_min);
	pdata[i++] = fyyp_hex_to_bcd(time.tm_sec);
	//上下行标识及正文长度
	data_len = SL651_BYTES_FRAME_HEAD - SL651_BYTES_START_SYMBOL - SL651_BYTES_UP_DOWN_LEN;
	pdata[data_len]		= (rt_uint8_t)((i - SL651_BYTES_FRAME_HEAD) >> 8);
	pdata[data_len++]	+= (SL651_FRAME_DIR_UP << 4);
	pdata[data_len]		= (rt_uint8_t)(i - SL651_BYTES_FRAME_HEAD);
	//帧尾
	pdata[i++] = SL651_FRAME_SYMBOL_ETX;
	data_len = _sl651_crc_value(pdata, i);
	pdata[i++] = (rt_uint8_t)(data_len >> 8);
	pdata[i++] = (rt_uint8_t)data_len;
	
	return i;
}

//数据解包
rt_uint8_t sl651_data_decoder(PTCL_REPORT_DATA *report_data_ptr, PTCL_RECV_DATA const *recv_data_ptr, PTCL_PARAM_INFO **param_info_ptr_ptr)
{
	rt_uint16_t				i, crc_value, packet_total = 0, packet_cur = 0;
	rt_uint8_t				center_addr, afn, pos, center, prio;
	struct tm				time;
	void					*pmem;
	rt_base_t				level;
	
	//数据分析解包
	//crc
	if(recv_data_ptr->data_len > SL651_BYTES_CRC_VALUE)
	{
		i = recv_data_ptr->data_len - SL651_BYTES_CRC_VALUE;
		crc_value = _sl651_crc_value(recv_data_ptr->pdata, i);
		if(recv_data_ptr->pdata[i++] != (rt_uint8_t)(crc_value >> 8))
		{
			return RT_FALSE;
		}
		if(recv_data_ptr->pdata[i] != (rt_uint8_t)crc_value)
		{
			return RT_FALSE;
		}
	}
	else
	{
		return RT_FALSE;
	}
	//帧头
	i = 0;
	if((i + SL651_BYTES_FRAME_HEAD) > recv_data_ptr->data_len)
	{
		return RT_FALSE;
	}
	//7E7E
	if(SL651_FRAME_SYMBOL != recv_data_ptr->pdata[i++])
	{
		return RT_FALSE;
	}
	if(SL651_FRAME_SYMBOL != recv_data_ptr->pdata[i++])
	{
		return RT_FALSE;
	}
	//RTU地址
	_sl651_param_pend(SL651_EVENT_PARAM_RTU_ADDR);
	for(crc_value = 0; crc_value < SL651_BYTES_RTU_ADDR; crc_value++)
	{
		if(m_sl651_rtu_addr[crc_value] != recv_data_ptr->pdata[i + crc_value])
		{
			break;
		}
	}
	_sl651_param_post(SL651_EVENT_PARAM_RTU_ADDR);
	if(crc_value < SL651_BYTES_RTU_ADDR)
	{
		i += (SL651_BYTES_RTU_ADDR - 2);
		if(0xff != recv_data_ptr->pdata[i++])
		{
			return RT_FALSE;
		}
		if(0xff != recv_data_ptr->pdata[i++])
		{
			return RT_FALSE;
		}
	}
	else
	{
		i += SL651_BYTES_RTU_ADDR;
	}
	//中心站地址
	center_addr = recv_data_ptr->pdata[i++];
	//密码
	crc_value = recv_data_ptr->pdata[i++];
	crc_value <<= 8;
	crc_value += recv_data_ptr->pdata[i++];
	_sl651_param_pend(SL651_EVENT_PARAM_PW);
	if((m_sl651_pw != crc_value) && (SL651_FRAME_DEFAULT_PW != crc_value))
	{
		_sl651_param_post(SL651_EVENT_PARAM_PW);
		return RT_FALSE;
	}
	_sl651_param_post(SL651_EVENT_PARAM_PW);
	//功能码
	afn = recv_data_ptr->pdata[i++];
	//上下行标识及长度
	if(SL651_FRAME_DIR_DOWN != (recv_data_ptr->pdata[i] >> 4))
	{
		return RT_FALSE;
	}
	crc_value = recv_data_ptr->pdata[i++] & 0x0f;
	crc_value <<= 8;
	crc_value += (recv_data_ptr->pdata[i++] + SL651_BYTES_FRAME_HEAD + SL651_BYTES_FRAME_END);
	if(recv_data_ptr->data_len != crc_value)
	{
		return RT_FALSE;
	}
	//起始符
	if((SL651_FRAME_SYMBOL_STX != recv_data_ptr->pdata[i]) && (SL651_FRAME_SYMBOL_SYN != recv_data_ptr->pdata[i]))
	{
		return RT_FALSE;
	}
	if(SL651_FRAME_SYMBOL_SYN == recv_data_ptr->pdata[i++])
	{
		if((i + SL651_BYTES_FRAME_PACKET_INFO) > recv_data_ptr->data_len)
		{
			return RT_FALSE;
		}
		packet_total = recv_data_ptr->pdata[i++];
		packet_total <<= 4;
		packet_total += (recv_data_ptr->pdata[i] >> 4);
		packet_cur = recv_data_ptr->pdata[i++] & 0x0f;
		packet_cur <<= 8;
		packet_cur += recv_data_ptr->pdata[i++];
	}
	//流水号
	if((i + SL651_BYTES_SN) > recv_data_ptr->data_len)
	{
		return RT_FALSE;
	}
	crc_value = recv_data_ptr->pdata[i++];
	crc_value <<= 8;
	crc_value += recv_data_ptr->pdata[i++];
	//多包结构时将包总数作为数据ID
	if(packet_total)
	{
		crc_value = packet_total;
	}
	//发报时间
	if((i + SL651_BYTES_SEND_TIME) > recv_data_ptr->data_len)
	{
		return RT_FALSE;
	}
	time.tm_year	= fyyp_bcd_to_hex(recv_data_ptr->pdata[i++]) + 2000;
	time.tm_mon		= fyyp_bcd_to_hex(recv_data_ptr->pdata[i++]) - 1;
	time.tm_mday	= fyyp_bcd_to_hex(recv_data_ptr->pdata[i++]);
	time.tm_hour	= fyyp_bcd_to_hex(recv_data_ptr->pdata[i++]);
	time.tm_min		= fyyp_bcd_to_hex(recv_data_ptr->pdata[i++]);
	time.tm_sec		= fyyp_bcd_to_hex(recv_data_ptr->pdata[i++]);
	
	//回执
	if(crc_value)
	{
		if(recv_data_ptr->data_len != (i + SL651_BYTES_FRAME_END))
		{
			return RT_FALSE;
		}
		//判断结束符
		if(SL651_FRAME_SYMBOL_EOT == recv_data_ptr->pdata[i])
		{
			if(PTCL_COMM_TYPE_IP == recv_data_ptr->comm_type)
			{
				dtu_hold_enable(DTU_COMM_TYPE_IP, recv_data_ptr->ch, RT_FALSE);
			}
		}
		else if(SL651_FRAME_SYMBOL_ESC == recv_data_ptr->pdata[i])
		{
			if(PTCL_COMM_TYPE_IP == recv_data_ptr->comm_type)
			{
				dtu_hold_enable(DTU_COMM_TYPE_IP, recv_data_ptr->ch, RT_TRUE);
			}
		}
		//发出回执
		if(SL651_FRAME_SYMBOL_NAK == recv_data_ptr->pdata[i])
		{
			ptcl_reply_post(recv_data_ptr->comm_type, recv_data_ptr->ch, crc_value, packet_cur);
		}
		else
		{
			ptcl_reply_post(recv_data_ptr->comm_type, recv_data_ptr->ch, crc_value, 0);
		}

		report_data_ptr->data_len = 0;
		return RT_TRUE;
	}

	//组应答包
	report_data_ptr->data_len = 0;
	if((report_data_ptr->data_len + SL651_BYTES_FRAME_HEAD + SL651_BYTES_SN + SL651_BYTES_SEND_TIME) >= PTCL_BYTES_ACK_REPORT)
	{
		return RT_FALSE;
	}
	//帧头
	report_data_ptr->data_len += _sl651_bao_tou(report_data_ptr->pdata, center_addr, afn, SL651_FRAME_SYMBOL_STX);
	//流水号、发报时间
	report_data_ptr->data_len += (SL651_BYTES_SN + SL651_BYTES_SEND_TIME);

	//解码
	switch(afn)
	{
	default:
		return RT_FALSE;
	//查询图片数据
	case SL651_AFN_QUERY_PHOTO:
		if((i + SL651_BYTES_FRAME_END) != recv_data_ptr->data_len)
		{
			return RT_FALSE;
		}
//		cam_photo_shoot(recv_data_ptr->comm_type, recv_data_ptr->ch, _sl651_multi_encoder, center_addr);
		report_data_ptr->data_len = 0;
		return RT_TRUE;
	//查询遥测站实时数据
	case SL651_AFN_QUERY_CUR_DATA:
		if((i + SL651_BYTES_FRAME_END) != recv_data_ptr->data_len)
		{
			return RT_FALSE;
		}
		//长度验证(遥测站地址、测站分类码、观测时间)
		if((report_data_ptr->data_len + SL651_BYTES_FRAME_RTU_ADDR + SL651_BYTES_RTU_TYPE + SL651_BYTES_FRAME_OBSERVE_TIME) >= PTCL_BYTES_ACK_REPORT)
		{
			return RT_FALSE;
		}
		//遥测站地址、测站分类码
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_RTU_CODE;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_RTU_CODE;
		_sl651_param_pend(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_RTU_TYPE);
		rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
		report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
		report_data_ptr->pdata[report_data_ptr->data_len++] = m_sl651_rtu_type;
		_sl651_param_post(SL651_EVENT_PARAM_RTU_ADDR + SL651_EVENT_PARAM_RTU_TYPE);
		//观测时间
		time = rtcee_rtc_get_calendar();
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_OBSERVE_TIME;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_OBSERVE_TIME;
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_year % 100);
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mon + 1);
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mday);
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_hour);
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_min);
		
		//要素数量
		i = 0;
		//累计雨量
		if((report_data_ptr->data_len + SL651_BYTES_FRAME_RAIN + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
		{
			if(RT_TRUE == sample_sensor_type_exist(SAMPLE_SENSOR_RAIN))
			{
				pmem = mempool_req(sizeof(SAMPLE_DATA_TYPE), RT_WAITING_NO);
				if((void *)0 != pmem)
				{
					sample_get_cur_data((SAMPLE_DATA_TYPE *)pmem, SAMPLE_SENSOR_RAIN, RT_FALSE);
					report_data_ptr->data_len += _sl651_data_format_with_symbol(report_data_ptr->pdata + report_data_ptr->data_len, SAMPLE_SENSOR_RAIN, ((SAMPLE_DATA_TYPE *)pmem)->sta, ((SAMPLE_DATA_TYPE *)pmem)->value);
					i++;
					rt_mp_free(pmem);
				}
			}
		}
		//瞬时水位
		if((report_data_ptr->data_len + SL651_BYTES_FRAME_WATER_CUR + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
		{
			if(RT_TRUE == sample_sensor_type_exist(SAMPLE_SENSOR_SHUIWEI_1))
			{
				pmem = mempool_req(sizeof(SAMPLE_DATA_TYPE), RT_WAITING_NO);
				if((void *)0 != pmem)
				{
					sample_get_cur_data((SAMPLE_DATA_TYPE *)pmem, SAMPLE_SENSOR_SHUIWEI_1, RT_FALSE);
					report_data_ptr->data_len += _sl651_data_format_with_symbol(report_data_ptr->pdata + report_data_ptr->data_len, SAMPLE_SENSOR_SHUIWEI_1, ((SAMPLE_DATA_TYPE *)pmem)->sta, ((SAMPLE_DATA_TYPE *)pmem)->value);
					i++;
					rt_mp_free(pmem);
				}
			}
		}
		//信号强度
		if((report_data_ptr->data_len + SL651_BYTES_FRAME_CSQ_EX + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
		{
			report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(SL651_BOOT_CSQ_EX >> 8);
			report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)SL651_BOOT_CSQ_EX;
			report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_CSQ_EX;
			report_data_ptr->pdata[report_data_ptr->data_len++] = 0;
			i++;
		}
		//错误码
		if((report_data_ptr->data_len + SL651_BYTES_FRAME_ERR_CODE_EX + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
		{
			report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(SL651_BOOT_ERR_CODE_EX >> 8);
			report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)SL651_BOOT_ERR_CODE_EX;
			report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_ERR_CODE_EX;
			report_data_ptr->pdata[report_data_ptr->data_len++] = 0;
			report_data_ptr->pdata[report_data_ptr->data_len++] = 0;
			report_data_ptr->pdata[report_data_ptr->data_len++] = 0;
			i++;
		}
		//电池电压、充电电压
		if((report_data_ptr->data_len + SL651_BYTES_FRAME_VOLTAGE + SL651_BYTES_FRAME_VOLTAGE_EX + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
		{
			pmem = mempool_req(sizeof(PWRM_PWRSYS_INFO), RT_WAITING_NO);
			if((void *)0 != pmem)
			{
				pwrm_get_pwrsys_info((PWRM_PWRSYS_INFO *)pmem);

				report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_PWR_VOLTAGE;
				report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_PWR_VOLTAGE;
				crc_value = (((PWRM_PWRSYS_INFO *)pmem)->batt_value > ((PWRM_PWRSYS_INFO *)pmem)->acdc_value) ? ((PWRM_PWRSYS_INFO *)pmem)->batt_value : ((PWRM_PWRSYS_INFO *)pmem)->acdc_value;
				report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(crc_value / 100 % 100);
				report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(crc_value % 100);
				i++;

				report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(SL651_BOOT_PWR_VOLTAGE_EX >> 8);
				report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)SL651_BOOT_PWR_VOLTAGE_EX;
				report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_PWR_VOLTAGE_EX;
				crc_value = (((PWRM_PWRSYS_INFO *)pmem)->solar_value > ((PWRM_PWRSYS_INFO *)pmem)->acdc_value) ? ((PWRM_PWRSYS_INFO *)pmem)->solar_value : ((PWRM_PWRSYS_INFO *)pmem)->acdc_value;
				report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(crc_value / 100 % 100);
				report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(crc_value % 100);
				i++;

				rt_mp_free(pmem);
			}
		}
		if(0 == i)
		{
			return RT_FALSE;
		}
		break;
	//查询时段数据
	case SL651_AFN_QUERY_OLD_DATA:
		//取出下载工具
		level = rt_hw_interrupt_disable();
		pmem = (void *)m_sl651_data_download_ptr;
		m_sl651_data_download_ptr = (SL651_DATA_DOWNLOAD *)0;
		rt_hw_interrupt_enable(level);
		if((void *)0 == pmem)
		{
			return RT_FALSE;
		}
		((SL651_DATA_DOWNLOAD *)pmem)->comm_type	= recv_data_ptr->comm_type;
		((SL651_DATA_DOWNLOAD *)pmem)->ch			= recv_data_ptr->ch;
		((SL651_DATA_DOWNLOAD *)pmem)->center_addr	= center_addr;
		
		while(1)
		{
			//时间范围
			if((i + SL651_BYTES_FRAME_TIME_RANGE) > recv_data_ptr->data_len)
			{
				break;
			}
			time.tm_year	= fyyp_bcd_to_hex(recv_data_ptr->pdata[i++]) + 2000;
			time.tm_mon		= fyyp_bcd_to_hex(recv_data_ptr->pdata[i++]) - 1;
			time.tm_mday	= fyyp_bcd_to_hex(recv_data_ptr->pdata[i++]);
			time.tm_hour	= fyyp_bcd_to_hex(recv_data_ptr->pdata[i++]);
			time.tm_min		= 0;
			time.tm_sec		= 0;
			((SL651_DATA_DOWNLOAD *)pmem)->unix_low = rtcee_rtc_calendar_to_unix(time);
			time.tm_year	= fyyp_bcd_to_hex(recv_data_ptr->pdata[i++]) + 2000;
			time.tm_mon		= fyyp_bcd_to_hex(recv_data_ptr->pdata[i++]) - 1;
			time.tm_mday	= fyyp_bcd_to_hex(recv_data_ptr->pdata[i++]);
			time.tm_hour	= fyyp_bcd_to_hex(recv_data_ptr->pdata[i++]);
			time.tm_min		= 0;
			time.tm_sec		= 0;
			((SL651_DATA_DOWNLOAD *)pmem)->unix_high = rtcee_rtc_calendar_to_unix(time);
			if(((SL651_DATA_DOWNLOAD *)pmem)->unix_low > ((SL651_DATA_DOWNLOAD *)pmem)->unix_high)
			{
				break;
			}
			//时间步长
			if((i + SL651_BYTES_FRAME_TIME_STEP) > recv_data_ptr->data_len)
			{
				break;
			}
			if(SL651_BOOT_TIME_STEP != recv_data_ptr->pdata[i++])
			{
				break;
			}
			if(SL651_LEN_TIME_STEP != recv_data_ptr->pdata[i++])
			{
				break;
			}
			((SL651_DATA_DOWNLOAD *)pmem)->unix_step = fyyp_bcd_to_hex(recv_data_ptr->pdata[i++]);
			((SL651_DATA_DOWNLOAD *)pmem)->unix_step *= 24;
			((SL651_DATA_DOWNLOAD *)pmem)->unix_step *= 60;
			((SL651_DATA_DOWNLOAD *)pmem)->unix_step *= 60;
			crc_value = fyyp_bcd_to_hex(recv_data_ptr->pdata[i++]);
			crc_value *= 60;
			crc_value *= 60;
			((SL651_DATA_DOWNLOAD *)pmem)->unix_step += crc_value;
			crc_value = fyyp_bcd_to_hex(recv_data_ptr->pdata[i++]);
			crc_value *= 60;
			((SL651_DATA_DOWNLOAD *)pmem)->unix_step += crc_value;
			if(0 == ((SL651_DATA_DOWNLOAD *)pmem)->unix_step)
			{
				break;
			}
			//要素标识符
			if((i + SL651_BYTES_FRAME_ELEMENT_SYMBOL) > recv_data_ptr->data_len)
			{
				break;
			}
			((SL651_DATA_DOWNLOAD *)pmem)->element_boot = recv_data_ptr->pdata[i++];
			if(SL651_FRAME_SYMBOL_ELEMENT_EX == ((SL651_DATA_DOWNLOAD *)pmem)->element_boot)
			{
				((SL651_DATA_DOWNLOAD *)pmem)->element_boot <<= 8;
				((SL651_DATA_DOWNLOAD *)pmem)->element_boot += recv_data_ptr->pdata[i++];
			}
			((SL651_DATA_DOWNLOAD *)pmem)->element_len = recv_data_ptr->pdata[i++];
			//验证长度
			if((i + SL651_BYTES_FRAME_END) == recv_data_ptr->data_len)
			{
				if(RT_EOK == rt_mb_send_wait(&m_sl651_mb_data_download, (rt_ubase_t)pmem, RT_WAITING_NO))
				{
					report_data_ptr->data_len = 0;
					return RT_TRUE;
				}
			}
			
			break;
		}

		level = rt_hw_interrupt_disable();
		m_sl651_data_download_ptr = (SL651_DATA_DOWNLOAD *)pmem;
		rt_hw_interrupt_enable(level);
		return RT_FALSE;
#if 0
	//查询人工置数
	case SL651_AFN_QUERY_MANUAL_DATA:
		//长度验证
		if((i + SL651_BYTES_FRAME_END) != recv_data_ptr->data_len)
		{
			return RT_FALSE;
		}
		//取出人工置数工具
		pManualData = (SL651_MANUAL_DATA *)0;
		level = rt_hw_interrupt_disable();
		if((SL651_MANUAL_DATA *)0 != m_sl651ManualDataP)
		{
			pManualData = m_sl651ManualDataP;
			m_sl651ManualDataP = (SL651_MANUAL_DATA *)0;
		}
		rt_hw_interrupt_enable(level);
		//无人工置数工具
		if((SL651_MANUAL_DATA *)0 == pManualData)
		{
			return RT_FALSE;
		}
		//长度验证
		if((report_data_ptr->data_len + SL651_BYTES_FRAME_ELEMENT_SYMBOL + pManualData->data_len + SL651_BYTES_FRAME_END) > PTCL_BYTES_ACK_REPORT)
		{
			level = rt_hw_interrupt_disable();
			m_sl651ManualDataP = pManualData;
			rt_hw_interrupt_enable(level);
			return RT_FALSE;
		}
		//人工置数数据
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_MANUAL_DATA;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_MANUAL_DATA;
		rt_memcpy(report_data_ptr->pdata + report_data_ptr->data_len, pManualData->data, pManualData->data_len);
		report_data_ptr->data_len += pManualData->data_len;
		//归还人工置数工具
		level = rt_hw_interrupt_disable();
		m_sl651ManualDataP = pManualData;
		rt_hw_interrupt_enable(level);
		break;
	//查询指定要素数据
	case SL651_AFN_QUERY_ELEMENT_DATA:
		//下行帧长度验证
		if((i + SL651_BYTES_FRAME_END) >= recv_data_ptr->data_len)
		{
			return RT_FALSE;
		}
		//上行帧长度验证(遥测站地址、测站分类码、观测时间)
		if((report_data_ptr->data_len + SL651_BYTES_FRAME_RTU_ADDR + SL651_BYTES_RTU_TYPE + SL651_BYTES_FRAME_OBSERVE_TIME) >= PTCL_BYTES_ACK_REPORT)
		{
			return RT_FALSE;
		}
		//遥测站地址、测站分类码
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_RTU_CODE;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_RTU_CODE;
		_sl651_param_pend();
		rt_memcpy(report_data_ptr->pdata + report_data_ptr->data_len, m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
		report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
		report_data_ptr->pdata[report_data_ptr->data_len++] = m_sl651_rtu_type;
		_sl651_param_post();
		//观测时间
		time = rtcee_rtc_get_calendar();
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_OBSERVE_TIME;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_OBSERVE_TIME;
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_year % 100);
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mon + 1);
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_mday);
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_hour);
		report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(time.tm_min);
		
		//要素数量
		packet_cur = 0;
		packet_total = recv_data_ptr->data_len - SL651_BYTES_FRAME_END;
		while(i < packet_total)
		{
			if((i + 2) > packet_total)
			{
				break;
			}
			afn			= recv_data_ptr->pdata[i++];
			center_addr	= recv_data_ptr->pdata[i++];
			crc_value	= (center_addr >> 3);
			crc_value	+= SL651_BYTES_FRAME_ELEMENT_SYMBOL;

			switch(afn)
			{
			//10CM墒情
			case SL651_BOOT_SHANGQING_10CM:
			//20CM墒情
			case SL651_BOOT_SHANGQING_20CM:
			//30CM墒情
			case SL651_BOOT_SHANGQING_30CM:
			//40CM墒情
			case SL651_BOOT_SHANGQING_40CM:
				if(SL651_LEN_SHANGQING == center_addr)
				{
					if((report_data_ptr->data_len + crc_value + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
					{
						if(RT_TRUE == Sl651ElementIsActive(SL651_ELEMENT_SHANGQING_10CM + afn - SL651_BOOT_SHANGQING_10CM))
						{
							SampleGetData(&sampleDataType, SAMPLE_SENSOR_SHANGQING_10 + afn - SL651_BOOT_SHANGQING_10CM, RT_FALSE);
							report_data_ptr->data_len += _sl651_data_format_with_symbol(report_data_ptr->pdata + report_data_ptr->data_len, SAMPLE_SENSOR_SHANGQING_10 + afn - SL651_BOOT_SHANGQING_10CM, sampleDataType.sta, sampleDataType.value);
							packet_cur++;
						}
					}
				}
				break;
			//电源电压
			case SL651_BOOT_PWR_VOLTAGE:
				if(SL651_LEN_PWR_VOLTAGE == center_addr)
				{
					if((report_data_ptr->data_len + crc_value + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
					{
						crc_value = CpuPwrVolt();
						report_data_ptr->pdata[report_data_ptr->data_len++] = afn;
						report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
						report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(crc_value / 100);
						report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(crc_value % 100);
						packet_cur++;
					}
				}
				break;
			//状态及报警信息
			case SL651_BOOT_STATUS_ALARM:
				if(SL651_LEN_STATUS_ALARM == center_addr)
				{
					if((report_data_ptr->data_len + crc_value + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
					{
						report_data_ptr->pdata[report_data_ptr->data_len++] = afn;
						report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
						report_data_ptr->data_len += _sl651_sta_info(report_data_ptr->pdata + report_data_ptr->data_len);
						packet_cur++;
					}
				}
				break;
			}
		}
		if(0 == packet_cur)
		{
			return RT_FALSE;
		}
		break;
#endif
	//修改基本参数
	case SL651_AFN_SET_BASIC_PARAM:
	//修改运行参数
	case SL651_AFN_SET_RUN_PARAM:
		if((i + SL651_BYTES_FRAME_END) >= recv_data_ptr->data_len)
		{
			return RT_FALSE;
		}
		
		//长度验证(遥测站地址)
		if((report_data_ptr->data_len + SL651_BYTES_FRAME_RTU_ADDR) >= PTCL_BYTES_ACK_REPORT)
		{
			return RT_FALSE;
		}
		//遥测站地址
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_RTU_CODE;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_RTU_CODE;
		_sl651_param_pend(SL651_EVENT_PARAM_RTU_ADDR);
		rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
		_sl651_param_post(SL651_EVENT_PARAM_RTU_ADDR);
		report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
		
		//参数数量
		packet_cur = 0;
		packet_total = recv_data_ptr->data_len - SL651_BYTES_FRAME_END;
		while(i < packet_total)
		{
			if((i + 2) > packet_total)
			{
				break;
			}
			crc_value = recv_data_ptr->pdata[i++];
			if(SL651_FRAME_SYMBOL_ELEMENT_EX == crc_value)
			{
				if((i + 2) > packet_total)
				{
					break;
				}
				crc_value <<= 8;
				crc_value += recv_data_ptr->pdata[i++];
			}
			center_addr	= recv_data_ptr->pdata[i++];
			afn			= (center_addr >> 3);
			if((i + afn) > packet_total)
			{
				break;
			}

			if((report_data_ptr->data_len + SL651_BYTES_FRAME_ELEMENT_SYMBOL + afn + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
			{
				switch(crc_value)
				{
				//中心站地址
				case SL651_BOOT_CENTER_ADDR:
					if(SL651_LEN_CENTER_ADDR == center_addr)
					{
						pmem = (void *)ptcl_param_info_req(afn, RT_WAITING_NO);
						if((void *)0 != pmem)
						{
							report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
							report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
							rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)(recv_data_ptr->pdata + i), afn);
							report_data_ptr->data_len += afn;

							((PTCL_PARAM_INFO *)pmem)->param_type	= PTCL_PARAM_INFO_CENTER_ADDR_ALL;
							((PTCL_PARAM_INFO *)pmem)->data_len		= afn;
							rt_memcpy((void *)(((PTCL_PARAM_INFO *)pmem)->pdata), (void *)(recv_data_ptr->pdata + i), afn);
							ptcl_param_info_add(param_info_ptr_ptr, (PTCL_PARAM_INFO *)pmem);

							packet_cur++;
						}
					}
					break;
				//RTU地址
				case SL651_BOOT_RTU_ADDR:
					if(SL651_LEN_RTU_ADDR == center_addr)
					{
						pmem = (void *)ptcl_param_info_req(afn, RT_WAITING_NO);
						if((void *)0 != pmem)
						{
							report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
							report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
							rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)(recv_data_ptr->pdata + i), afn);
							report_data_ptr->data_len += afn;

							((PTCL_PARAM_INFO *)pmem)->param_type = PTCL_PARAM_INFO_SL651_RTU_ADDR;
							rt_memcpy((void *)(((PTCL_PARAM_INFO *)pmem)->pdata), (void *)(recv_data_ptr->pdata + i), afn);
							ptcl_param_info_add(param_info_ptr_ptr, (PTCL_PARAM_INFO *)pmem);

							packet_cur++;
						}
					}
					break;
				//密码
				case SL651_BOOT_PW:
					if(SL651_LEN_PW == center_addr)
					{
						pmem = (void *)ptcl_param_info_req(sizeof(rt_uint16_t), RT_WAITING_NO);
						if((void *)0 != pmem)
						{
							report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
							report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
							rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)(recv_data_ptr->pdata + i), afn);
							report_data_ptr->data_len += afn;

							((PTCL_PARAM_INFO *)pmem)->param_type		= PTCL_PARAM_INFO_SL651_PW;
							*(((PTCL_PARAM_INFO *)pmem)->pdata + 1)		= recv_data_ptr->pdata[i];
							*(((PTCL_PARAM_INFO *)pmem)->pdata)			= recv_data_ptr->pdata[i + 1];
							ptcl_param_info_add(param_info_ptr_ptr, (PTCL_PARAM_INFO *)pmem);

							packet_cur++;
						}
					}
					break;
				//中心站1主信道地址
				case SL651_BOOT_CENTER_1_COMM_1:
				//中心站1备信道地址
				case SL651_BOOT_CENTER_1_COMM_2:
				//中心站2主信道地址
				case SL651_BOOT_CENTER_2_COMM_1:
				//中心站2备信道地址
				case SL651_BOOT_CENTER_2_COMM_2:
				//中心站3主信道地址
				case SL651_BOOT_CENTER_3_COMM_1:
				//中心站3备信道地址
				case SL651_BOOT_CENTER_3_COMM_2:
				//中心站4主信道地址
				case SL651_BOOT_CENTER_4_COMM_1:
				//中心站4备信道地址
				case SL651_BOOT_CENTER_4_COMM_2:
					if(afn)
					{
						pos = i;
						center		= (crc_value - SL651_BOOT_CENTER_1_COMM_1) / 2;
						prio		= (crc_value - SL651_BOOT_CENTER_1_COMM_1) % 2;
						if((center < PTCL_NUM_CENTER) && (prio < PTCL_NUM_COMM_PRIO))
						{
							switch(fyyp_bcd_to_hex(recv_data_ptr->pdata[pos++]))
							{
							case PTCL_COMM_TYPE_NONE:
								if(1 == afn)
								{
									pos = 0;
								}
								break;
							case PTCL_COMM_TYPE_IP:
								if((10 == afn) && (center < DTU_NUM_IP_CH))
								{
									if(RT_TRUE == fyyp_is_bcdcode(recv_data_ptr->pdata + pos, afn - 1))
									{
										pos = 0;
									}
								}
								break;
							case PTCL_COMM_TYPE_SMS:
								if(afn > 1)
								{
									pos = afn - 1;
									pos *= 2;
									if((pos < DTU_BYTES_SMS_ADDR) && (center < DTU_NUM_SMS_CH))
									{
										pos = 0;
									}
								}
								break;
							}
							if(0 == pos)
							{
								pmem = (void *)ptcl_param_info_req(afn, RT_WAITING_NO);
								if((void *)0 != pmem)
								{
									report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
									report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
									rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)(recv_data_ptr->pdata + i), afn);
									report_data_ptr->data_len += afn;

									((PTCL_PARAM_INFO *)pmem)->param_type	= PTCL_PARAM_INFO_SL651_CENTER_COMM;
									((PTCL_PARAM_INFO *)pmem)->data_len		= crc_value - SL651_BOOT_CENTER_1_COMM_1;
									((PTCL_PARAM_INFO *)pmem)->data_len		<<= 8;
									((PTCL_PARAM_INFO *)pmem)->data_len		+= afn;
									rt_memcpy((void *)(((PTCL_PARAM_INFO *)pmem)->pdata), (void *)(recv_data_ptr->pdata + i), afn);
									ptcl_param_info_add(param_info_ptr_ptr, (PTCL_PARAM_INFO *)pmem);

									packet_cur++;
								}
							}
						}
					}
					break;
#if 0
				//工作方式
				case SL651_BOOT_WORK_MODE:
					if(SL651_LEN_WORK_MODE == center_addr)
					{
						report_data_ptr->pdata[report_data_ptr->data_len++] = afn;
						report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
						rt_memcpy(report_data_ptr->pdata + report_data_ptr->data_len, recv_data_ptr->pdata + i, crc_value);
						report_data_ptr->data_len += crc_value;

						_sl651_param_pend();
						m_sl651WorkMode = fyyp_bcd_to_hex(recv_data_ptr->pdata[i]);
						Sl651SetWorkMode(m_sl651WorkMode);
						_sl651_param_post();
						
						packet_cur++;

						Sl651ErcUpdate(SL651_EVENT_PARAM_CHANGE);
					}
					break;
				//监测要素
				case SL651_BOOT_SAMPLE_ELEMENT:
					if(SL651_LEN_SAMPLE_ELEMENT == center_addr)
					{
						report_data_ptr->pdata[report_data_ptr->data_len++] = afn;
						report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
						rt_memcpy(report_data_ptr->pdata + report_data_ptr->data_len, recv_data_ptr->pdata + i, crc_value);
						report_data_ptr->data_len += crc_value;

						_sl651_param_pend();
						rt_memcpy(m_sl651Element, recv_data_ptr->pdata + i, SL651_BYTES_ELEMENT);
						Sl651SetElement(m_sl651Element);
						_sl651_param_post();
						
						packet_cur++;

						Sl651ErcUpdate(SL651_EVENT_PARAM_CHANGE);
					}
					break;
				//设备识别码
				case SL651_BOOT_DEVICE_ID:
					if(crc_value <= SL651_BYTES_DEVICE_ID)
					{
						report_data_ptr->pdata[report_data_ptr->data_len++] = afn;
						report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
						rt_memcpy(report_data_ptr->pdata + report_data_ptr->data_len, recv_data_ptr->pdata + i, crc_value);
						report_data_ptr->data_len += crc_value;

						_sl651_param_pend();
						rt_memcpy(m_sl651DeviceID.data, recv_data_ptr->pdata + i, crc_value);
						m_sl651DeviceID.data_len = crc_value;
						Sl651SetDeviceID(&m_sl651DeviceID);
						_sl651_param_post();
						
						packet_cur++;

						Sl651ErcUpdate(SL651_EVENT_PARAM_CHANGE);
					}
					break;
#endif
				//定时报间隔
				case SL651_BOOT_REPORT_TIMEING:
					if(SL651_LEN_REPORT_TIMEING == center_addr)
					{
						pos = fyyp_bcd_to_hex(recv_data_ptr->pdata[i]);
						if(pos <= 24)
						{
							pmem = (void *)ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
							if((void *)0 != pmem)
							{
								report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
								report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
								rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)(recv_data_ptr->pdata + i), afn);
								report_data_ptr->data_len += afn;

								((PTCL_PARAM_INFO *)pmem)->param_type	= PTCL_PARAM_INFO_SL651_REPORT_HOUR;
								*(((PTCL_PARAM_INFO *)pmem)->pdata)		= fyyp_bcd_to_hex(recv_data_ptr->pdata[i]);
								ptcl_param_info_add(param_info_ptr_ptr, (PTCL_PARAM_INFO *)pmem);

								packet_cur++;
							}
						}
					}
					break;
				//加报间隔
				case SL651_BOOT_REPORT_ADD:
					if(SL651_LEN_REPORT_ADD == center_addr)
					{
						pos = fyyp_bcd_to_hex(recv_data_ptr->pdata[i]);
						if(pos < 60)
						{
							pmem = (void *)ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
							if((void *)0 != pmem)
							{
								report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
								report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
								rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)(recv_data_ptr->pdata + i), afn);
								report_data_ptr->data_len += afn;

								((PTCL_PARAM_INFO *)pmem)->param_type	= PTCL_PARAM_INFO_SL651_REPORT_MIN;
								*(((PTCL_PARAM_INFO *)pmem)->pdata)		= pos;
								ptcl_param_info_add(param_info_ptr_ptr, (PTCL_PARAM_INFO *)pmem);

								packet_cur++;
							}
						}
					}
					break;
				//水位存储间隔
				//雨量计分辨力
				//雨量阈值
				//水位基值
				case SL651_BOOT_WATER_BASE_1:
					if(SL651_LEN_WATER_BASE == center_addr)
					{
						if(RT_TRUE == sample_sensor_type_exist(SAMPLE_SENSOR_SHUIWEI_1))
						{
							pmem = (void *)ptcl_param_info_req(sizeof(float), RT_WAITING_NO);
							if((void *)0 != pmem)
							{
								report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
								report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
								rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)(recv_data_ptr->pdata + i), afn);
								report_data_ptr->data_len += afn;

								((PTCL_PARAM_INFO *)pmem)->param_type			= PTCL_PARAM_INFO_SAMPLE_BASE;
								((PTCL_PARAM_INFO *)pmem)->data_len				= sample_get_ctrl_num_by_sensor_type(SAMPLE_SENSOR_SHUIWEI_1);
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	= (float)fyyp_bcd_to_hex(recv_data_ptr->pdata[i]);
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	*= (float)100;
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	+= (float)fyyp_bcd_to_hex(recv_data_ptr->pdata[i + 1]);
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	*= (float)100;
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	+= (float)fyyp_bcd_to_hex(recv_data_ptr->pdata[i + 2]);
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	*= (float)100;
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	+= (float)fyyp_bcd_to_hex(recv_data_ptr->pdata[i + 3]);
								ptcl_param_info_add(param_info_ptr_ptr, (PTCL_PARAM_INFO *)pmem);

								packet_cur++;
							}
						}
					}
					break;
				//水位修正值
				//水位阈值
				//加报水位1
				case 0x3a:
					if(SL651_LEN_WATER_BASE == center_addr)
					{
						if(RT_TRUE == sample_sensor_type_exist(SAMPLE_SENSOR_SHUIWEI_1))
						{
							pmem = (void *)ptcl_param_info_req(sizeof(float), RT_WAITING_NO);
							if((void *)0 != pmem)
							{
								report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
								report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
								rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)(recv_data_ptr->pdata + i), afn);
								report_data_ptr->data_len += afn;

								((PTCL_PARAM_INFO *)pmem)->param_type			= PTCL_PARAM_INFO_SAMPLE_DOWN;
								((PTCL_PARAM_INFO *)pmem)->data_len				= sample_get_ctrl_num_by_sensor_type(SAMPLE_SENSOR_SHUIWEI_1);
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	= (float)fyyp_bcd_to_hex(recv_data_ptr->pdata[i]);
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	*= (float)100;
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	+= (float)fyyp_bcd_to_hex(recv_data_ptr->pdata[i + 1]);
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	*= (float)100;
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	+= (float)fyyp_bcd_to_hex(recv_data_ptr->pdata[i + 2]);
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	*= (float)100;
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	+= (float)fyyp_bcd_to_hex(recv_data_ptr->pdata[i + 3]);
								ptcl_param_info_add(param_info_ptr_ptr, (PTCL_PARAM_INFO *)pmem);

								packet_cur++;
							}
						}
					}
					break;
				//加报水位2
				case 0x3b:
					if(SL651_LEN_WATER_BASE == center_addr)
					{
						if(RT_TRUE == sample_sensor_type_exist(SAMPLE_SENSOR_SHUIWEI_1))
						{
							pmem = (void *)ptcl_param_info_req(sizeof(float), RT_WAITING_NO);
							if((void *)0 != pmem)
							{
								report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
								report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
								rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)(recv_data_ptr->pdata + i), afn);
								report_data_ptr->data_len += afn;

								((PTCL_PARAM_INFO *)pmem)->param_type			= PTCL_PARAM_INFO_SAMPLE_UP;
								((PTCL_PARAM_INFO *)pmem)->data_len				= sample_get_ctrl_num_by_sensor_type(SAMPLE_SENSOR_SHUIWEI_1);
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	= (float)fyyp_bcd_to_hex(recv_data_ptr->pdata[i]);
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	*= (float)100;
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	+= (float)fyyp_bcd_to_hex(recv_data_ptr->pdata[i + 1]);
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	*= (float)100;
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	+= (float)fyyp_bcd_to_hex(recv_data_ptr->pdata[i + 2]);
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	*= (float)100;
								*(float *)(((PTCL_PARAM_INFO *)pmem)->pdata)	+= (float)fyyp_bcd_to_hex(recv_data_ptr->pdata[i + 3]);
								ptcl_param_info_add(param_info_ptr_ptr, (PTCL_PARAM_INFO *)pmem);

								packet_cur++;
							}
						}
					}
					break;
				}
			}
			
			i += afn;
		}
		if(0 == packet_cur)
		{
			return RT_FALSE;
		}
		break;
	//读取基本参数
	case SL651_AFN_QUERY_BASIC_PARAM:
	//读取运行参数
	case SL651_AFN_QUERY_RUN_PARAM:
		//下行帧长度验证
		if((i + SL651_BYTES_FRAME_END) >= recv_data_ptr->data_len)
		{
			return RT_FALSE;
		}
		//上行帧长度验证(遥测站地址)
		if((report_data_ptr->data_len + SL651_BYTES_FRAME_RTU_ADDR) >= PTCL_BYTES_ACK_REPORT)
		{
			return RT_FALSE;
		}
		//遥测站地址
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_RTU_CODE;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_RTU_CODE;
		_sl651_param_pend(SL651_EVENT_PARAM_RTU_ADDR);
		rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
		_sl651_param_post(SL651_EVENT_PARAM_RTU_ADDR);
		report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
		
		//要素数量
		packet_cur = 0;
		packet_total = recv_data_ptr->data_len - SL651_BYTES_FRAME_END;
		while(i < packet_total)
		{
			if((i + 2) > packet_total)
			{
				break;
			}
			crc_value = recv_data_ptr->pdata[i++];
			if(SL651_FRAME_SYMBOL_ELEMENT_EX == crc_value)
			{
				if((i + 2) > packet_total)
				{
					break;
				}
				crc_value <<= 8;
				crc_value += recv_data_ptr->pdata[i++];
			}
			center_addr	= recv_data_ptr->pdata[i++];
			afn			= (center_addr >> 3);

			switch(crc_value)
			{
			//中心站地址
			case SL651_BOOT_CENTER_ADDR:
				if(SL651_LEN_CENTER_ADDR == center_addr)
				{
					if((report_data_ptr->data_len + 2 + afn + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
					{
						report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
						report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
						for(center_addr = 0; center_addr < afn; center_addr++)
						{
							if(center_addr < PTCL_NUM_CENTER)
							{
								report_data_ptr->pdata[report_data_ptr->data_len++] = ptcl_get_center_addr(center_addr);
							}
							else
							{
								report_data_ptr->pdata[report_data_ptr->data_len++] = 0;
							}
						}
						
						packet_cur++;
					}
				}
				break;
			//RTU地址
			case SL651_BOOT_RTU_ADDR:
				if(SL651_LEN_RTU_ADDR == center_addr)
				{
					if((report_data_ptr->data_len + 2 + afn + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
					{
						report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
						report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
						_sl651_param_pend(SL651_EVENT_PARAM_RTU_ADDR);
						rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
						_sl651_param_post(SL651_EVENT_PARAM_RTU_ADDR);
						report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
						
						packet_cur++;
					}
				}
				break;
			//密码
			case SL651_BOOT_PW:
				if(SL651_LEN_PW == center_addr)
				{
					if((report_data_ptr->data_len + 2 + afn + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
					{
						report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
						report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
						_sl651_param_pend(SL651_EVENT_PARAM_PW);
						report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(m_sl651_pw >> 8);
						report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)m_sl651_pw;
						_sl651_param_post(SL651_EVENT_PARAM_PW);
						
						packet_cur++;
					}
				}
				break;
			//中心站1主信道地址
			case SL651_BOOT_CENTER_1_COMM_1:
			//中心站1备信道地址
			case SL651_BOOT_CENTER_1_COMM_2:
			//中心站2主信道地址
			case SL651_BOOT_CENTER_2_COMM_1:
			//中心站2备信道地址
			case SL651_BOOT_CENTER_2_COMM_2:
			//中心站3主信道地址
			case SL651_BOOT_CENTER_3_COMM_1:
			//中心站3备信道地址
			case SL651_BOOT_CENTER_3_COMM_2:
			//中心站4主信道地址
			case SL651_BOOT_CENTER_4_COMM_1:
			//中心站4备信道地址
			case SL651_BOOT_CENTER_4_COMM_2:
				center	= (crc_value - SL651_BOOT_CENTER_1_COMM_1) / 2;
				prio	= (crc_value - SL651_BOOT_CENTER_1_COMM_1) % 2;
				if((center < PTCL_NUM_CENTER) && (prio < PTCL_NUM_COMM_PRIO))
				{
					pos = ptcl_get_comm_type(center, prio);
					switch(pos)
					{
					default:
						afn	= 1;
						center_addr	= (afn << 3);
						if((report_data_ptr->data_len + 2 + afn + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
						{
							report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
							report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
							report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(pos);
							
							packet_cur++;
						}
						break;
					case PTCL_COMM_TYPE_IP:
						if(center < DTU_NUM_IP_CH)
						{
							if(RT_TRUE == dtu_get_ip_ch(center))
							{
								afn	= 10;
								center_addr	= (afn << 3);
								if((report_data_ptr->data_len + 2 + afn + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
								{
									pmem = mempool_req(sizeof(DTU_IP_ADDR), RT_WAITING_NO);
									if((void *)0 != pmem)
									{
										dtu_get_ip_addr((DTU_IP_ADDR *)pmem, center);
										report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
										report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
										report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(pos);
										report_data_ptr->data_len += fyyp_ip_str_to_bcd(report_data_ptr->pdata + report_data_ptr->data_len, ((DTU_IP_ADDR *)pmem)->addr_data, ((DTU_IP_ADDR *)pmem)->addr_len);
										rt_mp_free(pmem);
										
										packet_cur++;
									}
								}
							}
							else
							{
								afn	= 1;
								center_addr	= (afn << 3);
								if((report_data_ptr->data_len + 2 + afn + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
								{
									report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
									report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
									report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(pos);
									
									packet_cur++;
								}
							}
						}
						else
						{
							afn	= 1;
							center_addr	= (afn << 3);
							if((report_data_ptr->data_len + 2 + afn + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
							{
								report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
								report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(pos);
								
								packet_cur++;
							}
						}
						break;
					case PTCL_COMM_TYPE_SMS:
						if(center < DTU_NUM_SMS_CH)
						{
							if(RT_TRUE == dtu_get_sms_ch(center))
							{
								pmem = mempool_req(sizeof(DTU_SMS_ADDR), RT_WAITING_NO);
								if((void *)0 != pmem)
								{
									dtu_get_sms_addr((DTU_SMS_ADDR *)pmem, center);
									afn = ((DTU_SMS_ADDR *)pmem)->addr_len;
									if(afn % 2)
									{
										afn /= 2;
										afn++;
									}
									else
									{
										afn /= 2;
									}
									afn++;
									center_addr	= (afn << 3);
									if((report_data_ptr->data_len + 2 + afn + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
									{
										report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
										report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
										report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(pos);
										report_data_ptr->data_len += fyyp_sms_str_to_bcd(report_data_ptr->pdata + report_data_ptr->data_len, ((DTU_SMS_ADDR *)pmem)->addr_data, ((DTU_SMS_ADDR *)pmem)->addr_len);
										
										packet_cur++;
									}
									
									rt_mp_free(pmem);
								}
							}
							else
							{
								afn	= 1;
								center_addr	= (afn << 3);
								if((report_data_ptr->data_len + 2 + afn + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
								{
									report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
									report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
									report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(pos);
									
									packet_cur++;
								}
							}
						}
						else
						{
							afn	= 1;
							center_addr	= (afn << 3);
							if((report_data_ptr->data_len + 2 + afn + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
							{
								report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
								report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
								report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(pos);
								
								packet_cur++;
							}
						}
						break;
					}
				}
				break;
#if 0
			//工作方式
			case SL651_BOOT_WORK_MODE:
				if(SL651_LEN_WORK_MODE == center_addr)
				{
					if((report_data_ptr->data_len + 2 + crc_value + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
					{
						report_data_ptr->pdata[report_data_ptr->data_len++] = afn;
						report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
						_sl651_param_pend();
						report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(m_sl651WorkMode);
						_sl651_param_post();
						
						packet_cur++;
					}
				}
				break;
			//监测要素
			case SL651_BOOT_SAMPLE_ELEMENT:
				if(SL651_LEN_SAMPLE_ELEMENT == center_addr)
				{
					if((report_data_ptr->data_len + 2 + crc_value + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
					{
						report_data_ptr->pdata[report_data_ptr->data_len++] = afn;
						report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
						_sl651_param_pend();
						rt_memcpy(report_data_ptr->pdata + report_data_ptr->data_len, m_sl651Element, crc_value);
						_sl651_param_post();
						report_data_ptr->data_len += crc_value;
						
						packet_cur++;
					}
				}
				break;
			//设备识别码
			case SL651_BOOT_DEVICE_ID:
				_sl651_param_pend();
				crc_value	= m_sl651DeviceID.data_len;
				center_addr	= (crc_value << 3);
				if((report_data_ptr->data_len + 2 + crc_value + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
				{
					report_data_ptr->pdata[report_data_ptr->data_len++] = afn;
					report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
					rt_memcpy(report_data_ptr->pdata + report_data_ptr->data_len, m_sl651DeviceID.data, m_sl651DeviceID.data_len);
					report_data_ptr->data_len += m_sl651DeviceID.data_len;
					
					packet_cur++;
				}
				_sl651_param_post();
				break;
#endif
			//定时报间隔
			case SL651_BOOT_REPORT_TIMEING:
				if(SL651_LEN_REPORT_TIMEING == center_addr)
				{
					if((report_data_ptr->data_len + 2 + afn + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
					{
						report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
						report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
						_sl651_param_pend(SL651_EVENT_PARAM_REPORT_HOUR);
						report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(m_sl651_report_hour);
						_sl651_param_post(SL651_EVENT_PARAM_REPORT_HOUR);
						
						packet_cur++;
					}
				}
				break;
			//加报间隔
			case SL651_BOOT_REPORT_ADD:
				if(SL651_LEN_REPORT_ADD == center_addr)
				{
					if((report_data_ptr->data_len + 2 + afn + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
					{
						report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
						report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
						_sl651_param_pend(SL651_EVENT_PARAM_REPORT_MIN);
						report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(m_sl651_report_min);
						_sl651_param_post(SL651_EVENT_PARAM_REPORT_MIN);
						
						packet_cur++;
					}
				}
				break;
			//水位存储间隔
			//雨量计分辨力
			//雨量阈值
			//水位基值
			case SL651_BOOT_WATER_BASE_1:
				if(SL651_LEN_WATER_BASE == center_addr)
				{
					if((report_data_ptr->data_len + 2 + afn + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
					{
						if(RT_TRUE == sample_sensor_type_exist(SAMPLE_SENSOR_SHUIWEI_1))
						{
							*(rt_uint32_t *)(&pmem) = (rt_uint32_t)sample_get_base(sample_get_ctrl_num_by_sensor_type(SAMPLE_SENSOR_SHUIWEI_1));
							report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
							report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
							report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(*(rt_uint32_t *)(&pmem) / 1000000 % 100);
							report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(*(rt_uint32_t *)(&pmem) / 10000 % 100);
							report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(*(rt_uint32_t *)(&pmem) / 100 % 100);
							report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(*(rt_uint32_t *)(&pmem) % 100);
							
							packet_cur++;
						}
					}
				}
				break;
			//水位修正值
			//水位阈值
			//加报水位1
			case 0x3a:
				if(SL651_LEN_WATER_BASE == center_addr)
				{
					if((report_data_ptr->data_len + 2 + afn + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
					{
						if(RT_TRUE == sample_sensor_type_exist(SAMPLE_SENSOR_SHUIWEI_1))
						{
							*(rt_uint32_t *)(&pmem) = (rt_uint32_t)sample_get_down(sample_get_ctrl_num_by_sensor_type(SAMPLE_SENSOR_SHUIWEI_1));
							report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
							report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
							report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(*(rt_uint32_t *)(&pmem) / 1000000 % 100);
							report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(*(rt_uint32_t *)(&pmem) / 10000 % 100);
							report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(*(rt_uint32_t *)(&pmem) / 100 % 100);
							report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(*(rt_uint32_t *)(&pmem) % 100);
							
							packet_cur++;
						}
					}
				}
				break;
			//加报水位2
			case 0x3b:
				if(SL651_LEN_WATER_BASE == center_addr)
				{
					if((report_data_ptr->data_len + 2 + afn + SL651_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
					{
						if(RT_TRUE == sample_sensor_type_exist(SAMPLE_SENSOR_SHUIWEI_1))
						{
							*(rt_uint32_t *)(&pmem) = (rt_uint32_t)sample_get_up(sample_get_ctrl_num_by_sensor_type(SAMPLE_SENSOR_SHUIWEI_1));
							report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
							report_data_ptr->pdata[report_data_ptr->data_len++] = center_addr;
							report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(*(rt_uint32_t *)(&pmem) / 1000000 % 100);
							report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(*(rt_uint32_t *)(&pmem) / 10000 % 100);
							report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(*(rt_uint32_t *)(&pmem) / 100 % 100);
							report_data_ptr->pdata[report_data_ptr->data_len++] = fyyp_hex_to_bcd(*(rt_uint32_t *)(&pmem) % 100);
							
							packet_cur++;
						}
					}
				}
				break;
			}
		}
		if(0 == packet_cur)
		{
			return RT_FALSE;
		}
		break;
#if 0
	//查询软件版本
	case SL651_AFN_QUERY_SOFT_VER:
		if((i + SL651_BYTES_FRAME_END) != recv_data_ptr->data_len)
		{
			return RT_FALSE;
		}
		pSoftVer = PtclSoftVer();
		crc_value = strlen(pSoftVer);
		if((report_data_ptr->data_len + SL651_BYTES_FRAME_RTU_ADDR + 1 + crc_value + SL651_BYTES_FRAME_END) > PTCL_BYTES_ACK_REPORT)
		{
			return RT_FALSE;
		}
		//遥测站地址
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_RTU_CODE;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_RTU_CODE;
		_sl651_param_pend();
		rt_memcpy(report_data_ptr->pdata + report_data_ptr->data_len, m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
		_sl651_param_post();
		report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
		//软件版本
		report_data_ptr->pdata[report_data_ptr->data_len++] = crc_value;
		rt_memcpy(report_data_ptr->pdata + report_data_ptr->data_len, (rt_uint8_t const *)pSoftVer, crc_value);
		report_data_ptr->data_len += crc_value;
		break;
	//查询终端状态和报警信息
	case SL651_AFN_QUERY_RTU_STATUS:
		if((i + SL651_BYTES_FRAME_END) != recv_data_ptr->data_len)
		{
			return RT_FALSE;
		}
		if((report_data_ptr->data_len + SL651_BYTES_FRAME_RTU_ADDR + SL651_BYTES_FRAME_STATUS_ALARM + SL651_BYTES_FRAME_END) > PTCL_BYTES_ACK_REPORT)
		{
			return RT_FALSE;
		}
		//遥测站地址
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_RTU_CODE;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_RTU_CODE;
		_sl651_param_pend();
		rt_memcpy(report_data_ptr->pdata + report_data_ptr->data_len, m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
		_sl651_param_post();
		report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
		//状态和报警信息
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_STATUS_ALARM;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_STATUS_ALARM;
		report_data_ptr->data_len += _sl651_sta_info(report_data_ptr->pdata + report_data_ptr->data_len);
		break;
	//初始化固态存储数据
	case SL651_AFN_CLEAR_OLD_DATA:
		if((i + 2 + SL651_BYTES_FRAME_END) != recv_data_ptr->data_len)
		{
			return RT_FALSE;
		}
		if(SL651_BOOT_CLEAR_OLD_DATA != recv_data_ptr->pdata[i++])
		{
			return RT_FALSE;
		}
		if(SL651_LEN_CLEAR_OLD_DATA != recv_data_ptr->pdata[i])
		{
			return RT_FALSE;
		}
		if((report_data_ptr->data_len + SL651_BYTES_FRAME_RTU_ADDR + SL651_BYTES_FRAME_END) > PTCL_BYTES_ACK_REPORT)
		{
			return RT_FALSE;
		}

		//遥测站地址
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_RTU_CODE;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_RTU_CODE;
		_sl651_param_pend();
		rt_memcpy(report_data_ptr->pdata + report_data_ptr->data_len, m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
		_sl651_param_post();
		report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
		break;
	//恢复终端出厂设置
	case SL651_AFN_RTU_RESTORE:
		if((i + 2 + SL651_BYTES_FRAME_END) != recv_data_ptr->data_len)
		{
			return RT_FALSE;
		}
		if(SL651_BOOT_RTU_RESTORE != recv_data_ptr->pdata[i++])
		{
			return RT_FALSE;
		}
		if(SL651_LEN_RTU_RESTORE != recv_data_ptr->pdata[i])
		{
			return RT_FALSE;
		}
		if((report_data_ptr->data_len + SL651_BYTES_FRAME_RTU_ADDR + SL651_BYTES_FRAME_END) > PTCL_BYTES_ACK_REPORT)
		{
			return RT_FALSE;
		}

		if(RT_FALSE == PtclRunStaSet(PTCL_RUN_STA_RESTORE))
		{
			return RT_FALSE;
		}

		//遥测站地址
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_RTU_CODE;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_RTU_CODE;
		_sl651_param_pend();
		rt_memcpy(report_data_ptr->pdata + report_data_ptr->data_len, m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
		_sl651_param_post();
		report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
		break;
	//修改密码
	case SL651_AFN_CHANGE_PW:
		//长度验证
		if((i + 8 + SL651_BYTES_FRAME_END) != recv_data_ptr->data_len)
		{
			return RT_FALSE;
		}
		//旧密码
		if(SL651_BOOT_PW != recv_data_ptr->pdata[i++])
		{
			return RT_FALSE;
		}
		if(SL651_LEN_PW != recv_data_ptr->pdata[i++])
		{
			return RT_FALSE;
		}
		crc_value = recv_data_ptr->pdata[i++];
		crc_value <<= 8;
		crc_value += recv_data_ptr->pdata[i++];
		_sl651_param_pend();
		if(crc_value != m_sl651_pw)
		{
			_sl651_param_post();
			return RT_FALSE;
		}
		_sl651_param_post();
		//新密码
		if(SL651_BOOT_PW != recv_data_ptr->pdata[i++])
		{
			return RT_FALSE;
		}
		if(SL651_LEN_PW != recv_data_ptr->pdata[i++])
		{
			return RT_FALSE;
		}
		crc_value = recv_data_ptr->pdata[i++];
		crc_value <<= 8;
		crc_value += recv_data_ptr->pdata[i++];

		if((report_data_ptr->data_len + SL651_BYTES_FRAME_RTU_ADDR + 4 + SL651_BYTES_FRAME_END) > PTCL_BYTES_ACK_REPORT)
		{
			return RT_FALSE;
		}
		_sl651_param_pend();
		m_sl651_pw = crc_value;
		Sl651SetRtuPw(m_sl651_pw);
		_sl651_param_post();

		//遥测站地址
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_RTU_CODE;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_RTU_CODE;
		_sl651_param_pend();
		rt_memcpy(report_data_ptr->pdata + report_data_ptr->data_len, m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
		_sl651_param_post();
		report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
		//新密码
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_PW;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_PW;
		report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(crc_value >> 8);
		report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)crc_value;

		Sl651ErcUpdate(SL651_EVENT_PW_CHANGE);
		break;
	//设置时钟
	case SL651_AFN_SET_TIME:
		//长度验证
		if((i + SL651_BYTES_FRAME_END) != recv_data_ptr->data_len)
		{
			return RT_FALSE;
		}

		if((report_data_ptr->data_len + SL651_BYTES_FRAME_RTU_ADDR + SL651_BYTES_FRAME_END) > PTCL_BYTES_ACK_REPORT)
		{
			return RT_FALSE;
		}

		unixRtu = rtcee_rtc_calendar_to_unix(rtcee_rtc_get_calendar());
		unixSet = rtcee_rtc_calendar_to_unix(time);
		if(unixSet > unixRtu)
		{
			unixRtu = unixSet - unixRtu;
		}
		else
		{
			unixRtu -= unixSet;
		}

		if(unixRtu > 300u)
		{
			if(RT_FALSE == timeEn)
			{
				timeEn = RT_TRUE;
				report_data_ptr->data_len = 0;
				return RT_TRUE;
			}
			else
			{
				TimeSetCalendar(rtcee_rtc_unix_to_calendar(unixSet));
				timeEn = RT_FALSE;
			}
		}
		else
		{
			TimeSetCalendar(rtcee_rtc_unix_to_calendar(unixSet));
			timeEn = RT_FALSE;
		}

		//遥测站地址
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_RTU_CODE;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_RTU_CODE;
		_sl651_param_pend();
		rt_memcpy(report_data_ptr->pdata + report_data_ptr->data_len, m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
		_sl651_param_post();
		report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
		break;
	//查询事件记录
	case SL651_AFN_QUERY_EVENT_RECORD:
		//长度验证
		if((i + SL651_BYTES_FRAME_END) != recv_data_ptr->data_len)
		{
			return RT_FALSE;
		}

		if((report_data_ptr->data_len + SL651_BYTES_FRAME_RTU_ADDR + 64u + SL651_BYTES_FRAME_END) > PTCL_BYTES_ACK_REPORT)
		{
			return RT_FALSE;
		}

		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_RTU_CODE;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_RTU_CODE;
		_sl651_param_pend();
		rt_memcpy(report_data_ptr->pdata + report_data_ptr->data_len, m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
		report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
		for(crc_value = 0; crc_value < SL651_NUM_EVENT_TYPE; crc_value++)
		{
			report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(m_sl651EventRecord[crc_value] >> 8);
			report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)m_sl651EventRecord[crc_value];
		}
		_sl651_param_post();
		break;
	//查询时钟
	case SL651_AFN_QUERY_TIME:
		//长度验证
		if((i + SL651_BYTES_FRAME_END) != recv_data_ptr->data_len)
		{
			return RT_FALSE;
		}

		if((report_data_ptr->data_len + SL651_BYTES_FRAME_RTU_ADDR + SL651_BYTES_FRAME_END) > PTCL_BYTES_ACK_REPORT)
		{
			return RT_FALSE;
		}

		//遥测站地址
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_BOOT_RTU_CODE;
		report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_LEN_RTU_CODE;
		_sl651_param_pend();
		rt_memcpy(report_data_ptr->pdata + report_data_ptr->data_len, m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR);
		_sl651_param_post();
		report_data_ptr->data_len += SL651_BYTES_RTU_ADDR;
		break;
#endif
	}

	//上下行标识及正文长度
	i = SL651_BYTES_FRAME_HEAD - SL651_BYTES_START_SYMBOL - SL651_BYTES_UP_DOWN_LEN;
	crc_value = report_data_ptr->data_len - SL651_BYTES_FRAME_HEAD;
	report_data_ptr->pdata[i]		= (rt_uint8_t)(crc_value >> 8);
	report_data_ptr->pdata[i++]		+= (rt_uint8_t)(SL651_FRAME_DIR_UP << 4);
	report_data_ptr->pdata[i]		= (rt_uint8_t)crc_value;
	//流水号
	i = SL651_BYTES_FRAME_HEAD;
	crc_value = _sl651_sn_value(RT_TRUE);
	report_data_ptr->pdata[i++]		= (rt_uint8_t)(crc_value >> 8);
	report_data_ptr->pdata[i]		= (rt_uint8_t)crc_value;
	//帧尾
	report_data_ptr->pdata[report_data_ptr->data_len++] = SL651_FRAME_SYMBOL_ETX;
	report_data_ptr->data_len += SL651_BYTES_CRC_VALUE;
	
	report_data_ptr->fun_csq_update		= (void *)_sl651_csq_update;
	report_data_ptr->data_id			= crc_value;
	report_data_ptr->need_reply			= RT_FALSE;
	report_data_ptr->fcb_value			= SL651_FRAME_DEFAULT_FCB;
	report_data_ptr->ptcl_type			= PTCL_PTCL_TYPE_SL651;

	return RT_TRUE;
}

rt_uint8_t sl651_report_decoder(PTCL_REPORT_DATA *report_data_ptr, PTCL_RECV_DATA const *recv_data_ptr)
{
	rt_uint16_t			i, crc_value, element_boot, data_len, recv_len;
	rt_uint8_t			element_num = 0, element_len, warn_level;
	rt_uint32_t			data_value;
	void				*pmem;
	static rt_uint32_t	sn_ex = 0;
	
	i = 0;
	if((i + SL651_BYTES_FRAME_HEAD) > recv_data_ptr->data_len)
	{
		return RT_FALSE;
	}
	//7E7E
	if(SL651_FRAME_SYMBOL != recv_data_ptr->pdata[i++])
	{
		return RT_FALSE;
	}
	if(SL651_FRAME_SYMBOL != recv_data_ptr->pdata[i++])
	{
		return RT_FALSE;
	}
	//中心站地址
	i++;
	//RTU地址
	i += SL651_BYTES_RTU_ADDR;
	//密码
	i += SL651_BYTES_PW;
	//功能码
	i++;
	//上下行标识及长度
	if(SL651_FRAME_DIR_UP != (recv_data_ptr->pdata[i] >> 4))
	{
		return RT_FALSE;
	}
	recv_len = recv_data_ptr->pdata[i++] & 0x0f;
	recv_len <<= 8;
	recv_len += (recv_data_ptr->pdata[i] + SL651_BYTES_FRAME_HEAD + SL651_BYTES_FRAME_END);
	if(recv_data_ptr->data_len < recv_len)
	{
		return RT_FALSE;
	}
	
	//数据分析解包
	//crc
	if(recv_len > SL651_BYTES_CRC_VALUE)
	{
		i = recv_len - SL651_BYTES_CRC_VALUE;
		crc_value = _sl651_crc_value(recv_data_ptr->pdata, i);
		if(recv_data_ptr->pdata[i++] != (rt_uint8_t)(crc_value >> 8))
		{
			return RT_FALSE;
		}
		if(recv_data_ptr->pdata[i] != (rt_uint8_t)crc_value)
		{
			return RT_FALSE;
		}
	}
	else
	{
		return RT_FALSE;
	}
	//帧头
	i = 0;
	if((i + SL651_BYTES_FRAME_HEAD) > recv_len)
	{
		return RT_FALSE;
	}
	//7E7E
	if(SL651_FRAME_SYMBOL != recv_data_ptr->pdata[i++])
	{
		return RT_FALSE;
	}
	if(SL651_FRAME_SYMBOL != recv_data_ptr->pdata[i++])
	{
		return RT_FALSE;
	}
	//中心站地址
	i++;
	//RTU地址
	_sl651_param_pend(SL651_EVENT_PARAM_RTU_ADDR);
	crc_value = (0 == rt_memcmp((void *)(recv_data_ptr->pdata + i), (void *)m_sl651_rtu_addr, SL651_BYTES_RTU_ADDR)) ? RT_TRUE : RT_FALSE;
	_sl651_param_post(SL651_EVENT_PARAM_RTU_ADDR);
	if(RT_TRUE == crc_value)
	{
		return RT_FALSE;
	}
	i += (SL651_BYTES_RTU_ADDR - 2);
	data_value = *(rt_uint16_t *)(recv_data_ptr->pdata + i);
	data_value <<= 16;
	i += 2;
	//密码
	i += SL651_BYTES_PW;
	//功能码
	crc_value = recv_data_ptr->pdata[i++];
	if((SL651_AFN_REPORT_ADD != crc_value) && (SL651_AFN_REPORT_TIMING != crc_value))
	{
		return RT_FALSE;
	}
	//上下行标识及长度
	if(SL651_FRAME_DIR_UP != (recv_data_ptr->pdata[i] >> 4))
	{
		return RT_FALSE;
	}
	crc_value = recv_data_ptr->pdata[i++] & 0x0f;
	crc_value <<= 8;
	crc_value += (recv_data_ptr->pdata[i++] + SL651_BYTES_FRAME_HEAD + SL651_BYTES_FRAME_END);
	if(recv_len != crc_value)
	{
		return RT_FALSE;
	}
	//起始符
	if(SL651_FRAME_SYMBOL_STX != recv_data_ptr->pdata[i++])
	{
		return RT_FALSE;
	}
	//流水号
	if((i + SL651_BYTES_SN) > recv_len)
	{
		return RT_FALSE;
	}
	crc_value = recv_data_ptr->pdata[i++];
	crc_value <<= 8;
	crc_value += recv_data_ptr->pdata[i++];
	if(0 == crc_value)
	{
		return RT_FALSE;
	}
	//数据过滤
	data_value += crc_value;
	if(data_value == sn_ex)
	{
		return RT_FALSE;
	}
	sn_ex = data_value;
	//发报时间、遥测站地址、测站分类码、观测时间
	crc_value = SL651_BYTES_SEND_TIME + SL651_BYTES_FRAME_RTU_ADDR + SL651_BYTES_RTU_TYPE + SL651_BYTES_FRAME_OBSERVE_TIME;
	if((i + crc_value) > recv_len)
	{
		return RT_FALSE;
	}
	i += crc_value;
	//要素信息
	crc_value = recv_len - SL651_BYTES_FRAME_END;
	while(i < crc_value)
	{
		if((i + 2) > crc_value)
		{
			break;
		}
		element_boot = recv_data_ptr->pdata[i++];
		if(SL651_FRAME_SYMBOL_ELEMENT_EX == element_boot)
		{
			if((i + 2) > crc_value)
			{
				break;
			}
			element_boot <<= 8;
			element_boot += recv_data_ptr->pdata[i++];
		}
		element_len	= recv_data_ptr->pdata[i++];
		data_len = (element_len >> 3);
		if((i + data_len) > crc_value)
		{
			break;
		}

		switch(element_boot)
		{
		case SL651_BOOT_WATER_STA_EX:
			if(SL651_LEN_WATER_STA_EX == element_len)
			{
				warn_level = recv_data_ptr->pdata[i];
				
				element_num++;
			}
			break;
		case SL651_BOOT_WATER_CUR:
			if(SL651_LEN_WATER_CUR == element_len)
			{
				data_value = fyyp_bcd_to_hex(recv_data_ptr->pdata[i]);
				data_value *= 100;
				data_value += fyyp_bcd_to_hex(recv_data_ptr->pdata[i + 1]);
				data_value *= 100;
				data_value += fyyp_bcd_to_hex(recv_data_ptr->pdata[i + 2]);
				data_value *= 100;
				data_value += fyyp_bcd_to_hex(recv_data_ptr->pdata[i + 3]);
				
				element_num++;
			}
			break;
		}
		i += data_len;
	}

	if(2 == element_num)
	{
		pmem = mempool_req(SL651_BYTES_WARN_INFO_MAX, RT_WAITING_FOREVER);
		if((void *)0 != pmem)
		{
			data_len = sl651_get_warn_info((rt_uint8_t *)pmem, warn_level);
			if(data_len)
			{
				wm_warn_tts_trigger(WM_WARN_TYPE_GPRS, SL651_DEF_WARN_TIMES, DTU_TTS_DATA_TYPE_GBK, (rt_uint8_t *)pmem, data_len, RT_FALSE);
				bx5k_set_screen_txt((rt_uint8_t *)pmem, data_len);
			}
			
			rt_mp_free(pmem);
		}

		bx5k_set_screen_data(data_value);
		
		if(recv_len > PTCL_BYTES_ACK_REPORT)
		{
			report_data_ptr->data_len = 0;
		}
		else
		{
			rt_memcpy((void *)report_data_ptr->pdata, (void *)recv_data_ptr->pdata, recv_len);
			report_data_ptr->data_len			= recv_len;
			report_data_ptr->data_id			= 0;
			report_data_ptr->need_reply			= RT_FALSE;
			report_data_ptr->fcb_value			= 0;
			report_data_ptr->fun_csq_update		= (void *)0;
			report_data_ptr->ptcl_type			= PTCL_PTCL_TYPE_SL651;
		}

		return RT_TRUE;
	}

	return RT_FALSE;
}

