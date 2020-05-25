#include "drv_pin.h"
#include "drv_lpm.h"
#include "rthw.h"
#include "stm32f4xx.h"



static PIN_IRQ_HDR m_pin_irq_hdr[] = {
	{0, 0, (PIN_FUN_IRQ_HDR)0, (void *)0}, {0, 0, (PIN_FUN_IRQ_HDR)0, (void *)0}, {0, 0, (PIN_FUN_IRQ_HDR)0, (void *)0}, {0, 0, (PIN_FUN_IRQ_HDR)0, (void *)0},
	{0, 0, (PIN_FUN_IRQ_HDR)0, (void *)0}, {0, 0, (PIN_FUN_IRQ_HDR)0, (void *)0}, {0, 0, (PIN_FUN_IRQ_HDR)0, (void *)0}, {0, 0, (PIN_FUN_IRQ_HDR)0, (void *)0},
	{0, 0, (PIN_FUN_IRQ_HDR)0, (void *)0}, {0, 0, (PIN_FUN_IRQ_HDR)0, (void *)0}, {0, 0, (PIN_FUN_IRQ_HDR)0, (void *)0}, {0, 0, (PIN_FUN_IRQ_HDR)0, (void *)0},
	{0, 0, (PIN_FUN_IRQ_HDR)0, (void *)0}, {0, 0, (PIN_FUN_IRQ_HDR)0, (void *)0}, {0, 0, (PIN_FUN_IRQ_HDR)0, (void *)0}, {0, 0, (PIN_FUN_IRQ_HDR)0, (void *)0}
};



static GPIO_TypeDef *_pin_get_st_port(rt_uint16_t pin_index)
{
	switch(pin_index & 0xff00)
	{
	case PIN_PORT_A:
		return GPIOA;
	case PIN_PORT_B:
		return GPIOB;
	case PIN_PORT_C:
		return GPIOC;
	case PIN_PORT_D:
		return GPIOD;
	case PIN_PORT_E:
		return GPIOE;
	case PIN_PORT_F:
		return GPIOF;
	case PIN_PORT_G:
		return GPIOG;
	case PIN_PORT_H:
		return GPIOH;
	}

	return (GPIO_TypeDef *)0;
}

static rt_uint8_t _pin_get_irq_type(rt_uint16_t pin_index, IRQn_Type *irq_type)
{
	switch(pin_index & 0xff)
	{
	case 0:
		*irq_type = EXTI0_IRQn;
		return RT_TRUE;
	case 1:
		*irq_type = EXTI1_IRQn;
		return RT_TRUE;
	case 2:
		*irq_type = EXTI2_IRQn;
		return RT_TRUE;
	case 3:
		*irq_type = EXTI3_IRQn;
		return RT_TRUE;
	case 4:
		*irq_type = EXTI4_IRQn;
		return RT_TRUE;
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
		*irq_type = EXTI9_5_IRQn;
		return RT_TRUE;
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
		*irq_type = EXTI15_10_IRQn;
		return RT_TRUE;
	}

	return RT_FALSE;
}

static void _pin_irq_hdr(rt_uint8_t irq_hdr_index)
{
	if(irq_hdr_index >= (sizeof(m_pin_irq_hdr) / sizeof(m_pin_irq_hdr[0])))
	{
		return;
	}

	if((PIN_FUN_IRQ_HDR)0 != m_pin_irq_hdr[irq_hdr_index].hdr)
	{
		m_pin_irq_hdr[irq_hdr_index].hdr(m_pin_irq_hdr[irq_hdr_index].args);
	}
}



void EXTI0_IRQHandler(void)
{
	rt_interrupt_enter();

	if(GPIO_PIN_0 == __HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_0))
	{
		__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_0);

		_pin_irq_hdr(0);
	}

	rt_interrupt_leave();
}

void EXTI1_IRQHandler(void)
{
	rt_interrupt_enter();

	if(GPIO_PIN_1 == __HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_1))
	{
		__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_1);

		_pin_irq_hdr(1);
	}

	rt_interrupt_leave();
}

