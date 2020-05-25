#include "stdio.h"



//超时时间，秒
#define DTU_TIME_PWR_OFF					50						//断电时间
#define DTU_TIME_PWR_ON						3						//上电时间
#define DTU_TIME_STA_OFF					3						//关机时间
#define DTU_TIME_STA_ON						40						//开机时间
#define DTU_TIME_IP_SHUT					65						//IPSHUT超时时间
#define DTU_TIME_IP_CLOSE					5						//IPCLOSE超时时间
#define DTU_TIME_IP_START					75						//IPSTART超时时间
#define DTU_TIME_IP_SEND					30						//IPSEND超时时间
#define DTU_TIME_CMGS						60						//CMGS超时时间
#define DTU_TIME_ATH						20						//ATH超时时间
#define DTU_TIME_CLIP						15						//CLIP超时时间
#define DTU_TIME_CGATT						75						//CGATT超时时间
#define DTU_TIME_CIICR						85						//CIICR超时时间
#define DTU_TIME_AT_BASIC					3						//基本AT指令超时时间

//字节空间
#define DTU_BYTES_AT_CMD					60						//AT发送指令
#define DTU_BYTES_PDU_DATA					350						//PDU数据空间

//事件标志
#define DTU_EVENT_OK						0x01					//OK
#define DTU_EVENT_ERROR						0x02					//ERROR
#define DTU_EVENT_SEND_OK					0x04					//SEND OK
#define DTU_EVENT_SEND_FAIL					0x08					//SEND FAIL
#define DTU_EVENT_CLOSE_OK					0x10					//CLOSE OK
#define DTU_EVENT_CONN_OK					0x20					//CONNECT OK
#define DTU_EVENT_CONN_FAIL					0x40					//CONNECT FAIL
#define DTU_EVENT_SHUT_OK					0x80					//SHUT OK
#define DTU_EVENT_ARROW						0x100					//>
#define DTU_EVENT_CMGS						0x200					//CMGS
#define DTU_EVENT_CSQ						0x400					//CSQ
#define DTU_EVENT_NONE_YES					0x800
#define DTU_EVENT_NONE_NO					0x1000



static rt_uint8_t m_dtu_at_cmd_data[DTU_BYTES_AT_CMD], m_dtu_at_cmd_len;



static rt_uint8_t _dtu_at_cmd_cstt(rt_uint8_t *pcmd, DTU_APN const *papn)
{
	rt_uint8_t cmd_len;
	
	cmd_len = sprintf((char *)pcmd, "AT+CSTT=\"");
	fyyp_memcopy(pcmd + cmd_len, papn->apn_data, papn->apn_len);
	cmd_len += papn->apn_len;
	pcmd[cmd_len++] = '\"';
	pcmd[cmd_len++] = '\r';
	
	return cmd_len;
}

static rt_uint8_t _dtu_at_cmd_close_ip(rt_uint8_t *pcmd, rt_uint8_t ch)
{
	return sprintf((char *)pcmd, "AT+CIPCLOSE=%d\r", ch);
}

