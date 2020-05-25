#include "drv_lpm.h"
#include "rthw.h"
#include "stm32f4xx.h"
#include "string.h"



static rt_uint8_t m_lpm_cpu_ref_num, m_lpm_rcc_ref_num[LPM_NUM_PERIPH_RCC];



static void _lpm_sysclk_resume(void)
{
	HAL_StatusTypeDef		sta;
	RCC_OscInitTypeDef		osc_init;
	RCC_ClkInitTypeDef		clk_init;

	//HSE
	osc_init.OscillatorType			= RCC_OSCILLATORTYPE_HSE;
	osc_init.HSEState				= RCC_HSE_ON;
	osc_init.PLL.PLLState			= RCC_PLL_ON;
	osc_init.PLL.PLLSource			= RCC_PLLSOURCE_HSE;
	osc_init.PLL.PLLM				= 8;
	osc_init.PLL.PLLN				= 240;
	osc_init.PLL.PLLP				= RCC_PLLP_DIV6;
	osc_init.PLL.PLLQ				= 5;
	sta = HAL_RCC_OscConfig(&osc_init);
	while(HAL_OK != sta);
	//SYSCLK、HCLK、PCLK2、PCLK1配置
	clk_init.ClockType				= RCC_CLOCKTYPE_SYSCLK + RCC_CLOCKTYPE_HCLK + RCC_CLOCKTYPE_PCLK2 + RCC_CLOCKTYPE_PCLK1;
	clk_init.SYSCLKSource			= RCC_SYSCLKSOURCE_PLLCLK;
	clk_init.AHBCLKDivider			= RCC_SYSCLK_DIV1;
	clk_init.APB2CLKDivider			= RCC_HCLK_DIV1;
	clk_init.APB1CLKDivider			= RCC_HCLK_DIV1;
	sta = HAL_RCC_ClockConfig(&clk_init, FLASH_LATENCY_1);
	while(HAL_OK != sta);
	//HSI、LSI、LSE、PLL
	osc_init.OscillatorType			= RCC_OSCILLATORTYPE_HSI + RCC_OSCILLATORTYPE_LSE + RCC_OSCILLATORTYPE_LSI;
	osc_init.HSIState				= RCC_HSI_OFF;
	osc_init.LSEState				= RCC_LSE_OFF;
	osc_init.LSIState				= RCC_LSI_OFF;
	osc_init.PLL.PLLState			= RCC_PLL_NONE;
	sta = HAL_RCC_OscConfig(&osc_init);
	while(HAL_OK != sta);
}



static int _lpm_board_init(void)
{
	m_lpm_cpu_ref_num = 1;
	memset((void *)m_lpm_rcc_ref_num, 0, LPM_NUM_PERIPH_RCC);
	
	return 0;
}
INIT_BOARD_EXPORT(_lpm_board_init);

static int _lpm_prev_init(void)
{
	rt_thread_idle_sethook(lpm_enter_stop);
	__HAL_RCC_PWR_CLK_ENABLE();
	
	return 0;
}
INIT_PREV_EXPORT(_lpm_prev_init);



void lpm_rcc_en(rt_uint8_t rcc)
{
	rt_base_t level;

	switch(rcc)
	{
	case LPM_RCC_GPIOA:
		level = rt_hw_interrupt_disable();
		if(0 == m_lpm_rcc_ref_num[rcc])
		{
			__HAL_RCC_GPIOA_CLK_ENABLE();
		}
		m_lpm_rcc_ref_num[rcc]++;
		rt_hw_interrupt_enable(level);
		break;
	case LPM_RCC_GPIOB:
		level = rt_hw_interrupt_disable();
		if(0 == m_lpm_rcc_ref_num[rcc])
		{
			__HAL_RCC_GPIOB_CLK_ENABLE();
		}
		m_lpm_rcc_ref_num[rcc]++;
		rt_hw_interrupt_enable(level);
		break;
	case LPM_RCC_GPIOC:
		level = rt_hw_interrupt_disable();
		if(0 == m_lpm_rcc_ref_num[rcc])
		{
			__HAL_RCC_GPIOC_CLK_ENABLE();
		}
		m_lpm_rcc_ref_num[rcc]++;
		rt_hw_interrupt_enable(level);
		break;
	case LPM_RCC_GPIOD:
		level = rt_hw_interrupt_disable();
		if(0 == m_lpm_rcc_ref_num[rcc])
		{
			__HAL_RCC_GPIOD_CLK_ENABLE();
		}
		m_lpm_rcc_ref_num[rcc]++;
		rt_hw_interrupt_enable(level);
		break;
	case LPM_RCC_GPIOE:
		level = rt_hw_interrupt_disable();
		if(0 == m_lpm_rcc_ref_num[rcc])
		{
			__HAL_RCC_GPIOE_CLK_ENABLE();
		}
		m_lpm_rcc_ref_num[rcc]++;
		rt_hw_interrupt_enable(level);
		break;
	case LPM_RCC_GPIOF:
		level = rt_hw_interrupt_disable();
		if(0 == m_lpm_rcc_ref_num[rcc])
		{
			__HAL_RCC_GPIOF_CLK_ENABLE();
		}
		m_lpm_rcc_ref_num[rcc]++;
		rt_hw_interrupt_enable(level);
		break;
	case LPM_RCC_GPIOG:
		level = rt_hw_interrupt_disable();
		if(0 == m_lpm_rcc_ref_num[rcc])
		{
			__HAL_RCC_GPIOG_CLK_ENABLE();
		}
		m_lpm_rcc_ref_num[rcc]++;
		rt_hw_interrupt_enable(level);
		break;
	}
}

