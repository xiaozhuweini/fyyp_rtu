/*BOOT流程
**ATE0
**AT+CPIN?
**AT+CREG?
**AT+QURCCFG="urcport","uart1"
**AT+CLIP=1
**AT+CMGF=0
**AT+CNMI=2,2,0,0,0

**AT+QICSGP
**AT+QIDEACT
**AT+QIACT
**AT+QIACT?

**AT+QICLOSE
**AT+QIOPEN
**AT+QISEND
*/



#include "stdio.h"



//超时时间，秒
#define DTU_TIME_PWR_OFF			15						//断电
#define DTU_TIME_PWR_ON				3						//上电
#define DTU_TIME_STA_OFF			3						//关机
#define DTU_TIME_STA_ON				30						//开机
#define DTU_TIME_BASIC				3						//BASIC
#define DTU_TIME_CPIN				5						//CPIN
#define DTU_TIME_CLIP				15						//CLIP
#define DTU_TIME_CMGS				120						//CMGS
#define DTU_TIME_QIDEACT			40						//QIDEACT
#define DTU_TIME_QIACT				150						//QIACT
#define DTU_TIME_QICLOSE			10						//QICLOSE
#define DTU_TIME_QIOPEN				150						//QIOPEN
#define DTU_TIME_HANGUP				90						//HANGUP
#define DTU_TIME_ATA				90						//ATA
#define DTU_TIME_ATD				5						//ATD
#define DTU_TIME_IP_SEND			10						//ip发送超时时间

//字节空间
#define DTU_BYTES_AT_CMD			60						//AT发送指令
#define DTU_BYTES_PDU_DATA			350						//PDU短信数据

//事件标志
#define DTU_EVENT_NONE_YES			0x01
#define DTU_EVENT_NONE_NO			0x02
#define DTU_EVENT_OK				0x04					//OK
#define DTU_EVENT_ERROR				0x08					//ERROR
#define DTU_EVENT_CPIN_RDY			0x10					//CPIN READY
#define DTU_EVENT_CREG_RDY			0x20					//CREG READY
#define DTU_EVENT_CONN_OK			0x40					//CONNECT OK
#define DTU_EVENT_CONN_FAIL			0x80					//CONNECT FAIL
#define DTU_EVENT_ARROW				0x100					//>
#define DTU_EVENT_SEND_OK			0x200					//SEND OK
#define DTU_EVENT_SEND_FAIL			0x400					//SEND FAIL
#define DTU_EVENT_SEND_ACKED		0x800					//SEND ACKED
#define DTU_EVENT_SEND_UNACKED		0x1000					//SEND UNACKED
#define DTU_EVENT_CMGS				0x2000					//CMGS
#define DTU_EVENT_CSQ				0x4000					//CSQ
#define DTU_EVENT_NO_CARRIER		0x8000					//NO CARRIER
#define DTU_EVENT_QTTS_4111			0x10000
#define DTU_EVENT_INCOMING_PHONE	0x20000					//有来电需要接通
#define DTU_EVENT_QTTS_START		0x40000					//有TTS需要进行



static rt_uint8_t m_dtu_at_cmd_data[DTU_BYTES_AT_CMD], m_dtu_at_cmd_len;



static rt_uint8_t _dtu_at_cmd_qicsgp(rt_uint8_t *pcmd, DTU_APN const *papn)
{
	rt_uint8_t cmd_len;
	
	cmd_len = sprintf((char *)pcmd, "AT+QICSGP=1,1,\"");
	memcpy(pcmd + cmd_len, papn->apn_data, papn->apn_len);
	cmd_len += papn->apn_len;
	pcmd[cmd_len++] = '\"';
	pcmd[cmd_len++] = '\r';
	
	return cmd_len;
}

static rt_uint8_t _dtu_at_cmd_close_ip(rt_uint8_t *pcmd, rt_uint8_t ch)
{
	return sprintf((char *)pcmd, "AT+QICLOSE=%d\r", ch);
}

static rt_uint8_t _dtu_at_cmd_connect_ip(rt_uint8_t *pcmd, rt_uint8_t ch, DTU_IP_ADDR const *ip_addr_ptr, rt_uint8_t is_udp)
{
	rt_uint8_t cmd_len, i;
	
	if(RT_TRUE == is_udp)
	{
		cmd_len = sprintf((char *)pcmd, "AT+QIOPEN=1,%d,\"UDP\",\"", ch);
	}
	else
	{
		cmd_len = sprintf((char *)pcmd, "AT+QIOPEN=1,%d,\"TCP\",\"", ch);
	}
	for(i = 0; i < ip_addr_ptr->addr_len; i++)
	{
		if(':' == ip_addr_ptr->addr_data[i])
		{
			pcmd[cmd_len++] = '\"';
			pcmd[cmd_len++] = ',';
		}
		else
		{
			pcmd[cmd_len++] = ip_addr_ptr->addr_data[i];
		}
	}
	pcmd[cmd_len++] = ',';
	pcmd[cmd_len++] = '0';
	pcmd[cmd_len++] = ',';
	pcmd[cmd_len++] = '1';
	pcmd[cmd_len++] = '\r';
	
	return cmd_len;
}

static rt_uint8_t _dtu_at_cmd_send_ip(rt_uint8_t *pcmd, rt_uint8_t ch, rt_uint16_t data_len)
{
	return sprintf((char *)pcmd, "AT+QISEND=%d,%d\r", ch, data_len);
}

#ifdef DTU_SMS_MODE_PDU
static rt_uint8_t _dtu_at_cmd_send_sms(rt_uint8_t *pcmd, rt_uint16_t data_len)
{
	return sprintf((char *)pcmd, "AT+CMGS=%d\r", data_len);
}
#endif
#ifdef DTU_SMS_MODE_TEXT
static rt_uint8_t _dtu_at_cmd_send_sms(rt_uint8_t *pcmd, DTU_SMS_ADDR const *sms_addr_ptr)
{
	rt_uint8_t cmd_len;

	if(0 == sms_addr_ptr->addr_len)
	{
		return 0;
	}
	
	cmd_len = sprintf((char *)pcmd, "AT+CMGS=\"");
	memcpy(pcmd + cmd_len, sms_addr_ptr->addr_data, sms_addr_ptr->addr_len);
	cmd_len += sms_addr_ptr->addr_len;
	pcmd[cmd_len++] = '\"';
	pcmd[cmd_len++] = '\r';

	return cmd_len;
}
#endif

static rt_uint8_t _dtu_at_cmd_dial_phone(rt_uint8_t *pcmd, DTU_SMS_ADDR const *sms_addr_ptr)
{
	rt_uint8_t cmd_len;

	if(0 == sms_addr_ptr->addr_len)
	{
		return 0;
	}

	cmd_len = sprintf((char *)pcmd, "ATD");
	memcpy((void *)(pcmd + cmd_len), (void *)sms_addr_ptr->addr_data, sms_addr_ptr->addr_len);
	cmd_len += sms_addr_ptr->addr_len;
	pcmd[cmd_len++] = ';';
	pcmd[cmd_len++] = '\r';

	return cmd_len;
}

