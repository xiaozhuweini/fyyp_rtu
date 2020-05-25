#include "rtthread.h"
#include "board.h"
#include "stm32f4xx.h"



//rcc初始化
static void _board_init_rcc(void)
{
	HAL_StatusTypeDef		sta;
	RCC_OscInitTypeDef		osc_init;
	RCC_ClkInitTypeDef		clk_init;

	//时钟恢复默认
	sta = HAL_RCC_DeInit();
	while(HAL_OK != sta);
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

//systick初始化
static void _board_init_systick(void)
{
	//10ms
	if(HAL_SYSTICK_Config(400000))
	{
		while(1);
	}
	
	HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

//gpio初始化
static int _board_init_gpio(void)
{
	GPIO_InitTypeDef gpio_init;

	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	
	gpio_init.Pin		= GPIO_PIN_All;
	gpio_init.Mode		= GPIO_MODE_ANALOG;
	gpio_init.Pull		= GPIO_NOPULL;
	gpio_init.Speed		= GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &gpio_init);
	HAL_GPIO_Init(GPIOC, &gpio_init);
	HAL_GPIO_Init(GPIOD, &gpio_init);
	HAL_GPIO_Init(GPIOE, &gpio_init);
	gpio_init.Pin -= (GPIO_PIN_13 + GPIO_PIN_14);
	HAL_GPIO_Init(GPIOA, &gpio_init);

	__HAL_RCC_GPIOA_CLK_DISABLE();
	__HAL_RCC_GPIOB_CLK_DISABLE();
	__HAL_RCC_GPIOC_CLK_DISABLE();
	__HAL_RCC_GPIOD_CLK_DISABLE();
	__HAL_RCC_GPIOE_CLK_DISABLE();

	return 0;
}
INIT_BOARD_EXPORT(_board_init_gpio);



//硬件初始化
void rt_hw_board_init(void)
{
	//nvic初始化
	HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_1);
	
	//rcc初始化
	_board_init_rcc();

	//systick初始化
	_board_init_systick();

	//硬件外设初始化
#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif
}

