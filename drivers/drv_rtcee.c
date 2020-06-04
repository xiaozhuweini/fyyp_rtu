#include "drv_rtcee.h"
#include "drv_iic.h"



static struct rt_event m_rtcee_event;



static void _rtcee_pend(void)
{
	if(RT_EOK != rt_event_recv(&m_rtcee_event, RTCEE_EVENT_MODULE_PEND, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}
}

static void _rtcee_post(void)
{
	if(RT_EOK != rt_event_send(&m_rtcee_event, RTCEE_EVENT_MODULE_PEND))
	{
		while(1);
	}
}

static void _rtcee_rtc_open(void)
{
	pin_clock_enable(RTCEE_PIN_SCL, RT_TRUE);
	pin_mode(RTCEE_PIN_SCL, PIN_MODE_O_OD_NOPULL);

	pin_clock_enable(RTCEE_PIN_SDA, RT_TRUE);
	pin_mode(RTCEE_PIN_SDA, PIN_MODE_O_OD_NOPULL);
}

static void _rtcee_rtc_close(void)
{
	pin_mode(RTCEE_PIN_SCL, PIN_MODE_ANALOG);
	pin_clock_enable(RTCEE_PIN_SCL, RT_FALSE);

	pin_mode(RTCEE_PIN_SDA, PIN_MODE_ANALOG);
	pin_clock_enable(RTCEE_PIN_SDA, RT_FALSE);
}

static void _rtcee_eeprom_open(void)
{
	pin_clock_enable(RTCEE_PIN_SCL, RT_TRUE);
	pin_mode(RTCEE_PIN_SCL, PIN_MODE_O_OD_NOPULL);

	pin_clock_enable(RTCEE_PIN_SDA, RT_TRUE);
	pin_mode(RTCEE_PIN_SDA, PIN_MODE_O_OD_NOPULL);

	pin_clock_enable(RTCEE_PIN_EE_WP, RT_TRUE);
	pin_mode(RTCEE_PIN_EE_WP, PIN_MODE_O_OD_NOPULL);
	pin_write(RTCEE_PIN_EE_WP, PIN_STA_LOW);
}

static void _rtcee_eeprom_close(void)
{
	pin_mode(RTCEE_PIN_SCL, PIN_MODE_ANALOG);
	pin_clock_enable(RTCEE_PIN_SCL, RT_FALSE);

	pin_mode(RTCEE_PIN_SDA, PIN_MODE_ANALOG);
	pin_clock_enable(RTCEE_PIN_SDA, RT_FALSE);

	pin_write(RTCEE_PIN_EE_WP, PIN_STA_HIGH);
	pin_mode(RTCEE_PIN_EE_WP, PIN_MODE_ANALOG);
	pin_clock_enable(RTCEE_PIN_EE_WP, RT_FALSE);
}



#ifdef RTCEE_RTC

#include "drv_fyyp.h"



static rt_uint8_t m_rtcee_time_reg[RTCEE_RTC_NUM_TIME_REG];



static rt_uint8_t _rtcee_rtc_write_regs(rt_uint8_t reg_addr, rt_uint8_t const *pdata, rt_uint8_t data_len)
{
	rt_uint8_t sta;

	iic_start(RTCEE_PIN_SCL, RTCEE_PIN_SDA);
	if(RT_FALSE == iic_write_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, RTCEE_RTC_CTRL_WR))
	{
		sta = RTCEE_RTC_ERR_ACK;
		goto __exit;
	}
	if(RT_FALSE == iic_write_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, reg_addr))
	{
		sta = RTCEE_RTC_ERR_ACK;
		goto __exit;
	}
	while(data_len--)
	{
		if(RT_FALSE == iic_write_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, *pdata++))
		{
			sta = RTCEE_RTC_ERR_ACK;
			goto __exit;
		}
	}
	sta = RTCEE_RTC_ERR_NONE;

__exit:
	iic_stop(RTCEE_PIN_SCL, RTCEE_PIN_SDA);
	
	return sta;
}

static rt_uint8_t _rtcee_rtc_read_regs(rt_uint8_t reg_addr, rt_uint8_t *pdata, rt_uint8_t data_len)
{
	rt_uint8_t sta;

	iic_start(RTCEE_PIN_SCL, RTCEE_PIN_SDA);
	if(RT_FALSE == iic_write_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, RTCEE_RTC_CTRL_WR))
	{
		sta = RTCEE_RTC_ERR_ACK;
		goto __exit;
	}
	if(RT_FALSE == iic_write_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, reg_addr))
	{
		sta = RTCEE_RTC_ERR_ACK;
		goto __exit;
	}
	iic_start(RTCEE_PIN_SCL, RTCEE_PIN_SDA);
	if(RT_FALSE == iic_write_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, RTCEE_RTC_CTRL_RD))
	{
		sta = RTCEE_RTC_ERR_ACK;
		goto __exit;
	}
	while(data_len--)
	{
		*pdata++ = iic_read_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, (0 == data_len) ? RT_FALSE : RT_TRUE);
	}
	sta = RTCEE_RTC_ERR_NONE;