static rt_uint8_t _dtu_at_excute_cmd(rt_uint8_t const *pcmd, rt_uint16_t cmd_len, rt_uint32_t yes, rt_uint32_t no, rt_uint8_t err_times, rt_uint8_t timeout_times, rt_uint32_t timeout, rt_uint32_t wait_time)
{
	rt_uint32_t events_recv;
	
	while(1)
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n"));
		DEBUG_INFO_OUTPUT(DEBUG_INFO_TYPE_DTU, (pcmd, cmd_len));
		//清除事件
		rt_event_recv(&m_dtu_event_module, yes + no, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, RT_WAITING_NO, (rt_uint32_t *)0);
		//发送指令
		rt_thread_delay(RT_TICK_PER_SECOND / 2);
		_dtu_send_to_module(pcmd, cmd_len);
		//判断事件
		events_recv = 0;
		rt_event_recv(&m_dtu_event_module, yes + no, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, timeout * RT_TICK_PER_SECOND, &events_recv);
		if(events_recv & yes)
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n执行成功"));
			return RT_TRUE;
		}
		else if(events_recv & no)
		{
			if(err_times)
			{
				err_times--;
			}
			if(err_times)
			{
				rt_thread_delay(wait_time * RT_TICK_PER_SECOND);
			}
			else
			{
				DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n执行错误"));
				return RT_FALSE;
			}
		}
		else
		{
			if(timeout_times)
			{
				timeout_times--;
			}
			if(0 == timeout_times)
			{
				DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n执行超时"));
				return RT_FALSE;
			}
		}
	}
}

static rt_uint8_t _dtu_at_excute_cmd_ex(rt_uint8_t const *pcmd, rt_uint16_t cmd_len, rt_uint32_t yes, rt_uint32_t no, rt_uint8_t err_times, rt_uint8_t timeout_times, rt_uint32_t timeout, rt_uint32_t wait_time)
{
	rt_uint32_t events_recv;
	
	while(1)
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n"));
		DEBUG_INFO_OUTPUT(DEBUG_INFO_TYPE_DTU, (pcmd, cmd_len));
		//清除事件
		rt_event_recv(&m_dtu_event_module, yes + no, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, RT_WAITING_NO, (rt_uint32_t *)0);
		//发送指令
		rt_thread_delay(RT_TICK_PER_SECOND / 2);
		_dtu_send_to_module(pcmd, cmd_len);
		//判断事件
		events_recv = 0;
		rt_event_recv(&m_dtu_event_module, yes + no + DTU_EVENT_INCOMING_PHONE + DTU_EVENT_QTTS_START, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, timeout * RT_TICK_PER_SECOND, &events_recv);
		if(events_recv & yes)
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n执行成功"));
			return 0;
		}
		else if(events_recv & (DTU_EVENT_INCOMING_PHONE + DTU_EVENT_QTTS_START))
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n有来电需要处理或有TTS需要进行"));
			return 1;
		}
		else if(events_recv & no)
		{
			if(err_times)
			{
				err_times--;
			}
			if(err_times)
			{
				rt_thread_delay(wait_time * RT_TICK_PER_SECOND);
			}
			else
			{
				DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n执行错误"));
				return 2;
			}
		}
		else
		{
			if(timeout_times)
			{
				timeout_times--;
			}
			if(0 == timeout_times)
			{
				DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n执行超时"));
				return 3;
			}
		}
	}
}

static rt_uint8_t _dtu_at_close_ip(rt_uint8_t ch)
{
	m_dtu_at_cmd_len = _dtu_at_cmd_close_ip(m_dtu_at_cmd_data, ch);
	return _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_QICLOSE, 2);
}

static rt_uint8_t _dtu_at_connect_ip(rt_uint8_t ch)
{
	//生成断开连接指令
	m_dtu_at_cmd_len = _dtu_at_cmd_close_ip(m_dtu_at_cmd_data, ch);
	//执行断开连接指令
	_dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 1, 2, DTU_TIME_QICLOSE, 2);
	//生成连接指令
	_dtu_param_pend((DTU_PARAM_IP_ADDR << ch) + DTU_PARAM_IP_TYPE);
	m_dtu_at_cmd_len = _dtu_at_cmd_connect_ip(m_dtu_at_cmd_data, ch, m_dtu_ip_addr + ch, (m_dtu_ip_type & (1 << ch)) ? RT_TRUE : RT_FALSE);
	_dtu_param_post((DTU_PARAM_IP_ADDR << ch) + DTU_PARAM_IP_TYPE);
	//执行连接指令
	return ((0 == _dtu_at_excute_cmd_ex(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_CONN_OK, DTU_EVENT_CONN_FAIL, 1, 1, DTU_TIME_QIOPEN, 2)) ? RT_TRUE : RT_FALSE);
}

static rt_uint8_t _dtu_at_connect_http(DTU_IP_ADDR const *ip_addr_ptr)
{
	//生成断开连接指令
	m_dtu_at_cmd_len = _dtu_at_cmd_close_ip(m_dtu_at_cmd_data, DTU_NUM_IP_CH);
	//执行断开连接指令
	_dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 1, 2, DTU_TIME_QICLOSE, 2);
	//生成连接指令
	m_dtu_at_cmd_len = _dtu_at_cmd_connect_ip(m_dtu_at_cmd_data, DTU_NUM_IP_CH, ip_addr_ptr, RT_FALSE);
	//执行连接指令
	return ((0 == _dtu_at_excute_cmd_ex(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_CONN_OK, DTU_EVENT_CONN_FAIL, 1, 1, DTU_TIME_QIOPEN, 2)) ? RT_TRUE : RT_FALSE);
}

static rt_uint8_t _dtu_at_send_ip(rt_uint8_t ch, rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	if(0 == data_len)
	{
		return RT_FALSE;
	}
	
	//生成发送指令
	m_dtu_at_cmd_len = _dtu_at_cmd_send_ip(m_dtu_at_cmd_data, ch, data_len);
	//执行发送指令
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_ARROW, DTU_EVENT_ERROR, 1, 1, DTU_TIME_BASIC, 2))
	{
		return RT_FALSE;
	}

	return _dtu_at_excute_cmd(pdata, data_len, DTU_EVENT_SEND_OK, DTU_EVENT_SEND_FAIL + DTU_EVENT_ERROR, 1, 1, DTU_TIME_IP_SEND, 2);
}

static rt_uint8_t _dtu_at_send_http(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	return _dtu_at_send_ip(DTU_NUM_IP_CH, pdata, data_len);
}

