#include "drv_iic.h"
#include "drv_pin.h"



static void _iic_delay(void)
{
	rt_uint8_t i = 50;

	while(i--);
}

void iic_start(rt_uint16_t pin_scl, rt_uint16_t pin_sda)
{
	pin_write(pin_scl, PIN_STA_LOW);
	_iic_delay();
	pin_write(pin_sda, PIN_STA_HIGH);
	_iic_delay();
	pin_write(pin_scl, PIN_STA_HIGH);
	_iic_delay();
	pin_write(pin_sda, PIN_STA_LOW);
	_iic_delay();
}

void iic_stop(rt_uint16_t pin_scl, rt_uint16_t pin_sda)
{
	pin_write(pin_scl, PIN_STA_LOW);
	_iic_delay();
	pin_write(pin_sda, PIN_STA_LOW);
	_iic_delay();
	pin_write(pin_scl, PIN_STA_HIGH);
	_iic_delay();
	pin_write(pin_sda, PIN_STA_HIGH);
	_iic_delay();
}

rt_uint8_t iic_write_byte(rt_uint16_t pin_scl, rt_uint16_t pin_sda, rt_uint8_t wbyte)
{
	rt_uint8_t i;
	
	for(i = 0; i < 8; i++)
	{
		pin_write(pin_scl, PIN_STA_LOW);
		_iic_delay();
		if(wbyte & 0x80)
		{
			pin_write(pin_sda, PIN_STA_HIGH);
		}
		else
		{
			pin_write(pin_sda, PIN_STA_LOW);
		}
		_iic_delay();
		pin_write(pin_scl, PIN_STA_HIGH);
		_iic_delay();
		wbyte <<= 1;
	}
	//SCL置低，让芯片控制SDA给ACK
	pin_write(pin_scl, PIN_STA_LOW);
	_iic_delay();
	//SDA置高释放数据线
	pin_write(pin_sda, PIN_STA_HIGH);
	_iic_delay();
	//SDA切换为输入
	pin_mode(pin_sda, PIN_MODE_I_NOPULL);
	//读芯片的ACK
	pin_write(pin_scl, PIN_STA_HIGH);
	_iic_delay();
	i = (PIN_STA_LOW == pin_read(pin_sda)) ? RT_TRUE : RT_FALSE;
	//SDA切换为输出
	pin_write(pin_scl, PIN_STA_LOW);
	_iic_delay();
	pin_mode(pin_sda, PIN_MODE_O_OD_NOPULL);
	
	return i;
}

rt_uint8_t iic_read_byte(rt_uint16_t pin_scl, rt_uint16_t pin_sda, rt_uint8_t ack)
{
	rt_uint8_t i, rbyte = 0;
	
	//SCL置低，让芯片控制SDA给数据的bit7
	pin_write(pin_scl, PIN_STA_LOW);
	_iic_delay();
	//SDA置高释放数据线
	pin_write(pin_sda, PIN_STA_HIGH);
	_iic_delay();
	//SDA切换为输入
	pin_mode(pin_sda, PIN_MODE_I_NOPULL);
	//读芯片SDA
	for(i = 0; i < 8; i++)
	{
		pin_write(pin_scl, PIN_STA_HIGH);
		_iic_delay();
		rbyte <<= 1;
		rbyte += ((PIN_STA_HIGH == pin_read(pin_sda)) ? 1 : 0);
		pin_write(pin_scl, PIN_STA_LOW);
		_iic_delay();
	}
	//SDA切换为输出
	pin_mode(pin_sda, PIN_MODE_O_OD_NOPULL);
	//给芯片ACK
	if(RT_TRUE == ack)
	{
		pin_write(pin_sda, PIN_STA_LOW);
	}
	else
	{
		pin_write(pin_sda, PIN_STA_HIGH);
	}
	_iic_delay();
	pin_write(pin_scl, PIN_STA_HIGH);
	_iic_delay();
	
	return rbyte;
}