__exit:
	iic_stop(RTCEE_PIN_SCL, RTCEE_PIN_SDA);
	
	return sta;
}

static void _rtcee_rtc_irq_hdr(void *args)
{
	rt_event_send(&m_rtcee_event, RTCEE_EVENT_TIME_UPDATE);
}

#endif

#ifdef RTCEE_EEPROM

/*说明
**写：只能PAGE WRITE，即一次操作最多可写一页的数据；
**读：可以整个芯片读；
*/
static rt_uint8_t _rtcee_eeprom_write_page(rt_uint32_t addr, rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint8_t sta;
	
	iic_start(RTCEE_PIN_SCL, RTCEE_PIN_SDA);
#if (RTCEE_EEPROM_BYTES_CHIP > RTCEE_EEPROM_BYTES_SEPARATOR)
	if(RT_FALSE == iic_write_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, RTCEE_EEPROM_CTRL_WR))
	{
		sta = RTCEE_EEPROM_ERR_ACK;
		goto __exit;
	}
	if(RT_FALSE == iic_write_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, (rt_uint8_t)(addr >> 8)))
	{
		sta = RTCEE_EEPROM_ERR_ACK;
		goto __exit;
	}
#else
	if(RT_FALSE == iic_write_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, (rt_uint8_t)(RTCEE_EEPROM_CTRL_WR + ((addr >> 8) << 1))))
	{
		sta = RTCEE_EEPROM_ERR_ACK;
		goto __exit;
	}
#endif
	if(RT_FALSE == iic_write_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, (rt_uint8_t)addr))
	{
		sta = RTCEE_EEPROM_ERR_ACK;
		goto __exit;
	}
	while(data_len--)
	{
		if(RT_FALSE == iic_write_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, *pdata++))
		{
			sta = RTCEE_EEPROM_ERR_ACK;
			goto __exit;
		}
	}
	sta = RTCEE_EEPROM_ERR_NONE;

__exit:
	iic_stop(RTCEE_PIN_SCL, RTCEE_PIN_SDA);
	if(RTCEE_EEPROM_ERR_NONE == sta)
	{
		rt_thread_delay(RT_TICK_PER_SECOND / 20);
	}
	
	return sta;
}

#endif



