#ifndef __DRV_IIC_H__
#define __DRV_IIC_H__



#include "rtthread.h"



void iic_start(rt_uint16_t pin_scl, rt_uint16_t pin_sda);
void iic_stop(rt_uint16_t pin_scl, rt_uint16_t pin_sda);
rt_uint8_t iic_write_byte(rt_uint16_t pin_scl, rt_uint16_t pin_sda, rt_uint8_t wbyte);
rt_uint8_t iic_read_byte(rt_uint16_t pin_scl, rt_uint16_t pin_sda, rt_uint8_t ack);



#endif