static rt_uint8_t _dtu_at_cmd_connect_ip(rt_uint8_t *pcmd, rt_uint8_t ch, DTU_IP_ADDR const *ip_addr_ptr, rt_uint8_t is_udp)
{
	rt_uint8_t cmd_len, i;
	
	if(RT_TRUE == is_udp)
	{
		cmd_len = sprintf((char *)pcmd, "AT+CIPSTART=%d,\"UDP\",\"", ch);
	}
	else
	{
		cmd_len = sprintf((char *)pcmd, "AT+CIPSTART=%d,\"TCP\",\"", ch);
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
	pcmd[cmd_len++] = '\r';
	
	return cmd_len;
}

static rt_uint8_t _dtu_at_cmd_send_ip(rt_uint8_t *pcmd, rt_uint8_t ch, rt_uint16_t data_len)
{
	return sprintf((char *)pcmd, "AT+CIPSEND=%d,%d\r", ch, data_len);
}

static rt_uint8_t _dtu_at_cmd_send_sms(rt_uint8_t *pcmd, rt_uint16_t data_len)
{
	return sprintf((char *)pcmd, "AT+CMGS=%d\r", data_len);
}



static rt_uint8_t _dtu_at_excute_cmd(rt_uint8_t const *pcmd, rt_uint16_t cmd_len, rt_uint32_t yes, rt_uint32_t no, rt_uint8_t err_times, rt_uint8_t timeout_times, rt_uint8_t timeout, rt_uint8_t wait_time)
{
	rt_uint32_t events_recv;
	
	while(1)
	{
		//清除事件
		rt_event_recv(&m_dtu_event_module, yes + no, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, RT_WAITING_NO, (rt_uint32_t *)0);
		//发送指令
		if(m_dtu_at_cmd_data == pcmd)
		{
			_dtu_send_to_module("\r", 1);
			rt_thread_delay(RT_TICK_PER_SECOND / 2);
		}
		_dtu_send_to_module(pcmd, cmd_len);
		//判断事件
		events_recv = 0;
		rt_event_recv(&m_dtu_event_module, yes + no, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, timeout * RT_TICK_PER_SECOND, &events_recv);
		if(events_recv & yes)
		{
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
				return RT_FALSE;
			}
		}
	}
}



static rt_uint8_t _dtu_at_close_ip(rt_uint8_t ch)
{
	m_dtu_at_cmd_len = _dtu_at_cmd_close_ip(m_dtu_at_cmd_data, ch);
	return _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_CLOSE_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_IP_CLOSE, 2);
}

static rt_uint8_t _dtu_at_connect_ip(rt_uint8_t ch)
{
	//生成断开连接指令
	m_dtu_at_cmd_len = _dtu_at_cmd_close_ip(m_dtu_at_cmd_data, ch);
	//执行断开连接指令
	_dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_CLOSE_OK, DTU_EVENT_ERROR, 1, 2, DTU_TIME_IP_CLOSE, 2);
	//生成连接指令
	_dtu_param_pend((DTU_PARAM_IP_ADDR << ch) + DTU_PARAM_IP_TYPE);
	m_dtu_at_cmd_len = _dtu_at_cmd_connect_ip(m_dtu_at_cmd_data, ch, m_dtu_ip_addr + ch, (m_dtu_ip_type & (1 << ch)) ? RT_TRUE : RT_FALSE);
	_dtu_param_post((DTU_PARAM_IP_ADDR << ch) + DTU_PARAM_IP_TYPE);
	//执行连接指令
	return _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_CONN_OK, DTU_EVENT_CONN_FAIL, 1, 1, DTU_TIME_IP_START, 2);
}

static rt_uint8_t _dtu_at_send_ip(rt_uint8_t ch, rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	//生成发送指令
	m_dtu_at_cmd_len = _dtu_at_cmd_send_ip(m_dtu_at_cmd_data, ch, data_len);
	//执行发送指令
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_ARROW, DTU_EVENT_ERROR, 1, 1, DTU_TIME_AT_BASIC, 2))
	{
		return RT_FALSE;
	}
	//发送数据
	return _dtu_at_excute_cmd(pdata, data_len, DTU_EVENT_SEND_OK, DTU_EVENT_SEND_FAIL, 1, 1, DTU_TIME_IP_SEND, 2);
}

static rt_uint8_t _dtu_at_send_sms(rt_uint8_t ch, rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint8_t		*pmem, sta;
	rt_uint16_t		pdu_len;
	
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
		if(ch < DTU_NUM_SUPER_PHONE)
		{
			_dtu_param_pend(DTU_PARAM_SUPER_PHONE);
			pdu_len = _pdu_encoder(pmem, m_dtu_super_phone + ch, pdata, data_len);
			_dtu_param_post(DTU_PARAM_SUPER_PHONE);
		}
		else
		{
			//释放内存
			rt_mp_free((void *)pmem);
			return RT_FALSE;
		}
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
	sta = _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_ARROW, DTU_EVENT_ERROR, 1, 1, DTU_TIME_AT_BASIC, 2);
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