static int _rtcee_device_init(void)
{
	rt_uint8_t data;
	
	if(RT_EOK != rt_event_init(&m_rtcee_event, "rtcee", RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	if(RT_EOK != rt_event_send(&m_rtcee_event, RTCEE_EVENT_MODULE_PEND + RTCEE_EVENT_TIME_CONV))
	{
		while(1);
	}

#ifdef RTCEE_RTC

	pin_attach_irq(RTCEE_PIN_RTC_INT, PIN_IRQ_MODE_FALLING, _rtcee_rtc_irq_hdr, (void *)0);
	pin_clock_enable(RTCEE_PIN_RTC_INT, RT_TRUE);
	pin_irq_enable(RTCEE_PIN_RTC_INT, RT_TRUE);
	pin_clock_enable(RTCEE_PIN_RTC_INT, RT_FALSE);

	_rtcee_rtc_open();

	data = 0xa5;
	if(RTCEE_RTC_ERR_NONE != _rtcee_rtc_write_regs(RTCEE_RTC_REG_RAM, &data, 1))
	{
		while(1);
	}
	data = 0;
	if(RTCEE_RTC_ERR_NONE != _rtcee_rtc_read_regs(RTCEE_RTC_REG_RAM, &data, 1))
	{
		while(1);
	}
	while(0xa5 != data);

	//标志寄存器
	data = 0;
	if(RTCEE_RTC_ERR_NONE != _rtcee_rtc_write_regs(RTCEE_RTC_REG_FLAG, &data, 1))
	{
		while(1);
	}
	//控制寄存器
	data = 0x40;
	if(RTCEE_RTC_ERR_NONE != _rtcee_rtc_write_regs(RTCEE_RTC_REG_CTRL, &data, 1))
	{
		while(1);
	}
	//扩展寄存器
	data = 0;
	if(RTCEE_RTC_ERR_NONE != _rtcee_rtc_write_regs(RTCEE_RTC_REG_EXTENSION, &data, 1))
	{
		while(1);
	}
	//控制寄存器
	data = 0x60;
	if(RTCEE_RTC_ERR_NONE != _rtcee_rtc_write_regs(RTCEE_RTC_REG_CTRL, &data, 1))
	{
		while(1);
	}

	_rtcee_rtc_close();

#endif

	return 0;
}
INIT_DEVICE_EXPORT(_rtcee_device_init);



#ifdef RTCEE_RTC

struct tm rtcee_rtc_get_calendar(void)
{
	struct tm calendar_time;

	_rtcee_pend();
	_rtcee_rtc_open();
	
	if(RTCEE_RTC_ERR_NONE != _rtcee_rtc_read_regs(RTCEE_RTC_REG_SEC, m_rtcee_time_reg, RTCEE_RTC_NUM_TIME_REG))
	{
		_rtcee_rtc_close();
		_rtcee_post();
		
		calendar_time.tm_year	= 2000;
		calendar_time.tm_mon	= 0;
		calendar_time.tm_mday	= 1;
		calendar_time.tm_wday	= 0;
		calendar_time.tm_hour	= 0;
		calendar_time.tm_min	= 0;
		calendar_time.tm_sec	= 0;
	}
	else
	{
		calendar_time.tm_year	= 2000 + fyyp_bcd_to_hex(m_rtcee_time_reg[RTCEE_RTC_REG_YEAR]);
		calendar_time.tm_mon	= fyyp_bcd_to_hex(m_rtcee_time_reg[RTCEE_RTC_REG_MONTH]) - 1;
		calendar_time.tm_mday	= fyyp_bcd_to_hex(m_rtcee_time_reg[RTCEE_RTC_REG_MDAY]);
		for(calendar_time.tm_wday = 0; calendar_time.tm_wday < 7; calendar_time.tm_wday++)
		{
			if(m_rtcee_time_reg[RTCEE_RTC_REG_WDAY] == (1 << calendar_time.tm_wday))
			{
				break;
			}
		}
		calendar_time.tm_hour	= fyyp_bcd_to_hex(m_rtcee_time_reg[RTCEE_RTC_REG_HOUR]);
		calendar_time.tm_min	= fyyp_bcd_to_hex(m_rtcee_time_reg[RTCEE_RTC_REG_MIN]);
		calendar_time.tm_sec	= fyyp_bcd_to_hex(m_rtcee_time_reg[RTCEE_RTC_REG_SEC]);

		_rtcee_rtc_close();
		_rtcee_post();
	}
	
	return calendar_time;
}

void rtcee_rtc_set_calendar(struct tm calendar_time)
{
	_rtcee_pend();
	_rtcee_rtc_open();
	
	m_rtcee_time_reg[RTCEE_RTC_REG_SEC]			= fyyp_hex_to_bcd(calendar_time.tm_sec);
	m_rtcee_time_reg[RTCEE_RTC_REG_MIN]			= fyyp_hex_to_bcd(calendar_time.tm_min);
	m_rtcee_time_reg[RTCEE_RTC_REG_HOUR]		= fyyp_hex_to_bcd(calendar_time.tm_hour);
	m_rtcee_time_reg[RTCEE_RTC_REG_WDAY]		= (1 << calendar_time.tm_wday);
	m_rtcee_time_reg[RTCEE_RTC_REG_MDAY]		= fyyp_hex_to_bcd(calendar_time.tm_mday);
	m_rtcee_time_reg[RTCEE_RTC_REG_MONTH]		= fyyp_hex_to_bcd(calendar_time.tm_mon + 1);
	m_rtcee_time_reg[RTCEE_RTC_REG_YEAR]		= fyyp_hex_to_bcd(calendar_time.tm_year - 2000);
	_rtcee_rtc_write_regs(RTCEE_RTC_REG_SEC, m_rtcee_time_reg, RTCEE_RTC_NUM_TIME_REG);

	_rtcee_rtc_close();
	_rtcee_post();
}

time_t rtcee_rtc_calendar_to_unix(struct tm calendar_time)
{
	calendar_time.tm_isdst = 0;
	calendar_time.tm_year -= 1900;
	
	return mktime(&calendar_time);
}

struct tm rtcee_rtc_unix_to_calendar(time_t unix)
{
	struct tm *p_calendar_time, calendar_time;
	
	if(RT_EOK != rt_event_recv(&m_rtcee_event, RTCEE_EVENT_TIME_CONV, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}
	
	p_calendar_time = localtime(&unix);
	calendar_time = *p_calendar_time;
	
	if(RT_EOK != rt_event_send(&m_rtcee_event, RTCEE_EVENT_TIME_CONV))
	{
		while(1);
	}
	calendar_time.tm_year += 1900;
	
	return calendar_time;
}

void rtcee_rtc_sec_pend(void)
{
	if(RT_EOK != rt_event_recv(&m_rtcee_event, RTCEE_EVENT_TIME_UPDATE, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}
}

#endif

#ifdef RTCEE_EEPROM

rt_uint8_t rtcee_eeprom_read(rt_uint32_t addr, rt_uint8_t *pdata, rt_uint16_t data_len)
{
	rt_uint8_t sta;
	
	if(addr >= RTCEE_EEPROM_BYTES_CHIP)
	{
		return RTCEE_EEPROM_ERR_ARG;
	}
	if((rt_uint8_t *)0 == pdata)
	{
		return RTCEE_EEPROM_ERR_ARG;
	}
	if(0 == data_len)
	{
		return RTCEE_EEPROM_ERR_NONE;
	}
	
	_rtcee_pend();
	_rtcee_eeprom_open();
	
	iic_start(RTCEE_PIN_SCL, RTCEE_PIN_SDA);
#if (RTCEE_EEPROM_BYTES_CHIP > RTCEE_EEPROM_BYTES_SEPARATOR)
	if(RT_FALSE == iic_write_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, RTCEE_EEPROM_CTRL_WR))
	{
		sta = RTCEE_EEPROM_ERR_ACK;
		goto __exit;
	}
	if(RT_FALSE == iic_write_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, (rt_uint8_t)(addr >> 8)))
	{
		sta = RTCEE_EEPROM_ERR_ACK;
		goto __exit;
	}
#else
	if(RT_FALSE == iic_write_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, (rt_uint8_t)(RTCEE_EEPROM_CTRL_WR + ((addr >> 8) << 1))))
	{
		sta = RTCEE_EEPROM_ERR_ACK;
		goto __exit;
	}
#endif
	if(RT_FALSE == iic_write_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, (rt_uint8_t)addr))
	{
		sta = RTCEE_EEPROM_ERR_ACK;
		goto __exit;
	}
	iic_start(RTCEE_PIN_SCL, RTCEE_PIN_SDA);
#if (RTCEE_EEPROM_BYTES_CHIP > RTCEE_EEPROM_BYTES_SEPARATOR)
	if(RT_FALSE == iic_write_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, RTCEE_EEPROM_CTRL_RD))
	{
		sta = RTCEE_EEPROM_ERR_ACK;
		goto __exit;
	}
