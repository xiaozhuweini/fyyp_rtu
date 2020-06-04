/*note
ec20:status管脚低电平表示开机；开机脉冲不少于100ms；关机脉冲不少于600ms；
*/



#include "drv_pin.h"
#include "drv_uart.h"
#include "drv_rtcee.h"
#include "drv_lpm.h"
#include "drv_fyyp.h"
#include "stdio.h"



#define DTU_PIN_STATUS	PIN_INDEX_57
#define DTU_PIN_PWREN	PIN_INDEX_58
#define DTU_PIN_PWRKEY	PIN_INDEX_48
#define DTU_COMM_PORT	UART_PERIPH_3
#define DTU_COMM_RATE	UART_BAUD_RATE_115200

//ec20是低电平开机
#ifdef DTU_MODULE_EC20
#define DTU_MODULE_STA	((PIN_STA_LOW == pin_read(DTU_PIN_STATUS)) ? RT_TRUE : RT_FALSE)
#define DTU_PULSE_ON	110	//毫秒
#define DTU_PULSE_OFF	610	//毫秒
#define DTU_WAIT_ON		2	//秒
#define DTU_WAIT_OFF	60	//秒
#endif

//sim800c是高电平开机
#ifdef DTU_MODULE_SIM800C
#define DTU_MODULE_STA	((PIN_STA_HIGH == pin_read(DTU_PIN_STATUS)) ? RT_TRUE : RT_FALSE)
#endif



static void _dtu_open(void)
{
	lpm_cpu_ref(RT_TRUE);
	
	pin_clock_enable(DTU_PIN_PWREN, RT_TRUE);
	pin_mode(DTU_PIN_PWREN, PIN_MODE_O_PP_NOPULL);
	pin_write(DTU_PIN_PWREN, PIN_STA_LOW);

	pin_clock_enable(DTU_PIN_PWRKEY, RT_TRUE);
	pin_mode(DTU_PIN_PWRKEY, PIN_MODE_O_PP_NOPULL);
	pin_write(DTU_PIN_PWRKEY, PIN_STA_LOW);

	pin_clock_enable(DTU_PIN_STATUS, RT_TRUE);
	pin_mode(DTU_PIN_STATUS, PIN_MODE_I_NOPULL);

	uart_open(DTU_COMM_PORT, DTU_COMM_RATE, _dtu_recv_buf_fill, (void *)0);
}

static void _dtu_close(void)
{
	pin_write(DTU_PIN_PWREN, PIN_STA_LOW);
	pin_mode(DTU_PIN_PWREN, PIN_MODE_ANALOG);
	pin_clock_enable(DTU_PIN_PWREN, RT_FALSE);

	pin_write(DTU_PIN_PWRKEY, PIN_STA_LOW);
	pin_mode(DTU_PIN_PWRKEY, PIN_MODE_ANALOG);
	pin_clock_enable(DTU_PIN_PWRKEY, RT_FALSE);

	pin_mode(DTU_PIN_STATUS, PIN_MODE_ANALOG);
	pin_clock_enable(DTU_PIN_STATUS, RT_FALSE);

	uart_close(DTU_COMM_PORT);

	lpm_cpu_ref(RT_FALSE);
}

//开关机
static rt_uint8_t _dtu_module_sta_switch(rt_uint8_t module_sta)
{
	rt_uint8_t i = 0;
	
	while((module_sta != DTU_MODULE_STA) && (i < 3))
	{
		pin_write(DTU_PIN_PWRKEY, PIN_STA_HIGH);
		
		if(RT_TRUE == module_sta)
		{
			rt_thread_delay(DTU_PULSE_ON * RT_TICK_PER_SECOND / 1000);
		}
		else
		{
			rt_thread_delay(DTU_PULSE_OFF * RT_TICK_PER_SECOND / 1000);
		}
		
		pin_write(DTU_PIN_PWRKEY, PIN_STA_LOW);
		
		i++;
		if(RT_TRUE == module_sta)
		{
			rt_thread_delay(DTU_WAIT_ON * RT_TICK_PER_SECOND);
		}
		else
		{
			rt_thread_delay(DTU_WAIT_OFF * RT_TICK_PER_SECOND);
		}
	}
	
	return DTU_MODULE_STA;
}

//通断电
static void _dtu_pwr_sta_switch(rt_uint8_t pwr_sta)
{
	pin_write(DTU_PIN_PWREN, (RT_TRUE == pwr_sta) ? PIN_STA_HIGH : PIN_STA_LOW);
}

//给模块发送数据
static void _dtu_send_to_module(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	uart_send_data(DTU_COMM_PORT, pdata, data_len);
}