static rt_uint8_t _dtu_at_boot_sms(void)
{
	//ATE0
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "ATE0\r");
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_AT_BASIC, 2))
	{
		return RT_FALSE;
	}
	//AT+CLIP=1
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CLIP=1\r");
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_CLIP, 2))
	{
		return RT_FALSE;
	}
	//AT+CMGF=0
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CMGF=0\r");
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_AT_BASIC, 2))
	{
		return RT_FALSE;
	}
	//AT+CNMI=2,2,0,0,0
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CNMI=2,2,0,0,0\r");
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_AT_BASIC, 2))
	{
		return RT_FALSE;
	}
	
	return RT_TRUE;
}

static rt_uint8_t _dtu_at_boot_ip(void)
{
	//AT+CGATT=1
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CGATT=1\r");
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_CGATT, 10))
	{
		return RT_FALSE;
	}
	//AT+CIPSHUT
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CIPSHUT\r");
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_SHUT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_IP_SHUT, 2))
	{
		return RT_FALSE;
	}
	//AT+CIPMUX=1
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CIPMUX=1\r");
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_AT_BASIC, 2))
	{
		return RT_FALSE;
	}
	//AT+CSTT="*******"
	_dtu_param_pend(DTU_PARAM_APN);
	m_dtu_at_cmd_len = _dtu_at_cmd_cstt(m_dtu_at_cmd_data, &m_dtu_apn);
	_dtu_param_post(DTU_PARAM_APN);
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_AT_BASIC, 2))
	{
		return RT_FALSE;
	}
	//AT+CIICR
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CIICR\r");
	if(RT_FALSE == _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 1, 1, DTU_TIME_CIICR, 2))
	{
		return RT_FALSE;
	}
	//AT+CIFSR
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CIFSR\r");
	_dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_NONE_YES, DTU_EVENT_NONE_NO, 1, 1, DTU_TIME_AT_BASIC, 2);
	
	return RT_TRUE;
}

static rt_uint8_t _dtu_at_csq_update(void)
{
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CSQ\r");
	return _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_CSQ, DTU_EVENT_NONE_NO, 3, 3, DTU_TIME_AT_BASIC, 2);
}

static rt_uint8_t _dtu_at_close_net(void)
{
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "AT+CIPSHUT\r");
	return _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_SHUT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_IP_SHUT, 2);
}