#ifdef DTU_SMS_MODE_PDU
static rt_uint8_t _dtu_at_send_sms(rt_uint8_t ch, rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint8_t		*pmem, sta;
	rt_uint16_t		pdu_len;

	if(ch >= (DTU_NUM_SMS_CH + DTU_NUM_SUPER_PHONE))
	{
		return RT_FALSE;
	}
	if(0 == data_len)
	{
		return RT_FALSE;
	}
	
	//申请内存
	pmem = (rt_uint8_t *)mempool_req(data_len * 2 + 50, RT_WAITING_FOREVER);
	if((rt_uint8_t *)0 == pmem)
	{
		return RT_FALSE;
	}
	//PDU编码
	if(ch < DTU_NUM_SMS_CH)
	{
		_dtu_param_pend(DTU_PARAM_SMS_ADDR << ch);
		pdu_len = _pdu_encoder(pmem, m_dtu_sms_addr + ch, pdata, data_len);
		_dtu_param_post(DTU_PARAM_SMS_ADDR << ch);
	}
	else
	{
		ch -= DTU_NUM_SMS_CH;
		_dtu_param_pend(DTU_PARAM_SUPER_PHONE);
		pdu_len = _pdu_encoder(pmem, m_dtu_super_phone + ch, pdata, data_len);
		_dtu_param_post(DTU_PARAM_SUPER_PHONE);
	}
	if(0 == pdu_len)
	{
		//释放内存
		rt_mp_free((void *)pmem);
		return RT_FALSE;
	}
	//生成发送指令
	m_dtu_at_cmd_len = _dtu_at_cmd_send_sms(m_dtu_at_cmd_data, pdu_len / 2 - 1);
	//执行发送指令
	sta = _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_ARROW, DTU_EVENT_ERROR, 1, 1, DTU_TIME_BASIC, 2);
	if(RT_FALSE == sta)
	{
		//释放内存
		rt_mp_free((void *)pmem);
		return RT_FALSE;
	}
	//发送数据
	pmem[pdu_len++] = 0x1a;
	sta = _dtu_at_excute_cmd(pmem, pdu_len, DTU_EVENT_CMGS, DTU_EVENT_ERROR, 1, 1, DTU_TIME_CMGS, 2);
	//释放内存
	rt_mp_free((void *)pmem);
	
	return sta;
}
#endif
#ifdef DTU_SMS_MODE_TEXT
static rt_uint8_t _dtu_at_send_sms(rt_uint8_t ch, rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint8_t *pmem, sta;
	
	if(ch >= (DTU_NUM_SMS_CH + DTU_NUM_SUPER_PHONE))
	{
		return RT_FALSE;
	}
	if(0 == data_len)
	{
		return RT_FALSE;
	}

	//申请内存
	pmem = (rt_uint8_t *)mempool_req(data_len + 1, RT_WAITING_FOREVER);
	if((rt_uint8_t *)0 == pmem)
	{
		return RT_FALSE;
	}
	//text编码
	memcpy((void *)pmem, (void *)pdata, data_len);
	pmem[data_len++] = 0x1a;
	//生成发送指令
	if(ch < DTU_NUM_SMS_CH)
	{
		_dtu_param_pend(DTU_PARAM_SMS_ADDR << ch);
		m_dtu_at_cmd_len = _dtu_at_cmd_send_sms(m_dtu_at_cmd_data, m_dtu_sms_addr + ch);
		_dtu_param_post(DTU_PARAM_SMS_ADDR << ch);
	}
	else
	{
		ch -= DTU_NUM_SMS_CH;
		_dtu_param_pend(DTU_PARAM_SUPER_PHONE);
		m_dtu_at_cmd_len = _dtu_at_cmd_send_sms(m_dtu_at_cmd_data, m_dtu_super_phone + ch);
		_dtu_param_post(DTU_PARAM_SUPER_PHONE);
	}
	if(0 == m_dtu_at_cmd_len)
	{
		rt_mp_free((void *)pmem);
		return RT_FALSE;
	}
	//执行发送指令
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_ARROW, DTU_EVENT_ERROR, 1, 1, DTU_TIME_BASIC, 2))
	{
		rt_mp_free((void *)pmem);
		return RT_FALSE;
	}
	//发送数据
	sta = _dtu_at_excute_cmd(pmem, data_len, DTU_EVENT_CMGS, DTU_EVENT_ERROR, 1, 1, DTU_TIME_CMGS, 2);
	//释放内存
	rt_mp_free((void *)pmem);

	return sta;
}
#endif

static rt_uint8_t _dtu_at_boot_sms(void)
{
	//ATE0
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "ATE0\r");
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_BASIC, 2))
	{
		return RT_FALSE;
	}
	//AT+CPIN?
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CPIN?\r");
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_CPIN_RDY, DTU_EVENT_ERROR, 3, 3, DTU_TIME_CPIN, 2))
	{
		return RT_FALSE;
	}
	//AT+CREG?
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CREG?\r");
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_CREG_RDY, DTU_EVENT_ERROR, 3, 50, DTU_TIME_BASIC, 2))
	{
		return RT_FALSE;
	}
	//AT+QURCCFG="urcport","uart1"
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+QURCCFG=\"urcport\",\"uart1\"\r");
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_BASIC, 2))
	{
		return RT_FALSE;
	}
	//AT+CLIP=1
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CLIP=1\r");
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_CLIP, 2))
	{
		return RT_FALSE;
	}
#ifdef DTU_SMS_MODE_PDU
	//AT+CMGF=0
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CMGF=0\r");
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_BASIC, 2))
	{
		return RT_FALSE;
	}
#endif
#ifdef DTU_SMS_MODE_TEXT
	//AT+CMGF=1
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CMGF=1\r");
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_BASIC, 2))
	{
		return RT_FALSE;
	}
	//AT+CSDH=1
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CSDH=1\r");
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_BASIC, 2))
	{
		return RT_FALSE;
	}
#endif
	//AT+CNMI=2,2,0,0,0
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CNMI=2,2,0,0,0\r");
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_BASIC, 2))
	{
		return RT_FALSE;
	}
	
	return RT_TRUE;
}

static rt_uint8_t _dtu_at_boot_ip(void)
{
	rt_uint8_t sta;
	
	//AT+QICSGP
	_dtu_param_pend(DTU_PARAM_APN);
	m_dtu_at_cmd_len = _dtu_at_cmd_qicsgp(m_dtu_at_cmd_data, &m_dtu_apn);
	_dtu_param_post(DTU_PARAM_APN);
	sta = _dtu_at_excute_cmd_ex(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_BASIC, 2);
	if(sta)
	{
		return sta;
	}
	//AT+QIDEACT
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+QIDEACT=1\r");
	sta = _dtu_at_excute_cmd_ex(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_QIDEACT, 2);
	if(sta)
	{
		return sta;
	}
	//AT+QIACT
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+QIACT=1\r");
	sta = _dtu_at_excute_cmd_ex(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_QIACT, 2);
	if(sta)
	{
		return sta;
	}
	//AT+QIACT?
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+QIACT?\r");
	sta = _dtu_at_excute_cmd_ex(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_BASIC, 2);
	if(sta)
	{
		return sta;
	}
	
	return 0;
}

static rt_uint8_t _dtu_at_csq_update(void)
{
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CSQ\r");
	return _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_CSQ, DTU_EVENT_NONE_NO, 3, 3, DTU_TIME_BASIC, 2);
}

static rt_uint8_t _dtu_at_close_net(void)
{
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+QIDEACT=1\r");
	return _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_QIDEACT, 2);
}

static rt_uint8_t _dtu_at_hang_up(void)
{
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CHUP\r");
	return _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_HANGUP, 2);
}

static rt_uint8_t _dtu_at_ata(void)
{
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "ATA\r");
	return _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_NO_CARRIER, 1, 3, DTU_TIME_ATA, 2);
}