//参数集
static rt_uint8_t _dtu_get_param_set(DTU_PARAM_SET *param_set_ptr)
{
	return (RTCEE_EEPROM_ERR_NONE == rtcee_eeprom_read(DTU_BASE_ADDR_PARAM, (rt_uint8_t *)param_set_ptr, sizeof(DTU_PARAM_SET))) ? RT_TRUE : RT_FALSE;
}
static rt_uint8_t _dtu_set_param_set(DTU_PARAM_SET const *param_set_ptr)
{
	return (RTCEE_EEPROM_ERR_NONE == rtcee_eeprom_write(DTU_BASE_ADDR_PARAM, (rt_uint8_t *)param_set_ptr, sizeof(DTU_PARAM_SET))) ? RT_TRUE : RT_FALSE;
}
static void _dtu_validate_param_set(DTU_PARAM_SET *param_set_ptr)
{
	rt_uint8_t i;

	for(i = 0; i < DTU_NUM_IP_CH; i++)
	{
		if(param_set_ptr->ip_addr[i].addr_len > DTU_BYTES_IP_ADDR)
		{
			param_set_ptr->ip_addr[i].addr_len = 0;
		}

		if(param_set_ptr->run_mode[i] >= DTU_NUM_RUN_MODE)
		{
			param_set_ptr->run_mode[i] = DTU_RUN_MODE_PWR;
		}
	}

	for(i = 0; i < DTU_NUM_SMS_CH; i++)
	{
		if(param_set_ptr->sms_addr[i].addr_len > DTU_BYTES_SMS_ADDR)
		{
			param_set_ptr->sms_addr[i].addr_len = 0;
		}
	}

	for(i = 0; i < DTU_NUM_SUPER_PHONE; i++)
	{
		if(param_set_ptr->super_phone[i].addr_len > DTU_BYTES_SMS_ADDR)
		{
			param_set_ptr->super_phone[i].addr_len = 0;
		}
	}

	if(param_set_ptr->apn.apn_len > DTU_BYTES_APN)
	{
		param_set_ptr->apn.apn_len = 0;
	}
}
static void _dtu_reset_param_set(DTU_PARAM_SET *param_set_ptr)
{
	rt_uint8_t i;

	for(i = 0; i < DTU_NUM_IP_CH; i++)
	{
		param_set_ptr->ip_addr[i].addr_len	= 0;
		param_set_ptr->heart_period[i]		= 0;
		param_set_ptr->run_mode[i]			= DTU_RUN_MODE_PWR;
	}

	for(i = 0; i < DTU_NUM_SMS_CH; i++)
	{
		param_set_ptr->sms_addr[i].addr_len = 0;
	}

	for(i = 0; i < DTU_NUM_SUPER_PHONE; i++)
	{
		param_set_ptr->super_phone[i].addr_len = 0;
	}
	param_set_ptr->super_phone[0].addr_len = snprintf((char *)param_set_ptr->super_phone[0].addr_data, DTU_BYTES_SMS_ADDR, "18629257314");

	param_set_ptr->ip_type			= 0;
	param_set_ptr->ip_ch			= 0;
	param_set_ptr->sms_ch			= 0;
	param_set_ptr->apn.apn_len		= snprintf((char *)param_set_ptr->apn.apn_data, DTU_BYTES_APN, "CMNET");
}

//IP地址
static void _dtu_set_ip_addr(DTU_IP_ADDR const *p, rt_uint8_t ch)
{
	rt_uint32_t addr = DTU_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(DTU_PARAM_SET, ip_addr[ch]);

	rtcee_eeprom_write(addr, (rt_uint8_t *)p, sizeof(DTU_IP_ADDR));
}

//SMS地址
static void _dtu_set_sms_addr(DTU_SMS_ADDR const *p, rt_uint8_t ch)
{
	rt_uint32_t addr = DTU_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(DTU_PARAM_SET, sms_addr[ch]);

	rtcee_eeprom_write(addr, (rt_uint8_t *)p, sizeof(DTU_SMS_ADDR));
}

//IP连接类型
static void _dtu_set_ip_type(rt_uint8_t ip_type)
{
	rt_uint32_t addr = DTU_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(DTU_PARAM_SET, ip_type);

	rtcee_eeprom_write(addr, &ip_type, sizeof(rt_uint8_t));
}

//心跳包周期
static void _dtu_set_heart_period(rt_uint16_t heart_period, rt_uint8_t ch)
{
	rt_uint32_t addr = DTU_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(DTU_PARAM_SET, heart_period[ch]);

	rtcee_eeprom_write(addr, (rt_uint8_t *)&heart_period, sizeof(rt_uint16_t));
}

//运行模式
static void _dtu_set_run_mode(rt_uint8_t run_mode, rt_uint8_t ch)
{
	rt_uint32_t addr = DTU_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(DTU_PARAM_SET, run_mode[ch]);

	rtcee_eeprom_write(addr, &run_mode, sizeof(rt_uint8_t));
}

//权限手机
static void _dtu_set_super_phone(DTU_SMS_ADDR const *p, rt_uint8_t ch)
{
	rt_uint32_t addr = DTU_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(DTU_PARAM_SET, super_phone[ch]);

	rtcee_eeprom_write(addr, (rt_uint8_t *)p, sizeof(DTU_SMS_ADDR));
}

//IP通道
static void _dtu_set_ip_ch(rt_uint8_t ch)
{
	rt_uint32_t addr = DTU_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(DTU_PARAM_SET, ip_ch);

	rtcee_eeprom_write(addr, &ch, sizeof(rt_uint8_t));
}

//SMS通道
static void _dtu_set_sms_ch(rt_uint8_t ch)
{
	rt_uint32_t addr = DTU_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(DTU_PARAM_SET, sms_ch);

	rtcee_eeprom_write(addr, &ch, sizeof(rt_uint8_t));
}

//APN
static void _dtu_set_apn(DTU_APN const *p)
{
	rt_uint32_t addr = DTU_BASE_ADDR_PARAM;

	addr += FYYP_MEMBER_ADDR_OFFSET(DTU_PARAM_SET, apn);

	rtcee_eeprom_write(addr, (rt_uint8_t *)p, sizeof(DTU_APN));
}