static rt_uint8_t _dtu_at_hang_up(void)
{
	m_dtu_at_cmd_len = sprintf((char *)m_dtu_at_cmd_data, "ATH\r");
	return _dtu_at_excute_cmd(m_dtu_at_cmd_data, m_dtu_at_cmd_len, DTU_EVENT_OK, DTU_EVENT_ERROR, 3, 3, DTU_TIME_ATH, 2);
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
static void _dtu_at_event_close_ok(rt_uint8_t recv_data)
{
	static rt_uint8_t step = 0;
	
	switch(step)
	{
	case 0:
		if('C' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('L' == recv_data)
		{
			step = 2;
		}
		else if('C' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('O' == recv_data)
		{
			step = 3;
		}
		else if('C' == recv_data)
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
		else if('C' == recv_data)
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
		else if('C' == recv_data)
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
		else if('C' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 6:
		if('O' == recv_data)
		{
			step = 7;
		}
		else if('C' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 7:
		if('K' == recv_data)
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_CLOSE_OK);
			step = 0;
		}
		else if('C' == recv_data)
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
static void _dtu_at_event_conn(rt_uint8_t recv_data)
{
	static rt_uint8_t step = 0;
	
	switch(step)
	{
	case 0:
		if('C' == recv_data)
		{
			step = 1;
		}
		break;
	case 1:
		if('O' == recv_data)
		{
			step = 2;
		}
		else if('C' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('N' == recv_data)
		{
			step = 3;
		}
		else if('C' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 3:
		if('N' == recv_data)
		{
			step = 4;
		}
		else if('C' == recv_data)
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
		else if('C' == recv_data)
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
		else
		{
			step = 0;
		}
		break;
	case 6:
		if('T' == recv_data)
		{
			step = 7;
		}
		else if('C' == recv_data)
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
		else if('C' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 8:
		if('O' == recv_data)
		{
			step = 9;
		}
		else if('F' == recv_data)
		{
			step = 10;
		}
		else if('C' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 9:
		if('K' == recv_data)
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_CONN_OK);
			step = 0;
		}
		else if('C' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 10:
		if('A' == recv_data)
		{
			step = 11;
		}
		else if('C' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 11:
		if('I' == recv_data)
		{
			step = 12;
		}
		else if('C' == recv_data)
		{
			step = 1;
		}
		else
		{
			step = 0;
		}
		break;
	case 12:
		if('L' == recv_data)
		{
			rt_event_send(&m_dtu_event_module, DTU_EVENT_CONN_FAIL);
			step = 0;
		}
		else if('C' == recv_data)
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
static void _dtu_at_event_shut_ok(rt_uint8_t recv_data)
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
		if('H' == recv_data)
		{
			step = 2;
		}
		else if('S' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('U' == recv_data)
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
		if('T' == recv_data)
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
			rt_event_send(&m_dtu_event_module, DTU_EVENT_SHUT_OK);
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
		if('R' == recv_data)
		{
			step = 2;
		}
		else if('+' != recv_data)
		{
			step = 0;
		}
		break;
	case 2:
		if('E' == recv_data)
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
		if('C' == recv_data)
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
		if('I' == recv_data)
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
		if('V' == recv_data)
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
		if('E' == recv_data)
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
		if((recv_data >= '0') && (recv_data <= '7'))
		{
			ch = recv_data - '0';
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
		if(',' == recv_data)
		{
			data_len = 0;
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
		if(':' == recv_data)
		{
			step = 12;
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
	case 12:
		if(0x0d == recv_data)
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
				step = 14;
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
	case 14:
		recv_data_ptr->pdata[recv_data_ptr->data_len++] = recv_data;
		if(recv_data_ptr->data_len >= data_len)
		{
			recv_data_ptr->comm_type	= DTU_COMM_TYPE_IP;
			recv_data_ptr->ch			= ch;
			if(RT_EOK != rt_mb_send_wait(&m_dtu_mb_recv_data, (rt_ubase_t)recv_data_ptr, RT_WAITING_NO))
			{
				rt_mp_free((void *)recv_data_ptr);
			}
			step = 0;
		}
		break;
	}
}
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
static void _dtu_at_cmd_handler(rt_uint8_t recv_data)
{
	_dtu_at_event_ok(recv_data);
	_dtu_at_event_error(recv_data);
	_dtu_at_event_send(recv_data);
	_dtu_at_event_close_ok(recv_data);
	_dtu_at_event_conn(recv_data);
	_dtu_at_event_shut_ok(recv_data);
	_dtu_at_event_arrow(recv_data);
	_dtu_at_event_cmgs(recv_data);
	_dtu_at_event_csq(recv_data);
	_dtu_at_event_telephone(recv_data);
	_dtu_at_event_ip_data(recv_data);
	_dtu_at_event_sms_data(recv_data);
}



static rt_uint16_t _dtu_heart_encoder(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t ch)
{
	if(data_len < 11)
	{
		return 0;
	}

	data_len = 0;
	pdata[data_len++] = 's';
	pdata[data_len++] = 'i';
	pdata[data_len++] = 'm';
	pdata[data_len++] = '8';
	pdata[data_len++] = '0';
	pdata[data_len++] = '0';
	pdata[data_len++] = 'c';
	pdata[data_len++] = '-';
	pdata[data_len++] = 'c';
	pdata[data_len++] = 'h';
	pdata[data_len++] = '0' + ch;
	
	return data_len;
}

static void _dtu_sms_data_conv(DTU_RECV_DATA *recv_data_ptr)
{
	static DTU_SMS_ADDR		sms_addr;
	rt_uint8_t				*pmem, ch, is_valid = RT_FALSE;
	rt_uint16_t				data_len;

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
	fyyp_memcopy(recv_data_ptr->pdata, pmem, data_len);
	recv_data_ptr->data_len = data_len;
	//释放空间
	rt_mp_free((void *)pmem);
}

