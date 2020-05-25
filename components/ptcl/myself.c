#include "myself.h"
#include "string.h"
#include "stdio.h"
#include "drv_rtcee.h"
#include "drv_fyyp.h"
#include "dtu.h"
#include "drv_mempool.h"
#include "http.h"



//参数名称
static char const *m_my_param_type[MY_NUM_PARAM_TYPE] = {
"",				"RESET ",		"RESTORE ",		"TT ",		"VER ",			"CA1 ",			"CA2 ",			"CA3 ",			"CA4 ",			"C1P1 ",
"C1P2 ",		"C2P1 ",		"C2P2 ",		"C3P1 ",	"C3P2 ",		"C4P1 ",		"C4P2 ",		"PTCL1",		"PTCL2",		"PTCL3",
"PTCL4",		"IPADDR1 ",		"IPADDR2 ",		"IPADDR3 ",	"IPADDR4 ",		"IPADDR5 ",		"SMSADDR1 ",	"SMSADDR2 ",	"SMSADDR3 ",	"SMSADDR4 ",
"SMSADDR5 ",	"IPTYPE1 ",		"IPTYPE2 ",		"IPTYPE3 ",	"IPTYPE4 ",		"IPTYPE5 ",		"HEART1 ",		"HEART2 ",		"HEART3 ",		"HEART4 ",
"HEART5 ",		"DTUMODE1 ",	"DTUMODE2 ",	"DTUMODE3 ","DTUMODE4 ",	"DTUMODE5 ",	"IPCH1 ",		"IPCH2 ",		"IPCH3 ",		"IPCH4 ",
"IPCH5 ",		"SMSCH1 ",		"SMSCH2 ",		"SMSCH3 ",	"SMSCH4 ",		"SMSCH5 ",		"DTUAPN ",		"FIRMWARE "
};



static rt_uint8_t _my_param_type_identify(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint8_t param_type;
	
	if(0 == data_len)
	{
		return MY_PARAM_TYPE_NONE;
	}

	for(param_type = 0; param_type < MY_NUM_PARAM_TYPE; param_type++)
	{
		if(0 == strncmp(m_my_param_type[param_type], (char *)pdata, data_len))
		{
			return param_type;
		}
	}

	return MY_PARAM_TYPE_NONE;
}

static rt_uint16_t _my_crc_value(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint8_t	temp = 0;
	rt_uint16_t	crc_value;
	
	if(0 == data_len)
	{
		return 0;
	}

	while(data_len)
	{
		data_len--;
		temp += *pdata++;
	}
	temp = ~temp + 1;
	crc_value = temp >> 4;
	crc_value <<= 8;
	crc_value += (temp & 0x0f);
	crc_value += 0x6161;
	
	return crc_value;
}

static rt_uint8_t _my_comm_type_identify(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	if(0 == data_len)
	{
		return PTCL_NUM_COMM_TYPE;
	}
	
	//MY_STR_COMM_NONE
	if(0 == strncmp(MY_STR_COMM_NONE, (char *)pdata, data_len))
	{
		return PTCL_COMM_TYPE_NONE;
	}
	//MY_STR_COMM_SMS
	if(0 == strncmp(MY_STR_COMM_SMS, (char *)pdata, data_len))
	{
		return PTCL_COMM_TYPE_SMS;
	}
	//MY_STR_COMM_GPRS
	if(0 == strncmp(MY_STR_COMM_GPRS, (char *)pdata, data_len))
	{
		return PTCL_COMM_TYPE_IP;
	}
	//MY_STR_COMM_TC
	if(0 == strncmp(MY_STR_COMM_TC, (char *)pdata, data_len))
	{
		return PTCL_COMM_TYPE_TC;
	}
	
	return PTCL_NUM_COMM_TYPE;
}

static rt_uint8_t _my_ptcl_type_identify(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	if(0 == data_len)
	{
		return PTCL_NUM_PTCL_TYPE;
	}
	
	//MY_STR_SL427
	if(0 == strncmp(MY_STR_SL427, (char *)pdata, data_len))
	{
		return PTCL_PTCL_TYPE_SL427;
	}
	//MY_STR_SL651
	if(0 == strncmp(MY_STR_SL651, (char *)pdata, data_len))
	{
		return PTCL_PTCL_TYPE_SL651;
	}
	//MY_STR_HJ212
	if(0 == strncmp(MY_STR_HJ212, (char *)pdata, data_len))
	{
		return PTCL_PTCL_TYPE_HJ212;
	}
	
	return PTCL_NUM_PTCL_TYPE;
}

static rt_uint8_t _my_dtu_mode_identify(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	if(0 == data_len)
	{
		return DTU_NUM_RUN_MODE;
	}
	
	//MY_STR_DTUMODE_PWR
	if(0 == strncmp(MY_STR_DTUMODE_PWR, (char *)pdata, data_len))
	{
		return DTU_RUN_MODE_PWR;
	}
	//MY_STR_DTUMODE_ONLINE
	if(0 == strncmp(MY_STR_DTUMODE_ONLINE, (char *)pdata, data_len))
	{
		return DTU_RUN_MODE_ONLINE;
	}
	//MY_STR_DTUMODE_OFFLINE
	if(0 == strncmp(MY_STR_DTUMODE_OFFLINE, (char *)pdata, data_len))
	{
		return DTU_RUN_MODE_OFFLINE;
	}
	
	return DTU_NUM_RUN_MODE;
}

