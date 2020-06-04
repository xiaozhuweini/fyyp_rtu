#ifndef __DRV_UART_H__
#define __DRV_UART_H__



#include "rtthread.h"
#include "stm32f4xx.h"
#include "drv_pin.h"



#define UART_USE_PERIPH_1
#define UART_USE_PERIPH_2
#define UART_USE_PERIPH_3
#define UART_USE_PERIPH_4
#define UART_USE_PERIPH_5
#define UART_USE_PERIPH_6
//#define UART_USE_PERIPH_7
//#define UART_USE_PERIPH_8

#define UART_PERIPH_1				0
#define UART_PERIPH_2				1
#define UART_PERIPH_3				2
#define UART_PERIPH_4				3
#define UART_PERIPH_5				4
#define UART_PERIPH_6				5
#define UART_PERIPH_7				6
#define UART_PERIPH_8				7

#define UART_BAUD_RATE_2400			2400
#define UART_BAUD_RATE_4800			4800
#define UART_BAUD_RATE_9600			9600
#define UART_BAUD_RATE_19200		19200
#define UART_BAUD_RATE_38400		38400
#define UART_BAUD_RATE_57600		57600
#define UART_BAUD_RATE_115200		115200

#define UART_EVENT_PERIPH_PEND		0x01
#define UART_EVENT_VALUE_PERIPH		0xffffffff



typedef void (*UART_FUN_IRQ_HDR)(void *args, rt_uint8_t recv_data);

typedef struct uart_hw_info
{
	USART_TypeDef		*uart_port;
	IRQn_Type			irq_type;
#ifdef PIN_MODE_AF_EX
	rt_uint8_t			af_index;
#endif
	rt_uint16_t			pin_tx;
	rt_uint16_t			pin_rx;
	rt_uint16_t			pin_de;
	rt_uint16_t			pin_re;
	UART_FUN_IRQ_HDR	fun_hdr;
	void				*args;
} UART_HW_INFO;



rt_uint8_t uart_open(rt_uint8_t uart_periph, rt_uint32_t baud_rate, UART_FUN_IRQ_HDR fun_hdr, void *args);
rt_uint8_t uart_close(rt_uint8_t uart_periph);
rt_uint8_t uart_send_data(rt_uint8_t uart_periph, rt_uint8_t const *pdata, rt_uint16_t data_len);



#endif