void EXTI2_IRQHandler(void)
{
	rt_interrupt_enter();

	if(GPIO_PIN_2 == __HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_2))
	{
		__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_2);

		_pin_irq_hdr(2);
	}

	rt_interrupt_leave();
}

void EXTI3_IRQHandler(void)
{
	rt_interrupt_enter();

	if(GPIO_PIN_3 == __HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_3))
	{
		__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_3);

		_pin_irq_hdr(3);
	}

	rt_interrupt_leave();
}

void EXTI4_IRQHandler(void)
{
	rt_interrupt_enter();

	if(GPIO_PIN_4 == __HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_4))
	{
		__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_4);

		_pin_irq_hdr(4);
	}

	rt_interrupt_leave();
}

void EXTI9_5_IRQHandler(void)
{
	rt_interrupt_enter();

	if(GPIO_PIN_5 == __HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_5))
	{
		__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_5);

		_pin_irq_hdr(5);
	}

	if(GPIO_PIN_6 == __HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_6))
	{
		__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_6);

		_pin_irq_hdr(6);
	}

	if(GPIO_PIN_7 == __HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_7))
	{
		__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_7);

		_pin_irq_hdr(7);
	}

	if(GPIO_PIN_8 == __HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_8))
	{
		__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_8);

		_pin_irq_hdr(8);
	}

	if(GPIO_PIN_9 == __HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_9))
	{
		__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_9);

		_pin_irq_hdr(9);
	}

	rt_interrupt_leave();
}

void EXTI15_10_IRQHandler(void)
{
	rt_interrupt_enter();

	if(GPIO_PIN_10 == __HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_10))
	{
		__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_10);

		_pin_irq_hdr(10);
	}

	if(GPIO_PIN_11 == __HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_11))
	{
		__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_11);

		_pin_irq_hdr(11);
	}

	if(GPIO_PIN_12 == __HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_12))
	{
		__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_12);

		_pin_irq_hdr(12);
	}

	if(GPIO_PIN_13 == __HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_13))
	{
		__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_13);

		_pin_irq_hdr(13);
	}

	if(GPIO_PIN_14 == __HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_14))
	{
		__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_14);

		_pin_irq_hdr(14);
	}

	if(GPIO_PIN_15 == __HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_15))
	{
		__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_15);

		_pin_irq_hdr(15);
	}

	rt_interrupt_leave();
}



void pin_clock_enable(rt_uint16_t pin_index, rt_uint8_t en)
{
	switch(pin_index & 0xff00)
	{
	case PIN_PORT_A:
		pin_index = LPM_RCC_GPIOA;
		break;
	case PIN_PORT_B:
		pin_index = LPM_RCC_GPIOB;
		break;
	case PIN_PORT_C:
		pin_index = LPM_RCC_GPIOC;
		break;
	case PIN_PORT_D:
		pin_index = LPM_RCC_GPIOD;
		break;
	case PIN_PORT_E:
		pin_index = LPM_RCC_GPIOE;
		break;
	case PIN_PORT_F:
		pin_index = LPM_RCC_GPIOF;
		break;
	case PIN_PORT_G:
		pin_index = LPM_RCC_GPIOG;
		break;
	default:
		return;
	}

	if(RT_TRUE == en)
	{
		lpm_rcc_en(pin_index);
	}
	else
	{
		lpm_rcc_dis(pin_index);
	}
}