static rt_uint8_t _dtu_at_tts_start(rt_uint8_t data_type, rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint8_t	*pmem;
	rt_uint16_t	mem_len;

	if(DTU_TTS_DATA_TYPE_UCS2 == data_type)
	{
		pmem = (rt_uint8_t *)mempool_req(data_len * 2 + 15, RT_WAITING_NO);
	}
	else if(DTU_TTS_DATA_TYPE_GBK == data_type)
	{
		pmem = (rt_uint8_t *)mempool_req(data_len + 15, RT_WAITING_NO);
	}
	else
	{
		return RT_FALSE;
	}
	if((rt_uint8_t *)0 == pmem)
	{
		return RT_FALSE;
	}

	mem_len = sprintf((char *)pmem, "AT+QTTS=%d,\"", data_type);
	if(DTU_TTS_DATA_TYPE_UCS2 == data_type)
	{
		while(data_len)
		{
			data_len--;
			mem_len += sprintf((char *)(pmem + mem_len), "%02X", *pdata++);
		}
	}
	else
	{
		memcpy(pmem + mem_len, pdata, data_len);
		mem_len += data_len;
	}
	pmem[mem_len++] = '\"';
	pmem[mem_len++] = '\r';

	data_type = _dtu_at_excute_cmd(pmem, mem_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_BASIC, 2);
	rt_mp_free((void *)pmem);

	return data_type;
}

static rt_uint8_t _dtu_at_tts_query(void)
{
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+QTTS?\r");
	return _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_BASIC, 2);
}

static rt_uint8_t _dtu_at_tts_stop(void)
{
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+QTTS=0\r");
	return _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_QTTS_4111, DTU_EVENT_ERROR, 3, 3, DTU_TIME_BASIC, 2);
}

static rt_uint8_t _dtu_at_dial_phone(DTU_SMS_ADDR const *sms_addr_ptr)
{
	m_dtu_at_cmd_len = _dtu_at_cmd_dial_phone(m_dtu_at_cmd_data, sms_addr_ptr);
	return _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_NO_CARRIER, 3, 3, DTU_TIME_ATD, 2);
}

