#include "drv_uart.h"
#include "rthw.h"



#ifdef UART_USE_PERIPH_1
static UART_HW_INFO			m_uart_hw_info_1;
#endif
#ifdef UART_USE_PERIPH_2
static UART_HW_INFO			m_uart_hw_info_2;
#endif
#ifdef UART_USE_PERIPH_3
static UART_HW_INFO			m_uart_hw_info_3;
#endif
#ifdef UART_USE_PERIPH_4
static UART_HW_INFO			m_uart_hw_info_4;
#endif
#ifdef UART_USE_PERIPH_5
static UART_HW_INFO			m_uart_hw_info_5;
#endif
#ifdef UART_USE_PERIPH_6
static UART_HW_INFO			m_uart_hw_info_6;
#endif
#ifdef UART_USE_PERIPH_7
static UART_HW_INFO			m_uart_hw_info_7;
#endif
#ifdef UART_USE_PERIPH_8
static UART_HW_INFO			m_uart_hw_info_8;
#endif

static struct rt_event		m_uart_event;



static UART_HW_INFO *_uart_get_hw_info(rt_uint8_t uart_periph)
{
	switch(uart_periph)
	{
#ifdef UART_USE_PERIPH_1
	case UART_PERIPH_1:
		return &m_uart_hw_info_1;
#endif
#ifdef UART_USE_PERIPH_2
	case UART_PERIPH_2:
		return &m_uart_hw_info_2;
#endif
#ifdef UART_USE_PERIPH_3
	case UART_PERIPH_3:
		return &m_uart_hw_info_3;
#endif
#ifdef UART_USE_PERIPH_4
	case UART_PERIPH_4:
		return &m_uart_hw_info_4;
#endif
#ifdef UART_USE_PERIPH_5
	case UART_PERIPH_5:
		return &m_uart_hw_info_5;
#endif
#ifdef UART_USE_PERIPH_6
	case UART_PERIPH_6:
		return &m_uart_hw_info_6;
#endif
#ifdef UART_USE_PERIPH_7
	case UART_PERIPH_7:
		return &m_uart_hw_info_7;
#endif
#ifdef UART_USE_PERIPH_8
	case UART_PERIPH_8:
		return &m_uart_hw_info_8;
#endif
	}

	return (UART_HW_INFO *)0;
}

static void _uart_clock_enable(rt_uint8_t uart_periph, rt_uint8_t en)
{
	switch(uart_periph)
	{
#ifdef UART_USE_PERIPH_1
	case UART_PERIPH_1:
		if(RT_TRUE == en)
		{
			__HAL_RCC_USART1_CLK_ENABLE();
		}
		else
		{
			__HAL_RCC_USART1_CLK_DISABLE();
		}
		break;
#endif
#ifdef UART_USE_PERIPH_2
	case UART_PERIPH_2:
		if(RT_TRUE == en)
		{
			__HAL_RCC_USART2_CLK_ENABLE();
		}
		else
		{
			__HAL_RCC_USART2_CLK_DISABLE();
		}
		break;
#endif
#ifdef UART_USE_PERIPH_3
	case UART_PERIPH_3:
		if(RT_TRUE == en)
		{
			__HAL_RCC_USART3_CLK_ENABLE();
		}
		else
		{
			__HAL_RCC_USART3_CLK_DISABLE();
		}
		break;
#endif
#ifdef UART_USE_PERIPH_4
	case UART_PERIPH_4:
		if(RT_TRUE == en)
		{
			__HAL_RCC_UART4_CLK_ENABLE();
		}
		else
		{
			__HAL_RCC_UART4_CLK_DISABLE();
		}
		break;
#endif
#ifdef UART_USE_PERIPH_5
	case UART_PERIPH_5:
		if(RT_TRUE == en)
		{
			__HAL_RCC_UART5_CLK_ENABLE();
		}
		else
		{
			__HAL_RCC_UART5_CLK_DISABLE();
		}
		break;
#endif
#ifdef UART_USE_PERIPH_6
	case UART_PERIPH_6:
		if(RT_TRUE == en)
		{
			__HAL_RCC_USART6_CLK_ENABLE();
		}
		else
		{
			__HAL_RCC_USART6_CLK_DISABLE();
		}
		break;
#endif
#ifdef UART_USE_PERIPH_7
	case UART_PERIPH_7:
		if(RT_TRUE == en)
		{
			__HAL_RCC_UART7_CLK_ENABLE();
		}
		else
		{
			__HAL_RCC_UART7_CLK_DISABLE();
		}
		break;
#endif
#ifdef UART_USE_PERIPH_8
	case UART_PERIPH_8:
		if(RT_TRUE == en)
		{
			__HAL_RCC_UART8_CLK_ENABLE();
		}
		else
		{
			__HAL_RCC_UART8_CLK_DISABLE();
		}
		break;
#endif
	}
}