static rt_uint16_t _my_param_tt(rt_uint8_t *pdata, rt_uint16_t data_len, struct tm time)
{
	rt_uint16_t len;
	
	len = snprintf((char *)pdata, data_len, "TT %02d%02d%02d%02d%02d%02d ", time.tm_year % 100, time.tm_mon + 1, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
	if(len >= data_len)
	{
		len = 0;
	}

	return len;
}

static rt_uint16_t _my_param_ver(rt_uint8_t *pdata, rt_uint16_t data_len, char const *pstr)
{
	rt_uint16_t len;
	
	len = snprintf((char *)pdata, data_len, "VER %s ", pstr);
	if(len >= data_len)
	{
		len = 0;
	}

	return len;
}

static rt_uint16_t _my_param_ca(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t center)
{
	rt_uint16_t len;
	
	if(center >= PTCL_NUM_CENTER)
	{
		return 0;
	}
	
	len = snprintf((char *)pdata, data_len, "CA%d %d ", center + 1, ptcl_get_center_addr(center));
	if(len >= data_len)
	{
		len = 0;
	}

	return len;
}

static rt_uint16_t _my_param_cp(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t center, rt_uint8_t prio)
{
	rt_uint16_t len;
	
	if(center >= PTCL_NUM_CENTER)
	{
		return 0;
	}
	if(prio >= PTCL_NUM_COMM_PRIO)
	{
		return 0;
	}
	
	switch(ptcl_get_comm_type(center, prio))
	{
	default:
		len = snprintf((char *)pdata, data_len, "C%dP%d ---- ", center + 1, prio + 1);
		break;
	case PTCL_COMM_TYPE_NONE:
		len = snprintf((char *)pdata, data_len, "C%dP%d %s", center + 1, prio + 1, MY_STR_COMM_NONE);
		break;
	case PTCL_COMM_TYPE_SMS:
		len = snprintf((char *)pdata, data_len, "C%dP%d %s", center + 1, prio + 1, MY_STR_COMM_SMS);
		break;
	case PTCL_COMM_TYPE_IP:
		len = snprintf((char *)pdata, data_len, "C%dP%d %s", center + 1, prio + 1, MY_STR_COMM_GPRS);
		break;
	case PTCL_COMM_TYPE_TC:
		len = snprintf((char *)pdata, data_len, "C%dP%d %s", center + 1, prio + 1, MY_STR_COMM_TC);
		break;
	}
	if(len >= data_len)
	{
		len = 0;
	}
	
	return len;
}

static rt_uint16_t _my_param_ptcl_type(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t center)
{
	rt_uint16_t len;
	
	if(center >= PTCL_NUM_CENTER)
	{
		return 0;
	}
	
	switch(ptcl_get_ptcl_type(center))
	{
	default:
		len = snprintf((char *)pdata, data_len, "PTCL%d ---- ", center + 1);
		break;
	case PTCL_PTCL_TYPE_HJ212:
		len = snprintf((char *)pdata, data_len, "PTCL%d %s", center + 1, MY_STR_HJ212);
		break;
	case PTCL_PTCL_TYPE_SL427:
		len = snprintf((char *)pdata, data_len, "PTCL%d %s", center + 1, MY_STR_SL427);
		break;
	case PTCL_PTCL_TYPE_SL651:
		len = snprintf((char *)pdata, data_len, "PTCL%d %s", center + 1, MY_STR_SL651);
		break;
	}
	if(len >= data_len)
	{
		len = 0;
	}
	
	return len;
}

static rt_uint16_t _my_param_ip_addr(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t ch)
{
	rt_uint16_t		len;
	DTU_IP_ADDR		*ip_addr_ptr;
	
	if(ch >= DTU_NUM_IP_CH)
	{
		return 0;
	}

	ip_addr_ptr = (DTU_IP_ADDR *)mempool_req(sizeof(DTU_IP_ADDR), RT_WAITING_NO);
	if((DTU_IP_ADDR *)0 == ip_addr_ptr)
	{
		return 0;
	}
	dtu_get_ip_addr(ip_addr_ptr, ch);

	if(ip_addr_ptr->addr_len)
	{
		len = snprintf((char *)pdata, data_len, "IPADDR%d ", ch + 1);
		if(len >= data_len)
		{
			rt_mp_free((void *)ip_addr_ptr);
			return 0;
		}
		data_len -= len;
		if((ip_addr_ptr->addr_len + 1) > data_len)
		{
			rt_mp_free((void *)ip_addr_ptr);
			return 0;
		}
		memcpy(pdata + len, ip_addr_ptr->addr_data, ip_addr_ptr->addr_len);
		len += ip_addr_ptr->addr_len;
		pdata[len++] = MY_FRAME_SEPARATOR;
	}
	else
	{
		len = snprintf((char *)pdata, data_len, "IPADDR%d 0 ", ch + 1);
		if(len >= data_len)
		{
			rt_mp_free((void *)ip_addr_ptr);
			return 0;
		}
	}

	rt_mp_free((void *)ip_addr_ptr);
	
	return len;
}

static rt_uint16_t _my_param_sms_addr(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t ch)
{
	rt_uint16_t		len;
	DTU_SMS_ADDR	*sms_addr_ptr;
	
	if(ch >= DTU_NUM_SMS_CH)
	{
		return 0;
	}

	sms_addr_ptr = (DTU_SMS_ADDR *)mempool_req(sizeof(DTU_SMS_ADDR), RT_WAITING_NO);
	if((DTU_SMS_ADDR *)0 == sms_addr_ptr)
	{
		return 0;
	}
	dtu_get_sms_addr(sms_addr_ptr, ch);

	if(sms_addr_ptr->addr_len)
	{
		len = snprintf((char *)pdata, data_len, "SMSADDR%d ", ch + 1);
		if(len >= data_len)
		{
			rt_mp_free((void *)sms_addr_ptr);
			return 0;
		}
		data_len -= len;
		if((sms_addr_ptr->addr_len + 1) > data_len)
		{
			rt_mp_free((void *)sms_addr_ptr);
			return 0;
		}
		memcpy(pdata + len, sms_addr_ptr->addr_data, sms_addr_ptr->addr_len);
		len += sms_addr_ptr->addr_len;
		pdata[len++] = MY_FRAME_SEPARATOR;
	}
	else
	{
		len = snprintf((char *)pdata, data_len, "SMSADDR%d 0 ", ch + 1);
		if(len >= data_len)
		{
			rt_mp_free((void *)sms_addr_ptr);
			return 0;
		}
	}

	rt_mp_free((void *)sms_addr_ptr);
	
	return len;
}

static rt_uint16_t _my_param_ip_type(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t ch)
{
	rt_uint16_t len;
	
	if(ch >= DTU_NUM_IP_CH)
	{
		return 0;
	}
	
	if(RT_TRUE == dtu_get_ip_type(ch))
	{
		len = snprintf((char *)pdata, data_len, "IPTYPE%d %s", ch + 1, MY_STR_UDP);
	}
	else
	{
		len = snprintf((char *)pdata, data_len, "IPTYPE%d %s", ch + 1, MY_STR_TCP);
	}
	
	if(len >= data_len)
	{
		len = 0;
	}
	
	return len;
}

static rt_uint16_t _my_param_ip_ch(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t ch)
{
	rt_uint16_t len;
	
	if(ch >= DTU_NUM_IP_CH)
	{
		return 0;
	}
	
	if(RT_TRUE == dtu_get_ip_ch(ch))
	{
		len = snprintf((char *)pdata, data_len, "IPCH%d %s", ch + 1, MY_STR_ENABLE);
	}
	else
	{
		len = snprintf((char *)pdata, data_len, "IPCH%d %s", ch + 1, MY_STR_DISABLE);
	}
	
	if(len >= data_len)
	{
		len = 0;
	}
	
	return len;
}

static rt_uint16_t _my_param_sms_ch(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t ch)
{
	rt_uint16_t len;
	
	if(ch >= DTU_NUM_SMS_CH)
	{
		return 0;
	}
	
	if(RT_TRUE == dtu_get_sms_ch(ch))
	{
		len = snprintf((char *)pdata, data_len, "SMSCH%d %s", ch + 1, MY_STR_ENABLE);
	}
	else
	{
		len = snprintf((char *)pdata, data_len, "SMSCH%d %s", ch + 1, MY_STR_DISABLE);
	}
	
	if(len >= data_len)
	{
		len = 0;
	}
	
	return len;
}

static rt_uint16_t _my_param_dtu_apn(rt_uint8_t *pdata, rt_uint16_t data_len)
{
	rt_uint16_t	len;
	DTU_APN		*dtu_apn_ptr;
	
	dtu_apn_ptr = (DTU_APN *)mempool_req(sizeof(DTU_APN), RT_WAITING_NO);
	if((DTU_APN *)0 == dtu_apn_ptr)
	{
		return 0;
	}
	dtu_get_apn(dtu_apn_ptr);

	if(dtu_apn_ptr->apn_len)
	{
		len = snprintf((char *)pdata, data_len, "DTUAPN ");
		if(len >= data_len)
		{
			rt_mp_free((void *)dtu_apn_ptr);
			return 0;
		}
		data_len -= len;
		if((dtu_apn_ptr->apn_len + 1) > data_len)
		{
			rt_mp_free((void *)dtu_apn_ptr);
			return 0;
		}
		memcpy(pdata + len, dtu_apn_ptr->apn_data, dtu_apn_ptr->apn_len);
		len += dtu_apn_ptr->apn_len;
		pdata[len++] = MY_FRAME_SEPARATOR;
	}
	else
	{
		len = snprintf((char *)pdata, data_len, "DTUAPN 0 ");
		if(len >= data_len)
		{
			rt_mp_free((void *)dtu_apn_ptr);
			return 0;
		}
	}

	rt_mp_free((void *)dtu_apn_ptr);
	
	return len;
}

static rt_uint16_t _my_param_heart(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t ch)
{
	rt_uint16_t len;

	if(ch >= DTU_NUM_IP_CH)
	{
		return 0;
	}

	len = snprintf((char *)pdata, data_len, "HEART%d %d ", ch + 1, dtu_get_heart_period(ch));
	if(len >= data_len)
	{
		len = 0;
	}
	
	return len;
}

static rt_uint16_t _my_param_dtu_mode(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t ch)
{
	rt_uint16_t len;

	if(ch >= DTU_NUM_IP_CH)
	{
		return 0;
	}
	
	switch(dtu_get_run_mode(ch))
	{
	default:
		len = snprintf((char *)pdata, data_len, "DTUMODE%d ---- ", ch + 1);
		break;
	case DTU_RUN_MODE_PWR:
		len = snprintf((char *)pdata, data_len, "DTUMODE%d %s", ch + 1, MY_STR_DTUMODE_PWR);
		break;
	case DTU_RUN_MODE_ONLINE:
		len = snprintf((char *)pdata, data_len, "DTUMODE%d %s", ch + 1, MY_STR_DTUMODE_ONLINE);
		break;
	case DTU_RUN_MODE_OFFLINE:
		len = snprintf((char *)pdata, data_len, "DTUMODE%d %s", ch + 1, MY_STR_DTUMODE_OFFLINE);
		break;
	}
	if(len >= data_len)
	{
		len = 0;
	}
	
	return len;
}



//数据解包
rt_uint8_t my_data_decoder(PTCL_REPORT_DATA *report_data_ptr, PTCL_RECV_DATA const *recv_data_ptr, PTCL_PARAM_INFO **param_info_ptr_ptr)
{
	rt_uint8_t		is_param = RT_TRUE, param;
	rt_uint16_t		i, crc_value, cmd_pos, cmd_len;
	PTCL_PARAM_INFO	*param_info_ptr;

	if((PTCL_REPORT_DATA *)0 == report_data_ptr)
	{
		return RT_FALSE;
	}
	if((PTCL_RECV_DATA *)0 == recv_data_ptr)
	{
		return RT_FALSE;
	}
	if((PTCL_PARAM_INFO **)0 == param_info_ptr_ptr)
	{
		return RT_FALSE;
	}
	
	//iiNN
	if(recv_data_ptr->data_len <= MY_BYTES_FRAME_END)
	{
		return RT_FALSE;
	}
	i = recv_data_ptr->data_len - MY_BYTES_FRAME_END;
	cmd_pos = _my_crc_value(recv_data_ptr->pdata, i);
	crc_value = recv_data_ptr->pdata[i++];
	crc_value <<= 8;
	crc_value += recv_data_ptr->pdata[i++];
	if(cmd_pos != crc_value)
	{
		if(MY_FRAME_SUPER_CRC != (rt_uint8_t)(crc_value >> 8))
		{
			return RT_FALSE;
		}
		if(MY_FRAME_SUPER_CRC != (rt_uint8_t)crc_value)
		{
			return RT_FALSE;
		}
	}
	if(MY_FRAME_END != recv_data_ptr->pdata[i++])
	{
		return RT_FALSE;
	}
	if(MY_FRAME_END != recv_data_ptr->pdata[i])
	{
		return RT_FALSE;
	}
	//SM_
	i = 0;
	if((i + MY_BYTES_FRAME_HEAD) > recv_data_ptr->data_len)
	{
		return RT_FALSE;
	}
	if(MY_FRAME_HEAD_1 != recv_data_ptr->pdata[i++])
	{
		return RT_FALSE;
	}
	if(MY_FRAME_HEAD_2 != recv_data_ptr->pdata[i++])
	{
		return RT_FALSE;
	}
	if(MY_FRAME_SEPARATOR != recv_data_ptr->pdata[i++])
	{
		return RT_FALSE;
	}
	//应答包帧头
	report_data_ptr->data_len = 0;
	if((report_data_ptr->data_len + MY_BYTES_FRAME_HEAD) > PTCL_BYTES_ACK_REPORT)
	{
		return RT_TRUE;
	}
	report_data_ptr->pdata[report_data_ptr->data_len++] = MY_FRAME_HEAD_1;
	report_data_ptr->pdata[report_data_ptr->data_len++] = MY_FRAME_HEAD_2;
	report_data_ptr->pdata[report_data_ptr->data_len++] = MY_FRAME_SEPARATOR;
	
	cmd_pos = i;
	while(i < recv_data_ptr->data_len)
	{
		if(MY_FRAME_SEPARATOR == recv_data_ptr->pdata[i++])
		{
			cmd_len = i - cmd_pos;
			if(RT_TRUE == is_param)
			{
				param = _my_param_type_identify(recv_data_ptr->pdata + cmd_pos, cmd_len);
				switch(param)
				{
				//不识别的参数
				case MY_PARAM_TYPE_NONE:
					break;
				//复位
				case MY_PARAM_TYPE_RESET:
					if((report_data_ptr->data_len + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
					{
						if(RT_TRUE == ptcl_run_sta_set(PTCL_RUN_STA_RESET))
						{
							rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)(recv_data_ptr->pdata + cmd_pos), cmd_len);
							report_data_ptr->data_len += cmd_len;
						}
					}
					break;
				//重置
				case MY_PARAM_TYPE_RESTORE:
					if((report_data_ptr->data_len + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
					{
						if(RT_TRUE == ptcl_run_sta_set(PTCL_RUN_STA_RESTORE))
						{
							rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)(recv_data_ptr->pdata + cmd_pos), cmd_len);
							report_data_ptr->data_len += cmd_len;
						}
					}
					break;
				default:
					is_param = RT_FALSE;
					break;
				}
			}
			else
			{
				switch(param)
				{
				case MY_PARAM_TYPE_TT:
					if(0 == strncmp(MY_STR_QUERY, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						report_data_ptr->data_len += _my_param_tt(report_data_ptr->pdata + report_data_ptr->data_len, PTCL_BYTES_ACK_REPORT - report_data_ptr->data_len - MY_BYTES_FRAME_END, rtcee_rtc_get_calendar());
					}
					else if(13 == cmd_len)
					{
						if(RT_TRUE == fyyp_is_number(recv_data_ptr->pdata + cmd_pos, cmd_len - 1))
						{
							if((report_data_ptr->data_len + strlen(m_my_param_type[param]) + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
							{
								//申请参数信息
								param_info_ptr = ptcl_param_info_req(sizeof(struct tm), RT_WAITING_NO);
								if((PTCL_PARAM_INFO *)0 != param_info_ptr)
								{
									//参数信息赋值
									param_info_ptr->param_type = PTCL_PARAM_INFO_TT;
									((struct tm *)(param_info_ptr->pdata))->tm_year		= fyyp_str_to_hex(recv_data_ptr->pdata + cmd_pos, 2) + 2000;
									((struct tm *)(param_info_ptr->pdata))->tm_mon		= fyyp_str_to_hex(recv_data_ptr->pdata + cmd_pos + 2, 2) - 1;
									((struct tm *)(param_info_ptr->pdata))->tm_mday		= fyyp_str_to_hex(recv_data_ptr->pdata + cmd_pos + 4, 2);
									((struct tm *)(param_info_ptr->pdata))->tm_hour		= fyyp_str_to_hex(recv_data_ptr->pdata + cmd_pos + 6, 2);
									((struct tm *)(param_info_ptr->pdata))->tm_min		= fyyp_str_to_hex(recv_data_ptr->pdata + cmd_pos + 8, 2);
									((struct tm *)(param_info_ptr->pdata))->tm_sec		= fyyp_str_to_hex(recv_data_ptr->pdata + cmd_pos + 10, 2);
									*(struct tm *)(param_info_ptr->pdata) = rtcee_rtc_unix_to_calendar(rtcee_rtc_calendar_to_unix(*(struct tm *)(param_info_ptr->pdata)));
									//参数信息放入链表
									ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
									//回复
									report_data_ptr->data_len += sprintf((char *)(report_data_ptr->pdata + report_data_ptr->data_len), "%s", m_my_param_type[param]);
									rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)(recv_data_ptr->pdata + cmd_pos), cmd_len);
									report_data_ptr->data_len += cmd_len;
								}
							}
						}
					}
					break;
				case MY_PARAM_TYPE_VER:
					if(0 == strncmp(MY_STR_QUERY, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						report_data_ptr->data_len += _my_param_ver(report_data_ptr->pdata + report_data_ptr->data_len, PTCL_BYTES_ACK_REPORT - report_data_ptr->data_len - MY_BYTES_FRAME_END, PTCL_SOFTWARE_EDITION);
					}
					break;
				case MY_PARAM_TYPE_CA1:
				case MY_PARAM_TYPE_CA2:
				case MY_PARAM_TYPE_CA3:
				case MY_PARAM_TYPE_CA4:
					if(0 == strncmp(MY_STR_QUERY, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						report_data_ptr->data_len += _my_param_ca(report_data_ptr->pdata + report_data_ptr->data_len, PTCL_BYTES_ACK_REPORT - report_data_ptr->data_len - MY_BYTES_FRAME_END, param - MY_PARAM_TYPE_CA1);
					}
					else if(RT_TRUE == fyyp_is_number(recv_data_ptr->pdata + cmd_pos, cmd_len - 1))
					{
						crc_value = fyyp_str_to_hex(recv_data_ptr->pdata + cmd_pos, cmd_len - 1);
						if(crc_value < 0x100)
						{
							if((report_data_ptr->data_len + strlen(m_my_param_type[param]) + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
							{
								if((param - MY_PARAM_TYPE_CA1) < PTCL_NUM_CENTER)
								{
									//申请参数信息
									param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
									if((PTCL_PARAM_INFO *)0 != param_info_ptr)
									{
										//参数信息赋值
										param_info_ptr->param_type	= PTCL_PARAM_INFO_CENTER_ADDR;
										param_info_ptr->data_len	= param - MY_PARAM_TYPE_CA1;
										*(param_info_ptr->pdata)	= crc_value;
										//参数信息放入链表
										ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
										//回复
										report_data_ptr->data_len += sprintf((char *)(report_data_ptr->pdata + report_data_ptr->data_len), "%s", m_my_param_type[param]);
										rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)(recv_data_ptr->pdata + cmd_pos), cmd_len);
										report_data_ptr->data_len += cmd_len;
									}
								}
							}
						}
					}
					break;
				case MY_PARAM_TYPE_C1P1:
				case MY_PARAM_TYPE_C1P2:
				case MY_PARAM_TYPE_C2P1:
				case MY_PARAM_TYPE_C2P2:
				case MY_PARAM_TYPE_C3P1:
				case MY_PARAM_TYPE_C3P2:
				case MY_PARAM_TYPE_C4P1:
				case MY_PARAM_TYPE_C4P2:
					if(0 == strncmp(MY_STR_QUERY, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						param -= MY_PARAM_TYPE_C1P1;
						report_data_ptr->data_len += _my_param_cp(report_data_ptr->pdata + report_data_ptr->data_len, PTCL_BYTES_ACK_REPORT - report_data_ptr->data_len - MY_BYTES_FRAME_END, param / PTCL_NUM_COMM_PRIO, param % PTCL_NUM_COMM_PRIO);
					}
					else
					{
						crc_value = _my_comm_type_identify(recv_data_ptr->pdata + cmd_pos, cmd_len);
						if(crc_value < PTCL_NUM_COMM_TYPE)
						{
							if((report_data_ptr->data_len + strlen(m_my_param_type[param]) + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
							{
								if(((param - MY_PARAM_TYPE_C1P1) / PTCL_NUM_COMM_PRIO) < PTCL_NUM_CENTER)
								{
									//申请参数信息
									param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
									if((PTCL_PARAM_INFO *)0 != param_info_ptr)
									{
										//参数信息赋值
										param_info_ptr->param_type	= PTCL_PARAM_INFO_COMM_TYPE;
										param_info_ptr->data_len	= param - MY_PARAM_TYPE_C1P1;
										*(param_info_ptr->pdata)	= crc_value;
										//参数信息放入链表
										ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
										//回复
										report_data_ptr->data_len += sprintf((char *)(report_data_ptr->pdata + report_data_ptr->data_len), "%s", m_my_param_type[param]);
										rt_memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)(recv_data_ptr->pdata + cmd_pos), cmd_len);
										report_data_ptr->data_len += cmd_len;
									}
								}
							}
						}
					}
					break;
				case MY_PARAM_TYPE_PTCL1:
				case MY_PARAM_TYPE_PTCL2:
				case MY_PARAM_TYPE_PTCL3:
				case MY_PARAM_TYPE_PTCL4:
					if(0 == strncmp(MY_STR_QUERY, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						report_data_ptr->data_len += _my_param_ptcl_type(report_data_ptr->pdata + report_data_ptr->data_len, PTCL_BYTES_ACK_REPORT - report_data_ptr->data_len - MY_BYTES_FRAME_END, param - MY_PARAM_TYPE_PTCL1);
					}
					else
					{
						crc_value = _my_ptcl_type_identify(recv_data_ptr->pdata + cmd_pos, cmd_len);
						if(crc_value < PTCL_NUM_PTCL_TYPE)
						{
							if((report_data_ptr->data_len + strlen(m_my_param_type[param]) + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
							{
								if((param - MY_PARAM_TYPE_PTCL1) < PTCL_NUM_CENTER)
								{
									//申请参数信息
									param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
									if((PTCL_PARAM_INFO *)0 != param_info_ptr)
									{
										//参数信息赋值
										param_info_ptr->param_type	= PTCL_PARAM_INFO_PTCL_TYPE;
										param_info_ptr->data_len	= param - MY_PARAM_TYPE_PTCL1;
										*(param_info_ptr->pdata)	= crc_value;
										//参数信息放入链表
										ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
										//回复
										report_data_ptr->data_len += sprintf((char *)(report_data_ptr->pdata + report_data_ptr->data_len), "%s", m_my_param_type[param]);
										memcpy((void *)(report_data_ptr->pdata + report_data_ptr->data_len), (void *)(recv_data_ptr->pdata + cmd_pos), cmd_len);
										report_data_ptr->data_len += cmd_len;
									}
								}
							}
						}
					}
					break;
				case MY_PARAM_TYPE_IPADDR1:
				case MY_PARAM_TYPE_IPADDR2:
				case MY_PARAM_TYPE_IPADDR3:
				case MY_PARAM_TYPE_IPADDR4:
				case MY_PARAM_TYPE_IPADDR5:
					if(0 == strncmp(MY_STR_QUERY, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						report_data_ptr->data_len += _my_param_ip_addr(report_data_ptr->pdata + report_data_ptr->data_len, PTCL_BYTES_ACK_REPORT - report_data_ptr->data_len - MY_BYTES_FRAME_END, param - MY_PARAM_TYPE_IPADDR1);
					}
					else if((cmd_len - 1) <= DTU_BYTES_IP_ADDR)
					{
						if((report_data_ptr->data_len + strlen(m_my_param_type[param]) + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
						{
							if((param - MY_PARAM_TYPE_IPADDR1) < DTU_NUM_IP_CH)
							{
								//申请参数信息
								param_info_ptr = ptcl_param_info_req(sizeof(DTU_IP_ADDR), RT_WAITING_NO);
								if((PTCL_PARAM_INFO *)0 != param_info_ptr)
								{
									//参数信息赋值
									param_info_ptr->param_type	= PTCL_PARAM_INFO_IP_ADDR;
									param_info_ptr->data_len	= param - MY_PARAM_TYPE_IPADDR1;
									memcpy(((DTU_IP_ADDR *)(param_info_ptr->pdata))->addr_data, recv_data_ptr->pdata + cmd_pos, cmd_len - 1);
									((DTU_IP_ADDR *)(param_info_ptr->pdata))->addr_len = cmd_len - 1;
									//参数信息放入链表
									ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
									//回复
									report_data_ptr->data_len += sprintf((char *)(report_data_ptr->pdata + report_data_ptr->data_len), "%s", m_my_param_type[param]);
									memcpy(report_data_ptr->pdata + report_data_ptr->data_len, recv_data_ptr->pdata + cmd_pos, cmd_len);
									report_data_ptr->data_len += cmd_len;
								}
							}
						}
					}
					break;
				case MY_PARAM_TYPE_SMSADDR1:
				case MY_PARAM_TYPE_SMSADDR2:
				case MY_PARAM_TYPE_SMSADDR3:
				case MY_PARAM_TYPE_SMSADDR4:
				case MY_PARAM_TYPE_SMSADDR5:
					if(0 == strncmp(MY_STR_QUERY, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						report_data_ptr->data_len += _my_param_sms_addr(report_data_ptr->pdata + report_data_ptr->data_len, PTCL_BYTES_ACK_REPORT - report_data_ptr->data_len - MY_BYTES_FRAME_END, param - MY_PARAM_TYPE_SMSADDR1);
					}
					else if((cmd_len - 1) <= DTU_BYTES_SMS_ADDR)
					{
						if((report_data_ptr->data_len + strlen(m_my_param_type[param]) + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
						{
							if((param - MY_PARAM_TYPE_SMSADDR1) < DTU_NUM_SMS_CH)
							{
								//申请参数信息
								param_info_ptr = ptcl_param_info_req(sizeof(DTU_SMS_ADDR), RT_WAITING_NO);
								if((PTCL_PARAM_INFO *)0 != param_info_ptr)
								{
									//参数信息赋值
									param_info_ptr->param_type	= PTCL_PARAM_INFO_SMS_ADDR;
									param_info_ptr->data_len	= param - MY_PARAM_TYPE_SMSADDR1;
									memcpy(((DTU_SMS_ADDR *)(param_info_ptr->pdata))->addr_data, recv_data_ptr->pdata + cmd_pos, cmd_len - 1);
									((DTU_SMS_ADDR *)(param_info_ptr->pdata))->addr_len = cmd_len - 1;
									//参数信息放入链表
									ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
									//回复
									report_data_ptr->data_len += sprintf((char *)(report_data_ptr->pdata + report_data_ptr->data_len), "%s", m_my_param_type[param]);
									memcpy(report_data_ptr->pdata + report_data_ptr->data_len, recv_data_ptr->pdata + cmd_pos, cmd_len);
									report_data_ptr->data_len += cmd_len;
								}
							}
						}
					}
					break;
				case MY_PARAM_TYPE_IPTYPE1:
				case MY_PARAM_TYPE_IPTYPE2:
				case MY_PARAM_TYPE_IPTYPE3:
				case MY_PARAM_TYPE_IPTYPE4:
				case MY_PARAM_TYPE_IPTYPE5:
					if(0 == strncmp(MY_STR_QUERY, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						report_data_ptr->data_len += _my_param_ip_type(report_data_ptr->pdata + report_data_ptr->data_len, PTCL_BYTES_ACK_REPORT - report_data_ptr->data_len - MY_BYTES_FRAME_END, param - MY_PARAM_TYPE_IPTYPE1);
					}
					else if(0 == strncmp(MY_STR_TCP, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						if((report_data_ptr->data_len + strlen(m_my_param_type[param]) + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
						{
							if((param - MY_PARAM_TYPE_IPTYPE1) < DTU_NUM_IP_CH)
							{
								//申请参数信息
								param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
								if((PTCL_PARAM_INFO *)0 != param_info_ptr)
								{
									//参数信息赋值
									param_info_ptr->param_type	= PTCL_PARAM_INFO_IP_TYPE;
									param_info_ptr->data_len	= param - MY_PARAM_TYPE_IPTYPE1;
									*(param_info_ptr->pdata)	= RT_FALSE;
									//参数信息放入链表
									ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
									//回复
									report_data_ptr->data_len += sprintf((char *)(report_data_ptr->pdata + report_data_ptr->data_len), "%s", m_my_param_type[param]);
									memcpy(report_data_ptr->pdata + report_data_ptr->data_len, recv_data_ptr->pdata + cmd_pos, cmd_len);
									report_data_ptr->data_len += cmd_len;
								}
							}
						}
					}
					else if(0 == strncmp(MY_STR_UDP, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						if((report_data_ptr->data_len + strlen(m_my_param_type[param]) + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
						{
							if((param - MY_PARAM_TYPE_IPTYPE1) < DTU_NUM_IP_CH)
							{
								//申请参数信息
								param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
								if((PTCL_PARAM_INFO *)0 != param_info_ptr)
								{
									//参数信息赋值
									param_info_ptr->param_type	= PTCL_PARAM_INFO_IP_TYPE;
									param_info_ptr->data_len	= param - MY_PARAM_TYPE_IPTYPE1;
									*(param_info_ptr->pdata)	= RT_TRUE;
									//参数信息放入链表
									ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
									//回复
									report_data_ptr->data_len += sprintf((char *)(report_data_ptr->pdata + report_data_ptr->data_len), "%s", m_my_param_type[param]);
									memcpy(report_data_ptr->pdata + report_data_ptr->data_len, recv_data_ptr->pdata + cmd_pos, cmd_len);
									report_data_ptr->data_len += cmd_len;
								}
							}
						}
					}
					break;
				case MY_PARAM_TYPE_IPCH1:
				case MY_PARAM_TYPE_IPCH2:
				case MY_PARAM_TYPE_IPCH3:
				case MY_PARAM_TYPE_IPCH4:
				case MY_PARAM_TYPE_IPCH5:
					if(0 == strncmp(MY_STR_QUERY, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						report_data_ptr->data_len += _my_param_ip_ch(report_data_ptr->pdata + report_data_ptr->data_len, PTCL_BYTES_ACK_REPORT - report_data_ptr->data_len - MY_BYTES_FRAME_END, param - MY_PARAM_TYPE_IPCH1);
					}
					else if(0 == strncmp(MY_STR_DISABLE, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						if((report_data_ptr->data_len + strlen(m_my_param_type[param]) + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
						{
							if((param - MY_PARAM_TYPE_IPCH1) < DTU_NUM_IP_CH)
							{
								//申请参数信息
								param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
								if((PTCL_PARAM_INFO *)0 != param_info_ptr)
								{
									//参数信息赋值
									param_info_ptr->param_type	= PTCL_PARAM_INFO_IP_CH;
									param_info_ptr->data_len	= param - MY_PARAM_TYPE_IPCH1;
									*(param_info_ptr->pdata)	= RT_FALSE;
									//参数信息放入链表
									ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
									//回复
									report_data_ptr->data_len += sprintf((char *)(report_data_ptr->pdata + report_data_ptr->data_len), "%s", m_my_param_type[param]);
									memcpy(report_data_ptr->pdata + report_data_ptr->data_len, recv_data_ptr->pdata + cmd_pos, cmd_len);
									report_data_ptr->data_len += cmd_len;
								}
							}
						}
					}
					else if(0 == strncmp(MY_STR_ENABLE, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						if((report_data_ptr->data_len + strlen(m_my_param_type[param]) + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
						{
							if((param - MY_PARAM_TYPE_IPCH1) < DTU_NUM_IP_CH)
							{
								//申请参数信息
								param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
								if((PTCL_PARAM_INFO *)0 != param_info_ptr)
								{
									//参数信息赋值
									param_info_ptr->param_type	= PTCL_PARAM_INFO_IP_CH;
									param_info_ptr->data_len	= param - MY_PARAM_TYPE_IPCH1;
									*(param_info_ptr->pdata)	= RT_TRUE;
									//参数信息放入链表
									ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
									//回复
									report_data_ptr->data_len += sprintf((char *)(report_data_ptr->pdata + report_data_ptr->data_len), "%s", m_my_param_type[param]);
									memcpy(report_data_ptr->pdata + report_data_ptr->data_len, recv_data_ptr->pdata + cmd_pos, cmd_len);
									report_data_ptr->data_len += cmd_len;
								}
							}
						}
					}
					break;
				case MY_PARAM_TYPE_SMSCH1:
				case MY_PARAM_TYPE_SMSCH2:
				case MY_PARAM_TYPE_SMSCH3:
				case MY_PARAM_TYPE_SMSCH4:
				case MY_PARAM_TYPE_SMSCH5:
					if(0 == strncmp(MY_STR_QUERY, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						report_data_ptr->data_len += _my_param_sms_ch(report_data_ptr->pdata + report_data_ptr->data_len, PTCL_BYTES_ACK_REPORT - report_data_ptr->data_len - MY_BYTES_FRAME_END, param - MY_PARAM_TYPE_SMSCH1);
					}
					else if(0 == strncmp(MY_STR_DISABLE, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						if((report_data_ptr->data_len + strlen(m_my_param_type[param]) + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
						{
							if((param - MY_PARAM_TYPE_SMSCH1) < DTU_NUM_SMS_CH)
							{
								//申请参数信息
								param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
								if((PTCL_PARAM_INFO *)0 != param_info_ptr)
								{
									//参数信息赋值
									param_info_ptr->param_type	= PTCL_PARAM_INFO_SMS_CH;
									param_info_ptr->data_len	= param - MY_PARAM_TYPE_SMSCH1;
									*(param_info_ptr->pdata)	= RT_FALSE;
									//参数信息放入链表
									ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
									//回复
									report_data_ptr->data_len += sprintf((char *)(report_data_ptr->pdata + report_data_ptr->data_len), "%s", m_my_param_type[param]);
									memcpy(report_data_ptr->pdata + report_data_ptr->data_len, recv_data_ptr->pdata + cmd_pos, cmd_len);
									report_data_ptr->data_len += cmd_len;
								}
							}
						}
					}
					else if(0 == strncmp(MY_STR_ENABLE, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						if((report_data_ptr->data_len + strlen(m_my_param_type[param]) + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
						{
							if((param - MY_PARAM_TYPE_SMSCH1) < DTU_NUM_SMS_CH)
							{
								//申请参数信息
								param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
								if((PTCL_PARAM_INFO *)0 != param_info_ptr)
								{
									//参数信息赋值
									param_info_ptr->param_type	= PTCL_PARAM_INFO_SMS_CH;
									param_info_ptr->data_len	= param - MY_PARAM_TYPE_SMSCH1;
									*(param_info_ptr->pdata)	= RT_TRUE;
									//参数信息放入链表
									ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
									//回复
									report_data_ptr->data_len += sprintf((char *)(report_data_ptr->pdata + report_data_ptr->data_len), "%s", m_my_param_type[param]);
									memcpy(report_data_ptr->pdata + report_data_ptr->data_len, recv_data_ptr->pdata + cmd_pos, cmd_len);
									report_data_ptr->data_len += cmd_len;
								}
							}
						}
					}
					break;
				case MY_PARAM_TYPE_DTUAPN:
					if(0 == strncmp(MY_STR_QUERY, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						report_data_ptr->data_len += _my_param_dtu_apn(report_data_ptr->pdata + report_data_ptr->data_len, PTCL_BYTES_ACK_REPORT - report_data_ptr->data_len - MY_BYTES_FRAME_END);
					}
					else if((cmd_len - 1) <= DTU_BYTES_APN)
					{
						if((report_data_ptr->data_len + strlen(m_my_param_type[param]) + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
						{
							//申请参数信息
							param_info_ptr = ptcl_param_info_req(sizeof(DTU_APN), RT_WAITING_NO);
							if((PTCL_PARAM_INFO *)0 != param_info_ptr)
							{
								//参数信息赋值
								param_info_ptr->param_type	= PTCL_PARAM_INFO_DTU_APN;
								memcpy(((DTU_APN *)(param_info_ptr->pdata))->apn_data, recv_data_ptr->pdata + cmd_pos, cmd_len - 1);
								((DTU_APN *)(param_info_ptr->pdata))->apn_len = cmd_len - 1;
								//参数信息放入链表
								ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
								//回复
								report_data_ptr->data_len += sprintf((char *)(report_data_ptr->pdata + report_data_ptr->data_len), "%s", m_my_param_type[param]);
								memcpy(report_data_ptr->pdata + report_data_ptr->data_len, recv_data_ptr->pdata + cmd_pos, cmd_len);
								report_data_ptr->data_len += cmd_len;
							}
						}
					}
					break;
				case MY_PARAM_TYPE_HEART1:
				case MY_PARAM_TYPE_HEART2:
				case MY_PARAM_TYPE_HEART3:
				case MY_PARAM_TYPE_HEART4:
				case MY_PARAM_TYPE_HEART5:
					if(0 == strncmp(MY_STR_QUERY, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						report_data_ptr->data_len += _my_param_heart(report_data_ptr->pdata + report_data_ptr->data_len, PTCL_BYTES_ACK_REPORT - report_data_ptr->data_len - MY_BYTES_FRAME_END, param - MY_PARAM_TYPE_HEART1);
					}
					else if((param - MY_PARAM_TYPE_HEART1) < DTU_NUM_IP_CH)
					{
						if(RT_TRUE == fyyp_is_number(recv_data_ptr->pdata + cmd_pos, cmd_len - 1))
						{
							if((report_data_ptr->data_len + strlen(m_my_param_type[param]) + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
							{
								//申请参数信息
								param_info_ptr = ptcl_param_info_req(sizeof(rt_uint16_t), RT_WAITING_NO);
								if((PTCL_PARAM_INFO *)0 != param_info_ptr)
								{
									//参数信息赋值
									param_info_ptr->param_type	= PTCL_PARAM_INFO_HEART_PERIOD;
									param_info_ptr->data_len	= param - MY_PARAM_TYPE_HEART1;
									*(rt_uint16_t *)(param_info_ptr->pdata) = fyyp_str_to_hex(recv_data_ptr->pdata + cmd_pos, cmd_len - 1);
									//参数信息放入链表
									ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
									//回复
									report_data_ptr->data_len += sprintf((char *)(report_data_ptr->pdata + report_data_ptr->data_len), "%s", m_my_param_type[param]);
									memcpy(report_data_ptr->pdata + report_data_ptr->data_len, recv_data_ptr->pdata + cmd_pos, cmd_len);
									report_data_ptr->data_len += cmd_len;
								}
							}
						}
					}
					break;
				case MY_PARAM_TYPE_DTUMODE1:
				case MY_PARAM_TYPE_DTUMODE2:
				case MY_PARAM_TYPE_DTUMODE3:
				case MY_PARAM_TYPE_DTUMODE4:
				case MY_PARAM_TYPE_DTUMODE5:
					if(0 == strncmp(MY_STR_QUERY, (char *)(recv_data_ptr->pdata + cmd_pos), cmd_len))
					{
						report_data_ptr->data_len += _my_param_dtu_mode(report_data_ptr->pdata + report_data_ptr->data_len, PTCL_BYTES_ACK_REPORT - report_data_ptr->data_len - MY_BYTES_FRAME_END, param - MY_PARAM_TYPE_DTUMODE1);
					}
					else if((param - MY_PARAM_TYPE_DTUMODE1) < DTU_NUM_IP_CH)
					{
						crc_value = _my_dtu_mode_identify(recv_data_ptr->pdata + cmd_pos, cmd_len);
						if(crc_value < DTU_NUM_RUN_MODE)
						{
							if((report_data_ptr->data_len + strlen(m_my_param_type[param]) + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
							{
								//申请参数信息
								param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
								if((PTCL_PARAM_INFO *)0 != param_info_ptr)
								{
									//参数信息赋值
									param_info_ptr->param_type	= PTCL_PARAM_INFO_DTU_MODE;
									param_info_ptr->data_len	= param - MY_PARAM_TYPE_DTUMODE1;
									*(param_info_ptr->pdata) 	= crc_value;
									//参数信息放入链表
									ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
									//回复
									report_data_ptr->data_len += sprintf((char *)(report_data_ptr->pdata + report_data_ptr->data_len), "%s", m_my_param_type[param]);
									memcpy(report_data_ptr->pdata + report_data_ptr->data_len, recv_data_ptr->pdata + cmd_pos, cmd_len);
									report_data_ptr->data_len += cmd_len;
								}
							}
						}
					}
					break;
				case MY_PARAM_TYPE_FIRMWARE:
					if((report_data_ptr->data_len + strlen(m_my_param_type[param]) + cmd_len + MY_BYTES_FRAME_END) <= PTCL_BYTES_ACK_REPORT)
					{
						if(RT_TRUE == http_get_req_trigger(HTTP_FILE_TYPE_FIRMWARE, recv_data_ptr->pdata + cmd_pos, cmd_len - 1))
						{
							//回复
							report_data_ptr->data_len += sprintf((char *)(report_data_ptr->pdata + report_data_ptr->data_len), "%s", m_my_param_type[param]);
							memcpy(report_data_ptr->pdata + report_data_ptr->data_len, recv_data_ptr->pdata + cmd_pos, cmd_len);
							report_data_ptr->data_len += cmd_len;
						}
					}
					break;
				}
				is_param = RT_TRUE;
			}
			cmd_pos = i;
		}
	}
	
	if(report_data_ptr->data_len > MY_BYTES_FRAME_HEAD)
	{
		crc_value = _my_crc_value(report_data_ptr->pdata, report_data_ptr->data_len);
		report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)(crc_value >> 8);
		report_data_ptr->pdata[report_data_ptr->data_len++] = (rt_uint8_t)crc_value;
		report_data_ptr->pdata[report_data_ptr->data_len++] = MY_FRAME_END;
		report_data_ptr->pdata[report_data_ptr->data_len++] = MY_FRAME_END;
		
		report_data_ptr->fun_csq_update		= (void *)0;
		report_data_ptr->data_id			= 0;
		report_data_ptr->need_reply			= RT_FALSE;
		report_data_ptr->fcb_value			= 0;
		report_data_ptr->ptcl_type			= PTCL_PTCL_TYPE_MYSELF;
	}
	else
	{
		report_data_ptr->data_len = 0;
	}
	
	return RT_TRUE;
}