static void _dtu_at_event_ok(rt_uint8_t recv_data)
{
	static rt_uint8_t step = 0;
	
	switch(step)
	{
	case 0:
		if(0x0d == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if(0x0a == recv_data)
		{
			step = 2;
		}
		else if(0x0d != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('O' == recv_data)
		{
			step = 3;
		}
		else if(0x0d == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('K' == recv_data)
		{
			step = 4;
		}
		else if(0x0d == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if(0x0d == recv_data)
		{
			step = 5;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if(0x0a == recv_data)
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_OK);
			step = 0;
		}
		else if(0x0d == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	}
}
static void _dtu_at_event_error(rt_uint8_t recv_data)
{
	static rt_uint8_t step = 0;
	
	switch(step)
	{
	case 0:
		if(0x0d == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if(0x0a == recv_data)
		{
			step = 2;
		}
		else if(0x0d != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('E' == recv_data)
		{
			step = 3;
		}
		else if(0x0d == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('R' == recv_data)
		{
			step = 4;
		}
		else if(0x0d == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if('R' == recv_data)
		{
			step = 5;
		}
		else if(0x0d == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if('O' == recv_data)
		{
			step = 6;
		}
		else if(0x0d == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 6:
		if('R' == recv_data)
		{
			step = 7;
		}
		else if(0x0d == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 7:
		if(0x0d == recv_data)
		{
			step = 8;
		}
		else
		{
			step = 0;
		}
		break;
	case 8:
		if(0x0a == recv_data)
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_ERROR);
			step = 0;
		}
		else if(0x0d == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	}
}
static void _dtu_at_event_cpin(rt_uint8_t recv_data)
{
	static rt_uint8_t step = 0;
	
	switch(step)
	{
	case 0:
		if('+' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('C' == recv_data)
		{
			step = 2;
		}
		else if('+' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('P' == recv_data)
		{
			step = 3;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('I' == recv_data)
		{
			step = 4;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if('N' == recv_data)
		{
			step = 5;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if(':' == recv_data)
		{
			step = 6;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 6:
		if(' ' == recv_data)
		{
			step = 7;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 7:
		if('R' == recv_data)
		{
			step = 8;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 8:
		if('E' == recv_data)
		{
			step = 9;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 9:
		if('A' == recv_data)
		{
			step = 10;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 10:
		if('D' == recv_data)
		{
			step = 11;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 11:
		if('Y' == recv_data)
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_CPIN_RDY);
			step = 0;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	}
}
static void _dtu_at_event_creg(rt_uint8_t recv_data)
{
	static rt_uint8_t step = 0;
	
	switch(step)
	{
	case 0:
		if('+' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('C' == recv_data)
		{
			step = 2;
		}
		else if('+' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('R' == recv_data)
		{
			step = 3;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('E' == recv_data)
		{
			step = 4;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if('G' == recv_data)
		{
			step = 5;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if(':' == recv_data)
		{
			step = 6;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 6:
		if(' ' == recv_data)
		{
			step = 7;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 7:
		step = 8;
		break;
	case 8:
		if(',' == recv_data)
		{
			step = 9;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 9:
		if(('1' == recv_data) || ('5' == recv_data))
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_CREG_RDY);
			step = 0;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	}
}
static void _dtu_at_event_send(rt_uint8_t recv_data)
{
	static rt_uint8_t step = 0;
	
	switch(step)
	{
	case 0:
		if('S' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('E' == recv_data)
		{
			step = 2;
		}
		else if('S' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('N' == recv_data)
		{
			step = 3;
		}
		else if('S' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('D' == recv_data)
		{
			step = 4;
		}
		else if('S' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if(' ' == recv_data)
		{
			step = 5;
		}
		else if('S' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if('O' == recv_data)
		{
			step = 6;
		}
		else if('F' == recv_data)
		{
			step = 7;
		}
		else if('S' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 6:
		if('K' == recv_data)
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_SEND_OK);
			step = 0;
		}
		else if('S' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 7:
		if('A' == recv_data)
		{
			step = 8;
		}
		else if('S' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 8:
		if('I' == recv_data)
		{
			step = 9;
		}
		else if('S' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 9:
		if('L' == recv_data)
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_SEND_FAIL);
			step = 0;
		}
		else if('S' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	}
}
static void _dtu_at_event_ack(rt_uint8_t recv_data)
{
	static rt_uint8_t step = 0;
	
	switch(step)
	{
	case 0:
		if('+' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('Q' == recv_data)
		{
			step = 2;
		}
		else if('+' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('I' == recv_data)
		{
			step = 3;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('S' == recv_data)
		{
			step = 4;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if('E' == recv_data)
		{
			step = 5;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if('N' == recv_data)
		{
			step = 6;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 6:
		if('D' == recv_data)
		{
			step = 7;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 7:
		if(':' == recv_data)
		{
			step = 8;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 8:
		if(' ' == recv_data)
		{
			step = 9;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 9:
		if(',' == recv_data)
		{
			step = 10;
		}
		else if((recv_data < '0') || (recv_data > '9'))
		{
			step = 0;
		}
		break;
	case 10:
		if(',' == recv_data)
		{
			step = 11;
		}
		else if((recv_data < '0') || (recv_data > '9'))
		{
			step = 0;
		}
		break;
	case 11:
		if('0' == recv_data)
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_SEND_ACKED);
		}
		else
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_SEND_UNACKED);
		}
		step = 0;
		break;
	}
}
static void _dtu_at_event_conn(rt_uint8_t recv_data)
{
	static rt_uint8_t step = 0;
	
	switch(step)
	{
	case 0:
		if('+' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('Q' == recv_data)
		{
			step = 2;
		}
		else if('+' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('I' == recv_data)
		{
			step = 3;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('O' == recv_data)
		{
			step = 4;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if('P' == recv_data)
		{
			step = 5;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if('E' == recv_data)
		{
			step = 6;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 6:
		if('N' == recv_data)
		{
			step = 7;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 7:
		if(':' == recv_data)
		{
			step = 8;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 8:
		if(' ' == recv_data)
		{
			step = 9;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 9:
		step = 10;
		break;
	case 10:
		if(',' == recv_data)
		{
			step = 11;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 11:
		if('0' == recv_data)
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_CONN_OK);
		}
		else
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_CONN_FAIL);
		}
		step = 0;
		break;
	}
}
static void _dtu_at_event_arrow(rt_uint8_t recv_data)
{
	static rt_uint8_t step = 0;
	
	switch(step)
	{
	case 0:
		if(0x0d == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if(0x0a == recv_data)
		{
			step = 2;
		}
		else if(0x0d != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('>' == recv_data)
		{
			step = 3;
		}
		else if(0x0d == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if(' ' == recv_data)
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_ARROW);
			step = 0;
		}
		else if(0x0d == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	}
}
static void _dtu_at_event_cmgs(rt_uint8_t recv_data)
{
	static rt_uint8_t step = 0;
	
	switch(step)
	{
	case 0:
		if('+' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('C' == recv_data)
		{
			step = 2;
		}
		else if('+' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('M' == recv_data)
		{
			step = 3;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('G' == recv_data)
		{
			step = 4;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if('S' == recv_data)
		{
			step = 5;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if(':' == recv_data)
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_CMGS);
			step = 0;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	}
}
static void _dtu_at_event_csq(rt_uint8_t recv_data)
{
	static rt_uint8_t step = 0;
	
	switch(step)
	{
	case 0:
		if('+' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('C' == recv_data)
		{
			step = 2;
		}
		else if('+' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('S' == recv_data)
		{
			step = 3;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('Q' == recv_data)
		{
			step = 4;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if(':' == recv_data)
		{
			step = 5;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if(' ' == recv_data)
		{
			step = 6;
			m_dtu_csq_value = 0;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 6:
		if(',' == recv_data)
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_CSQ);
			step = 0;
		}
		else if((recv_data >= '0') && (recv_data <= '9'))
		{
			m_dtu_csq_value *= 10;
			m_dtu_csq_value += (recv_data - '0');
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	}
}
static void _dtu_at_event_telephone(rt_uint8_t recv_data)
{
	static rt_uint8_t		step = 0;
	static DTU_SMS_ADDR		*telephone_nbr_ptr;
	rt_base_t				level;
	
	switch(step)
	{
	case 0:
		if('+' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('C' == recv_data)
		{
			step = 2;
		}
		else if('+' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('L' == recv_data)
		{
			step = 3;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('I' == recv_data)
		{
			step = 4;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if('P' == recv_data)
		{
			step = 5;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if(':' == recv_data)
		{
			step = 6;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 6:
		if(' ' == recv_data)
		{
			step = 7;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 7:
		if('"' == recv_data)
		{
			level = rt_hw_interrupt_disable();
			if((DTU_SMS_ADDR *)0 == m_dtu_telephone_nbr_ptr)
			{
				rt_hw_interrupt_enable(level);
				step = 0;
			}
			else
			{
				telephone_nbr_ptr = m_dtu_telephone_nbr_ptr;
				m_dtu_telephone_nbr_ptr = (DTU_SMS_ADDR *)0;
				rt_hw_interrupt_enable(level);
				telephone_nbr_ptr->addr_len = 0;
				step = 8;
			}
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 8:
		if('"' == recv_data)
		{
			if(RT_EOK != rt_mb_send_wait(&m_dtu_mb_telephone_nbr, (rt_ubase_t)telephone_nbr_ptr, RT_WAITING_NO))
			{
				level = rt_hw_interrupt_disable();
				m_dtu_telephone_nbr_ptr = telephone_nbr_ptr;
				rt_hw_interrupt_enable(level);
			}
			step = 0;
		}
		else if(telephone_nbr_ptr->addr_len < DTU_BYTES_SMS_ADDR)
		{
			telephone_nbr_ptr->addr_data[telephone_nbr_ptr->addr_len++] = recv_data;
		}
		else
		{
			if(RT_EOK != rt_mb_send_wait(&m_dtu_mb_telephone_nbr, (rt_ubase_t)telephone_nbr_ptr, RT_WAITING_NO))
			{
				level = rt_hw_interrupt_disable();
				m_dtu_telephone_nbr_ptr = telephone_nbr_ptr;
				rt_hw_interrupt_enable(level);
			}
			step = 0;
		}
		break;
	}
}
static void _dtu_at_event_ip_data(rt_uint8_t recv_data)
{
	static rt_uint8_t		step = 0;
	static rt_uint8_t		ch;
	static rt_uint16_t		data_len;
	static DTU_RECV_DATA	*recv_data_ptr;
	
	switch(step)
	{
	case 0:
		if('+' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('Q' == recv_data)
		{
			step = 2;
		}
		else if('+' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('I' == recv_data)
		{
			step = 3;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('U' == recv_data)
		{
			step = 4;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if('R' == recv_data)
		{
			step = 5;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if('C' == recv_data)
		{
			step = 6;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 6:
		if(':' == recv_data)
		{
			step = 7;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 7:
		if(' ' == recv_data)
		{
			step = 8;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 8:
		if('"' == recv_data)
		{
			step = 9;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 9:
		if('r' == recv_data)
		{
			step = 10;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 10:
		if('e' == recv_data)
		{
			step = 11;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 11:
		if('c' == recv_data)
		{
			step = 12;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 12:
		if('v' == recv_data)
		{
			step = 13;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 13:
		if('"' == recv_data)
		{
			step = 14;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 14:
		if(',' == recv_data)
		{
			step = 15;
			ch = 0;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 15:
		if(',' == recv_data)
		{
			step = 16;
			data_len = 0;
		}
		else if((recv_data >= '0') && (recv_data <= '9'))
		{
			ch *= 10;
			ch += (recv_data - '0');
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 16:
		if(0x0d == recv_data)
		{
			step = 17;
		}
		else if((recv_data >= '0') && (recv_data <= '9'))
		{
			data_len *= 10;
			data_len += (recv_data - '0');
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 17:
		if(0x0a == recv_data)
		{
			recv_data_ptr = _dtu_recv_data_req(data_len, RT_WAITING_NO);
			if((DTU_RECV_DATA *)0 == recv_data_ptr)
			{
				step = 0;
			}
			else
			{
				recv_data_ptr->data_len = 0;
				step = 18;
			}
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 18:
		recv_data_ptr->pdata[recv_data_ptr->data_len++] = recv_data;
		if(recv_data_ptr->data_len >= data_len)
		{
			recv_data_ptr->comm_type	= DTU_COMM_TYPE_IP;
			recv_data_ptr->ch			= ch;

			//普通通道
			if(ch < DTU_NUM_IP_CH)
			{
				if(RT_EOK != rt_mb_send_wait(&m_dtu_mb_recv_data, (rt_ubase_t)recv_data_ptr, RT_WAITING_NO))
				{
					rt_mp_free((void *)recv_data_ptr);
				}
			}
			//http通道
			else if(DTU_NUM_IP_CH == ch)
			{
				if(RT_EOK != rt_mb_send_wait(&m_dtu_mb_http_data, (rt_ubase_t)recv_data_ptr, RT_WAITING_NO))
				{
					rt_mp_free((void *)recv_data_ptr);
				}
			}
			//其他通道可扩展
			else
			{
				rt_mp_free((void *)recv_data_ptr);
			}
			
			step = 0;
		}
		break;
	}
}
#ifdef DTU_SMS_MODE_PDU
static void _dtu_at_event_sms_data(rt_uint8_t recv_data)
{
	static rt_uint8_t		step = 0;
	static DTU_RECV_DATA	*recv_data_ptr;
	
	switch(step)
	{
	case 0:
		if('+' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('C' == recv_data)
		{
			step = 2;
		}
		else if('+' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('M' == recv_data)
		{
			step = 3;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('T' == recv_data)
		{
			step = 4;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if(':' == recv_data)
		{
			step = 5;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if(0x0d == recv_data)
		{
			step = 6;
		}
		break;
	case 6:
		if(0x0a == recv_data)
		{
			recv_data_ptr = _dtu_recv_data_req(DTU_BYTES_PDU_DATA, RT_WAITING_NO);
			if((DTU_RECV_DATA *)0 == recv_data_ptr)
			{
				step = 0;
			}
			else
			{
				recv_data_ptr->data_len = 0;
				step = 7;
			}
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 7:
		if(0x0d == recv_data)
		{
			step = 8;
		}
		else if(recv_data_ptr->data_len < DTU_BYTES_PDU_DATA)
		{
			recv_data_ptr->pdata[recv_data_ptr->data_len++] = recv_data;
		}
		else
		{
			step = 0;
			rt_mp_free((void *)recv_data_ptr);
		}
		break;
	case 8:
		if(0x0a == recv_data)
		{
			recv_data_ptr->comm_type	= DTU_COMM_TYPE_SMS;
			recv_data_ptr->ch			= DTU_NUM_SMS_CH;
			if(RT_EOK != rt_mb_send_wait(&m_dtu_mb_recv_data, (rt_ubase_t)recv_data_ptr, RT_WAITING_NO))
			{
				rt_mp_free((void *)recv_data_ptr);
			}
		}
		else
		{
			rt_mp_free((void *)recv_data_ptr);
		}
		step = 0;
		break;
	}
}
#endif
#ifdef DTU_SMS_MODE_TEXT
static void _dtu_at_event_sms_data(rt_uint8_t recv_data)
{
	static rt_uint8_t		step = 0;
	static DTU_RECV_DATA	*recv_data_ptr;
	
	switch(step)
	{
	case 0:
		if('+' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('C' == recv_data)
		{
			step = 2;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 2:
		if('M' == recv_data)
		{
			step = 3;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('T' == recv_data)
		{
			step = 4;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if(':' == recv_data)
		{
			step = 5;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if(' ' == recv_data)
		{
			step = 6;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 6:
		if('"' == recv_data)
		{
			recv_data_ptr = _dtu_recv_data_req(DTU_BYTES_PDU_DATA, RT_WAITING_NO);
			if((DTU_RECV_DATA *)0 == recv_data_ptr)
			{
				step = 0;
			}
			else
			{
				recv_data_ptr->ch		= 0;
				recv_data_ptr->data_len	= 0;
				step = 7;
			}
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 7:
		if('"' == recv_data)
		{
			while(recv_data_ptr->data_len < DTU_BYTES_SMS_ADDR)
			{
				recv_data_ptr->pdata[recv_data_ptr->data_len++] = 0;
			}
			step = 8;
		}
		else if(recv_data_ptr->data_len < DTU_BYTES_SMS_ADDR)
		{
			recv_data_ptr->pdata[recv_data_ptr->data_len++] = recv_data;
		}
		else
		{
			rt_mp_free((void *)recv_data_ptr);
			step = 0;
		}
		break;
	case 8:
		if(',' == recv_data)
		{
			recv_data_ptr->ch++;
			if(recv_data_ptr->ch >= 7)
			{
				step = 9;
			}
		}
		break;
	case 9:
		if('0' == recv_data)
		{
			recv_data_ptr->ch = RT_FALSE;
			step = 10;
		}
		else if('8' == recv_data)
		{
			recv_data_ptr->ch = RT_TRUE;
			step = 10;
		}
		else
		{
			rt_mp_free((void *)recv_data_ptr);
			step = 0;
		}
		break;
	case 10:
		if(0x0d == recv_data)
		{
			step = 11;
		}
		break;
	case 11:
		if(0x0a == recv_data)
		{
			step = 12;
		}
		else
		{
			rt_mp_free((void *)recv_data_ptr);
			step = 0;
		}
		break;
	case 12:
		if(0x0d == recv_data)
		{
			step = 13;
		}
		else if(recv_data_ptr->data_len < DTU_BYTES_PDU_DATA)
		{
			recv_data_ptr->pdata[recv_data_ptr->data_len++] = recv_data;
		}
		else
		{
			rt_mp_free((void *)recv_data_ptr);
			step = 0;
		}
		break;
	case 13:
		if(0x0a == recv_data)
		{
			recv_data_ptr->comm_type = DTU_COMM_TYPE_SMS;
			if(RT_EOK != rt_mb_send_wait(&m_dtu_mb_recv_data, (rt_ubase_t)recv_data_ptr, RT_WAITING_NO))
			{
				rt_mp_free((void *)recv_data_ptr);
			}
		}
		else
		{
			rt_mp_free((void *)recv_data_ptr);
		}
		step = 0;
		break;
	}
}
//ch表示数据是ascii还是unicode；数据域中的前DTU_BYTES_SMS_ADDR是短信号码，后面是短信内容
static void _dtu_at_event_sms_data_cdma(rt_uint8_t recv_data)
{
	static rt_uint8_t		step = 0;
	static DTU_RECV_DATA	*recv_data_ptr;
	
	switch(step)
	{
	case 0:
		if('^' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('H' == recv_data)
		{
			step = 2;
		}
		else if('^' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('C' == recv_data)
		{
			step = 3;
		}
		else if('^' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('M' == recv_data)
		{
			step = 4;
		}
		else if('^' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if('T' == recv_data)
		{
			step = 5;
		}
		else if('^' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if(':' == recv_data)
		{
			step = 6;
		}
		else if('^' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 6:
		if(' ' == recv_data)
		{
			recv_data_ptr = _dtu_recv_data_req(DTU_BYTES_PDU_DATA, RT_WAITING_NO);
			if((DTU_RECV_DATA *)0 == recv_data_ptr)
			{
				step = 0;
			}
			else
			{
				recv_data_ptr->ch		= 0;
				recv_data_ptr->data_len	= 0;
				step = 7;
			}
		}
		else if('^' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 7:
		if(',' == recv_data)
		{
			while(recv_data_ptr->data_len < DTU_BYTES_SMS_ADDR)
			{
				recv_data_ptr->pdata[recv_data_ptr->data_len++] = 0;
			}
			step = 8;
		}
		else if(recv_data_ptr->data_len < DTU_BYTES_SMS_ADDR)
		{
			recv_data_ptr->pdata[recv_data_ptr->data_len++] = recv_data;
		}
		else
		{
			rt_mp_free((void *)recv_data_ptr);
			step = 0;
		}
		break;
	case 8:
		if(',' == recv_data)
		{
			recv_data_ptr->ch++;
			if(recv_data_ptr->ch >= 3)
			{
				step = 9;
			}
		}
		break;
	case 9:
		if('1' == recv_data)
		{
			recv_data_ptr->ch = RT_FALSE;
			step = 10;
		}
		else if('6' == recv_data)
		{
			recv_data_ptr->ch = RT_TRUE;
			step = 10;
		}
		else
		{
			rt_mp_free((void *)recv_data_ptr);
			step = 0;
		}
		break;
	case 10:
		if(0x0d == recv_data)
		{
			step = 11;
		}
		break;
	case 11:
		if(0x0a == recv_data)
		{
			step = 12;
		}
		else
		{
			rt_mp_free((void *)recv_data_ptr);
			step = 0;
		}
		break;
	case 12:
		if(0x0d == recv_data)
		{
			step = 13;
		}
		else if(recv_data_ptr->data_len < DTU_BYTES_PDU_DATA)
		{
			recv_data_ptr->pdata[recv_data_ptr->data_len++] = recv_data;
		}
		else
		{
			rt_mp_free((void *)recv_data_ptr);
			step = 0;
		}
		break;
	case 13:
		if(0x0a == recv_data)
		{
			recv_data_ptr->comm_type = DTU_COMM_TYPE_SMS;
			if(RT_EOK != rt_mb_send_wait(&m_dtu_mb_recv_data, (rt_ubase_t)recv_data_ptr, RT_WAITING_NO))
			{
				rt_mp_free((void *)recv_data_ptr);
			}
		}
		else
		{
			rt_mp_free((void *)recv_data_ptr);
		}
		step = 0;
		break;
	}
}
#endif
static void _dtu_at_event_no_carrier(rt_uint8_t recv_data)
{
	static rt_uint8_t step = 0;
	
	switch(step)
	{
	case 0:
		if('N' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('O' == recv_data)
		{
			step = 2;
		}
		else if('N' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if(' ' == recv_data)
		{
			step = 3;
		}
		else if('N' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('C' == recv_data)
		{
			step = 4;
		}
		else if('N' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if('A' == recv_data)
		{
			step = 5;
		}
		else if('N' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if('R' == recv_data)
		{
			step = 6;
		}
		else if('N' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 6:
		if('R' == recv_data)
		{
			step = 7;
		}
		else if('N' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 7:
		if('I' == recv_data)
		{
			step = 8;
		}
		else if('N' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 8:
		if('E' == recv_data)
		{
			step = 9;
		}
		else if('N' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 9:
		if('R' == recv_data)
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_NO_CARRIER);
			rt_event_send(&m_dtu_event_module_ex, DTU_EVENT_HANG_UP_SHE);
			step = 0;
		}
		else if('N' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	}
}

static void _dtu_at_event_tts_over(rt_uint8_t recv_data)
{
	static rt_uint8_t step = 0;
	
	switch(step)
	{
	case 0:
		if('+' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('Q' == recv_data)
		{
			step = 2;
		}
		else if('+' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('T' == recv_data)
		{
			step = 3;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('T' == recv_data)
		{
			step = 4;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if('S' == recv_data)
		{
			step = 5;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if(':' == recv_data)
		{
			step = 6;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 6:
		if(' ' == recv_data)
		{
			step = 7;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 7:
		if('0' == recv_data)
		{
			rt_event_send(&m_dtu_event_module_ex, DTU_EVENT_TTS_STOP_SHE);
			step = 0;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	}
}

static void _dtu_at_event_tts_suspend(rt_uint8_t recv_data)
{
	static rt_uint8_t step = 0;
	
	switch(step)
	{
	case 0:
		if('+' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('Q' == recv_data)
		{
			step = 2;
		}
		else if('+' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('T' == recv_data)
		{
			step = 3;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('T' == recv_data)
		{
			step = 4;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if('S' == recv_data)
		{
			step = 5;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if(':' == recv_data)
		{
			step = 6;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 6:
		if(' ' == recv_data)
		{
			step = 7;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 7:
		if('4' == recv_data)
		{
			step = 8;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 8:
		if('1' == recv_data)
		{
			step = 9;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 9:
		if('1' == recv_data)
		{
			step = 10;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 10:
		if('1' == recv_data)
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_QTTS_4111);
			rt_event_send(&m_dtu_event_module_ex, DTU_EVENT_TTS_STOP_SHE);
			step = 0;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	}
}

static void _dtu_at_event_ip_closed(rt_uint8_t recv_data)
{
	static rt_uint8_t step = 0;
	static rt_uint8_t ch;

	switch(step)
	{
	case 0:
		if('+' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('Q' == recv_data)
		{
			step = 2;
		}
		else if('+' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('I' == recv_data)
		{
			step = 3;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('U' == recv_data)
		{
			step = 4;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 4:
		if('R' == recv_data)
		{
			step = 5;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 5:
		if('C' == recv_data)
		{
			step = 6;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 6:
		if(':' == recv_data)
		{
			step = 7;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 7:
		if(' ' == recv_data)
		{
			step = 8;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 8:
		if('"' == recv_data)
		{
			step = 9;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 9:
		if('c' == recv_data)
		{
			step = 10;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 10:
		if('l' == recv_data)
		{
			step = 11;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 11:
		if('o' == recv_data)
		{
			step = 12;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 12:
		if('s' == recv_data)
		{
			step = 100;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 100:
		if('e' == recv_data)
		{
			step = 101;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 101:
		if('d' == recv_data)
		{
			step = 13;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 13:
		if('"' == recv_data)
		{
			step = 14;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 14:
		if(',' == recv_data)
		{
			step = 15;
			ch = 0;
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 15:
		if(0x0d == recv_data)
		{
			rt_event_send(&m_dtu_event_module_ex, DTU_EVENT_NEED_CONN_CH << ch);
			step = 0;
		}
		else if((recv_data >= '0') && (recv_data <= '9'))
		{
			ch *= 10;
			ch += (recv_data - '0');
		}
		else if('+' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	}
}

static void _dtu_at_event_incoming_phone(rt_uint8_t type)
{
	if(RT_TRUE == type)
	{
		if(RT_EOK != rt_event_send(&m_dtu_event_module, DTU_EVENT_INCOMING_PHONE))
		{
			while(1);
		}
	}
	else
	{
		rt_event_recv(&m_dtu_event_module, DTU_EVENT_INCOMING_PHONE, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, RT_WAITING_NO, (rt_uint32_t *)0);
	}
}

static void _dtu_at_event_qtts_start(rt_uint8_t type)
{
	if(RT_TRUE == type)
	{
		if(RT_EOK != rt_event_send(&m_dtu_event_module, DTU_EVENT_QTTS_START))
		{
			while(1);
		}
	}
	else
	{
		rt_event_recv(&m_dtu_event_module, DTU_EVENT_QTTS_START, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, RT_WAITING_NO, (rt_uint32_t *)0);
	}
}

static void _dtu_at_cmd_handler(rt_uint8_t recv_data)
{
	_dtu_at_event_ok(recv_data);
	_dtu_at_event_error(recv_data);
	_dtu_at_event_cpin(recv_data);
	_dtu_at_event_creg(recv_data);
	_dtu_at_event_send(recv_data);
	_dtu_at_event_ack(recv_data);
	_dtu_at_event_conn(recv_data);
	_dtu_at_event_arrow(recv_data);
	_dtu_at_event_cmgs(recv_data);
	_dtu_at_event_csq(recv_data);
	_dtu_at_event_telephone(recv_data);
	_dtu_at_event_ip_data(recv_data);
	_dtu_at_event_sms_data(recv_data);
#ifdef DTU_SMS_MODE_TEXT
	_dtu_at_event_sms_data_cdma(recv_data);
#endif
	_dtu_at_event_no_carrier(recv_data);
	_dtu_at_event_tts_over(recv_data);
	_dtu_at_event_tts_suspend(recv_data);
	_dtu_at_event_ip_closed(recv_data);
}

static rt_uint16_t _dtu_heart_encoder(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t ch)
{
	return snprintf((char *)pdata, data_len, "ec20-ch%d", ch);
}

#ifdef DTU_SMS_MODE_PDU
static void _dtu_sms_data_conv(DTU_RECV_DATA *recv_data_ptr)
{
	static DTU_SMS_ADDR		sms_addr;
	rt_uint8_t				*pmem, ch, is_valid = RT_FALSE;
	rt_uint16_t				data_len;

	if(DTU_COMM_TYPE_SMS != recv_data_ptr->comm_type)
	{
		return;
	}
	if(0 == recv_data_ptr->data_len)
	{
		return;
	}

	//申请空间用于存放解码后的数据
	pmem = (rt_uint8_t *)mempool_req(recv_data_ptr->data_len, RT_WAITING_FOREVER);
	if((rt_uint8_t *)0 == pmem)
	{
		recv_data_ptr->data_len = 0;
		return;
	}
	//解码
	data_len = _pdu_decoder(pmem, &sms_addr, recv_data_ptr->pdata, recv_data_ptr->data_len);
	if(0 == data_len)
	{
		rt_mp_free((void *)pmem);
		recv_data_ptr->data_len = 0;
		return;
	}
	//判断是哪个通道发来的数据
	while(1)
	{
		//权限号码
		_dtu_param_pend(DTU_PARAM_SUPER_PHONE);
		for(ch = 0; ch < DTU_NUM_SUPER_PHONE; ch++)
		{
			if(m_dtu_super_phone[ch].addr_len)
			{
				if(RT_TRUE == _dtu_compare_phone(&sms_addr, m_dtu_super_phone + ch))
				{
					recv_data_ptr->ch = ch + DTU_NUM_SMS_CH;
					is_valid = RT_TRUE;
					break;
				}
			}
		}
		_dtu_param_post(DTU_PARAM_SUPER_PHONE);
		if(RT_TRUE == is_valid)
		{
			break;
		}
		//各通道号码
		for(ch = 0; ch < DTU_NUM_SMS_CH; ch++)
		{
			_dtu_param_pend(DTU_PARAM_SMS_CH + (DTU_PARAM_SMS_ADDR << ch));
			if(m_dtu_sms_ch & (1 << ch))
			{
				if(RT_TRUE == _dtu_compare_phone(&sms_addr, m_dtu_sms_addr + ch))
				{
					_dtu_param_post(DTU_PARAM_SMS_CH + (DTU_PARAM_SMS_ADDR << ch));
					recv_data_ptr->ch = ch;
					is_valid = RT_TRUE;
					break;
				}
			}
			_dtu_param_post(DTU_PARAM_SMS_CH + (DTU_PARAM_SMS_ADDR << ch));
		}
		
		break;
	}
	//号码不符合要求，数据丢弃
	if(RT_FALSE == is_valid)
	{
		rt_mp_free((void *)pmem);
		recv_data_ptr->data_len = 0;
		return;
	}
	//数据拷贝
	memcpy(recv_data_ptr->pdata, pmem, data_len);
	recv_data_ptr->data_len = data_len;
	//释放空间
	rt_mp_free((void *)pmem);
}
#endif
#ifdef DTU_SMS_MODE_TEXT
static void _dtu_sms_data_conv(DTU_RECV_DATA *recv_data_ptr)
{
	static DTU_SMS_ADDR		sms_addr;
	rt_uint8_t				*pmem, ch, is_valid = RT_FALSE;
	rt_uint16_t				data_len;

	if(DTU_COMM_TYPE_SMS != recv_data_ptr->comm_type)
	{
		return;
	}
	if(recv_data_ptr->data_len <= DTU_BYTES_SMS_ADDR)
	{
		recv_data_ptr->data_len = 0;
		return;
	}

	//短信号码
	sms_addr.addr_len = 0;
	for(data_len = 0; data_len < DTU_BYTES_SMS_ADDR; data_len++)
	{
		if(0 == recv_data_ptr->pdata[data_len])
		{
			break;
		}
		sms_addr.addr_data[sms_addr.addr_len++] = recv_data_ptr->pdata[data_len];
	}
	//判断短信通道
	while(1)
	{
		//权限号码
		_dtu_param_pend(DTU_PARAM_SUPER_PHONE);
		for(ch = 0; ch < DTU_NUM_SUPER_PHONE; ch++)
		{
			if(m_dtu_super_phone[ch].addr_len)
			{
				if(RT_TRUE == _dtu_compare_phone(&sms_addr, m_dtu_super_phone + ch))
				{
					data_len = ch + DTU_NUM_SMS_CH;
					is_valid = RT_TRUE;
					break;
				}
			}
		}
		_dtu_param_post(DTU_PARAM_SUPER_PHONE);
		if(RT_TRUE == is_valid)
		{
			break;
		}
		//各通道号码
		for(ch = 0; ch < DTU_NUM_SMS_CH; ch++)
		{
			_dtu_param_pend(DTU_PARAM_SMS_CH + (DTU_PARAM_SMS_ADDR << ch));
			if(m_dtu_sms_ch & (1 << ch))
			{
				if(RT_TRUE == _dtu_compare_phone(&sms_addr, m_dtu_sms_addr + ch))
				{
					_dtu_param_post(DTU_PARAM_SMS_CH + (DTU_PARAM_SMS_ADDR << ch));
					data_len = ch;
					is_valid = RT_TRUE;
					break;
				}
			}
			_dtu_param_post(DTU_PARAM_SMS_CH + (DTU_PARAM_SMS_ADDR << ch));
		}
		
		break;
	}
	//号码不符合要求，数据丢弃
	if(RT_FALSE == is_valid)
	{
		recv_data_ptr->data_len = 0;
		return;
	}
	is_valid = recv_data_ptr->ch;
	recv_data_ptr->ch = data_len;
	data_len = recv_data_ptr->data_len - DTU_BYTES_SMS_ADDR;
	//申请空间用于存放解码后的数据
	pmem = (rt_uint8_t *)mempool_req(data_len, RT_WAITING_FOREVER);
	if((rt_uint8_t *)0 == pmem)
	{
		recv_data_ptr->data_len = 0;
		return;
	}
	//短信内容
	if(RT_TRUE == is_valid)
	{
		data_len = fyyp_str_to_array(pmem, recv_data_ptr->pdata + DTU_BYTES_SMS_ADDR, data_len);
	}
	else
	{
		memcpy(pmem, recv_data_ptr->pdata + DTU_BYTES_SMS_ADDR, data_len);
	}
	memcpy(recv_data_ptr->pdata, pmem, data_len);
	recv_data_ptr->data_len = data_len;
	//释放空间
	rt_mp_free((void *)pmem);
}
#endif