static void _uart_reset(rt_uint8_t uart_periph)
{
	switch(uart_periph)
	{
#ifdef UART_USE_PERIPH_1
	case UART_PERIPH_1:
		__HAL_RCC_USART1_FORCE_RESET();
		__HAL_RCC_USART1_RELEASE_RESET();
		break;
#endif
#ifdef UART_USE_PERIPH_2
	case UART_PERIPH_2:
		__HAL_RCC_USART2_FORCE_RESET();
		__HAL_RCC_USART2_RELEASE_RESET();
		break;
#endif
#ifdef UART_USE_PERIPH_3
	case UART_PERIPH_3:
		__HAL_RCC_USART3_FORCE_RESET();
		__HAL_RCC_USART3_RELEASE_RESET();
		break;
#endif
#ifdef UART_USE_PERIPH_4
	case UART_PERIPH_4:
		__HAL_RCC_UART4_FORCE_RESET();
		__HAL_RCC_UART4_RELEASE_RESET();
		break;
#endif
#ifdef UART_USE_PERIPH_5
	case UART_PERIPH_5:
		__HAL_RCC_UART5_FORCE_RESET();
		__HAL_RCC_UART5_RELEASE_RESET();
		break;
#endif
#ifdef UART_USE_PERIPH_6
	case UART_PERIPH_6:
		__HAL_RCC_USART6_FORCE_RESET();
		__HAL_RCC_USART6_RELEASE_RESET();
		break;
#endif
#ifdef UART_USE_PERIPH_7
	case UART_PERIPH_7:
		__HAL_RCC_UART7_FORCE_RESET();
		__HAL_RCC_UART7_RELEASE_RESET();
		break;
#endif
#ifdef UART_USE_PERIPH_8
	case UART_PERIPH_8:
		__HAL_RCC_UART8_FORCE_RESET();
		__HAL_RCC_UART8_RELEASE_RESET();
		break;
#endif
	}
}

