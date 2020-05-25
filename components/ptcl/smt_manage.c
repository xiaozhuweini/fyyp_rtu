#include "smt_manage.h"
#include "drv_mempool.h"
#include "dtu.h"
#include "drv_fyyp.h"
#include "stm32f4xx.h"
#include "stdio.h"
#include "string.h"
#include "drv_tevent.h"
#include "drv_lpm.h"
#include "xmodem.h"
#include "sample.h"



//运行时间
static rt_uint32_t					m_smtmng_run_time		= 0;

//事件参数互斥
static struct rt_event				m_smtmng_event_module;

//任务
static struct rt_thread				m_smtmng_thread_time_tick;
static rt_uint8_t					m_smtmng_stk_time_tick[SMTMNG_STK_TIME_TICK];

//参数集合
static SMTMNG_PARAM_SET				m_smtmng_param_set;
#define m_smtmng_period_sta			m_smtmng_param_set.period_sta
#define m_smtmng_period_timing		m_smtmng_param_set.period_timing
#define m_smtmng_period_heart		m_smtmng_param_set.period_heart



#include "smt_manage_io.c"



//参数互斥
static void _smtmng_param_pend(rt_uint32_t param)
{
	if(RT_EOK != rt_event_recv(&m_smtmng_event_module, param, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}
}
static void _smtmng_param_post(rt_uint32_t param)
{
	if(RT_EOK != rt_event_send(&m_smtmng_event_module, param))
	{
		while(1);
	}
}

static rt_uint16_t _smtmng_bao_tou(rt_uint8_t *pdata)
{
	rt_uint16_t i = 0;

	pdata[i++] = SMTMNG_FRAME_HEAD;
	pdata[i++] = SMTMNG_UUID_TYPE_STM32;
	*(rt_uint32_t *)(pdata + i)			= HAL_GetUIDw0();
	*((rt_uint32_t *)(pdata + i) + 1)	= HAL_GetUIDw1();
	*((rt_uint32_t *)(pdata + i) + 2)	= HAL_GetUIDw2();
	i += SMTMNG_BYTES_FRAME_UUID_VALUE;
	pdata[i++] = 0;
	pdata[i++] = 0;

	return i;
}

static void _smtmng_shell_encoder(PTCL_REPORT_DATA **report_data_ptr_ptr)
{
	PTCL_REPORT_DATA *psrc, *pdst;

	psrc = *report_data_ptr_ptr;
	if((PTCL_REPORT_DATA *)0 == psrc)
	{
		return;
	}
	pdst = ptcl_report_data_req(psrc->data_len + SMTMNG_BYTES_FRAME_HEAD + SMTMNG_BYTES_FRAME_END + SMTMNG_BYTES_FRAME_ELEMENT_TYPE + SMTMNG_BYTES_FRAME_ELEMENT_LEN, RT_WAITING_FOREVER);
	if((PTCL_REPORT_DATA *)0 == pdst)
	{
		rt_mp_free((void *)psrc);
		*report_data_ptr_ptr = pdst;
		return;
	}
	
	pdst->data_len = _smtmng_bao_tou(pdst->pdata);
	*(rt_uint16_t *)(pdst->pdata + pdst->data_len) = SMTMNG_ELEMENT_TYPE_SHELL_UP;
	pdst->data_len += sizeof(rt_uint16_t);
	*(rt_uint16_t *)(pdst->pdata + pdst->data_len) = psrc->data_len;
	pdst->data_len += sizeof(rt_uint16_t);
	memcpy((void *)(pdst->pdata + pdst->data_len), (void *)psrc->pdata, psrc->data_len);
	pdst->data_len += psrc->data_len;
	*(rt_uint16_t *)(pdst->pdata + SMTMNG_BYTES_FRAME_HEAD - sizeof(rt_uint16_t)) = pdst->data_len - SMTMNG_BYTES_FRAME_HEAD;
	pdst->pdata[pdst->data_len++] = SMTMNG_FRAME_CRC_VALUE;
	pdst->pdata[pdst->data_len++] = SMTMNG_FRAME_END;

	pdst->fun_csq_update	= (void *)0;
	pdst->data_id			= SMTMNG_ELEMENT_TYPE_SHELL_UP;
	pdst->need_reply		= RT_FALSE;
	pdst->fcb_value			= 0;
	pdst->ptcl_type			= PTCL_PTCL_TYPE_SMT_MNG;

	rt_mp_free((void *)psrc);
	*report_data_ptr_ptr = pdst;
}

static void _smtmng_shell_encoder_ota(PTCL_REPORT_DATA **report_data_ptr_ptr)
{
	PTCL_REPORT_DATA *psrc, *pdst;

	psrc = *report_data_ptr_ptr;
	if((PTCL_REPORT_DATA *)0 == psrc)
	{
		return;
	}
	pdst = ptcl_report_data_req(psrc->data_len + SMTMNG_BYTES_FRAME_HEAD + SMTMNG_BYTES_FRAME_END + SMTMNG_BYTES_FRAME_ELEMENT_TYPE + SMTMNG_BYTES_FRAME_ELEMENT_LEN, RT_WAITING_FOREVER);
	if((PTCL_REPORT_DATA *)0 == pdst)
	{
		rt_mp_free((void *)psrc);
		*report_data_ptr_ptr = pdst;
		return;
	}
	
	pdst->data_len = _smtmng_bao_tou(pdst->pdata);
	*(rt_uint16_t *)(pdst->pdata + pdst->data_len) = SMTMNG_ELEMENT_TYPE_OTA_UP;
	pdst->data_len += sizeof(rt_uint16_t);
	*(rt_uint16_t *)(pdst->pdata + pdst->data_len) = psrc->data_len;
	pdst->data_len += sizeof(rt_uint16_t);
	memcpy((void *)(pdst->pdata + pdst->data_len), (void *)psrc->pdata, psrc->data_len);
	pdst->data_len += psrc->data_len;
	*(rt_uint16_t *)(pdst->pdata + SMTMNG_BYTES_FRAME_HEAD - sizeof(rt_uint16_t)) = pdst->data_len - SMTMNG_BYTES_FRAME_HEAD;
	pdst->pdata[pdst->data_len++] = SMTMNG_FRAME_CRC_VALUE;
	pdst->pdata[pdst->data_len++] = SMTMNG_FRAME_END;

	pdst->fun_csq_update	= (void *)0;
	pdst->data_id			= SMTMNG_ELEMENT_TYPE_SHELL_UP;
	pdst->need_reply		= RT_FALSE;
	pdst->fcb_value 		= 0;
	pdst->ptcl_type 		= PTCL_PTCL_TYPE_SMT_MNG;

	rt_mp_free((void *)psrc);
	*report_data_ptr_ptr = pdst;
}

static void _smtmng_csq_update(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t csq_value)
{
	rt_uint16_t i = 0;

	if((i + SMTMNG_BYTES_FRAME_HEAD + SMTMNG_BYTES_FRAME_END) >= data_len)
	{
		return;
	}

	if(SMTMNG_FRAME_HEAD != pdata[i])
	{
		return;
	}
	i = data_len - SMTMNG_BYTES_FRAME_END;
	if(SMTMNG_FRAME_CRC_VALUE != pdata[i++])
	{
		return;
	}
	if(SMTMNG_FRAME_END != pdata[i])
	{
		return;
	}

	i = SMTMNG_BYTES_FRAME_HEAD;
	data_len -= SMTMNG_BYTES_FRAME_END;
	while((i + SMTMNG_BYTES_FRAME_ELEMENT_TYPE + SMTMNG_BYTES_FRAME_ELEMENT_LEN + SMTMNG_ELEMENT_LEN_GSM_CSQ) <= data_len)
	{
		if((SMTMNG_ELEMENT_TYPE_GSM_CSQ == *(rt_uint16_t *)(pdata + i)) && (SMTMNG_ELEMENT_LEN_GSM_CSQ == *(rt_uint16_t *)(pdata + i + 2)))
		{
			i += (SMTMNG_BYTES_FRAME_ELEMENT_TYPE + SMTMNG_BYTES_FRAME_ELEMENT_LEN);
			pdata[i] = csq_value * 3;
			break;
		}
		else
		{
			i++;
		}
	}
}

static rt_uint16_t _smtmng_encode_rtu_addr(rt_uint8_t *pdata, rt_uint16_t data_len)
{
	rt_uint16_t i = SMTMNG_BYTES_FRAME_ELEMENT_TYPE + SMTMNG_BYTES_FRAME_ELEMENT_LEN + SMTMNG_BYTES_FRAME_END, len;

	if(i >= data_len)
	{
		return 0;
	}
	data_len -= i;

	len = snprintf((char *)(pdata + SMTMNG_BYTES_FRAME_ELEMENT_TYPE + SMTMNG_BYTES_FRAME_ELEMENT_LEN), data_len, "smt-e100-001");
	if(len >= data_len)
	{
		return 0;
	}

	i = 0;
	*(rt_uint16_t *)(pdata + i) = SMTMNG_ELEMENT_TYPE_RTU_ADDR;
	i += SMTMNG_BYTES_FRAME_ELEMENT_TYPE;
	*(rt_uint16_t *)(pdata + i) = len;
	i += SMTMNG_BYTES_FRAME_ELEMENT_LEN;
	i += len;

	return i;
	
#if 0
	rt_uint16_t	i = SMTMNG_BYTES_FRAME_ELEMENT_TYPE + SMTMNG_BYTES_FRAME_ELEMENT_LEN + SMTMNG_BYTES_FRAME_END;
	rt_uint8_t	*pmem;

	i += SL427_BYTES_RTU_ADDR;
	if(i > max_len)
	{
		return 0;
	}
	pmem = (rt_uint8_t *)mempool_req(SL427_BYTES_RTU_ADDR, RT_WAITING_NO);
	if((rt_uint8_t *)0 == pmem)
	{
		return 0;
	}
	sl427_get_rtu_addr(pmem);

	i = 0;
	*(rt_uint16_t *)(pdata + i) = SMTMNG_ELEMENT_TYPE_RTU_ADDR;
	i += SMTMNG_BYTES_FRAME_ELEMENT_TYPE;
	
	max_len = 0;
	if(0 == pmem[max_len])
	{
		*(rt_uint16_t *)(pdata + i) = 8;
		i += SMTMNG_BYTES_FRAME_ELEMENT_LEN;
		max_len++;
		while(max_len < 5)
		{
			i += rt_sprintf((char *)(pdata + i), "%02x", pmem[max_len++]);
		}
	}
	else
	{
		i += SMTMNG_BYTES_FRAME_ELEMENT_LEN;
		for(; max_len < SL427_BYTES_RTU_ADDR; max_len++)
		{
			if(0 == pmem[max_len])
			{
				break;
			}
			pdata[i++] = pmem[max_len];
		}
		*(rt_uint16_t *)(pdata + i - max_len - SMTMNG_BYTES_FRAME_ELEMENT_LEN) = max_len;
	}
	rt_mp_free((void *)pmem);

	return i;
#endif
}

static rt_uint16_t _smtmng_encode_software(rt_uint8_t *pdata, char const *psoft, rt_uint16_t data_len)
{
	rt_uint16_t i = SMTMNG_BYTES_FRAME_ELEMENT_TYPE + SMTMNG_BYTES_FRAME_ELEMENT_LEN + SMTMNG_BYTES_FRAME_END, len;

	if(i >= data_len)
	{
		return 0;
	}
	data_len -= i;

	len = snprintf((char *)(pdata + SMTMNG_BYTES_FRAME_ELEMENT_TYPE + SMTMNG_BYTES_FRAME_ELEMENT_LEN), data_len, "%s", psoft);
	if(len >= data_len)
	{
		return 0;
	}

	i = 0;
	*(rt_uint16_t *)(pdata + i) = SMTMNG_ELEMENT_TYPE_SOFTWARE_EDITION;
	i += SMTMNG_BYTES_FRAME_ELEMENT_TYPE;
	*(rt_uint16_t *)(pdata + i) = len;
	i += SMTMNG_BYTES_FRAME_ELEMENT_LEN;
	i += len;

	return i;
}

static rt_uint16_t _smtmng_encode_run_time(rt_uint8_t *pdata, rt_uint32_t value, rt_uint16_t data_len)
{
	rt_uint16_t i = SMTMNG_BYTES_FRAME_ELEMENT_TYPE + SMTMNG_BYTES_FRAME_ELEMENT_LEN + SMTMNG_ELEMENT_LEN_RUN_TIME + SMTMNG_BYTES_FRAME_END;

	if(i > data_len)
	{
		return 0;
	}

	i = 0;
	*(rt_uint16_t *)(pdata + i) = SMTMNG_ELEMENT_TYPE_RUN_TIME;
	i += SMTMNG_BYTES_FRAME_ELEMENT_TYPE;
	*(rt_uint16_t *)(pdata + i) = SMTMNG_ELEMENT_LEN_RUN_TIME;
	i += SMTMNG_BYTES_FRAME_ELEMENT_LEN;
	*(rt_uint32_t *)(pdata + i) = value;
	i += SMTMNG_ELEMENT_LEN_RUN_TIME;

	return i;
}

static rt_uint16_t _smtmng_encode_supply_volt(rt_uint8_t *pdata, rt_uint16_t value, rt_uint16_t data_len)
{
	rt_uint16_t i = SMTMNG_BYTES_FRAME_ELEMENT_TYPE + SMTMNG_BYTES_FRAME_ELEMENT_LEN + SMTMNG_ELEMENT_LEN_SUPPLY_VOLT + SMTMNG_BYTES_FRAME_END;

	if(i > data_len)
	{
		return 0;
	}

	i = 0;
	*(rt_uint16_t *)(pdata + i) = SMTMNG_ELEMENT_TYPE_SUPPLY_VOLT;
	i += SMTMNG_BYTES_FRAME_ELEMENT_TYPE;
	*(rt_uint16_t *)(pdata + i) = SMTMNG_ELEMENT_LEN_SUPPLY_VOLT;
	i += SMTMNG_BYTES_FRAME_ELEMENT_LEN;
	*(rt_uint16_t *)(pdata + i) = value;
	i += SMTMNG_ELEMENT_LEN_SUPPLY_VOLT;

	return i;
}

static rt_uint16_t _smtmng_encode_solar_volt(rt_uint8_t *pdata, rt_uint16_t value, rt_uint16_t data_len)
{
	rt_uint16_t i = SMTMNG_BYTES_FRAME_ELEMENT_TYPE + SMTMNG_BYTES_FRAME_ELEMENT_LEN + SMTMNG_ELEMENT_LEN_SOLAR_VOLT + SMTMNG_BYTES_FRAME_END;

	if(i > data_len)
	{
		return 0;
	}

	i = 0;
	*(rt_uint16_t *)(pdata + i) = SMTMNG_ELEMENT_TYPE_SOLAR_VOLT;
	i += SMTMNG_BYTES_FRAME_ELEMENT_TYPE;
	*(rt_uint16_t *)(pdata + i) = SMTMNG_ELEMENT_LEN_SOLAR_VOLT;
	i += SMTMNG_BYTES_FRAME_ELEMENT_LEN;
	*(rt_uint16_t *)(pdata + i) = value;
	i += SMTMNG_ELEMENT_LEN_SOLAR_VOLT;

	return i;
}

static rt_uint16_t _smtmng_encode_gsm_csq(rt_uint8_t *pdata, rt_uint8_t value, rt_uint16_t data_len)
{
	rt_uint16_t i = SMTMNG_BYTES_FRAME_ELEMENT_TYPE + SMTMNG_BYTES_FRAME_ELEMENT_LEN + SMTMNG_ELEMENT_LEN_GSM_CSQ + SMTMNG_BYTES_FRAME_END;

	if(i > data_len)
	{
		return 0;
	}

	i = 0;
	*(rt_uint16_t *)(pdata + i) = SMTMNG_ELEMENT_TYPE_GSM_CSQ;
	i += SMTMNG_BYTES_FRAME_ELEMENT_TYPE;
	*(rt_uint16_t *)(pdata + i) = SMTMNG_ELEMENT_LEN_GSM_CSQ;
	i += SMTMNG_BYTES_FRAME_ELEMENT_LEN;
	pdata[i++] = value;

	return i;
}

static void _smtmng_task_time_tick(void *parg)
{
	rt_uint16_t			min, period;
	PTCL_REPORT_DATA	*report_data_ptr;
	struct tm			time;
	rt_uint8_t			flag = RT_TRUE;

	while(1)
	{
		tevent_pend(TEVENT_EVENT_MIN);
		lpm_cpu_ref(RT_TRUE);

		m_smtmng_run_time++;

		time = rtcee_rtc_get_calendar();
		min = time.tm_hour;
		min *= 60;
		min += time.tm_min;

		_smtmng_param_pend(SMTMNG_EVENT_PARAM_PERIOD_HEART);
		period	= m_smtmng_period_heart;
		_smtmng_param_post(SMTMNG_EVENT_PARAM_PERIOD_HEART);
		if(period)
		{
			if((0 == (min % period)) || (RT_TRUE == flag))
			{
				flag = RT_FALSE;
				//因为在这里发心跳是作为数据发送的，所以如果在连接状态下发送的话，会更新下线时间的，如果心跳间隔短的话会导致永不下线，这个是不希望的
				if(RT_FALSE == dtu_get_conn_sta(DTU_NUM_IP_CH - 1))
				{
					report_data_ptr = ptcl_report_data_req(SMTMNG_BYTES_FRAME_HEAD + SMTMNG_BYTES_FRAME_END + SMTMNG_BYTES_FRAME_ELEMENT_TYPE + SMTMNG_BYTES_FRAME_ELEMENT_LEN, RT_WAITING_NO);
					if((PTCL_REPORT_DATA *)0 != report_data_ptr)
					{
						report_data_ptr->data_len = _smtmng_bao_tou(report_data_ptr->pdata);
						*(rt_uint16_t *)(report_data_ptr->pdata + report_data_ptr->data_len) = SMTMNG_ELEMENT_TYPE_HEART_UP;
						report_data_ptr->data_len += sizeof(rt_uint16_t);
						*(rt_uint16_t *)(report_data_ptr->pdata + report_data_ptr->data_len) = SMTMNG_ELEMENT_LEN_HEART_UP;
						report_data_ptr->data_len += sizeof(rt_uint16_t);
						*(rt_uint16_t *)(report_data_ptr->pdata + SMTMNG_BYTES_FRAME_HEAD - sizeof(rt_uint16_t)) = report_data_ptr->data_len - SMTMNG_BYTES_FRAME_HEAD;
						report_data_ptr->pdata[report_data_ptr->data_len++] = SMTMNG_FRAME_CRC_VALUE;
						report_data_ptr->pdata[report_data_ptr->data_len++] = SMTMNG_FRAME_END;

						report_data_ptr->fun_csq_update	= (void *)0;
						report_data_ptr->data_id		= SMTMNG_ELEMENT_TYPE_HEART_DOWN;
						report_data_ptr->need_reply		= RT_FALSE;
						report_data_ptr->fcb_value		= 0;
						report_data_ptr->ptcl_type		= PTCL_PTCL_TYPE_SMT_MNG;

						ptcl_report_data_send(report_data_ptr, PTCL_COMM_TYPE_IP, DTU_NUM_IP_CH - 1, 0, 0, (rt_uint16_t *)0);

						rt_mp_free((void *)report_data_ptr);
					}
				}
			}
		}

		_smtmng_param_pend(SMTMNG_EVENT_PARAM_PERIOD_STA);
		period	= m_smtmng_period_sta;
		_smtmng_param_post(SMTMNG_EVENT_PARAM_PERIOD_STA);
		if(period)
		{
			if(0 == (min % period))
			{
				report_data_ptr = ptcl_report_data_req(SMTMNG_BYTES_STA_REPORT, RT_WAITING_NO);
				if((PTCL_REPORT_DATA *)0 != report_data_ptr)
				{
					while(1)
					{
						//包头
						report_data_ptr->data_len = 0;
						if((report_data_ptr->data_len + SMTMNG_BYTES_FRAME_HEAD) >= SMTMNG_BYTES_STA_REPORT)
						{
							break;
						}
						report_data_ptr->data_len += _smtmng_bao_tou(report_data_ptr->pdata + report_data_ptr->data_len);
						//正文
						while(1)
						{
							//测站编码
							report_data_ptr->data_len += _smtmng_encode_rtu_addr(report_data_ptr->pdata + report_data_ptr->data_len, SMTMNG_BYTES_STA_REPORT - report_data_ptr->data_len);
							//固件版本
							report_data_ptr->data_len += _smtmng_encode_software(report_data_ptr->pdata + report_data_ptr->data_len, PTCL_SOFTWARE_EDITION, SMTMNG_BYTES_STA_REPORT - report_data_ptr->data_len);
							//运行时间
							report_data_ptr->data_len += _smtmng_encode_run_time(report_data_ptr->pdata + report_data_ptr->data_len, m_smtmng_run_time / 60, SMTMNG_BYTES_STA_REPORT - report_data_ptr->data_len);
							//电池电压
							report_data_ptr->data_len += _smtmng_encode_supply_volt(report_data_ptr->pdata + report_data_ptr->data_len, sample_supply_volt(SAMPLE_PIN_ADC_BATT), SMTMNG_BYTES_STA_REPORT - report_data_ptr->data_len);
							//开关电源电压
							report_data_ptr->data_len += _smtmng_encode_solar_volt(report_data_ptr->pdata + report_data_ptr->data_len, sample_supply_volt(SAMPLE_PIN_ADC_VIN), SMTMNG_BYTES_STA_REPORT - report_data_ptr->data_len);
							//GSM信号强度
							report_data_ptr->data_len += _smtmng_encode_gsm_csq(report_data_ptr->pdata + report_data_ptr->data_len, 0, SMTMNG_BYTES_STA_REPORT - report_data_ptr->data_len);
							
							break;
						}
						//包尾
						if(report_data_ptr->data_len > SMTMNG_BYTES_FRAME_HEAD)
						{
							*(rt_uint16_t *)(report_data_ptr->pdata + SMTMNG_BYTES_FRAME_HEAD - sizeof(rt_uint16_t)) = report_data_ptr->data_len - SMTMNG_BYTES_FRAME_HEAD;
							report_data_ptr->pdata[report_data_ptr->data_len++] = SMTMNG_FRAME_CRC_VALUE;
							report_data_ptr->pdata[report_data_ptr->data_len++] = SMTMNG_FRAME_END;
							
							report_data_ptr->fun_csq_update	= (void *)_smtmng_csq_update;
							report_data_ptr->data_id		= 0;
							report_data_ptr->need_reply		= RT_FALSE;
							report_data_ptr->fcb_value		= 0;
							report_data_ptr->ptcl_type		= PTCL_PTCL_TYPE_SMT_MNG;
							
							ptcl_report_data_send(report_data_ptr, PTCL_COMM_TYPE_IP, DTU_NUM_IP_CH - 1, 0, 0, (rt_uint16_t *)0);
						}

						break;
					}

					rt_mp_free((void *)report_data_ptr);
				}
			}
		}

		_smtmng_param_pend(SMTMNG_EVENT_PARAM_PERIOD_TIMING);
		period	= m_smtmng_period_timing;
		_smtmng_param_post(SMTMNG_EVENT_PARAM_PERIOD_TIMING);
		if(period)
		{
			if(0 == (min % period))
			{
				report_data_ptr = ptcl_report_data_req(SMTMNG_BYTES_FRAME_HEAD + SMTMNG_BYTES_FRAME_END + SMTMNG_BYTES_FRAME_ELEMENT_TYPE + SMTMNG_BYTES_FRAME_ELEMENT_LEN, RT_WAITING_NO);
				if((PTCL_REPORT_DATA *)0 != report_data_ptr)
				{
					//包头
					report_data_ptr->data_len = _smtmng_bao_tou(report_data_ptr->pdata);
					//正文
					*(rt_uint16_t *)(report_data_ptr->pdata + report_data_ptr->data_len) = SMTMNG_ELEMENT_TYPE_TIMING_UP;
					report_data_ptr->data_len += SMTMNG_BYTES_FRAME_ELEMENT_TYPE;
					*(rt_uint16_t *)(report_data_ptr->pdata + report_data_ptr->data_len) = SMTMNG_ELEMENT_LEN_TIMING_UP;
					report_data_ptr->data_len += SMTMNG_BYTES_FRAME_ELEMENT_LEN;
					//包尾
					*(rt_uint16_t *)(report_data_ptr->pdata + SMTMNG_BYTES_FRAME_HEAD - sizeof(rt_uint16_t)) = report_data_ptr->data_len - SMTMNG_BYTES_FRAME_HEAD;
					report_data_ptr->pdata[report_data_ptr->data_len++] = SMTMNG_FRAME_CRC_VALUE;
					report_data_ptr->pdata[report_data_ptr->data_len++] = SMTMNG_FRAME_END;
					
					report_data_ptr->fun_csq_update	= (void *)0;
					report_data_ptr->data_id		= 0;
					report_data_ptr->need_reply		= RT_FALSE;
					report_data_ptr->fcb_value		= 0;
					report_data_ptr->ptcl_type		= PTCL_PTCL_TYPE_SMT_MNG;
					
					ptcl_report_data_send(report_data_ptr, PTCL_COMM_TYPE_IP, DTU_NUM_IP_CH - 1, 0, 0, (rt_uint16_t *)0);

					rt_mp_free((void *)report_data_ptr);
				}
			}
		}

		lpm_cpu_ref(RT_FALSE);
	}
}

static int _smtmng_component_init(void)
{
	if(RT_EOK != rt_event_init(&m_smtmng_event_module, "smtmng", RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	if(RT_EOK != rt_event_send(&m_smtmng_event_module, SMTMNG_EVENT_PARAM_ALL))
	{
		while(1);
	}

	if(RT_FALSE == _smtmng_get_param_set(&m_smtmng_param_set))
	{
		_smtmng_reset_param_set(&m_smtmng_param_set);
	}
	
	return 0;
}
INIT_COMPONENT_EXPORT(_smtmng_component_init);

static int _smtmng_app_init(void)
{
	if(RT_EOK != rt_thread_init(&m_smtmng_thread_time_tick, "smtmng", _smtmng_task_time_tick, (void *)0, (void *)m_smtmng_stk_time_tick, SMTMNG_STK_TIME_TICK, SMTMNG_PRIO_TIME_TICK, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_smtmng_thread_time_tick))
	{
		while(1);
	}
	
	return 0;
}
INIT_APP_EXPORT(_smtmng_app_init);



rt_uint16_t smtmng_get_sta_period(void)
{
	rt_uint16_t period;

	_smtmng_param_pend(SMTMNG_EVENT_PARAM_PERIOD_STA);
	period = m_smtmng_period_sta;
	_smtmng_param_post(SMTMNG_EVENT_PARAM_PERIOD_STA);

	return period;
}
void smtmng_set_sta_period(rt_uint16_t period)
{
	_smtmng_param_pend(SMTMNG_EVENT_PARAM_PERIOD_STA);
	if(period != m_smtmng_period_sta)
	{
		m_smtmng_period_sta = period;
		_smtmng_set_sta_period(period);
	}
	_smtmng_param_post(SMTMNG_EVENT_PARAM_PERIOD_STA);
}

rt_uint16_t smtmng_get_timing_period(void)
{
	rt_uint16_t period;

	_smtmng_param_pend(SMTMNG_EVENT_PARAM_PERIOD_TIMING);
	period = m_smtmng_period_timing;
	_smtmng_param_post(SMTMNG_EVENT_PARAM_PERIOD_TIMING);

	return period;
}
void smtmng_set_timing_period(rt_uint16_t period)
{
	_smtmng_param_pend(SMTMNG_EVENT_PARAM_PERIOD_TIMING);
	if(period != m_smtmng_period_timing)
	{
		m_smtmng_period_timing = period;
		_smtmng_set_timing_period(period);
	}
	_smtmng_param_post(SMTMNG_EVENT_PARAM_PERIOD_TIMING);
}

rt_uint16_t smtmng_get_heart_period(void)
{
	rt_uint16_t period;

	_smtmng_param_pend(SMTMNG_EVENT_PARAM_PERIOD_HEART);
	period = m_smtmng_period_heart;
	_smtmng_param_post(SMTMNG_EVENT_PARAM_PERIOD_HEART);

	return period;
}
void smtmng_set_heart_period(rt_uint16_t period)
{
	_smtmng_param_pend(SMTMNG_EVENT_PARAM_PERIOD_HEART);
	if(period != m_smtmng_period_heart)
	{
		m_smtmng_period_heart = period;
		_smtmng_set_heart_period(period);
	}
	_smtmng_param_post(SMTMNG_EVENT_PARAM_PERIOD_HEART);
}

void smtmng_param_restore(void)
{
	_smtmng_param_pend(SMTMNG_EVENT_PARAM_ALL);

	m_smtmng_period_heart	= SMTMNG_DEFAULT_PERIOD_HEART;
	m_smtmng_period_sta		= SMTMNG_DEFAULT_PERIOD_STA;
	m_smtmng_period_timing	= SMTMNG_DEFAULT_PERIOD_TIMING;

	_smtmng_set_param_set(&m_smtmng_param_set);
	
	_smtmng_param_post(SMTMNG_EVENT_PARAM_ALL);
}

rt_uint16_t smtmng_heart_encoder(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t ch)
{
	if(data_len < (SMTMNG_BYTES_FRAME_HEAD + SMTMNG_BYTES_FRAME_ELEMENT_TYPE + SMTMNG_BYTES_FRAME_ELEMENT_LEN + SMTMNG_BYTES_FRAME_END))
	{
		return 0;
	}

	data_len = _smtmng_bao_tou(pdata);
	*(rt_uint16_t *)(pdata + data_len) = SMTMNG_ELEMENT_TYPE_HEART_UP;
	data_len += sizeof(rt_uint16_t);
	*(rt_uint16_t *)(pdata + data_len) = SMTMNG_ELEMENT_LEN_HEART_UP;
	data_len += sizeof(rt_uint16_t);
	*(rt_uint16_t *)(pdata + SMTMNG_BYTES_FRAME_HEAD - sizeof(rt_uint16_t)) = data_len - SMTMNG_BYTES_FRAME_HEAD;
	pdata[data_len++] = SMTMNG_FRAME_CRC_VALUE;
	pdata[data_len++] = SMTMNG_FRAME_END;

	return data_len;
}

rt_uint8_t smtmng_data_decoder(PTCL_REPORT_DATA *report_data_ptr, PTCL_RECV_DATA const *recv_data_ptr, PTCL_PARAM_INFO **param_info_ptr_ptr)
{
	rt_uint16_t		i = 0, data_len, element_type, element_len;
	PTCL_PARAM_INFO	*param_info_ptr;
	PTCL_RECV_DATA	*recv_shell_ptr;

	//包头
	if((i + SMTMNG_BYTES_FRAME_HEAD) >= recv_data_ptr->data_len)
	{
		return RT_FALSE;
	}
	if(SMTMNG_FRAME_HEAD != recv_data_ptr->pdata[i++])
	{
		return RT_FALSE;
	}
	if(SMTMNG_UUID_TYPE_STM32 != recv_data_ptr->pdata[i++])
	{
		return RT_FALSE;
	}
	i += SMTMNG_BYTES_FRAME_UUID_VALUE;
	data_len = *(rt_uint16_t *)(recv_data_ptr->pdata + i);
	i += sizeof(rt_uint16_t);
	if((i + data_len + SMTMNG_BYTES_FRAME_END) != recv_data_ptr->data_len)
	{
		return RT_FALSE;
	}
	//包尾
	i += data_len;
	if(SMTMNG_FRAME_CRC_VALUE != recv_data_ptr->pdata[i++])
	{
		return RT_FALSE;
	}
	if(SMTMNG_FRAME_END != recv_data_ptr->pdata[i])
	{
		return RT_FALSE;
	}

	//确认为该协议报文
	report_data_ptr->data_len = 0;
	//解码
	i = SMTMNG_BYTES_FRAME_HEAD;
	data_len += SMTMNG_BYTES_FRAME_HEAD;
	while(i < data_len)
	{
		//标识符
		if((i + SMTMNG_BYTES_FRAME_ELEMENT_TYPE) >= data_len)
		{
			break;
		}
		element_type = *(rt_uint16_t *)(recv_data_ptr->pdata + i);
		i += SMTMNG_BYTES_FRAME_ELEMENT_TYPE;
		//长度判断，分两种情况，平台命令下行帧中标识符后面无长度要素，直接就是数据
		if(SMTMNG_ELEMENT_TYPE_CMD_DOWN == element_type)
		{
			element_len = data_len - i;
		}
		else
		{
			//长度
			if((i + SMTMNG_BYTES_FRAME_ELEMENT_LEN) > data_len)
			{
				break;
			}
			element_len = *(rt_uint16_t *)(recv_data_ptr->pdata + i);
			i += SMTMNG_BYTES_FRAME_ELEMENT_LEN;
			//保证后面有这么多的数据
			if((i + element_len) > data_len)
			{
				break;
			}
		}

		switch(element_type)
		{
		//心跳
		case SMTMNG_ELEMENT_TYPE_HEART_DOWN:
			if((0 == element_len) && (PTCL_COMM_TYPE_IP == recv_data_ptr->comm_type))
			{
				dtu_hold_enable(DTU_COMM_TYPE_IP, recv_data_ptr->ch, RT_TRUE);
			}
			break;
		//校时
		case SMTMNG_ELEMENT_TYPE_TIMING_DOWN:
			if(SMTMNG_ELEMENT_LEN_TIMING_DOWN == element_len)
			{
				if(RT_TRUE == fyyp_is_bcdcode(recv_data_ptr->pdata + i, element_len))
				{
					param_info_ptr = ptcl_param_info_req(sizeof(struct tm), RT_WAITING_NO);
					if((PTCL_PARAM_INFO *)0 != param_info_ptr)
					{
						param_info_ptr->param_type = PTCL_PARAM_INFO_TT;
						element_type = i;
						((struct tm *)(param_info_ptr->pdata))->tm_year		= fyyp_bcd_to_hex(recv_data_ptr->pdata[element_type++]) + 2000;
						((struct tm *)(param_info_ptr->pdata))->tm_mon		= fyyp_bcd_to_hex(recv_data_ptr->pdata[element_type++]) - 1;
						((struct tm *)(param_info_ptr->pdata))->tm_mday		= fyyp_bcd_to_hex(recv_data_ptr->pdata[element_type++]);
						((struct tm *)(param_info_ptr->pdata))->tm_hour		= fyyp_bcd_to_hex(recv_data_ptr->pdata[element_type++]);
						((struct tm *)(param_info_ptr->pdata))->tm_min		= fyyp_bcd_to_hex(recv_data_ptr->pdata[element_type++]);
						((struct tm *)(param_info_ptr->pdata))->tm_sec		= fyyp_bcd_to_hex(recv_data_ptr->pdata[element_type]);
						*(struct tm *)(param_info_ptr->pdata) = rtcee_rtc_unix_to_calendar(rtcee_rtc_calendar_to_unix(*(struct tm *)(param_info_ptr->pdata)));
						ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
					}
				}
			}
			break;
		//平台命令
		case SMTMNG_ELEMENT_TYPE_CMD_DOWN:
			if((element_len == strlen(SMTMNG_CMD_OTA_START)) && (0 == memcmp((void *)(recv_data_ptr->pdata + i), (void *)SMTMNG_CMD_OTA_START, element_len)))
			{
				xm_file_trans_trigger(recv_data_ptr->comm_type, recv_data_ptr->ch, XM_DIR_FROM_PLATFORM, XM_FILE_TYPE_FIRMWARE, _smtmng_shell_encoder_ota);
			}
			break;
		//固件升级
		case SMTMNG_ELEMENT_TYPE_OTA_DOWN:
		//壳子包
		case SMTMNG_ELEMENT_TYPE_SHELL_DOWN:
			if(element_len)
			{
				recv_shell_ptr = ptcl_recv_data_req(element_len + sizeof(void *), RT_WAITING_NO);
				if((PTCL_RECV_DATA *)0 != recv_shell_ptr)
				{
					recv_shell_ptr->comm_type	= recv_data_ptr->comm_type;
					recv_shell_ptr->ch			= recv_data_ptr->ch;
					recv_shell_ptr->data_len	= element_len;
					memcpy((void *)recv_shell_ptr->pdata, (void *)(recv_data_ptr->pdata + i), element_len);
					*(void **)(recv_shell_ptr->pdata + element_len) = (SMTMNG_ELEMENT_TYPE_SHELL_DOWN == element_type) ? (void *)_smtmng_shell_encoder : (void *)_smtmng_shell_encoder_ota;
//					*(void **)(recv_shell_ptr->pdata + element_len) = (void *)_smtmng_shell_encoder;

					if(RT_FALSE == ptcl_shell_data_post(recv_shell_ptr))
					{
						rt_mp_free((void *)recv_shell_ptr);
					}
				}
			}
			break;
		}
		i += element_len;
	}

	return RT_TRUE;
}