void pin_write(rt_uint16_t pin_index, rt_uint8_t pin_sta)
{
	GPIO_TypeDef *pin_port;

	pin_port = _pin_get_st_port(pin_index);
	if((GPIO_TypeDef *)0 == pin_port)
	{
		return;
	}

	HAL_GPIO_WritePin(pin_port, _pin_get_st_pin(pin_index), (PIN_STA_LOW == pin_sta) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void pin_toggle(rt_uint16_t pin_index)
{
	GPIO_TypeDef *pin_port;

	pin_port = _pin_get_st_port(pin_index);
	if((GPIO_TypeDef *)0 == pin_port)
	{
		return;
	}

	HAL_GPIO_TogglePin(pin_port, _pin_get_st_pin(pin_index));
}

rt_uint8_t pin_read(rt_uint16_t pin_index)
{
	GPIO_TypeDef *pin_port;

	pin_port = _pin_get_st_port(pin_index);
	if((GPIO_TypeDef *)0 == pin_port)
	{
		return PIN_STA_LOW;
	}

	return (GPIO_PIN_RESET == HAL_GPIO_ReadPin(pin_port, _pin_get_st_pin(pin_index))) ? PIN_STA_LOW : PIN_STA_HIGH;
}

void pin_mode(rt_uint16_t pin_index, rt_uint8_t mode)
{
	GPIO_TypeDef		*pin_port;
	GPIO_InitTypeDef	gpio_init;

	pin_port = _pin_get_st_port(pin_index);
	if((GPIO_TypeDef *)0 == pin_port)
	{
		return;
	}

	gpio_init.Pin = _pin_get_st_pin(pin_index);

	switch(mode)
	{
	default:
		return;
	case PIN_MODE_ANALOG:
		gpio_init.Mode		= GPIO_MODE_ANALOG;
		gpio_init.Pull		= GPIO_NOPULL;
		gpio_init.Speed		= GPIO_SPEED_FREQ_HIGH;
		break;
	case PIN_MODE_I_NOPULL:
		gpio_init.Mode		= GPIO_MODE_INPUT;
		gpio_init.Pull		= GPIO_NOPULL;
		gpio_init.Speed		= GPIO_SPEED_FREQ_HIGH;
		break;
	case PIN_MODE_I_PULLDOWN:
		gpio_init.Mode		= GPIO_MODE_INPUT;
		gpio_init.Pull		= GPIO_PULLDOWN;
		gpio_init.Speed		= GPIO_SPEED_FREQ_HIGH;
		break;
	case PIN_MODE_I_PULLUP:
		gpio_init.Mode		= GPIO_MODE_INPUT;
		gpio_init.Pull		= GPIO_PULLUP;
		gpio_init.Speed		= GPIO_SPEED_FREQ_HIGH;
		break;
	case PIN_MODE_O_OD_NOPULL:
		gpio_init.Mode		= GPIO_MODE_OUTPUT_OD;
		gpio_init.Pull		= GPIO_NOPULL;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		break;
	case PIN_MODE_O_OD_PULLDOWN:
		gpio_init.Mode		= GPIO_MODE_OUTPUT_OD;
		gpio_init.Pull		= GPIO_PULLDOWN;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		break;
	case PIN_MODE_O_OD_PULLUP:
		gpio_init.Mode		= GPIO_MODE_OUTPUT_OD;
		gpio_init.Pull		= GPIO_PULLUP;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		break;
	case PIN_MODE_O_PP_NOPULL:
		gpio_init.Mode		= GPIO_MODE_OUTPUT_PP;
		gpio_init.Pull		= GPIO_NOPULL;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		break;
	case PIN_MODE_O_PP_PULLDOWN:
		gpio_init.Mode		= GPIO_MODE_OUTPUT_PP;
		gpio_init.Pull		= GPIO_PULLDOWN;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		break;
	case PIN_MODE_O_PP_PULLUP:
		gpio_init.Mode		= GPIO_MODE_OUTPUT_PP;
		gpio_init.Pull		= GPIO_PULLUP;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		break;
	case PIN_MODE_AF_PP_NOPULL:
		gpio_init.Mode		= GPIO_MODE_AF_PP;
		gpio_init.Pull		= GPIO_NOPULL;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		break;
	case PIN_MODE_AF_PP_PULLUP:
		gpio_init.Mode		= GPIO_MODE_AF_PP;
		gpio_init.Pull		= GPIO_PULLUP;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		break;
	case PIN_MODE_AF_PP_PULLDOWN:
		gpio_init.Mode		= GPIO_MODE_AF_PP;
		gpio_init.Pull		= GPIO_PULLDOWN;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		break;
	case PIN_MODE_AF_OD_NOPULL:
		gpio_init.Mode		= GPIO_MODE_AF_OD;
		gpio_init.Pull		= GPIO_NOPULL;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		break;
	case PIN_MODE_AF_OD_PULLUP:
		gpio_init.Mode		= GPIO_MODE_AF_OD;
		gpio_init.Pull		= GPIO_PULLUP;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		break;
	case PIN_MODE_AF_OD_PULLDOWN:
		gpio_init.Mode		= GPIO_MODE_AF_OD;
		gpio_init.Pull		= GPIO_PULLDOWN;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		break;
	}

	HAL_GPIO_Init(pin_port, &gpio_init);
}

#ifdef PIN_MODE_AF_EX
void pin_mode_ex(rt_uint16_t pin_index, rt_uint8_t mode, rt_uint8_t af_index)
{
	GPIO_TypeDef		*pin_port;
	GPIO_InitTypeDef	gpio_init;

	pin_port = _pin_get_st_port(pin_index);
	if((GPIO_TypeDef *)0 == pin_port)
	{
		return;
	}

	gpio_init.Pin = _pin_get_st_pin(pin_index);

	switch(mode)
	{
	default:
		return;
	case PIN_MODE_AF_PP_NOPULL:
		gpio_init.Mode		= GPIO_MODE_AF_PP;
		gpio_init.Pull		= GPIO_NOPULL;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		gpio_init.Alternate	= af_index;
		break;
	case PIN_MODE_AF_PP_PULLUP:
		gpio_init.Mode		= GPIO_MODE_AF_PP;
		gpio_init.Pull		= GPIO_PULLUP;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		gpio_init.Alternate	= af_index;
		break;
	case PIN_MODE_AF_PP_PULLDOWN:
		gpio_init.Mode		= GPIO_MODE_AF_PP;
		gpio_init.Pull		= GPIO_PULLDOWN;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		gpio_init.Alternate	= af_index;
		break;
	case PIN_MODE_AF_OD_NOPULL:
		gpio_init.Mode		= GPIO_MODE_AF_OD;
		gpio_init.Pull		= GPIO_NOPULL;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		gpio_init.Alternate	= af_index;
		break;
	case PIN_MODE_AF_OD_PULLUP:
		gpio_init.Mode		= GPIO_MODE_AF_OD;
		gpio_init.Pull		= GPIO_PULLUP;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		gpio_init.Alternate	= af_index;
		break;
	case PIN_MODE_AF_OD_PULLDOWN:
		gpio_init.Mode		= GPIO_MODE_AF_OD;
		gpio_init.Pull		= GPIO_PULLDOWN;
		gpio_init.Speed		= GPIO_SPEED_FREQ_VERY_HIGH;
		gpio_init.Alternate	= af_index;
		break;
	}

	HAL_GPIO_Init(pin_port, &gpio_init);
}
#endif

rt_uint8_t pin_attach_irq(rt_uint16_t pin_index, rt_uint8_t irq_mode, PIN_FUN_IRQ_HDR hdr, void *args)
{
	rt_base_t	level;
	rt_uint8_t	irq_hdr_index;
	
	if((GPIO_TypeDef *)0 == _pin_get_st_port(pin_index))
	{
		return RT_FALSE;
	}

	irq_hdr_index = (rt_uint8_t)pin_index;
	if(irq_hdr_index >= (sizeof(m_pin_irq_hdr) / sizeof(m_pin_irq_hdr[0])))
	{
		return RT_FALSE;
	}

	level = rt_hw_interrupt_disable();
	if(0 != m_pin_irq_hdr[irq_hdr_index].pin)
	{
		rt_hw_interrupt_enable(level);
		return RT_FALSE;
	}

	m_pin_irq_hdr[irq_hdr_index].pin	= pin_index;
	m_pin_irq_hdr[irq_hdr_index].mode	= irq_mode;
	m_pin_irq_hdr[irq_hdr_index].hdr	= hdr;
	m_pin_irq_hdr[irq_hdr_index].args	= args;

	rt_hw_interrupt_enable(level);
	return RT_TRUE;
}

rt_uint8_t pin_detach_irq(rt_uint16_t pin_index)
{
	rt_base_t	level;
	rt_uint8_t	irq_hdr_index;
	
	if((GPIO_TypeDef *)0 == _pin_get_st_port(pin_index))
	{
		return RT_FALSE;
	}

	irq_hdr_index = (rt_uint8_t)pin_index;
	if(irq_hdr_index >= (sizeof(m_pin_irq_hdr) / sizeof(m_pin_irq_hdr[0])))
	{
		return RT_FALSE;
	}

	level = rt_hw_interrupt_disable();
	if(0 == m_pin_irq_hdr[irq_hdr_index].pin)
	{
		rt_hw_interrupt_enable(level);
		return RT_TRUE;
	}

	m_pin_irq_hdr[irq_hdr_index].pin	= 0;
	m_pin_irq_hdr[irq_hdr_index].mode	= 0;
	m_pin_irq_hdr[irq_hdr_index].hdr	= (PIN_FUN_IRQ_HDR)0;
	m_pin_irq_hdr[irq_hdr_index].args	= (void *)0;

	rt_hw_interrupt_enable(level);
	return RT_TRUE;
}

rt_uint8_t pin_irq_enable(rt_uint16_t pin_index, rt_uint8_t en)
{
	rt_base_t			level;
	rt_uint8_t			irq_hdr_index;
	GPIO_InitTypeDef	gpio_init;
	IRQn_Type			irq_type;
	
	if((GPIO_TypeDef *)0 == _pin_get_st_port(pin_index))
	{
		return RT_FALSE;
	}
	if(RT_FALSE == _pin_get_irq_type(pin_index, &irq_type))
	{
		return RT_FALSE;
	}

	if(RT_FALSE == en)
	{
		HAL_NVIC_DisableIRQ(irq_type);

		gpio_init.Pin	= _pin_get_st_pin(pin_index);
		gpio_init.Mode	= GPIO_MODE_ANALOG;
		gpio_init.Pull	= GPIO_NOPULL;
		gpio_init.Speed	= GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(_pin_get_st_port(pin_index), &gpio_init);
	}
	else
	{
		irq_hdr_index = (rt_uint8_t)pin_index;
		if(irq_hdr_index >= (sizeof(m_pin_irq_hdr) / sizeof(m_pin_irq_hdr[0])))
		{
			return RT_FALSE;
		}
		
		level = rt_hw_interrupt_disable();
		if(pin_index != m_pin_irq_hdr[irq_hdr_index].pin)
		{
			rt_hw_interrupt_enable(level);
			return RT_FALSE;
		}

		gpio_init.Pin	= _pin_get_st_pin(pin_index);
		gpio_init.Speed	= GPIO_SPEED_FREQ_HIGH;
		switch(m_pin_irq_hdr[irq_hdr_index].mode)
		{
		case PIN_IRQ_MODE_FALLING:
			gpio_init.Mode = GPIO_MODE_IT_FALLING;
			gpio_init.Pull = GPIO_NOPULL;
			break;
		case PIN_IRQ_MODE_RISING:
			gpio_init.Mode = GPIO_MODE_IT_RISING;
			gpio_init.Pull = GPIO_NOPULL;
			break;
		case PIN_IRQ_MODE_RISING_FALLING:
			gpio_init.Mode = GPIO_MODE_IT_RISING_FALLING;
			gpio_init.Pull = GPIO_NOPULL;
			break;
		}
		rt_hw_interrupt_enable(level);
		HAL_GPIO_Init(_pin_get_st_port(pin_index), &gpio_init);

		HAL_NVIC_SetPriority(irq_type, 1, 0);
		HAL_NVIC_EnableIRQ(irq_type);
	}

	return RT_TRUE;
}