#else
	if(RT_FALSE == iic_write_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, (rt_uint8_t)(RTCEE_EEPROM_CTRL_RD + ((addr >> 8) << 1))))
	{
		sta = RTCEE_EEPROM_ERR_ACK;
		goto __exit;
	}
#endif
	while(data_len--)
	{
		*pdata++ = iic_read_byte(RTCEE_PIN_SCL, RTCEE_PIN_SDA, (0 == data_len) ? RT_FALSE : RT_TRUE);
	}
	sta = RTCEE_EEPROM_ERR_NONE;

__exit:
	iic_stop(RTCEE_PIN_SCL, RTCEE_PIN_SDA);

	_rtcee_eeprom_close();
	_rtcee_post();
	
	return sta;
}

rt_uint8_t rtcee_eeprom_write(rt_uint32_t addr, rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint16_t	pos = 0, wlen;
	rt_uint8_t	sta;
	
	if(addr >= RTCEE_EEPROM_BYTES_CHIP)
	{
		return RTCEE_EEPROM_ERR_ARG;
	}
	if((rt_uint8_t *)0 == pdata)
	{
		return RTCEE_EEPROM_ERR_ARG;
	}
	if(0 == data_len)
	{
		return RTCEE_EEPROM_ERR_NONE;
	}
	
	_rtcee_pend();
	_rtcee_eeprom_open();
	
	while(pos < data_len)
	{
		wlen = addr % RTCEE_EEPROM_BYTES_PAGE;
		wlen = RTCEE_EEPROM_BYTES_PAGE - wlen;
		if((pos + wlen) > data_len)
		{
			wlen = data_len - pos;
		}
		if(RTCEE_EEPROM_ERR_NONE != _rtcee_eeprom_write_page(addr, pdata + pos, wlen))
		{
			sta = RTCEE_EEPROM_ERR_WR_PAGE;
			goto __exit;
		}
		pos += wlen;
		addr += wlen;
		if(addr >= RTCEE_EEPROM_BYTES_CHIP)
		{
			addr = 0;
		}
	}
	sta = RTCEE_EEPROM_ERR_NONE;

__exit:
	_rtcee_eeprom_close();
	_rtcee_post();
	
	return sta;
}

#endif