static void _uart_pend(rt_uint8_t uart_periph)
{
	if(RT_EOK != rt_event_recv(&m_uart_event, UART_EVENT_PERIPH_PEND << uart_periph, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}
}

static void _uart_post(rt_uint8_t uart_periph)
{
	if(RT_EOK != rt_event_send(&m_uart_event, UART_EVENT_PERIPH_PEND << uart_periph))
	{
		while(1);
	}
}

static void _uart_irq_hdr(rt_uint8_t uart_periph, rt_uint8_t recv_data)
{
	UART_HW_INFO *pinfo;

	pinfo = _uart_get_hw_info(uart_periph);
	if((UART_HW_INFO *)0 == pinfo)
	{
		return;
	}

	if((UART_FUN_IRQ_HDR)0 != pinfo->fun_hdr)
	{
		pinfo->fun_hdr(pinfo->args, recv_data);
	}
}



static int _uart_device_init(void)
{
#ifdef UART_USE_PERIPH_1
	m_uart_hw_info_1.uart_port	= USART1;
	m_uart_hw_info_1.irq_type	= USART1_IRQn;
#ifdef PIN_MODE_AF_EX
	m_uart_hw_info_1.af_index	= GPIO_AF7_USART1;
#endif
	m_uart_hw_info_1.pin_tx		= PIN_INDEX_68;
	m_uart_hw_info_1.pin_rx		= PIN_INDEX_69;
	m_uart_hw_info_1.pin_de		= 0;
	m_uart_hw_info_1.pin_re		= 0;
	m_uart_hw_info_1.fun_hdr	= (UART_FUN_IRQ_HDR)0;

	pin_clock_enable(m_uart_hw_info_1.pin_de, RT_TRUE);
	pin_mode(m_uart_hw_info_1.pin_de, PIN_MODE_O_PP_NOPULL);
	pin_write(m_uart_hw_info_1.pin_de, PIN_STA_LOW);
	pin_clock_enable(m_uart_hw_info_1.pin_de, RT_FALSE);
	pin_clock_enable(m_uart_hw_info_1.pin_re, RT_TRUE);
	pin_mode(m_uart_hw_info_1.pin_re, PIN_MODE_O_PP_NOPULL);
	pin_write(m_uart_hw_info_1.pin_re, PIN_STA_HIGH);
	pin_clock_enable(m_uart_hw_info_1.pin_re, RT_FALSE);
#endif
#ifdef UART_USE_PERIPH_2
	m_uart_hw_info_2.uart_port	= USART2;
	m_uart_hw_info_2.irq_type	= USART2_IRQn;
#ifdef PIN_MODE_AF_EX
	m_uart_hw_info_2.af_index	= GPIO_AF7_USART2;
#endif
	m_uart_hw_info_2.pin_tx		= PIN_INDEX_25;
	m_uart_hw_info_2.pin_rx		= PIN_INDEX_26;
	m_uart_hw_info_2.pin_de		= 0;
	m_uart_hw_info_2.pin_re		= 0;
	m_uart_hw_info_2.fun_hdr	= (UART_FUN_IRQ_HDR)0;

	pin_clock_enable(m_uart_hw_info_2.pin_de, RT_TRUE);
	pin_mode(m_uart_hw_info_2.pin_de, PIN_MODE_O_PP_NOPULL);
	pin_write(m_uart_hw_info_2.pin_de, PIN_STA_LOW);
	pin_clock_enable(m_uart_hw_info_2.pin_de, RT_FALSE);
	pin_clock_enable(m_uart_hw_info_2.pin_re, RT_TRUE);
	pin_mode(m_uart_hw_info_2.pin_re, PIN_MODE_O_PP_NOPULL);
	pin_write(m_uart_hw_info_2.pin_re, PIN_STA_HIGH);
	pin_clock_enable(m_uart_hw_info_2.pin_re, RT_FALSE);
#endif
#ifdef UART_USE_PERIPH_3
	m_uart_hw_info_3.uart_port	= USART3;
	m_uart_hw_info_3.irq_type	= USART3_IRQn;
#ifdef PIN_MODE_AF_EX
	m_uart_hw_info_3.af_index	= GPIO_AF7_USART3;
#endif
	m_uart_hw_info_3.pin_tx		= PIN_INDEX_55;
	m_uart_hw_info_3.pin_rx		= PIN_INDEX_56;
	m_uart_hw_info_3.pin_de		= 0;
	m_uart_hw_info_3.pin_re		= 0;
	m_uart_hw_info_3.fun_hdr	= (UART_FUN_IRQ_HDR)0;

	pin_clock_enable(m_uart_hw_info_3.pin_de, RT_TRUE);
	pin_mode(m_uart_hw_info_3.pin_de, PIN_MODE_O_PP_NOPULL);
	pin_write(m_uart_hw_info_3.pin_de, PIN_STA_LOW);
	pin_clock_enable(m_uart_hw_info_3.pin_de, RT_FALSE);
	pin_clock_enable(m_uart_hw_info_3.pin_re, RT_TRUE);
	pin_mode(m_uart_hw_info_3.pin_re, PIN_MODE_O_PP_NOPULL);
	pin_write(m_uart_hw_info_3.pin_re, PIN_STA_HIGH);
	pin_clock_enable(m_uart_hw_info_3.pin_re, RT_FALSE);
#endif
#ifdef UART_USE_PERIPH_4
	m_uart_hw_info_4.uart_port	= UART4;
	m_uart_hw_info_4.irq_type	= UART4_IRQn;
#ifdef PIN_MODE_AF_EX
	m_uart_hw_info_4.af_index	= GPIO_AF8_UART4;
#endif
	m_uart_hw_info_4.pin_tx		= PIN_INDEX_78;
	m_uart_hw_info_4.pin_rx		= PIN_INDEX_79;
	m_uart_hw_info_4.pin_de		= PIN_INDEX_77;
	m_uart_hw_info_4.pin_re		= PIN_INDEX_84;
	m_uart_hw_info_4.fun_hdr	= (UART_FUN_IRQ_HDR)0;

	pin_clock_enable(m_uart_hw_info_4.pin_de, RT_TRUE);
	pin_mode(m_uart_hw_info_4.pin_de, PIN_MODE_O_PP_NOPULL);
	pin_write(m_uart_hw_info_4.pin_de, PIN_STA_LOW);
	pin_clock_enable(m_uart_hw_info_4.pin_de, RT_FALSE);
	pin_clock_enable(m_uart_hw_info_4.pin_re, RT_TRUE);
	pin_mode(m_uart_hw_info_4.pin_re, PIN_MODE_O_PP_NOPULL);
	pin_write(m_uart_hw_info_4.pin_re, PIN_STA_HIGH);
	pin_clock_enable(m_uart_hw_info_4.pin_re, RT_FALSE);
#endif
#ifdef UART_USE_PERIPH_5
	m_uart_hw_info_5.uart_port	= UART5;
	m_uart_hw_info_5.irq_type	= UART5_IRQn;
#ifdef PIN_MODE_AF_EX
	m_uart_hw_info_5.af_index	= GPIO_AF8_UART5;
#endif
	m_uart_hw_info_5.pin_tx		= PIN_INDEX_80;
	m_uart_hw_info_5.pin_rx		= PIN_INDEX_83;
	m_uart_hw_info_5.pin_de		= PIN_INDEX_81;
	m_uart_hw_info_5.pin_re		= PIN_INDEX_82;
	m_uart_hw_info_5.fun_hdr	= (UART_FUN_IRQ_HDR)0;

	pin_clock_enable(m_uart_hw_info_5.pin_de, RT_TRUE);
	pin_mode(m_uart_hw_info_5.pin_de, PIN_MODE_O_PP_NOPULL);
	pin_write(m_uart_hw_info_5.pin_de, PIN_STA_LOW);
	pin_clock_enable(m_uart_hw_info_5.pin_de, RT_FALSE);
	pin_clock_enable(m_uart_hw_info_5.pin_re, RT_TRUE);
	pin_mode(m_uart_hw_info_5.pin_re, PIN_MODE_O_PP_NOPULL);
	pin_write(m_uart_hw_info_5.pin_re, PIN_STA_HIGH);
	pin_clock_enable(m_uart_hw_info_5.pin_re, RT_FALSE);
#endif
#ifdef UART_USE_PERIPH_6
	m_uart_hw_info_6.uart_port	= USART6;
	m_uart_hw_info_6.irq_type	= USART6_IRQn;
#ifdef PIN_MODE_AF_EX
	m_uart_hw_info_6.af_index	= GPIO_AF8_USART6;
#endif
	m_uart_hw_info_6.pin_tx		= PIN_INDEX_63;
	m_uart_hw_info_6.pin_rx		= PIN_INDEX_64;
	m_uart_hw_info_6.pin_de		= 0;
	m_uart_hw_info_6.pin_re		= 0;
	m_uart_hw_info_6.fun_hdr	= (UART_FUN_IRQ_HDR)0;

	pin_clock_enable(m_uart_hw_info_6.pin_de, RT_TRUE);
	pin_mode(m_uart_hw_info_6.pin_de, PIN_MODE_O_PP_NOPULL);
	pin_write(m_uart_hw_info_6.pin_de, PIN_STA_LOW);
	pin_clock_enable(m_uart_hw_info_6.pin_de, RT_FALSE);
	pin_clock_enable(m_uart_hw_info_6.pin_re, RT_TRUE);
	pin_mode(m_uart_hw_info_6.pin_re, PIN_MODE_O_PP_NOPULL);
	pin_write(m_uart_hw_info_6.pin_re, PIN_STA_HIGH);
	pin_clock_enable(m_uart_hw_info_6.pin_re, RT_FALSE);
#endif
#ifdef UART_USE_PERIPH_7
	m_uart_hw_info_7.uart_port	= UART7;
	m_uart_hw_info_7.irq_type	= UART7_IRQn;
#ifdef PIN_MODE_AF_EX
	m_uart_hw_info_7.af_index	= GPIO_AF8_UART7;
#endif
	m_uart_hw_info_7.pin_tx		= PIN_INDEX_19;
	m_uart_hw_info_7.pin_rx		= PIN_INDEX_18;
	m_uart_hw_info_7.pin_de		= PIN_INDEX_20;
	m_uart_hw_info_7.pin_re		= PIN_INDEX_21;
	m_uart_hw_info_7.fun_hdr	= (UART_FUN_IRQ_HDR)0;

	pin_clock_enable(m_uart_hw_info_7.pin_de, RT_TRUE);
	pin_mode(m_uart_hw_info_7.pin_de, PIN_MODE_O_PP_NOPULL);
	pin_write(m_uart_hw_info_7.pin_de, PIN_STA_LOW);
	pin_clock_enable(m_uart_hw_info_7.pin_de, RT_FALSE);
	pin_clock_enable(m_uart_hw_info_7.pin_re, RT_TRUE);
	pin_mode(m_uart_hw_info_7.pin_re, PIN_MODE_O_PP_NOPULL);
	pin_write(m_uart_hw_info_7.pin_re, PIN_STA_HIGH);
	pin_clock_enable(m_uart_hw_info_7.pin_re, RT_FALSE);
#endif
#ifdef UART_USE_PERIPH_8
	m_uart_hw_info_8.uart_port	= UART8;
	m_uart_hw_info_8.irq_type	= UART8_IRQn;
#ifdef PIN_MODE_AF_EX
	m_uart_hw_info_8.af_index	= GPIO_AF8_UART8;
#endif
	m_uart_hw_info_8.pin_tx		= PIN_INDEX_142;
	m_uart_hw_info_8.pin_rx		= PIN_INDEX_141;
	m_uart_hw_info_8.pin_de		= 0;
	m_uart_hw_info_8.pin_re		= 0;
	m_uart_hw_info_8.fun_hdr	= (UART_FUN_IRQ_HDR)0;

	pin_clock_enable(m_uart_hw_info_8.pin_de, RT_TRUE);
	pin_mode(m_uart_hw_info_8.pin_de, PIN_MODE_O_PP_NOPULL);
	pin_write(m_uart_hw_info_8.pin_de, PIN_STA_LOW);
	pin_clock_enable(m_uart_hw_info_8.pin_de, RT_FALSE);
	pin_clock_enable(m_uart_hw_info_8.pin_re, RT_TRUE);
	pin_mode(m_uart_hw_info_8.pin_re, PIN_MODE_O_PP_NOPULL);
	pin_write(m_uart_hw_info_8.pin_re, PIN_STA_HIGH);
	pin_clock_enable(m_uart_hw_info_8.pin_re, RT_FALSE);
#endif

	if(RT_EOK != rt_event_init(&m_uart_event, "uart", RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	if(RT_EOK != rt_event_send(&m_uart_event, UART_EVENT_VALUE_PERIPH))
	{
		while(1);
	}
	
	return 0;
}
INIT_DEVICE_EXPORT(_uart_device_init);



#ifdef UART_USE_PERIPH_1
void USART1_IRQHandler(void)
{
	rt_uint8_t recv_data;
	
	rt_interrupt_enter();

	if(UART_FLAG_RXNE == (USART1->SR & UART_FLAG_RXNE))
	{
		recv_data = USART1->DR;
		_uart_irq_hdr(UART_PERIPH_1, recv_data);
	}
	else
	{
		recv_data = USART1->DR;
	}

	rt_interrupt_leave();
}
#endif

#ifdef UART_USE_PERIPH_2
void USART2_IRQHandler(void)
{
	rt_uint8_t recv_data;

	rt_interrupt_enter();

	if(UART_FLAG_RXNE == (USART2->SR & UART_FLAG_RXNE))
	{
		recv_data = USART2->DR;
		_uart_irq_hdr(UART_PERIPH_2, recv_data);
	}
	else
	{
		recv_data = USART2->DR;
	}

	rt_interrupt_leave();
}
#endif

#ifdef UART_USE_PERIPH_3
void USART3_IRQHandler(void)
{
	rt_uint8_t recv_data;

	rt_interrupt_enter();

	if(UART_FLAG_RXNE == (USART3->SR & UART_FLAG_RXNE))
	{
		recv_data = USART3->DR;
		_uart_irq_hdr(UART_PERIPH_3, recv_data);
	}
	else
	{
		recv_data = USART3->DR;
	}

	rt_interrupt_leave();
}
#endif

#ifdef UART_USE_PERIPH_4
void UART4_IRQHandler(void)
{
	rt_uint8_t recv_data;

	rt_interrupt_enter();

	if(UART_FLAG_RXNE == (UART4->SR & UART_FLAG_RXNE))
	{
		recv_data = UART4->DR;
		_uart_irq_hdr(UART_PERIPH_4, recv_data);
	}
	else
	{
		recv_data = UART4->DR;
	}

	rt_interrupt_leave();
}
#endif

#ifdef UART_USE_PERIPH_5
void UART5_IRQHandler(void)
{
	rt_uint8_t recv_data;

	rt_interrupt_enter();

	if(UART_FLAG_RXNE == (UART5->SR & UART_FLAG_RXNE))
	{
		recv_data = UART5->DR;
		_uart_irq_hdr(UART_PERIPH_5, recv_data);
	}
	else
	{
		recv_data = UART5->DR;
	}

	rt_interrupt_leave();
}
#endif

#ifdef UART_USE_PERIPH_6
void USART6_IRQHandler(void)
{
	rt_uint8_t recv_data;

	rt_interrupt_enter();

	if(UART_FLAG_RXNE == (USART6->SR & UART_FLAG_RXNE))
	{
		recv_data = USART6->DR;
		_uart_irq_hdr(UART_PERIPH_6, recv_data);
	}
	else
	{
		recv_data = USART6->DR;
	}

	rt_interrupt_leave();
}
#endif

#ifdef UART_USE_PERIPH_7
void UART7_IRQHandler(void)
{
	rt_uint8_t recv_data;

	rt_interrupt_enter();

	if(UART_FLAG_RXNE == (UART7->SR & UART_FLAG_RXNE))
	{
		recv_data = UART7->DR;
		_uart_irq_hdr(UART_PERIPH_7, recv_data);
	}
	else
	{
		recv_data = UART7->DR;
	}

	rt_interrupt_leave();
}
#endif

#ifdef UART_USE_PERIPH_8
void UART8_IRQHandler(void)
{
	rt_uint8_t recv_data;

	rt_interrupt_enter();

	if(UART_FLAG_RXNE == (UART8->SR & UART_FLAG_RXNE))
	{
		recv_data = UART8->DR;
		_uart_irq_hdr(UART_PERIPH_8, recv_data);
	}
	else
	{
		recv_data = UART8->DR;
	}

	rt_interrupt_leave();
}
#endif



rt_uint8_t uart_open(rt_uint8_t uart_periph, rt_uint32_t baud_rate, UART_FUN_IRQ_HDR fun_hdr, void *args)
{
	rt_base_t			level;
	UART_HW_INFO		*pinfo;
	UART_HandleTypeDef	uart_handle;

	pinfo = _uart_get_hw_info(uart_periph);
	if((UART_HW_INFO *)0 == pinfo)
	{
		return RT_FALSE;
	}

	_uart_pend(uart_periph);

	//pin_tx
	pin_clock_enable(pinfo->pin_tx, RT_TRUE);
#ifdef PIN_MODE_AF_EX
	pin_mode_ex(pinfo->pin_tx, PIN_MODE_AF_PP_PULLUP, pinfo->af_index);
#else
	pin_mode(pinfo->pin_tx, PIN_MODE_AF_PP_PULLUP);
#endif
	//pin_rx
	pin_clock_enable(pinfo->pin_rx, RT_TRUE);
#ifdef PIN_MODE_AF_EX
	pin_mode_ex(pinfo->pin_rx, PIN_MODE_AF_PP_PULLUP, pinfo->af_index);
#else
	pin_mode(pinfo->pin_rx, PIN_MODE_AF_PP_PULLUP);
#endif
	//pin_de
	pin_clock_enable(pinfo->pin_de, RT_TRUE);
	pin_mode(pinfo->pin_de, PIN_MODE_O_PP_NOPULL);
	pin_write(pinfo->pin_de, PIN_STA_LOW);
	//pin_re
	pin_clock_enable(pinfo->pin_re, RT_TRUE);
	pin_mode(pinfo->pin_re, PIN_MODE_O_PP_NOPULL);
	pin_write(pinfo->pin_re, PIN_STA_LOW);
	//uart
	_uart_clock_enable(uart_periph, RT_TRUE);
	uart_handle.Instance				= pinfo->uart_port;
	uart_handle.Init.BaudRate			= baud_rate;
	uart_handle.Init.WordLength			= UART_WORDLENGTH_8B;
	uart_handle.Init.StopBits			= UART_STOPBITS_1;
	uart_handle.Init.Parity				= UART_PARITY_NONE;
	uart_handle.Init.Mode				= UART_MODE_TX_RX;
	uart_handle.Init.HwFlowCtl			= UART_HWCONTROL_NONE;
	uart_handle.Init.OverSampling		= UART_OVERSAMPLING_16;
	if(HAL_OK != HAL_UART_Init(&uart_handle))
	{
		while(1);
	}
	__HAL_UART_ENABLE_IT(&uart_handle, UART_IT_RXNE);
	HAL_NVIC_SetPriority(pinfo->irq_type, 1, 1);
	HAL_NVIC_EnableIRQ(pinfo->irq_type);
	//irq_hdr
	level = rt_hw_interrupt_disable();
	pinfo->fun_hdr	= fun_hdr;
	pinfo->args		= args;
	rt_hw_interrupt_enable(level);

	return RT_TRUE;
}

rt_uint8_t uart_close(rt_uint8_t uart_periph)
{
	rt_base_t			level;
	UART_HW_INFO		*pinfo;
	UART_HandleTypeDef	uart_handle;

	pinfo = _uart_get_hw_info(uart_periph);
	if((UART_HW_INFO *)0 == pinfo)
	{
		return RT_FALSE;
	}

	uart_handle.Instance = pinfo->uart_port;

	//irq_hdr
	level = rt_hw_interrupt_disable();
	pinfo->fun_hdr = (UART_FUN_IRQ_HDR)0;
	rt_hw_interrupt_enable(level);
	//uart
	HAL_NVIC_DisableIRQ(pinfo->irq_type);
	__HAL_UART_DISABLE_IT(&uart_handle, UART_IT_RXNE);
	__HAL_UART_DISABLE(&uart_handle);
	_uart_reset(uart_periph);
	_uart_clock_enable(uart_periph, RT_FALSE);
	//pin_re
	pin_write(pinfo->pin_re, PIN_STA_HIGH);
	pin_clock_enable(pinfo->pin_re, RT_FALSE);
	//pin_de
	pin_write(pinfo->pin_de, PIN_STA_LOW);
	pin_clock_enable(pinfo->pin_de, RT_FALSE);
	//pin_rx
	pin_mode(pinfo->pin_rx, PIN_MODE_ANALOG);
	pin_clock_enable(pinfo->pin_rx, RT_FALSE);
	//pin_tx
	pin_mode(pinfo->pin_tx, PIN_MODE_ANALOG);
	pin_clock_enable(pinfo->pin_tx, RT_FALSE);

	_uart_post(uart_periph);

	return RT_TRUE;
}

rt_uint8_t uart_send_data(rt_uint8_t uart_periph, rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	UART_HW_INFO *pinfo;

	pinfo = _uart_get_hw_info(uart_periph);
	if((UART_HW_INFO *)0 == pinfo)
	{
		return RT_FALSE;
	}
	if((rt_uint8_t *)0 == pdata)
	{
		return RT_FALSE;
	}
	if(0 == data_len)
	{
		return RT_FALSE;
	}

	pin_write(pinfo->pin_de, PIN_STA_HIGH);
	pin_write(pinfo->pin_re, PIN_STA_HIGH);

	while(data_len)
	{
		while(UART_FLAG_TXE != (pinfo->uart_port->SR & UART_FLAG_TXE));
		pinfo->uart_port->DR = *pdata++;
		data_len--;
	}
	while(UART_FLAG_TC != (pinfo->uart_port->SR & UART_FLAG_TC));

	pin_write(pinfo->pin_de, PIN_STA_LOW);
	pin_write(pinfo->pin_re, PIN_STA_LOW);

	return RT_TRUE;
}