void lpm_rcc_dis(rt_uint8_t rcc)
{
	rt_base_t level;

	switch(rcc)
	{
	case LPM_RCC_GPIOA:
		level = rt_hw_interrupt_disable();
		if(m_lpm_rcc_ref_num[rcc])
		{
			m_lpm_rcc_ref_num[rcc]--;
		}
		if(0 == m_lpm_rcc_ref_num[rcc])
		{
			__HAL_RCC_GPIOA_CLK_DISABLE();
		}
		rt_hw_interrupt_enable(level);
		break;
	case LPM_RCC_GPIOB:
		level = rt_hw_interrupt_disable();
		if(m_lpm_rcc_ref_num[rcc])
		{
			m_lpm_rcc_ref_num[rcc]--;
		}
		if(0 == m_lpm_rcc_ref_num[rcc])
		{
			__HAL_RCC_GPIOB_CLK_DISABLE();
		}
		rt_hw_interrupt_enable(level);
		break;
	case LPM_RCC_GPIOC:
		level = rt_hw_interrupt_disable();
		if(m_lpm_rcc_ref_num[rcc])
		{
			m_lpm_rcc_ref_num[rcc]--;
		}
		if(0 == m_lpm_rcc_ref_num[rcc])
		{
			__HAL_RCC_GPIOC_CLK_DISABLE();
		}
		rt_hw_interrupt_enable(level);
		break;
	case LPM_RCC_GPIOD:
		level = rt_hw_interrupt_disable();
		if(m_lpm_rcc_ref_num[rcc])
		{
			m_lpm_rcc_ref_num[rcc]--;
		}
		if(0 == m_lpm_rcc_ref_num[rcc])
		{
			__HAL_RCC_GPIOD_CLK_DISABLE();
		}
		rt_hw_interrupt_enable(level);
		break;
	case LPM_RCC_GPIOE:
		level = rt_hw_interrupt_disable();
		if(m_lpm_rcc_ref_num[rcc])
		{
			m_lpm_rcc_ref_num[rcc]--;
		}
		if(0 == m_lpm_rcc_ref_num[rcc])
		{
			__HAL_RCC_GPIOE_CLK_DISABLE();
		}
		rt_hw_interrupt_enable(level);
		break;
	case LPM_RCC_GPIOF:
		level = rt_hw_interrupt_disable();
		if(m_lpm_rcc_ref_num[rcc])
		{
			m_lpm_rcc_ref_num[rcc]--;
		}
		if(0 == m_lpm_rcc_ref_num[rcc])
		{
			__HAL_RCC_GPIOF_CLK_DISABLE();
		}
		rt_hw_interrupt_enable(level);
		break;
	case LPM_RCC_GPIOG:
		level = rt_hw_interrupt_disable();
		if(m_lpm_rcc_ref_num[rcc])
		{
			m_lpm_rcc_ref_num[rcc]--;
		}
		if(0 == m_lpm_rcc_ref_num[rcc])
		{
			__HAL_RCC_GPIOG_CLK_DISABLE();
		}
		rt_hw_interrupt_enable(level);
		break;
	}
}

void lpm_cpu_ref(rt_uint8_t dir)
{
	rt_base_t level;
	
	if(RT_FALSE == dir)
	{
		level = rt_hw_interrupt_disable();
		if(m_lpm_cpu_ref_num)
		{
			m_lpm_cpu_ref_num--;
		}
		rt_hw_interrupt_enable(level);
	}
	else
	{
		level = rt_hw_interrupt_disable();
		m_lpm_cpu_ref_num++;
		rt_hw_interrupt_enable(level);
	}
}

void lpm_enter_stop(void)
{
	rt_enter_critical();
	if(0 == m_lpm_cpu_ref_num)
	{
		//进入低功耗模式
		HAL_PWREx_EnableFlashPowerDown();
		HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
		//唤醒后配置时钟，STM32的STOP模式唤醒后，SYSCLK使用的是HSI
		_lpm_sysclk_resume();
	}
	rt_exit_critical();
}
