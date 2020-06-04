#include "smt_cfg.h"
#include "drv_fyyp.h"
#include "drv_mempool.h"
#include "drv_rtcee.h"
#include "dtu.h"
#include "xmodem.h"
#include "smt_manage.h"
#include "sample.h"
//#include "sl651.h"
#include "drv_debug.h"
#include "stm32f4xx.h"
#include "stdio.h"
#include "string.h"
#include "http.h"
#include "fatfs.h"
#include "drv_ads.h"
#include "drv_bluetooth.h"
#include "drv_ds18b20.h"
//#include "hj212.h"



static rt_uint16_t _smtcfg_item_decoder(rt_uint8_t is_wr, rt_uint8_t *pdst, rt_uint16_t max_len, PTCL_PARAM_INFO **param_info_ptr_ptr, rt_uint8_t const *psrc, rt_uint16_t src_len, rt_uint16_t grp, PTCL_RECV_DATA const *recv_data_ptr)
{
	rt_uint16_t		dst_len, i = 0, pos, len;
	void			*pmem;
	struct tm		time;
	PTCL_PARAM_INFO	*param_info_ptr;

	//'#'
	if((i + 1) >= src_len)
	{
		return 0;
	}
	if('#' != psrc[i++])
	{
		return 0;
	}
	grp <<= 8;
	//item
	if(RT_TRUE == is_wr)
	{
		grp += SMTCFG_TAG_WRITE;
		pos = i;
		while(i < src_len)
		{
			if(',' == psrc[i++])
			{
				break;
			}
		}
		if(i >= src_len)
		{
			return 0;
		}
		len = i - pos - 1;
		if(RT_FALSE == fyyp_is_number(psrc + pos, len))
		{
			return 0;
		}
		grp += fyyp_str_to_hex(psrc + pos, len);
	}
	else
	{
		grp += SMTCFG_TAG_READ;
		if(i >= src_len)
		{
			return 0;
		}
		len = src_len - i;
		if(RT_FALSE == fyyp_is_number(psrc + i, len))
		{
			return 0;
		}
		grp += fyyp_str_to_hex(psrc + i, len);
	}
	//应答帧头
	dst_len = snprintf((char *)pdst, max_len, "=#%d,100", (rt_uint8_t)grp);
	if(dst_len >= max_len)
	{
		return 0;
	}
	
	//解码
	switch(grp)
	{
	default:
		return 0;
	//获取设备信息
	case SMTCFG_AFN_GET_DEVICE_INFO:
		if((dst_len + rt_strlen(PTCL_SOFTWARE_EDITION) + 10 + 38) >= max_len)
		{
			return 0;
		}
		//软件版本
		dst_len += sprintf((char *)(pdst + dst_len), ",%s,", PTCL_SOFTWARE_EDITION);
		//uid
		pmem = mempool_req(12, RT_WAITING_NO);
		if((void *)0 == pmem)
		{
			dst_len += sprintf((char *)(pdst + dst_len), "00 00 00 00 00 00 00 00 00 00 00 00,");
		}
		else
		{
			rt_memset(pmem, 0, 12);
			*(rt_uint32_t *)pmem		= HAL_GetUIDw0();
			*((rt_uint32_t *)pmem + 1)	= HAL_GetUIDw1();
			*((rt_uint32_t *)pmem + 2)	= HAL_GetUIDw2();
			for(len = 0; len < 12; len++)
			{
				dst_len += sprintf((char *)(pdst + dst_len), "%02x ", *((rt_uint8_t *)pmem + len));
			}
			rt_mp_free(pmem);
			pdst[dst_len - 1] = ',';
		}
		//站号
		dst_len += sprintf((char *)(pdst + dst_len), "1234567890");
		break;
	//重启设备
	case SMTCFG_AFN_RESET_DEVICE:
		if((i + 1) != src_len)
		{
			return 0;
		}
		if(('0' != psrc[i]) && ('1' != psrc[i]))
		{
			return 0;
		}
		len = ('0' == psrc[i]) ? PTCL_RUN_STA_RESET : PTCL_RUN_STA_RESTORE;
		if(RT_FALSE == ptcl_run_sta_set(len))
		{
			return 0;
		}
		break;
	//设置时间
	case SMTCFG_AFN_SET_TIME:
	case SMTCFG_AFN_CALIBRATE_TIME:
		if((i + 17) != src_len)
		{
			return 0;
		}
		//年
		if(RT_FALSE == fyyp_is_number(psrc + i, 2))
		{
			return 0;
		}
		time.tm_year = fyyp_str_to_hex(psrc + i, 2) + 2000;
		i += 2;
		if('-' != psrc[i++])
		{
			return 0;
		}
		//月
		if(RT_FALSE == fyyp_is_number(psrc + i, 2))
		{
			return 0;
		}
		time.tm_mon = fyyp_str_to_hex(psrc + i, 2) - 1;
		i += 2;
		if('-' != psrc[i++])
		{
			return 0;
		}
		//日
		if(RT_FALSE == fyyp_is_number(psrc + i, 2))
		{
			return 0;
		}
		time.tm_mday = fyyp_str_to_hex(psrc + i, 2);
		i += 2;
		if(',' != psrc[i++])
		{
			return 0;
		}
		//时
		if(RT_FALSE == fyyp_is_number(psrc + i, 2))
		{
			return 0;
		}
		time.tm_hour = fyyp_str_to_hex(psrc + i, 2);
		i += 2;
		if('-' != psrc[i++])
		{
			return 0;
		}
		//分
		if(RT_FALSE == fyyp_is_number(psrc + i, 2))
		{
			return 0;
		}
		time.tm_min = fyyp_str_to_hex(psrc + i, 2);
		i += 2;
		if('-' != psrc[i++])
		{
			return 0;
		}
		//秒
		if(RT_FALSE == fyyp_is_number(psrc + i, 2))
		{
			return 0;
		}
		time.tm_sec = fyyp_str_to_hex(psrc + i, 2);

		time = rtcee_rtc_unix_to_calendar(rtcee_rtc_calendar_to_unix(time));
		rtcee_rtc_set_calendar(time);
		break;
	//读取时间
	case SMTCFG_AFN_GET_TIME:
		//时间
		time = rtcee_rtc_get_calendar();
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%02d-%02d-%02d,%02d-%02d-%02d", time.tm_year % 100, time.tm_mon + 1, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//设置中心站地址
	case SMTCFG_AFN_SET_CA_0:
	case SMTCFG_AFN_SET_CA_1:
	case SMTCFG_AFN_SET_CA_2:
	case SMTCFG_AFN_SET_CA_3:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_CENTER_ADDR;
		param_info_ptr->data_len	= grp - SMTCFG_AFN_SET_CA_0;
		*param_info_ptr->pdata		= fyyp_str_to_hex(psrc + i, src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	//读取中心站地址
	case SMTCFG_AFN_GET_CA_0:
	case SMTCFG_AFN_GET_CA_1:
	case SMTCFG_AFN_GET_CA_2:
	case SMTCFG_AFN_GET_CA_3:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", ptcl_get_center_addr(grp - SMTCFG_AFN_GET_CA_0));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//协议类型
	case SMTCFG_AFN_SET_PTCL_0:
	case SMTCFG_AFN_SET_PTCL_1:
	case SMTCFG_AFN_SET_PTCL_2:
	case SMTCFG_AFN_SET_PTCL_3:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		len = fyyp_str_to_hex(psrc + i, src_len - i);
		if(len >= PTCL_NUM_PTCL_TYPE)
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_PTCL_TYPE;
		param_info_ptr->data_len	= grp - SMTCFG_AFN_SET_PTCL_0;
		*param_info_ptr->pdata		= len;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_PTCL_0:
	case SMTCFG_AFN_GET_PTCL_1:
	case SMTCFG_AFN_GET_PTCL_2:
	case SMTCFG_AFN_GET_PTCL_3:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", ptcl_get_ptcl_type(grp - SMTCFG_AFN_GET_PTCL_0));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//设置主信道
	case SMTCFG_AFN_SET_COMM_FIRST_0:
	case SMTCFG_AFN_SET_COMM_FIRST_1:
	case SMTCFG_AFN_SET_COMM_FIRST_2:
	case SMTCFG_AFN_SET_COMM_FIRST_3:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		len = fyyp_str_to_hex(psrc + i, src_len - i);
		if(len >= PTCL_NUM_COMM_TYPE)
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_COMM_TYPE;
		param_info_ptr->data_len	= (grp - SMTCFG_AFN_SET_COMM_FIRST_0) * 2;
		*param_info_ptr->pdata		= len;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	//读取主信道
	case SMTCFG_AFN_GET_COMM_FIRST_0:
	case SMTCFG_AFN_GET_COMM_FIRST_1:
	case SMTCFG_AFN_GET_COMM_FIRST_2:
	case SMTCFG_AFN_GET_COMM_FIRST_3:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", ptcl_get_comm_type(grp - SMTCFG_AFN_GET_COMM_FIRST_0, 0));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//设置备信道
	case SMTCFG_AFN_SET_COMM_SECOND_0:
	case SMTCFG_AFN_SET_COMM_SECOND_1:
	case SMTCFG_AFN_SET_COMM_SECOND_2:
	case SMTCFG_AFN_SET_COMM_SECOND_3:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		len = fyyp_str_to_hex(psrc + i, src_len - i);
		if(len >= PTCL_NUM_COMM_TYPE)
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_COMM_TYPE;
		param_info_ptr->data_len	= (grp - SMTCFG_AFN_SET_COMM_SECOND_0) * 2 + 1;
		*param_info_ptr->pdata		= len;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	//读取备信道
	case SMTCFG_AFN_GET_COMM_SECOND_0:
	case SMTCFG_AFN_GET_COMM_SECOND_1:
	case SMTCFG_AFN_GET_COMM_SECOND_2:
	case SMTCFG_AFN_GET_COMM_SECOND_3:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", ptcl_get_comm_type(grp - SMTCFG_AFN_GET_COMM_SECOND_0, 1));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//设置dtu工作模式
	case SMTCFG_AFN_SET_MODE_0:
	case SMTCFG_AFN_SET_MODE_1:
	case SMTCFG_AFN_SET_MODE_2:
	case SMTCFG_AFN_SET_MODE_3:
	case SMTCFG_AFN_SET_MODE_4:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		len = fyyp_str_to_hex(psrc + i, src_len - i);
		if(len >= DTU_NUM_RUN_MODE)
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_DTU_MODE;
		if(SMTCFG_AFN_SET_MODE_0 == grp)
		{
			param_info_ptr->data_len = 0;
		}
		else if(SMTCFG_AFN_SET_MODE_1 == grp)
		{
			param_info_ptr->data_len = 1;
		}
		else if(SMTCFG_AFN_SET_MODE_2 == grp)
		{
			param_info_ptr->data_len = 2;
		}
		else if(SMTCFG_AFN_SET_MODE_3 == grp)
		{
			param_info_ptr->data_len = 3;
		}
		else
		{
			param_info_ptr->data_len = 4;
		}
		*param_info_ptr->pdata		= len;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	//读取dtu工作模式
	case SMTCFG_AFN_GET_MODE_0:
	case SMTCFG_AFN_GET_MODE_1:
	case SMTCFG_AFN_GET_MODE_2:
	case SMTCFG_AFN_GET_MODE_3:
	case SMTCFG_AFN_GET_MODE_4:
		if(SMTCFG_AFN_GET_MODE_0 == grp)
		{
			grp = 0;
		}
		else if(SMTCFG_AFN_GET_MODE_1 == grp)
		{
			grp = 1;
		}
		else if(SMTCFG_AFN_GET_MODE_2 == grp)
		{
			grp = 2;
		}
		else if(SMTCFG_AFN_GET_MODE_3 == grp)
		{
			grp = 3;
		}
		else
		{
			grp = 4;
		}
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", dtu_get_run_mode(grp));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//设置apn
	case SMTCFG_AFN_SET_DTU_APN:
		if((i + DTU_BYTES_APN) < src_len)
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(DTU_APN), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type = PTCL_PARAM_INFO_DTU_APN;
		((DTU_APN *)param_info_ptr->pdata)->apn_len = src_len - i;
		memcpy((void *)(((DTU_APN *)param_info_ptr->pdata)->apn_data), (void *)(psrc + i), src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	//读取apn
	case SMTCFG_AFN_GET_DTU_APN:
		if((dst_len + DTU_BYTES_APN + 1) > max_len)
		{
			return 0;
		}
		pmem = mempool_req(sizeof(DTU_APN), RT_WAITING_NO);
		if((void *)0 == pmem)
		{
			return 0;
		}
		dtu_get_apn((DTU_APN *)pmem);
		pdst[dst_len++] = ',';
		memcpy((void *)(pdst + dst_len), (void *)(((DTU_APN *)pmem)->apn_data), ((DTU_APN *)pmem)->apn_len);
		dst_len += ((DTU_APN *)pmem)->apn_len;
		rt_mp_free(pmem);
		break;
	//设置心跳
	case SMTCFG_AFN_SET_HEART_0:
	case SMTCFG_AFN_SET_HEART_1:
	case SMTCFG_AFN_SET_HEART_2:
	case SMTCFG_AFN_SET_HEART_3:
	case SMTCFG_AFN_SET_HEART_4:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint16_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type = PTCL_PARAM_INFO_HEART_PERIOD;
		if(SMTCFG_AFN_SET_HEART_0 == grp)
		{
			param_info_ptr->data_len = 0;
		}
		else if(SMTCFG_AFN_SET_HEART_1 == grp)
		{
			param_info_ptr->data_len = 1;
		}
		else if(SMTCFG_AFN_SET_HEART_2 == grp)
		{
			param_info_ptr->data_len = 2;
		}
		else if(SMTCFG_AFN_SET_HEART_3 == grp)
		{
			param_info_ptr->data_len = 3;
		}
		else
		{
			param_info_ptr->data_len = 4;
		}
		*(rt_uint16_t *)param_info_ptr->pdata = fyyp_str_to_hex(psrc + i, src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	//读取心跳
	case SMTCFG_AFN_GET_HEART_0:
	case SMTCFG_AFN_GET_HEART_1:
	case SMTCFG_AFN_GET_HEART_2:
	case SMTCFG_AFN_GET_HEART_3:
	case SMTCFG_AFN_GET_HEART_4:
		if(SMTCFG_AFN_GET_HEART_0 == grp)
		{
			grp = 0;
		}
		else if(SMTCFG_AFN_GET_HEART_1 == grp)
		{
			grp = 1;
		}
		else if(SMTCFG_AFN_GET_HEART_2 == grp)
		{
			grp = 2;
		}
		else if(SMTCFG_AFN_GET_HEART_3 == grp)
		{
			grp = 3;
		}
		else
		{
			grp = 4;
		}
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", dtu_get_heart_period(grp));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//设置ip通道
	case SMTCFG_AFN_SET_IPCH_0:
	case SMTCFG_AFN_SET_IPCH_1:
	case SMTCFG_AFN_SET_IPCH_2:
	case SMTCFG_AFN_SET_IPCH_3:
	case SMTCFG_AFN_SET_IPCH_4:
		if((i + 1) != src_len)
		{
			return 0;
		}
		if(('0' != psrc[i]) && ('1' != psrc[i]))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type = PTCL_PARAM_INFO_IP_CH;
		if(SMTCFG_AFN_SET_IPCH_0 == grp)
		{
			param_info_ptr->data_len = 0;
		}
		else if(SMTCFG_AFN_SET_IPCH_1 == grp)
		{
			param_info_ptr->data_len = 1;
		}
		else if(SMTCFG_AFN_SET_IPCH_2 == grp)
		{
			param_info_ptr->data_len = 2;
		}
		else if(SMTCFG_AFN_SET_IPCH_3 == grp)
		{
			param_info_ptr->data_len = 3;
		}
		else
		{
			param_info_ptr->data_len = 4;
		}
		*param_info_ptr->pdata = ('0' == psrc[i]) ? RT_FALSE : RT_TRUE;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	//读取ip通道
	case SMTCFG_AFN_GET_IPCH_0:
	case SMTCFG_AFN_GET_IPCH_1:
	case SMTCFG_AFN_GET_IPCH_2:
	case SMTCFG_AFN_GET_IPCH_3:
	case SMTCFG_AFN_GET_IPCH_4:
		if(SMTCFG_AFN_GET_IPCH_0 == grp)
		{
			grp = 0;
		}
		else if(SMTCFG_AFN_GET_IPCH_1 == grp)
		{
			grp = 1;
		}
		else if(SMTCFG_AFN_GET_IPCH_2 == grp)
		{
			grp = 2;
		}
		else if(SMTCFG_AFN_GET_IPCH_3 == grp)
		{
			grp = 3;
		}
		else
		{
			grp = 4;
		}
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%c", (RT_FALSE == dtu_get_ip_ch(grp)) ? '0' : '1');
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//设置sms通道
	case SMTCFG_AFN_SET_SMSCH_0:
	case SMTCFG_AFN_SET_SMSCH_1:
	case SMTCFG_AFN_SET_SMSCH_2:
	case SMTCFG_AFN_SET_SMSCH_3:
	case SMTCFG_AFN_SET_SMSCH_4:
		if((i + 1) != src_len)
		{
			return 0;
		}
		if(('0' != psrc[i]) && ('1' != psrc[i]))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type = PTCL_PARAM_INFO_SMS_CH;
		if(SMTCFG_AFN_SET_SMSCH_0 == grp)
		{
			param_info_ptr->data_len = 0;
		}
		else if(SMTCFG_AFN_SET_SMSCH_1 == grp)
		{
			param_info_ptr->data_len = 1;
		}
		else if(SMTCFG_AFN_SET_SMSCH_2 == grp)
		{
			param_info_ptr->data_len = 2;
		}
		else if(SMTCFG_AFN_SET_SMSCH_3 == grp)
		{
			param_info_ptr->data_len = 3;
		}
		else
		{
			param_info_ptr->data_len = 4;
		}
		*param_info_ptr->pdata = ('0' == psrc[i]) ? RT_FALSE : RT_TRUE;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	//读取sms通道
	case SMTCFG_AFN_GET_SMSCH_0:
	case SMTCFG_AFN_GET_SMSCH_1:
	case SMTCFG_AFN_GET_SMSCH_2:
	case SMTCFG_AFN_GET_SMSCH_3:
	case SMTCFG_AFN_GET_SMSCH_4:
		if(SMTCFG_AFN_GET_SMSCH_0 == grp)
		{
			grp = 0;
		}
		else if(SMTCFG_AFN_GET_SMSCH_1 == grp)
		{
			grp = 1;
		}
		else if(SMTCFG_AFN_GET_SMSCH_2 == grp)
		{
			grp = 2;
		}
		else if(SMTCFG_AFN_GET_SMSCH_3 == grp)
		{
			grp = 3;
		}
		else
		{
			grp = 4;
		}
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%c", (RT_FALSE == dtu_get_sms_ch(grp)) ? '0' : '1');
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//设置ip地址
	case SMTCFG_AFN_SET_IPADDR_0:
	case SMTCFG_AFN_SET_IPADDR_1:
	case SMTCFG_AFN_SET_IPADDR_2:
	case SMTCFG_AFN_SET_IPADDR_3:
	case SMTCFG_AFN_SET_IPADDR_4:
		if((i + DTU_BYTES_IP_ADDR) < src_len)
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(DTU_IP_ADDR), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type = PTCL_PARAM_INFO_IP_ADDR;
		if(SMTCFG_AFN_SET_IPADDR_0 == grp)
		{
			param_info_ptr->data_len = 0;
		}
		else if(SMTCFG_AFN_SET_IPADDR_1 == grp)
		{
			param_info_ptr->data_len = 1;
		}
		else if(SMTCFG_AFN_SET_IPADDR_2 == grp)
		{
			param_info_ptr->data_len = 2;
		}
		else if(SMTCFG_AFN_SET_IPADDR_3 == grp)
		{
			param_info_ptr->data_len = 3;
		}
		else
		{
			param_info_ptr->data_len = 4;
		}
		((DTU_IP_ADDR *)param_info_ptr->pdata)->addr_len = src_len - i;
		memcpy((void *)(((DTU_IP_ADDR *)param_info_ptr->pdata)->addr_data), (void *)(psrc + i), src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	//读取ip地址
	case SMTCFG_AFN_GET_IPADDR_0:
	case SMTCFG_AFN_GET_IPADDR_1:
	case SMTCFG_AFN_GET_IPADDR_2:
	case SMTCFG_AFN_GET_IPADDR_3:
	case SMTCFG_AFN_GET_IPADDR_4:
		if((dst_len + DTU_BYTES_IP_ADDR + 1) > max_len)
		{
			return 0;
		}
		pmem = mempool_req(sizeof(DTU_IP_ADDR), RT_WAITING_NO);
		if((void *)0 == pmem)
		{
			return 0;
		}
		if(SMTCFG_AFN_GET_IPADDR_0 == grp)
		{
			grp = 0;
		}
		else if(SMTCFG_AFN_GET_IPADDR_1 == grp)
		{
			grp = 1;
		}
		else if(SMTCFG_AFN_GET_IPADDR_2 == grp)
		{
			grp = 2;
		}
		else if(SMTCFG_AFN_GET_IPADDR_3 == grp)
		{
			grp = 3;
		}
		else
		{
			grp = 4;
		}
		dtu_get_ip_addr((DTU_IP_ADDR *)pmem, grp);
		pdst[dst_len++] = ',';
		memcpy((void *)(pdst + dst_len), (void *)(((DTU_IP_ADDR *)pmem)->addr_data), ((DTU_IP_ADDR *)pmem)->addr_len);
		dst_len += ((DTU_IP_ADDR *)pmem)->addr_len;
		rt_mp_free(pmem);
		break;
	//设置sms地址
	case SMTCFG_AFN_SET_SMSADDR_0:
	case SMTCFG_AFN_SET_SMSADDR_1:
	case SMTCFG_AFN_SET_SMSADDR_2:
	case SMTCFG_AFN_SET_SMSADDR_3:
	case SMTCFG_AFN_SET_SMSADDR_4:
		if((i + DTU_BYTES_SMS_ADDR) < src_len)
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(DTU_SMS_ADDR), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type = PTCL_PARAM_INFO_SMS_ADDR;
		if(SMTCFG_AFN_SET_SMSADDR_0 == grp)
		{
			param_info_ptr->data_len = 0;
		}
		else if(SMTCFG_AFN_SET_SMSADDR_1 == grp)
		{
			param_info_ptr->data_len = 1;
		}
		else if(SMTCFG_AFN_SET_SMSADDR_2 == grp)
		{
			param_info_ptr->data_len = 2;
		}
		else if(SMTCFG_AFN_SET_SMSADDR_3 == grp)
		{
			param_info_ptr->data_len = 3;
		}
		else
		{
			param_info_ptr->data_len = 4;
		}
		((DTU_SMS_ADDR *)param_info_ptr->pdata)->addr_len = src_len - i;
		memcpy((void *)(((DTU_SMS_ADDR *)param_info_ptr->pdata)->addr_data), (void *)(psrc + i), src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	//读取sms地址
	case SMTCFG_AFN_GET_SMSADDR_0:
	case SMTCFG_AFN_GET_SMSADDR_1:
	case SMTCFG_AFN_GET_SMSADDR_2:
	case SMTCFG_AFN_GET_SMSADDR_3:
	case SMTCFG_AFN_GET_SMSADDR_4:
		if((dst_len + DTU_BYTES_SMS_ADDR + 1) > max_len)
		{
			return 0;
		}
		pmem = mempool_req(sizeof(DTU_SMS_ADDR), RT_WAITING_NO);
		if((void *)0 == pmem)
		{
			return 0;
		}
		if(SMTCFG_AFN_GET_SMSADDR_0 == grp)
		{
			grp = 0;
		}
		else if(SMTCFG_AFN_GET_SMSADDR_1 == grp)
		{
			grp = 1;
		}
		else if(SMTCFG_AFN_GET_SMSADDR_2 == grp)
		{
			grp = 2;
		}
		else if(SMTCFG_AFN_GET_SMSADDR_3 == grp)
		{
			grp = 3;
		}
		else
		{
			grp = 4;
		}
		dtu_get_sms_addr((DTU_SMS_ADDR *)pmem, grp);
		pdst[dst_len++] = ',';
		memcpy((void *)(pdst + dst_len), (void *)(((DTU_SMS_ADDR *)pmem)->addr_data), ((DTU_SMS_ADDR *)pmem)->addr_len);
		dst_len += ((DTU_SMS_ADDR *)pmem)->addr_len;
		rt_mp_free(pmem);
		break;
	//设置ip_type
	case SMTCFG_AFN_SET_IPTYPE_0:
	case SMTCFG_AFN_SET_IPTYPE_1:
	case SMTCFG_AFN_SET_IPTYPE_2:
	case SMTCFG_AFN_SET_IPTYPE_3:
	case SMTCFG_AFN_SET_IPTYPE_4:
		if((i + 1) != src_len)
		{
			return 0;
		}
		if(('0' != psrc[i]) && ('1' != psrc[i]))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type = PTCL_PARAM_INFO_IP_TYPE;
		if(SMTCFG_AFN_SET_IPTYPE_0 == grp)
		{
			param_info_ptr->data_len = 0;
		}
		else if(SMTCFG_AFN_SET_IPTYPE_1 == grp)
		{
			param_info_ptr->data_len = 1;
		}
		else if(SMTCFG_AFN_SET_IPTYPE_2 == grp)
		{
			param_info_ptr->data_len = 2;
		}
		else if(SMTCFG_AFN_SET_IPTYPE_3 == grp)
		{
			param_info_ptr->data_len = 3;
		}
		else
		{
			param_info_ptr->data_len = 4;
		}
		*param_info_ptr->pdata = ('0' == psrc[i]) ? RT_FALSE : RT_TRUE;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	//读取ip_type
	case SMTCFG_AFN_GET_IPTYPE_0:
	case SMTCFG_AFN_GET_IPTYPE_1:
	case SMTCFG_AFN_GET_IPTYPE_2:
	case SMTCFG_AFN_GET_IPTYPE_3:
	case SMTCFG_AFN_GET_IPTYPE_4:
		if(SMTCFG_AFN_GET_IPTYPE_0 == grp)
		{
			grp = 0;
		}
		else if(SMTCFG_AFN_GET_IPTYPE_1 == grp)
		{
			grp = 1;
		}
		else if(SMTCFG_AFN_GET_IPTYPE_2 == grp)
		{
			grp = 2;
		}
		else if(SMTCFG_AFN_GET_IPTYPE_3 == grp)
		{
			grp = 3;
		}
		else
		{
			grp = 4;
		}
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%c", (RT_FALSE == dtu_get_ip_type(grp)) ? '0' : '1');
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//添加管理号
	case SMTCFG_AFN_ADD_PREVILIGE_PHONE:
	//添加授权号
	case SMTCFG_AFN_ADD_SUPER_PHONE:
	//删除号码
	case SMTCFG_AFN_DEL_SUPER_PHONE:
		if((i + DTU_BYTES_SMS_ADDR) < src_len)
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(DTU_SMS_ADDR), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type = (SMTCFG_AFN_DEL_SUPER_PHONE == grp) ? PTCL_PARAM_INFO_DEL_SUPER_PHONE : PTCL_PARAM_INFO_ADD_SUPER_PHONE;
		if(SMTCFG_AFN_ADD_PREVILIGE_PHONE == grp)
		{
			param_info_ptr->data_len = RT_TRUE;
		}
		else if(SMTCFG_AFN_ADD_SUPER_PHONE == grp)
		{
			param_info_ptr->data_len = RT_FALSE;
		}
		((DTU_SMS_ADDR *)param_info_ptr->pdata)->addr_len = src_len - i;
		memcpy((void *)(((DTU_SMS_ADDR *)param_info_ptr->pdata)->addr_data), (void *)(psrc + i), src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	//查询管理号
	case SMTCFG_AFN_QUERY_PREVILIGE_PHONE:
	//查询管理号
	case SMTCFG_AFN_QUERY_SUPER_PHONE:
		pmem = mempool_req(sizeof(DTU_SMS_ADDR), RT_WAITING_NO);
		if((void *)0 == pmem)
		{
			return 0;
		}
		if(SMTCFG_AFN_QUERY_PREVILIGE_PHONE == grp)
		{
			pos = 0;
			len = DTU_NUM_PREVILIGE_PHONE;
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n当前管理号:"));
		}
		else
		{
			pos = DTU_NUM_PREVILIGE_PHONE;
			len = DTU_NUM_SUPER_PHONE;
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n当前授权号:"));
		}
		while(pos < len)
		{
			dtu_get_super_phone((DTU_SMS_ADDR *)pmem, pos++);
			if(0 != ((DTU_SMS_ADDR *)pmem)->addr_len)
			{
				DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n"));
				DEBUG_INFO_OUTPUT(DEBUG_INFO_TYPE_DTU, (((DTU_SMS_ADDR *)pmem)->addr_data, ((DTU_SMS_ADDR *)pmem)->addr_len));
			}
		}
		rt_mp_free(pmem);
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_DTU, ("\r\n"));
		
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",号码见调试信息");
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//拨打电话
	case SMTCFG_AFN_DIAL_PHONE:
		if((i + DTU_BYTES_SMS_ADDR) < src_len)
		{
			return 0;
		}
		pmem = mempool_req(sizeof(DTU_SMS_ADDR), RT_WAITING_NO);
		if((void *)0 == pmem)
		{
			return 0;
		}
		((DTU_SMS_ADDR *)pmem)->addr_len = src_len - i;
		memcpy((void *)(((DTU_SMS_ADDR *)pmem)->addr_data), (void *)(psrc + i), ((DTU_SMS_ADDR *)pmem)->addr_len);
		if(RT_FALSE == dtu_dial_phone((DTU_SMS_ADDR *)pmem))
		{
			rt_mp_free(pmem);
			return 0;
		}
		break;
	//传输文件到设备
	case SMTCFG_AFN_FILE_TO_DEVICE:
	//从设备提取文件
	case SMTCFG_AFN_FILE_FROM_DEVICE:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		if(RT_FALSE == ptcl_have_shell())
		{
			pmem = (void *)0;
		}
		else
		{
			pmem = *(void **)(recv_data_ptr->pdata + recv_data_ptr->data_len);
		}
		if(RT_FALSE == xm_file_trans_trigger(recv_data_ptr->comm_type, recv_data_ptr->ch, (SMTCFG_AFN_FILE_TO_DEVICE == grp) ? XM_DIR_TO_DEVICE : XM_DIR_FROM_DEVICE, fyyp_str_to_hex(psrc + i, src_len - i), (XM_FUN_SHELL)pmem))
		{
			return 0;
		}
		break;
	//读取软件版本
	case SMTCFG_AFN_GET_SOFTWARE:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%s", PTCL_SOFTWARE_EDITION);
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//状态上报间隔
	case SMTCFG_AFN_SET_STA_PERIOD:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint16_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type = PTCL_PARAM_INFO_SMTMNG_STA_PERIOD;
		*(rt_uint16_t *)param_info_ptr->pdata = fyyp_str_to_hex(psrc + i, src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_STA_PERIOD:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", smtmng_get_sta_period());
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//校时间隔
	case SMTCFG_AFN_SET_TIMING_PERIOD:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint16_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type = PTCL_PARAM_INFO_SMTMNG_TIMING_PERIOD;
		*(rt_uint16_t *)param_info_ptr->pdata = fyyp_str_to_hex(psrc + i, src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_TIMING_PERIOD:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", smtmng_get_timing_period());
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//心跳间隔
	case SMTCFG_AFN_SET_HEART_PERIOD:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint16_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type = PTCL_PARAM_INFO_SMTMNG_HEART_PERIOD;
		*(rt_uint16_t *)param_info_ptr->pdata = fyyp_str_to_hex(psrc + i, src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_HEART_PERIOD:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", smtmng_get_heart_period());
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//清除历史数据
	case SMTCFG_AFN_CLEAR_OLD_DATA:
		if((i + 1) != src_len)
		{
			return 0;
		}
		if('1' != psrc[i])
		{
			return 0;
		}
		sample_clear_old_data();
		break;
	//传感器类型
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_TYPE_0:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_TYPE_1:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_TYPE_2:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_TYPE_3:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_TYPE_4:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_TYPE_5:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_TYPE_6:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_TYPE_7:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_TYPE_8:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_TYPE_9:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		len = fyyp_str_to_hex(psrc + i, src_len - i);
		if(len > SAMPLE_NUM_SENSOR_TYPE)
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_SAMPLE_SENSOR_TYPE;
		param_info_ptr->data_len	= ((grp - SMTCFG_AFN_SET_SAMPLE_SENSOR_TYPE_0) >> 8);
		*param_info_ptr->pdata		= len;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_TYPE_0:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_TYPE_1:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_TYPE_2:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_TYPE_3:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_TYPE_4:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_TYPE_5:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_TYPE_6:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_TYPE_7:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_TYPE_8:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_TYPE_9:
		grp -= SMTCFG_AFN_GET_SAMPLE_SENSOR_TYPE_0;
		grp >>= 8;
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sample_get_sensor_type(grp));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//传感器地址
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_ADDR_0:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_ADDR_1:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_ADDR_2:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_ADDR_3:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_ADDR_4:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_ADDR_5:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_ADDR_6:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_ADDR_7:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_ADDR_8:
	case SMTCFG_AFN_SET_SAMPLE_SENSOR_ADDR_9:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_SAMPLE_SENSOR_ADDR;
		param_info_ptr->data_len	= ((grp - SMTCFG_AFN_SET_SAMPLE_SENSOR_ADDR_0) >> 8);
		*param_info_ptr->pdata		= fyyp_str_to_hex(psrc + i, src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_ADDR_0:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_ADDR_1:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_ADDR_2:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_ADDR_3:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_ADDR_4:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_ADDR_5:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_ADDR_6:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_ADDR_7:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_ADDR_8:
	case SMTCFG_AFN_GET_SAMPLE_SENSOR_ADDR_9:
		grp -= SMTCFG_AFN_GET_SAMPLE_SENSOR_ADDR_0;
		grp >>= 8;
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sample_get_sensor_addr(grp));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//传感器硬件接口
	case SMTCFG_AFN_SET_SAMPLE_HW_PORT_0:
	case SMTCFG_AFN_SET_SAMPLE_HW_PORT_1:
	case SMTCFG_AFN_SET_SAMPLE_HW_PORT_2:
	case SMTCFG_AFN_SET_SAMPLE_HW_PORT_3:
	case SMTCFG_AFN_SET_SAMPLE_HW_PORT_4:
	case SMTCFG_AFN_SET_SAMPLE_HW_PORT_5:
	case SMTCFG_AFN_SET_SAMPLE_HW_PORT_6:
	case SMTCFG_AFN_SET_SAMPLE_HW_PORT_7:
	case SMTCFG_AFN_SET_SAMPLE_HW_PORT_8:
	case SMTCFG_AFN_SET_SAMPLE_HW_PORT_9:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		len = fyyp_str_to_hex(psrc + i, src_len - i);
		if(len >= SAMPLE_NUM_HW_PORT)
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_SAMPLE_HW_PORT;
		param_info_ptr->data_len	= ((grp - SMTCFG_AFN_SET_SAMPLE_HW_PORT_0) >> 8);
		*param_info_ptr->pdata		= len;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SAMPLE_HW_PORT_0:
	case SMTCFG_AFN_GET_SAMPLE_HW_PORT_1:
	case SMTCFG_AFN_GET_SAMPLE_HW_PORT_2:
	case SMTCFG_AFN_GET_SAMPLE_HW_PORT_3:
	case SMTCFG_AFN_GET_SAMPLE_HW_PORT_4:
	case SMTCFG_AFN_GET_SAMPLE_HW_PORT_5:
	case SMTCFG_AFN_GET_SAMPLE_HW_PORT_6:
	case SMTCFG_AFN_GET_SAMPLE_HW_PORT_7:
	case SMTCFG_AFN_GET_SAMPLE_HW_PORT_8:
	case SMTCFG_AFN_GET_SAMPLE_HW_PORT_9:
		grp -= SMTCFG_AFN_GET_SAMPLE_HW_PORT_0;
		grp >>= 8;
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sample_get_hw_port(grp));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//传感器协议
	case SMTCFG_AFN_SET_SAMPLE_PROTOCOL_0:
	case SMTCFG_AFN_SET_SAMPLE_PROTOCOL_1:
	case SMTCFG_AFN_SET_SAMPLE_PROTOCOL_2:
	case SMTCFG_AFN_SET_SAMPLE_PROTOCOL_3:
	case SMTCFG_AFN_SET_SAMPLE_PROTOCOL_4:
	case SMTCFG_AFN_SET_SAMPLE_PROTOCOL_5:
	case SMTCFG_AFN_SET_SAMPLE_PROTOCOL_6:
	case SMTCFG_AFN_SET_SAMPLE_PROTOCOL_7:
	case SMTCFG_AFN_SET_SAMPLE_PROTOCOL_8:
	case SMTCFG_AFN_SET_SAMPLE_PROTOCOL_9:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		len = fyyp_str_to_hex(psrc + i, src_len - i);
		if(len >= SAMPLE_NUM_PTCL)
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_SAMPLE_PROTOCOL;
		param_info_ptr->data_len	= ((grp - SMTCFG_AFN_SET_SAMPLE_PROTOCOL_0) >> 8);
		*param_info_ptr->pdata		= len;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SAMPLE_PROTOCOL_0:
	case SMTCFG_AFN_GET_SAMPLE_PROTOCOL_1:
	case SMTCFG_AFN_GET_SAMPLE_PROTOCOL_2:
	case SMTCFG_AFN_GET_SAMPLE_PROTOCOL_3:
	case SMTCFG_AFN_GET_SAMPLE_PROTOCOL_4:
	case SMTCFG_AFN_GET_SAMPLE_PROTOCOL_5:
	case SMTCFG_AFN_GET_SAMPLE_PROTOCOL_6:
	case SMTCFG_AFN_GET_SAMPLE_PROTOCOL_7:
	case SMTCFG_AFN_GET_SAMPLE_PROTOCOL_8:
	case SMTCFG_AFN_GET_SAMPLE_PROTOCOL_9:
		grp -= SMTCFG_AFN_GET_SAMPLE_PROTOCOL_0;
		grp >>= 8;
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sample_get_protocol(grp));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//硬件速率
	case SMTCFG_AFN_SET_SAMPLE_HW_RATE_0:
	case SMTCFG_AFN_SET_SAMPLE_HW_RATE_1:
	case SMTCFG_AFN_SET_SAMPLE_HW_RATE_2:
	case SMTCFG_AFN_SET_SAMPLE_HW_RATE_3:
	case SMTCFG_AFN_SET_SAMPLE_HW_RATE_4:
	case SMTCFG_AFN_SET_SAMPLE_HW_RATE_5:
	case SMTCFG_AFN_SET_SAMPLE_HW_RATE_6:
	case SMTCFG_AFN_SET_SAMPLE_HW_RATE_7:
	case SMTCFG_AFN_SET_SAMPLE_HW_RATE_8:
	case SMTCFG_AFN_SET_SAMPLE_HW_RATE_9:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint32_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type				= PTCL_PARAM_INFO_SAMPLE_HW_RATE;
		param_info_ptr->data_len				= ((grp - SMTCFG_AFN_SET_SAMPLE_HW_RATE_0) >> 8);
		*(rt_uint32_t *)param_info_ptr->pdata	= fyyp_str_to_hex(psrc + i, src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SAMPLE_HW_RATE_0:
	case SMTCFG_AFN_GET_SAMPLE_HW_RATE_1:
	case SMTCFG_AFN_GET_SAMPLE_HW_RATE_2:
	case SMTCFG_AFN_GET_SAMPLE_HW_RATE_3:
	case SMTCFG_AFN_GET_SAMPLE_HW_RATE_4:
	case SMTCFG_AFN_GET_SAMPLE_HW_RATE_5:
	case SMTCFG_AFN_GET_SAMPLE_HW_RATE_6:
	case SMTCFG_AFN_GET_SAMPLE_HW_RATE_7:
	case SMTCFG_AFN_GET_SAMPLE_HW_RATE_8:
	case SMTCFG_AFN_GET_SAMPLE_HW_RATE_9:
		grp -= SMTCFG_AFN_GET_SAMPLE_HW_RATE_0;
		grp >>= 8;
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sample_get_hw_rate(grp));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//k
	case SMTCFG_AFN_SET_SAMPLE_K_0:
	case SMTCFG_AFN_SET_SAMPLE_K_1:
	case SMTCFG_AFN_SET_SAMPLE_K_2:
	case SMTCFG_AFN_SET_SAMPLE_K_3:
	case SMTCFG_AFN_SET_SAMPLE_K_4:
	case SMTCFG_AFN_SET_SAMPLE_K_5:
	case SMTCFG_AFN_SET_SAMPLE_K_6:
	case SMTCFG_AFN_SET_SAMPLE_K_7:
	case SMTCFG_AFN_SET_SAMPLE_K_8:
	case SMTCFG_AFN_SET_SAMPLE_K_9:
		pmem = (void *)fyyp_mem_find(psrc + i, src_len - i, ",", 1);
		if((void *)0 == pmem)
		{
			return 0;
		}
		len = (rt_uint8_t *)pmem - psrc - i;
		if((i + len + 2) != src_len)
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(float), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		if(RT_FALSE == fyyp_str_to_float((float *)(param_info_ptr->pdata), psrc + i, len))
		{
			rt_mp_free((void *)param_info_ptr);
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_SAMPLE_K;
		param_info_ptr->data_len	= ((grp - SMTCFG_AFN_SET_SAMPLE_K_0) >> 8);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		
		i += len;
		i++;
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_SAMPLE_KOPT;
		param_info_ptr->data_len	= ((grp - SMTCFG_AFN_SET_SAMPLE_K_0) >> 8);
		*param_info_ptr->pdata		= ('1' == psrc[i]) ? RT_TRUE : RT_FALSE;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SAMPLE_K_0:
	case SMTCFG_AFN_GET_SAMPLE_K_1:
	case SMTCFG_AFN_GET_SAMPLE_K_2:
	case SMTCFG_AFN_GET_SAMPLE_K_3:
	case SMTCFG_AFN_GET_SAMPLE_K_4:
	case SMTCFG_AFN_GET_SAMPLE_K_5:
	case SMTCFG_AFN_GET_SAMPLE_K_6:
	case SMTCFG_AFN_GET_SAMPLE_K_7:
	case SMTCFG_AFN_GET_SAMPLE_K_8:
	case SMTCFG_AFN_GET_SAMPLE_K_9:
		grp -= SMTCFG_AFN_GET_SAMPLE_K_0;
		grp >>= 8;
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%.2f,%c", sample_get_k(grp), (RT_TRUE == sample_get_kopt(grp)) ? '1' : '0');
		if(dst_len + len < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//b
	case SMTCFG_AFN_SET_SAMPLE_B_0:
	case SMTCFG_AFN_SET_SAMPLE_B_1:
	case SMTCFG_AFN_SET_SAMPLE_B_2:
	case SMTCFG_AFN_SET_SAMPLE_B_3:
	case SMTCFG_AFN_SET_SAMPLE_B_4:
	case SMTCFG_AFN_SET_SAMPLE_B_5:
	case SMTCFG_AFN_SET_SAMPLE_B_6:
	case SMTCFG_AFN_SET_SAMPLE_B_7:
	case SMTCFG_AFN_SET_SAMPLE_B_8:
	case SMTCFG_AFN_SET_SAMPLE_B_9:
		param_info_ptr = ptcl_param_info_req(sizeof(float), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		if(RT_FALSE == fyyp_str_to_float((float *)(param_info_ptr->pdata), psrc + i, src_len - i))
		{
			rt_mp_free((void *)param_info_ptr);
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_SAMPLE_B;
		param_info_ptr->data_len	= ((grp - SMTCFG_AFN_SET_SAMPLE_B_0) >> 8);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SAMPLE_B_0:
	case SMTCFG_AFN_GET_SAMPLE_B_1:
	case SMTCFG_AFN_GET_SAMPLE_B_2:
	case SMTCFG_AFN_GET_SAMPLE_B_3:
	case SMTCFG_AFN_GET_SAMPLE_B_4:
	case SMTCFG_AFN_GET_SAMPLE_B_5:
	case SMTCFG_AFN_GET_SAMPLE_B_6:
	case SMTCFG_AFN_GET_SAMPLE_B_7:
	case SMTCFG_AFN_GET_SAMPLE_B_8:
	case SMTCFG_AFN_GET_SAMPLE_B_9:
		grp -= SMTCFG_AFN_GET_SAMPLE_B_0;
		grp >>= 8;
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%.2f", sample_get_b(grp));
		if(dst_len + len < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//base
	case SMTCFG_AFN_SET_SAMPLE_BASE_0:
	case SMTCFG_AFN_SET_SAMPLE_BASE_1:
	case SMTCFG_AFN_SET_SAMPLE_BASE_2:
	case SMTCFG_AFN_SET_SAMPLE_BASE_3:
	case SMTCFG_AFN_SET_SAMPLE_BASE_4:
	case SMTCFG_AFN_SET_SAMPLE_BASE_5:
	case SMTCFG_AFN_SET_SAMPLE_BASE_6:
	case SMTCFG_AFN_SET_SAMPLE_BASE_7:
	case SMTCFG_AFN_SET_SAMPLE_BASE_8:
	case SMTCFG_AFN_SET_SAMPLE_BASE_9:
		param_info_ptr = ptcl_param_info_req(sizeof(float), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		if(RT_FALSE == fyyp_str_to_float((float *)(param_info_ptr->pdata), psrc + i, src_len - i))
		{
			rt_mp_free((void *)param_info_ptr);
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_SAMPLE_BASE;
		param_info_ptr->data_len	= ((grp - SMTCFG_AFN_SET_SAMPLE_BASE_0) >> 8);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SAMPLE_BASE_0:
	case SMTCFG_AFN_GET_SAMPLE_BASE_1:
	case SMTCFG_AFN_GET_SAMPLE_BASE_2:
	case SMTCFG_AFN_GET_SAMPLE_BASE_3:
	case SMTCFG_AFN_GET_SAMPLE_BASE_4:
	case SMTCFG_AFN_GET_SAMPLE_BASE_5:
	case SMTCFG_AFN_GET_SAMPLE_BASE_6:
	case SMTCFG_AFN_GET_SAMPLE_BASE_7:
	case SMTCFG_AFN_GET_SAMPLE_BASE_8:
	case SMTCFG_AFN_GET_SAMPLE_BASE_9:
		grp -= SMTCFG_AFN_GET_SAMPLE_BASE_0;
		grp >>= 8;
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%.2f", sample_get_base(grp));
		if(dst_len + len < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//offset
	case SMTCFG_AFN_SET_SAMPLE_OFFSET_0:
	case SMTCFG_AFN_SET_SAMPLE_OFFSET_1:
	case SMTCFG_AFN_SET_SAMPLE_OFFSET_2:
	case SMTCFG_AFN_SET_SAMPLE_OFFSET_3:
	case SMTCFG_AFN_SET_SAMPLE_OFFSET_4:
	case SMTCFG_AFN_SET_SAMPLE_OFFSET_5:
	case SMTCFG_AFN_SET_SAMPLE_OFFSET_6:
	case SMTCFG_AFN_SET_SAMPLE_OFFSET_7:
	case SMTCFG_AFN_SET_SAMPLE_OFFSET_8:
	case SMTCFG_AFN_SET_SAMPLE_OFFSET_9:
		param_info_ptr = ptcl_param_info_req(sizeof(float), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		if(RT_FALSE == fyyp_str_to_float((float *)(param_info_ptr->pdata), psrc + i, src_len - i))
		{
			rt_mp_free((void *)param_info_ptr);
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_SAMPLE_OFFSET;
		param_info_ptr->data_len	= ((grp - SMTCFG_AFN_SET_SAMPLE_OFFSET_0) >> 8);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SAMPLE_OFFSET_0:
	case SMTCFG_AFN_GET_SAMPLE_OFFSET_1:
	case SMTCFG_AFN_GET_SAMPLE_OFFSET_2:
	case SMTCFG_AFN_GET_SAMPLE_OFFSET_3:
	case SMTCFG_AFN_GET_SAMPLE_OFFSET_4:
	case SMTCFG_AFN_GET_SAMPLE_OFFSET_5:
	case SMTCFG_AFN_GET_SAMPLE_OFFSET_6:
	case SMTCFG_AFN_GET_SAMPLE_OFFSET_7:
	case SMTCFG_AFN_GET_SAMPLE_OFFSET_8:
	case SMTCFG_AFN_GET_SAMPLE_OFFSET_9:
		grp -= SMTCFG_AFN_GET_SAMPLE_OFFSET_0;
		grp >>= 8;
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%.2f", sample_get_offset(grp));
		if(dst_len + len < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//up
	case SMTCFG_AFN_SET_SAMPLE_UP_0:
	case SMTCFG_AFN_SET_SAMPLE_UP_1:
	case SMTCFG_AFN_SET_SAMPLE_UP_2:
	case SMTCFG_AFN_SET_SAMPLE_UP_3:
	case SMTCFG_AFN_SET_SAMPLE_UP_4:
	case SMTCFG_AFN_SET_SAMPLE_UP_5:
	case SMTCFG_AFN_SET_SAMPLE_UP_6:
	case SMTCFG_AFN_SET_SAMPLE_UP_7:
	case SMTCFG_AFN_SET_SAMPLE_UP_8:
	case SMTCFG_AFN_SET_SAMPLE_UP_9:
		param_info_ptr = ptcl_param_info_req(sizeof(float), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		if(RT_FALSE == fyyp_str_to_float((float *)(param_info_ptr->pdata), psrc + i, src_len - i))
		{
			rt_mp_free((void *)param_info_ptr);
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_SAMPLE_UP;
		param_info_ptr->data_len	= ((grp - SMTCFG_AFN_SET_SAMPLE_UP_0) >> 8);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SAMPLE_UP_0:
	case SMTCFG_AFN_GET_SAMPLE_UP_1:
	case SMTCFG_AFN_GET_SAMPLE_UP_2:
	case SMTCFG_AFN_GET_SAMPLE_UP_3:
	case SMTCFG_AFN_GET_SAMPLE_UP_4:
	case SMTCFG_AFN_GET_SAMPLE_UP_5:
	case SMTCFG_AFN_GET_SAMPLE_UP_6:
	case SMTCFG_AFN_GET_SAMPLE_UP_7:
	case SMTCFG_AFN_GET_SAMPLE_UP_8:
	case SMTCFG_AFN_GET_SAMPLE_UP_9:
		grp -= SMTCFG_AFN_GET_SAMPLE_UP_0;
		grp >>= 8;
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%.2f", sample_get_up(grp));
		if(dst_len + len < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//down
	case SMTCFG_AFN_SET_SAMPLE_DOWN_0:
	case SMTCFG_AFN_SET_SAMPLE_DOWN_1:
	case SMTCFG_AFN_SET_SAMPLE_DOWN_2:
	case SMTCFG_AFN_SET_SAMPLE_DOWN_3:
	case SMTCFG_AFN_SET_SAMPLE_DOWN_4:
	case SMTCFG_AFN_SET_SAMPLE_DOWN_5:
	case SMTCFG_AFN_SET_SAMPLE_DOWN_6:
	case SMTCFG_AFN_SET_SAMPLE_DOWN_7:
	case SMTCFG_AFN_SET_SAMPLE_DOWN_8:
	case SMTCFG_AFN_SET_SAMPLE_DOWN_9:
		param_info_ptr = ptcl_param_info_req(sizeof(float), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		if(RT_FALSE == fyyp_str_to_float((float *)(param_info_ptr->pdata), psrc + i, src_len - i))
		{
			rt_mp_free((void *)param_info_ptr);
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_SAMPLE_DOWN;
		param_info_ptr->data_len	= ((grp - SMTCFG_AFN_SET_SAMPLE_DOWN_0) >> 8);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SAMPLE_DOWN_0:
	case SMTCFG_AFN_GET_SAMPLE_DOWN_1:
	case SMTCFG_AFN_GET_SAMPLE_DOWN_2:
	case SMTCFG_AFN_GET_SAMPLE_DOWN_3:
	case SMTCFG_AFN_GET_SAMPLE_DOWN_4:
	case SMTCFG_AFN_GET_SAMPLE_DOWN_5:
	case SMTCFG_AFN_GET_SAMPLE_DOWN_6:
	case SMTCFG_AFN_GET_SAMPLE_DOWN_7:
	case SMTCFG_AFN_GET_SAMPLE_DOWN_8:
	case SMTCFG_AFN_GET_SAMPLE_DOWN_9:
		grp -= SMTCFG_AFN_GET_SAMPLE_DOWN_0;
		grp >>= 8;
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%.2f", sample_get_down(grp));
		if(dst_len + len < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//threshold
	case SMTCFG_AFN_SET_SAMPLE_THRESHOLD_0:
	case SMTCFG_AFN_SET_SAMPLE_THRESHOLD_1:
	case SMTCFG_AFN_SET_SAMPLE_THRESHOLD_2:
	case SMTCFG_AFN_SET_SAMPLE_THRESHOLD_3:
	case SMTCFG_AFN_SET_SAMPLE_THRESHOLD_4:
	case SMTCFG_AFN_SET_SAMPLE_THRESHOLD_5:
	case SMTCFG_AFN_SET_SAMPLE_THRESHOLD_6:
	case SMTCFG_AFN_SET_SAMPLE_THRESHOLD_7:
	case SMTCFG_AFN_SET_SAMPLE_THRESHOLD_8:
	case SMTCFG_AFN_SET_SAMPLE_THRESHOLD_9:
		param_info_ptr = ptcl_param_info_req(sizeof(float), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		if(RT_FALSE == fyyp_str_to_float((float *)(param_info_ptr->pdata), psrc + i, src_len - i))
		{
			rt_mp_free((void *)param_info_ptr);
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_SAMPLE_THRESHOLD;
		param_info_ptr->data_len	= ((grp - SMTCFG_AFN_SET_SAMPLE_THRESHOLD_0) >> 8);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SAMPLE_THRESHOLD_0:
	case SMTCFG_AFN_GET_SAMPLE_THRESHOLD_1:
	case SMTCFG_AFN_GET_SAMPLE_THRESHOLD_2:
	case SMTCFG_AFN_GET_SAMPLE_THRESHOLD_3:
	case SMTCFG_AFN_GET_SAMPLE_THRESHOLD_4:
	case SMTCFG_AFN_GET_SAMPLE_THRESHOLD_5:
	case SMTCFG_AFN_GET_SAMPLE_THRESHOLD_6:
	case SMTCFG_AFN_GET_SAMPLE_THRESHOLD_7:
	case SMTCFG_AFN_GET_SAMPLE_THRESHOLD_8:
	case SMTCFG_AFN_GET_SAMPLE_THRESHOLD_9:
		grp -= SMTCFG_AFN_GET_SAMPLE_THRESHOLD_0;
		grp >>= 8;
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%.2f", sample_get_threshold(grp));
		if(dst_len + len < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//数据存储间隔
	case SMTCFG_AFN_SET_SAMPLE_STORE_PERIOD_0:
	case SMTCFG_AFN_SET_SAMPLE_STORE_PERIOD_1:
	case SMTCFG_AFN_SET_SAMPLE_STORE_PERIOD_2:
	case SMTCFG_AFN_SET_SAMPLE_STORE_PERIOD_3:
	case SMTCFG_AFN_SET_SAMPLE_STORE_PERIOD_4:
	case SMTCFG_AFN_SET_SAMPLE_STORE_PERIOD_5:
	case SMTCFG_AFN_SET_SAMPLE_STORE_PERIOD_6:
	case SMTCFG_AFN_SET_SAMPLE_STORE_PERIOD_7:
	case SMTCFG_AFN_SET_SAMPLE_STORE_PERIOD_8:
	case SMTCFG_AFN_SET_SAMPLE_STORE_PERIOD_9:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint16_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type				= PTCL_PARAM_INFO_SAMPLE_STORE_PERIOD;
		param_info_ptr->data_len				= ((grp - SMTCFG_AFN_SET_SAMPLE_STORE_PERIOD_0) >> 8);
		*(rt_uint16_t *)param_info_ptr->pdata	= fyyp_str_to_hex(psrc + i, src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SAMPLE_STORE_PERIOD_0:
	case SMTCFG_AFN_GET_SAMPLE_STORE_PERIOD_1:
	case SMTCFG_AFN_GET_SAMPLE_STORE_PERIOD_2:
	case SMTCFG_AFN_GET_SAMPLE_STORE_PERIOD_3:
	case SMTCFG_AFN_GET_SAMPLE_STORE_PERIOD_4:
	case SMTCFG_AFN_GET_SAMPLE_STORE_PERIOD_5:
	case SMTCFG_AFN_GET_SAMPLE_STORE_PERIOD_6:
	case SMTCFG_AFN_GET_SAMPLE_STORE_PERIOD_7:
	case SMTCFG_AFN_GET_SAMPLE_STORE_PERIOD_8:
	case SMTCFG_AFN_GET_SAMPLE_STORE_PERIOD_9:
		grp -= SMTCFG_AFN_GET_SAMPLE_STORE_PERIOD_0;
		grp >>= 8;
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sample_get_store_period(grp));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//污水流量
	case SMTCFG_AFN_GET_SAMPLE_WUSHUI:
	//化学需氧量COD
	case SMTCFG_AFN_GET_SAMPLE_COD:
		pmem = mempool_req(sizeof(SAMPLE_DATA_TYPE), RT_WAITING_NO);
		if((void *)0 == pmem)
		{
			return 0;
		}
		*(SAMPLE_DATA_TYPE *)pmem = sample_get_cur_data(grp - SMTCFG_AFN_GET_SAMPLE_WUSHUI);
		if(RT_FALSE == ((SAMPLE_DATA_TYPE *)pmem)->sta)
		{
			len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",无效值");
		}
		else
		{
			len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%.3f", ((SAMPLE_DATA_TYPE *)pmem)->value);
		}
		rt_mp_free(pmem);
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
#if 0
	//exti unit
	case SMTCFG_AFN_SET_SAMPLE_EXTI_UNIT:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_SAMPLE_EXTI_UNIT;
		*param_info_ptr->pdata		= fyyp_str_to_hex(psrc + i, src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SAMPLE_EXTI_UNIT:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sample_get_exti_unit());
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//exti total
	case SMTCFG_AFN_SET_SAMPLE_EXTI_TOTAL:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint32_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type				= PTCL_PARAM_INFO_SAMPLE_EXTI_TOTAL;
		*(rt_uint32_t *)param_info_ptr->pdata	= fyyp_str_to_hex(psrc + i, src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SAMPLE_EXTI_TOTAL:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sample_get_exti_total());
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//SL651地址
	case SMTCFG_AFN_SET_SL651_RTU_ADDR:
		if((i + 10) != src_len)
		{
			return 0;
		}
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(SL651_BYTES_RTU_ADDR, RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_SL651_RTU_ADDR;
		fyyp_str_to_array(param_info_ptr->pdata, psrc + i, src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SL651_RTU_ADDR:
		if((dst_len + 11) >= max_len)
		{
			return 0;
		}
		pmem = mempool_req(SL651_BYTES_RTU_ADDR, RT_WAITING_NO);
		if((void *)0 == pmem)
		{
			return 0;
		}
		sl651_get_rtu_addr((rt_uint8_t *)pmem);
		pdst[dst_len++] = ',';
		for(len = 0; len < SL651_BYTES_RTU_ADDR; len++)
		{
			dst_len += sprintf((char *)(pdst + dst_len), "%02x", *((rt_uint8_t *)pmem + len));
		}
		rt_mp_free(pmem);
		break;
	//SL651密码
	case SMTCFG_AFN_SET_SL651_PW:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint16_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type = PTCL_PARAM_INFO_SL651_PW;
		*(rt_uint16_t *)param_info_ptr->pdata = fyyp_str_to_hex(psrc + i, src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SL651_PW:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sl651_get_pw());
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//SL651流水号
	case SMTCFG_AFN_SET_SL651_SN:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		len = fyyp_str_to_hex(psrc + i, src_len - i);
		if(0 == len)
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint16_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type = PTCL_PARAM_INFO_SL651_SN;
		*(rt_uint16_t *)param_info_ptr->pdata = len;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SL651_SN:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sl651_get_sn());
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//SL651测站类型
	case SMTCFG_AFN_SET_SL651_RTU_TYPE:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		len = fyyp_str_to_hex(psrc + i, src_len - i);
		switch(len)
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
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type = PTCL_PARAM_INFO_SL651_RTU_TYPE;
		*param_info_ptr->pdata = len;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SL651_RTU_TYPE:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sl651_get_rtu_type());
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//SL651定时报间隔
	case SMTCFG_AFN_SET_SL651_REPORT_HOUR:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		len = fyyp_str_to_hex(psrc + i, src_len - i);
		if(len > 24)
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type = PTCL_PARAM_INFO_SL651_REPORT_HOUR;
		*param_info_ptr->pdata = len;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SL651_REPORT_HOUR:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sl651_get_report_hour());
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//SL651加报间隔
	case SMTCFG_AFN_SET_SL651_REPORT_MIN:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		len = fyyp_str_to_hex(psrc + i, src_len - i);
		if(len >= 60)
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type = PTCL_PARAM_INFO_SL651_REPORT_MIN;
		*param_info_ptr->pdata = len;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SL651_REPORT_MIN:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sl651_get_report_min());
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//SL651随机采集间隔
	case SMTCFG_AFN_SET_SL651_RANDOM_PERIOD:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint16_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type = PTCL_PARAM_INFO_SL651_RANDOM_PERIOD;
		*(rt_uint16_t *)param_info_ptr->pdata = fyyp_str_to_hex(psrc + i, src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_SL651_RANDOM_PERIOD:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sl651_get_random_period());
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
#endif
	//调试信息
	case SMTCFG_AFN_SET_DEBUG_INFO_TYPE_DTU:
	case SMTCFG_AFN_SET_DEBUG_INFO_TYPE_AT:
	case SMTCFG_AFN_SET_DEBUG_INFO_TYPE_SAMPLE:
	case SMTCFG_AFN_SET_DEBUG_INFO_TYPE_HTTP:
	case SMTCFG_AFN_SET_DEBUG_INFO_TYPE_FATFS:
	case SMTCFG_AFN_SET_DEBUG_INFO_TYPE_BT:
	case SMTCFG_AFN_SET_DEBUG_INFO_TYPE_DS18B20:
	case SMTCFG_AFN_SET_DEBUG_INFO_TYPE_HJ212:
	case SMTCFG_AFN_SET_DEBUG_INFO_TYPE_TEST:
		if((i + 1) != src_len)
		{
			return 0;
		}
		if(('0' != psrc[i]) && ('1' != psrc[i]))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_DEBUG_INFO_TYPE_STA;
		param_info_ptr->data_len	= grp - SMTCFG_AFN_SET_DEBUG_INFO_TYPE_DTU;
		*param_info_ptr->pdata		= ('0' == psrc[i]) ? RT_FALSE : RT_TRUE;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_DEBUG_INFO_TYPE_DTU:
	case SMTCFG_AFN_GET_DEBUG_INFO_TYPE_AT:
	case SMTCFG_AFN_GET_DEBUG_INFO_TYPE_SAMPLE:
	case SMTCFG_AFN_GET_DEBUG_INFO_TYPE_HTTP:
	case SMTCFG_AFN_GET_DEBUG_INFO_TYPE_FATFS:
	case SMTCFG_AFN_GET_DEBUG_INFO_TYPE_BT:
	case SMTCFG_AFN_GET_DEBUG_INFO_TYPE_DS18B20:
	case SMTCFG_AFN_GET_DEBUG_INFO_TYPE_HJ212:
	case SMTCFG_AFN_GET_DEBUG_INFO_TYPE_TEST:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%c", (RT_FALSE == debug_get_info_type_sta(grp - SMTCFG_AFN_GET_DEBUG_INFO_TYPE_DTU)) ? '0' : '1');
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//开关调试信息
	case SMTCFG_AFN_DEBUG_INFO_SWITCH:
		if((i + 1) != src_len)
		{
			return 0;
		}
		if(('0' != psrc[i]) && ('1' != psrc[i]))
		{
			return 0;
		}
		debug_close_all();
		break;
	//立即发送
	case SMTCFG_AFN_SET_IMMEDIATELY_0:
	case SMTCFG_AFN_SET_IMMEDIATELY_1:
	case SMTCFG_AFN_SET_IMMEDIATELY_2:
	case SMTCFG_AFN_SET_IMMEDIATELY_3:
	case SMTCFG_AFN_SET_IMMEDIATELY_4:
		if((i + 1) != src_len)
		{
			return 0;
		}
		if(('0' != psrc[i]) && ('1' != psrc[i]))
		{
			return 0;
		}
		if('1' == psrc[i])
		{
			if(SMTCFG_AFN_SET_IMMEDIATELY_0 == grp)
			{
				grp = 0;
			}
			else if(SMTCFG_AFN_SET_IMMEDIATELY_1 == grp)
			{
				grp = 1;
			}
			else if(SMTCFG_AFN_SET_IMMEDIATELY_2 == grp)
			{
				grp = 2;
			}
			else if(SMTCFG_AFN_SET_IMMEDIATELY_3 == grp)
			{
				grp = 3;
			}
			else
			{
				grp = 4;
			}
			dtu_ip_send_immediately(grp);
		}
		break;
	//读取dtu信号强度
	case SMTCFG_AFN_GET_DTU_RSSI:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", dtu_get_csq_value());
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//设置固件URL
	case SMTCFG_AFN_SET_FIRMWARE_URL:
		if(RT_FALSE == http_get_req_trigger(HTTP_FILE_TYPE_FIRMWARE, psrc + i, src_len - i))
		{
			return 0;
		}
		break;
	//读取供电电压
	case SMTCFG_AFN_GET_SUPPLY_VOLT:
		pos = sample_supply_volt(SAMPLE_PIN_ADC_BATT);
		grp = sample_supply_volt(SAMPLE_PIN_ADC_VIN);
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d.%dV,%d.%dV", pos / 100, pos % 100, grp / 100, grp % 100);
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//格式化文件系统
	case SMTCFG_AFN_SET_FATFS_MKFS:
		if((i + 1) != src_len)
		{
			return 0;
		}
		if('1' != psrc[i])
		{
			return 0;
		}
		if(FATFS_ERR_NONE != fatfs_mkfs())
		{
			return 0;
		}
		break;
	//测试文件系统
	case SMTCFG_AFN_SET_FATFS_TEST:
		pmem = (void *)fyyp_mem_find(psrc + i, src_len - i, ",", strlen(","));
		if((void *)0 == pmem)
		{
			return 0;
		}
		len = (rt_uint8_t *)pmem - (psrc + i);
		len++;
		if((i + len) >= src_len)
		{
			return 0;
		}
		pmem = mempool_req(len, RT_WAITING_NO);
		if((void *)0 == pmem)
		{
			return 0;
		}
		memset(pmem, 0, len);
		memcpy(pmem, (void *)(psrc + i), len - 1);
		i += len;
		len = fatfs_write((char *)pmem, psrc + i, src_len - i);
		rt_mp_free(pmem);
		if(FATFS_ERR_NONE != len)
		{
			return 0;
		}
		break;
	//ads校准值
	case SMTCFG_AFN_SET_AD_CAL_VAL_0:
	case SMTCFG_AFN_SET_AD_CAL_VAL_1:
	case SMTCFG_AFN_SET_AD_CAL_VAL_2:
	case SMTCFG_AFN_SET_AD_CAL_VAL_3:
	case SMTCFG_AFN_SET_AD_CAL_VAL_4:
	case SMTCFG_AFN_SET_AD_CAL_VAL_5:
	case SMTCFG_AFN_SET_AD_CAL_VAL_6:
	case SMTCFG_AFN_SET_AD_CAL_VAL_7:
		param_info_ptr = ptcl_param_info_req(sizeof(rt_int32_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		if(RT_FALSE == fyyp_str_to_int((rt_int32_t *)(param_info_ptr->pdata), psrc + i, src_len - i))
		{
			rt_mp_free((void *)param_info_ptr);
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_AD_CAL_VAL;
		param_info_ptr->data_len	= grp - SMTCFG_AFN_SET_AD_CAL_VAL_0;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_AD_CAL_VAL_0:
	case SMTCFG_AFN_GET_AD_CAL_VAL_1:
	case SMTCFG_AFN_GET_AD_CAL_VAL_2:
	case SMTCFG_AFN_GET_AD_CAL_VAL_3:
	case SMTCFG_AFN_GET_AD_CAL_VAL_4:
	case SMTCFG_AFN_GET_AD_CAL_VAL_5:
	case SMTCFG_AFN_GET_AD_CAL_VAL_6:
	case SMTCFG_AFN_GET_AD_CAL_VAL_7:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", ads_get_cal_val(grp - SMTCFG_AFN_GET_AD_CAL_VAL_0));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//ads转换值
	case SMTCFG_AFN_GET_AD_VAL_0:
	case SMTCFG_AFN_GET_AD_VAL_1:
	case SMTCFG_AFN_GET_AD_VAL_2:
	case SMTCFG_AFN_GET_AD_VAL_3:
	case SMTCFG_AFN_GET_AD_VAL_4:
	case SMTCFG_AFN_GET_AD_VAL_5:
	case SMTCFG_AFN_GET_AD_VAL_6:
	case SMTCFG_AFN_GET_AD_VAL_7:
		if(RT_TRUE == ads_conv_value(grp - SMTCFG_AFN_GET_AD_VAL_0, &pos))
		{
			len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", pos);
		}
		else
		{
			len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",无效值");
		}
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//蓝牙模块名称
	case SMTCFG_AFN_SET_BT_MODULE_NAME:
		if(RT_FALSE == bt_set_module_name(psrc + i, src_len - i))
		{
			return 0;
		}
		break;
	case SMTCFG_AFN_GET_BT_MODULE_NAME:
		if(RT_FALSE == bt_get_module_name((char **)&pmem))
		{
			return 0;
		}
		if((void *)0 == pmem)
		{
			len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",空");
			if((dst_len + len) < max_len)
			{
				dst_len += len;
			}
			else
			{
				return 0;
			}
		}
		else
		{
			len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%s", (char *)pmem);
			rt_mp_free(pmem);
			if((dst_len + len) < max_len)
			{
				dst_len += len;
			}
			else
			{
				return 0;
			}
		}
		break;
	//蓝牙软件版本
	case SMTCFG_AFN_GET_BT_SOFT_VER:
		if(RT_FALSE == bt_get_soft_ver(&pos))
		{
			return 0;
		}
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",V%d.%d", pos % 0x100, pos / 0x100);
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//蓝牙发射功率
	case SMTCFG_AFN_SET_BT_TX_ENERGY:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		if(RT_FALSE == bt_set_tx_energy(fyyp_str_to_hex(psrc + i, src_len - i)))
		{
			return 0;
		}
		break;
	case SMTCFG_AFN_GET_BT_TX_ENERGY:
		if(RT_FALSE == bt_get_tx_energy((rt_uint8_t *)&pos))
		{
			return 0;
		}
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", *(rt_uint8_t *)&pos);
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//蓝牙接收增益
	case SMTCFG_AFN_SET_BT_RX_AMP:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		if(RT_FALSE == bt_set_rx_amp(fyyp_str_to_hex(psrc + i, src_len - i)))
		{
			return 0;
		}
		break;
	case SMTCFG_AFN_GET_BT_RX_AMP:
		if(RT_FALSE == bt_get_rx_amp((rt_uint8_t *)&pos))
		{
			return 0;
		}
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", *(rt_uint8_t *)&pos);
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//蓝牙广播开关状态
	case SMTCFG_AFN_SET_BT_ADV_STA:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		if(RT_FALSE == bt_set_adv_sta(fyyp_str_to_hex(psrc + i, src_len - i)))
		{
			return 0;
		}
		break;
	case SMTCFG_AFN_GET_BT_ADV_STA:
		if(RT_FALSE == bt_get_adv_sta((rt_uint8_t *)&pos))
		{
			return 0;
		}
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", *(rt_uint8_t *)&pos);
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//蓝牙广播间隔
	case SMTCFG_AFN_SET_BT_ADV_INTERVAL:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		if(RT_FALSE == bt_set_adv_interval(fyyp_str_to_hex(psrc + i, src_len - i)))
		{
			return 0;
		}
		break;
	case SMTCFG_AFN_GET_BT_ADV_INTERVAL:
		if(RT_FALSE == bt_get_adv_interval((rt_uint32_t *)&pmem))
		{
			return 0;
		}
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", (rt_uint32_t)pmem);
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//蓝牙连接间隔
	case SMTCFG_AFN_SET_BT_CON_INTERVAL:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		if(RT_FALSE == bt_set_con_interval(fyyp_str_to_hex(psrc + i, src_len - i)))
		{
			return 0;
		}
		break;
	case SMTCFG_AFN_GET_BT_CON_INTERVAL:
		if(RT_FALSE == bt_get_con_interval((rt_uint32_t *)&pmem))
		{
			return 0;
		}
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", (rt_uint32_t)pmem);
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//内存池信息
	case SMTCFG_AFN_GET_MEMPOOL_128:
	case SMTCFG_AFN_GET_MEMPOOL_256:
	case SMTCFG_AFN_GET_MEMPOOL_512:
	case SMTCFG_AFN_GET_MEMPOOL_1024:
	case SMTCFG_AFN_GET_MEMPOOL_2048:
		pos = mempool_info(grp - SMTCFG_AFN_GET_MEMPOOL_128);
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d,%d", pos / 0x100, pos % 0x100);
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//板载温度值
	case SMTCFG_AFN_GET_DS18B20_TEMPER:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", ds_get_temper());
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//SN
	case SMTCFG_AFN_GET_DS18B20_SN:
		if((dst_len + 25) >= max_len)
		{
			return 0;
		}
		//sn
		pmem = mempool_req(DS_BYTES_SN, RT_WAITING_NO);
		if((void *)0 == pmem)
		{
			return 0;
		}
		if(RT_FALSE == ds_get_sn((rt_uint8_t *)pmem))
		{
			rt_mp_free(pmem);
			return 0;
		}
		pdst[dst_len++] = ',';
		for(len = 0; len < DS_BYTES_SN; len++)
		{
			dst_len += sprintf((char *)(pdst + dst_len), "%02x ", *((rt_uint8_t *)pmem + len));
		}
		rt_mp_free(pmem);
		dst_len--;
		break;
	//测试汉字编码
	case SMTCFG_AFN_SET_FONT_TEST:
		DEBUG_INFO_OUTPUT_HEX(DEBUG_INFO_TYPE_FATFS, (psrc + i, src_len - i));
		DEBUG_INFO_OUTPUT_HEX(DEBUG_INFO_TYPE_FATFS, ("汉字编码123", strlen("汉字编码123")));
		break;
#if 0
	//hj212系统编码
	case SMTCFG_AFN_SET_HJ212_ST:
		if(HJ212_BYTES_ST != (src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(HJ212_BYTES_ST, RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		memcpy((void *)param_info_ptr->pdata, (void *)(psrc + i), HJ212_BYTES_ST);
		param_info_ptr->param_type = PTCL_PARAM_INFO_HJ212_ST;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_HJ212_ST:
		if((dst_len + HJ212_BYTES_ST + 1) > max_len)
		{
			return 0;
		}
		pdst[dst_len++] = ',';
		hj212_get_st(pdst + dst_len);
		dst_len += HJ212_BYTES_ST;
		break;
	//hj212密码
	case SMTCFG_AFN_SET_HJ212_PW:
		if(HJ212_BYTES_PW != (src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(HJ212_BYTES_PW, RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		memcpy((void *)param_info_ptr->pdata, (void *)(psrc + i), HJ212_BYTES_PW);
		param_info_ptr->param_type = PTCL_PARAM_INFO_HJ212_PW;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_HJ212_PW:
		if((dst_len + HJ212_BYTES_PW + 1) > max_len)
		{
			return 0;
		}
		pdst[dst_len++] = ',';
		hj212_get_pw(pdst + dst_len);
		dst_len += HJ212_BYTES_PW;
		break;
	//hj212设备唯一标识
	case SMTCFG_AFN_SET_HJ212_MN:
		if(HJ212_BYTES_MN != (src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(HJ212_BYTES_MN, RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		memcpy((void *)param_info_ptr->pdata, (void *)(psrc + i), HJ212_BYTES_MN);
		param_info_ptr->param_type = PTCL_PARAM_INFO_HJ212_MN;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_HJ212_MN:
		if((dst_len + HJ212_BYTES_MN + 1) > max_len)
		{
			return 0;
		}
		pdst[dst_len++] = ',';
		hj212_get_mn(pdst + dst_len);
		dst_len += HJ212_BYTES_MN;
		break;
	//hj212超时时间
	case SMTCFG_AFN_SET_HJ212_OVER_TIME:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint16_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type				= PTCL_PARAM_INFO_HJ212_OVER_TIME;
		*(rt_uint16_t *)param_info_ptr->pdata	= fyyp_str_to_hex(psrc + i, src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_HJ212_OVER_TIME:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", hj212_get_over_time());
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//hj212重发次数
	case SMTCFG_AFN_SET_HJ212_RECOUNT:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_HJ212_RECOUNT;
		*param_info_ptr->pdata		= fyyp_str_to_hex(psrc + i, src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_HJ212_RECOUNT:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", hj212_get_recount());
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//hj212实时数据上报使能
	case SMTCFG_AFN_SET_HJ212_RTD_EN:
		if((i + 1) != src_len)
		{
			return 0;
		}
		if(('0' != psrc[i]) && ('1' != psrc[i]))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_HJ212_RTD_EN;
		*param_info_ptr->pdata		= ('0' == psrc[i]) ? RT_FALSE : RT_TRUE;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_HJ212_RTD_EN:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%c", (RT_FALSE == hj212_get_rtd_en()) ? '0' : '1');
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//hj212实时数据上报间隔
	case SMTCFG_AFN_SET_HJ212_RTD_INTERVAL:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint16_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type				= PTCL_PARAM_INFO_HJ212_RTD_INTERVAL;
		*(rt_uint16_t *)param_info_ptr->pdata	= fyyp_str_to_hex(psrc + i, src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_HJ212_RTD_INTERVAL:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", hj212_get_rtd_interval());
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//hj212分钟数据上报间隔
	case SMTCFG_AFN_SET_HJ212_MIN_INTERVAL:
		if(RT_FALSE == fyyp_is_number(psrc + i, src_len - i))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint16_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type				= PTCL_PARAM_INFO_HJ212_MIN_INTERVAL;
		*(rt_uint16_t *)param_info_ptr->pdata	= fyyp_str_to_hex(psrc + i, src_len - i);
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_HJ212_MIN_INTERVAL:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", hj212_get_min_interval());
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	//校时请求
	case SMTCFG_AFN_SET_HJ212_TIME_CALI_REQ:
		if((i + 1) != src_len)
		{
			return 0;
		}
		if('1' == psrc[i])
		{
			hj212_time_cali_req();
		}
		break;
	//hj212工况数据上报使能
	case SMTCFG_AFN_SET_HJ212_RS_EN:
		if((i + 1) != src_len)
		{
			return 0;
		}
		if(('0' != psrc[i]) && ('1' != psrc[i]))
		{
			return 0;
		}
		param_info_ptr = ptcl_param_info_req(sizeof(rt_uint8_t), RT_WAITING_NO);
		if((PTCL_PARAM_INFO *)0 == param_info_ptr)
		{
			return 0;
		}
		param_info_ptr->param_type	= PTCL_PARAM_INFO_HJ212_RS_EN;
		*param_info_ptr->pdata		= ('0' == psrc[i]) ? RT_FALSE : RT_TRUE;
		ptcl_param_info_add(param_info_ptr_ptr, param_info_ptr);
		break;
	case SMTCFG_AFN_GET_HJ212_RS_EN:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%c", (RT_FALSE == hj212_get_rs_en()) ? '0' : '1');
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
#endif
	//参数集合信息
	case SMTCFG_AFN_SET_PARAM_SET_ADS1110:
		ads_param_restore();
		break;
	case SMTCFG_AFN_GET_PARAM_SET_ADS1110:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sizeof(ADS_PARAM_SET));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	case SMTCFG_AFN_SET_PARAM_SET_DTU:
		dtu_param_restore();
		break;
	case SMTCFG_AFN_GET_PARAM_SET_DTU:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sizeof(DTU_PARAM_SET));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	case SMTCFG_AFN_SET_PARAM_SET_HJ212:
		hj212_param_restore();
		break;
	case SMTCFG_AFN_GET_PARAM_SET_HJ212:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sizeof(HJ212_PARAM_SET));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	case SMTCFG_AFN_SET_PARAM_SET_PTCL:
		ptcl_param_restore();
		break;
	case SMTCFG_AFN_GET_PARAM_SET_PTCL:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sizeof(PTCL_PARAM_SET));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	case SMTCFG_AFN_SET_PARAM_SET_SAMPLE:
		sample_param_restore();
		break;
	case SMTCFG_AFN_GET_PARAM_SET_SAMPLE:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sizeof(SAMPLE_PARAM_SET));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	case SMTCFG_AFN_SET_PARAM_SET_SMT_MANAGE:
		smtmng_param_restore();
		break;
	case SMTCFG_AFN_GET_PARAM_SET_SMT_MANAGE:
		len = snprintf((char *)(pdst + dst_len), max_len - dst_len, ",%d", sizeof(SMTMNG_PARAM_SET));
		if((dst_len + len) < max_len)
		{
			dst_len += len;
		}
		else
		{
			return 0;
		}
		break;
	}

	return dst_len;
}



/*
>TX+CMDR14=#10
>RX+CMDR14=#10,RTU2-2A0A-DZJSP-V1.0-DEBUG,5A 3A A2 0A 00 00,9999000002

>TX+CMDW14=#30,19-03-08,15-14-07
>RX+CMDW14=#30,100
*/
rt_uint8_t smtcfg_data_decoder(PTCL_REPORT_DATA *report_data_ptr, PTCL_RECV_DATA const *recv_data_ptr, PTCL_PARAM_INFO **param_info_ptr_ptr)
{
	rt_uint8_t	is_wr, grp;
	rt_uint16_t	i, cmd_pos, cmd_len;

	//>TX+CMD
	i = rt_strlen(SMTCFG_FRAME_HEAD_DOWN);
	if(i >= recv_data_ptr->data_len)
	{
		return RT_FALSE;
	}
	if(0 != rt_memcmp((void *)recv_data_ptr->pdata, (void *)SMTCFG_FRAME_HEAD_DOWN, i))
	{
		return RT_FALSE;
	}

	//确认是smt_cfg协议
	report_data_ptr->data_len = 0;
	//WR
	if((i + 1) >= recv_data_ptr->data_len)
	{
		return RT_TRUE;
	}
	if(SMTCFG_FRAME_WRITE == recv_data_ptr->pdata[i])
	{
		is_wr = RT_TRUE;
	}
	else if(SMTCFG_FRAME_READ == recv_data_ptr->pdata[i])
	{
		is_wr = RT_FALSE;
	}
	else
	{
		return RT_TRUE;
	}
	i++;
	//grp
	cmd_pos = i;
	while(i < recv_data_ptr->data_len)
	{
		if(SMTCFG_FRAME_SEPARATOR == recv_data_ptr->pdata[i++])
		{
			cmd_len = i - cmd_pos - 1;
			if(0 == cmd_len)
			{
				return RT_TRUE;
			}
			if(RT_FALSE == fyyp_is_number(recv_data_ptr->pdata + cmd_pos, cmd_len))
			{
				return RT_TRUE;
			}
			grp = fyyp_str_to_hex(recv_data_ptr->pdata + cmd_pos, cmd_len);
			break;
		}
	}
	if(i >= recv_data_ptr->data_len)
	{
		return RT_TRUE;
	}
	//应答帧头
	cmd_len = snprintf((char *)(report_data_ptr->pdata + report_data_ptr->data_len), PTCL_BYTES_ACK_REPORT, "%s%c%d", SMTCFG_FRAME_HEAD_UP, (RT_TRUE == is_wr) ? SMTCFG_FRAME_WRITE : SMTCFG_FRAME_READ, grp);
	if((report_data_ptr->data_len + cmd_len) >= PTCL_BYTES_ACK_REPORT)
	{
		return RT_TRUE;
	}
	report_data_ptr->data_len += cmd_len;
	//解码
	cmd_pos = i;
	while(i < recv_data_ptr->data_len)
	{
		if(SMTCFG_FRAME_SEPARATOR == recv_data_ptr->pdata[i++])
		{
			cmd_len = i - cmd_pos - 1;
			report_data_ptr->data_len += _smtcfg_item_decoder(is_wr, report_data_ptr->pdata + report_data_ptr->data_len, PTCL_BYTES_ACK_REPORT - report_data_ptr->data_len, param_info_ptr_ptr, recv_data_ptr->pdata + cmd_pos, cmd_len, grp, recv_data_ptr);
			cmd_pos = i;
		}
	}
	cmd_len = i - cmd_pos;
	report_data_ptr->data_len += _smtcfg_item_decoder(is_wr, report_data_ptr->pdata + report_data_ptr->data_len, PTCL_BYTES_ACK_REPORT - report_data_ptr->data_len, param_info_ptr_ptr, recv_data_ptr->pdata + cmd_pos, cmd_len, grp, recv_data_ptr);

	report_data_ptr->fun_csq_update	= (void *)0;
	report_data_ptr->data_id		= 0;
	report_data_ptr->need_reply		= RT_FALSE;
	report_data_ptr->fcb_value		= 0;
	report_data_ptr->ptcl_type		= PTCL_PTCL_TYPE_SMT_CFG;

	return RT_TRUE;
}

