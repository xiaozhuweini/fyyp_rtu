/**
  ******************************************************************************
  * @file    USB_Device/CDC_Standalone/Src/usbd_cdc_interface.c
  * @author  MCD Application Team
  * @brief   Source file for USBD CDC interface
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics International N.V. 
  * All rights reserved.</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
#include "usbd_cdc_interface.h"
#include "usbd_cdc.h"
#include "usbd_desc.h"
#include "drv_lpm.h"
#include "rthw.h"



static rt_uint8_t			m_usbd_recv_buf[CDC_DATA_FS_MAX_PACKET_SIZE];
static struct rt_event		m_usbd_event_module;
static USBD_HandleTypeDef	USBD_Device;
static rt_uint8_t			m_usbd_sta = RT_FALSE;
static USBD_FUN_RECV_HDR	m_usbd_fun_recv_hdr = (USBD_FUN_RECV_HDR)0;

static struct rt_thread		m_usbd_thread_sta_update;
static rt_uint8_t			m_usbd_stk_sta_update[USBD_STK_STA_UPDATE];

USBD_CDC_LineCodingTypeDef LineCoding = {
  115200,                       /* baud rate */
  0x00,                         /* stop bits-1 */
  0x00,                         /* parity - none */
  0x08                          /* nb. of bits 8 */
};



/* Private function prototypes ----------------------------------------------- */
static int8_t CDC_Itf_Init(void);
static int8_t CDC_Itf_DeInit(void);
static int8_t CDC_Itf_Control(uint8_t cmd, uint8_t * pbuf, uint16_t length);
static int8_t CDC_Itf_Receive(uint8_t * pbuf, uint32_t * Len);

USBD_CDC_ItfTypeDef USBD_CDC_fops = {
  CDC_Itf_Init,
  CDC_Itf_DeInit,
  CDC_Itf_Control,
  CDC_Itf_Receive
};

/* Private functions --------------------------------------------------------- */

/**
  * @brief  Initializes the CDC media low layer      
  * @param  None
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_Init(void)
{
	USBD_CDC_SetTxBuffer(&USBD_Device, m_usbd_recv_buf, 0);
	USBD_CDC_SetRxBuffer(&USBD_Device, m_usbd_recv_buf);
	
	return (USBD_OK);
}

/**
  * @brief  CDC_Itf_DeInit
  *         DeInitializes the CDC media low layer
  * @param  None
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_DeInit(void)
{
	return (USBD_OK);
}

/**
  * @brief  CDC_Itf_Control
  *         Manage the CDC class requests
  * @param  Cmd: Command code            
  * @param  Buf: Buffer containing command data (request parameters)
  * @param  Len: Number of data to be sent (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_Control(uint8_t cmd, uint8_t * pbuf, uint16_t length)
{
  switch (cmd)
  {
  case CDC_SEND_ENCAPSULATED_COMMAND:
    /* Add your code here */
    break;

  case CDC_GET_ENCAPSULATED_RESPONSE:
    /* Add your code here */
    break;

  case CDC_SET_COMM_FEATURE:
    /* Add your code here */
    break;

  case CDC_GET_COMM_FEATURE:
    /* Add your code here */
    break;

  case CDC_CLEAR_COMM_FEATURE:
    /* Add your code here */
    break;

  case CDC_SET_LINE_CODING:
    LineCoding.bitrate = (uint32_t) (pbuf[0] | (pbuf[1] << 8) |
                                     (pbuf[2] << 16) | (pbuf[3] << 24));
    LineCoding.format = pbuf[4];
    LineCoding.paritytype = pbuf[5];
    LineCoding.datatype = pbuf[6];
    break;

  case CDC_GET_LINE_CODING:
    pbuf[0] = (uint8_t) (LineCoding.bitrate);
    pbuf[1] = (uint8_t) (LineCoding.bitrate >> 8);
    pbuf[2] = (uint8_t) (LineCoding.bitrate >> 16);
    pbuf[3] = (uint8_t) (LineCoding.bitrate >> 24);
    pbuf[4] = LineCoding.format;
    pbuf[5] = LineCoding.paritytype;
    pbuf[6] = LineCoding.datatype;
    break;

  case CDC_SET_CONTROL_LINE_STATE:
    /* Add your code here */
    break;

  case CDC_SEND_BREAK:
    /* Add your code here */
    break;

  default:
    break;
  }

  return (USBD_OK);
}

/**
  * @brief  CDC_Itf_DataRx
  *         Data received over USB OUT endpoint are sent over CDC interface 
  *         through this function.
  * @param  Buf: Buffer of data to be transmitted
  * @param  Len: Number of data received (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_Receive(uint8_t * Buf, uint32_t * Len)
{
	rt_base_t			level;
	USBD_FUN_RECV_HDR	recv_hdr;

	level = rt_hw_interrupt_disable();
	recv_hdr = m_usbd_fun_recv_hdr;
	rt_hw_interrupt_enable(level);

	if((USBD_FUN_RECV_HDR)0 != recv_hdr)
	{
		recv_hdr(Buf, *Len);
	}
	USBD_CDC_ReceivePacket(&USBD_Device);

	return (USBD_OK);
}



static void _usbd_module_pend(void)
{
	if(RT_EOK != rt_event_recv(&m_usbd_event_module, USBD_EVENT_MODULE_PEND, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}
}

static void _usbd_module_post(void)
{
	if(RT_EOK != rt_event_send(&m_usbd_event_module, USBD_EVENT_MODULE_PEND))
	{
		while(1);
	}
}

static void _usbd_module_switch(rt_uint8_t module_sta)
{
	if(RT_TRUE == module_sta)
	{
		USBD_Init(&USBD_Device, &VCP_Desc, 0);
		USBD_RegisterClass(&USBD_Device, USBD_CDC_CLASS);
		USBD_CDC_RegisterInterface(&USBD_Device, &USBD_CDC_fops);
		USBD_Start(&USBD_Device);
	}
	else
	{
		USBD_DeInit(&USBD_Device);
	}
}

static void _usbd_plug_hdr(void *arg)
{
	rt_event_send(&m_usbd_event_module, USBD_EVENT_STA_UPDATE);
}

static void _usbd_task_sta_update(void *parg)
{
	rt_uint32_t timeout = 0;
	
	while(1)
	{
		if(RT_EOK != rt_event_recv(&m_usbd_event_module, USBD_EVENT_STA_UPDATE, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
		{
			while(1);
		}

		lpm_cpu_ref(RT_TRUE);
		pin_clock_enable(USBD_PIN_PLUG_DEC, RT_TRUE);
		while(1)
		{
			rt_thread_delay(RT_TICK_PER_SECOND / 20);
			_usbd_module_pend();
			
			if(PIN_STA_HIGH == pin_read(USBD_PIN_PLUG_DEC))
			{
				if(RT_FALSE == m_usbd_sta)
				{
					timeout += 50;
					if(timeout >= USBD_TIMEOUT_PLUG)
					{
						timeout = 0;
						_usbd_module_switch(RT_TRUE);
						m_usbd_sta = RT_TRUE;
					}
				}
				else
				{
					timeout = 0;
				}

				_usbd_module_post();
			}
			else
			{
				if(RT_TRUE == m_usbd_sta)
				{
					timeout += 50;
					if(timeout >= USBD_TIMEOUT_PLUG)
					{
						timeout = 0;
						_usbd_module_switch(RT_FALSE);
						m_usbd_sta = RT_FALSE;

						_usbd_module_post();
						rt_event_recv(&m_usbd_event_module, USBD_EVENT_STA_UPDATE, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, RT_WAITING_NO, (rt_uint32_t *)0);
						break;
					}
					else
					{
						_usbd_module_post();
					}
				}
				else
				{
					timeout = 0;
					
					_usbd_module_post();
					rt_event_recv(&m_usbd_event_module, USBD_EVENT_STA_UPDATE, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, RT_WAITING_NO, (rt_uint32_t *)0);
					break;
				}
			}
		}
		pin_clock_enable(USBD_PIN_PLUG_DEC, RT_FALSE);
		lpm_cpu_ref(RT_FALSE);
	}
}



static int usbd_device_init(void)
{
	if(RT_EOK != rt_event_init(&m_usbd_event_module, "usbd", RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	if(RT_EOK != rt_event_send(&m_usbd_event_module, USBD_EVENT_MODULE_PEND))
	{
		while(1);
	}

	pin_attach_irq(USBD_PIN_PLUG_DEC, PIN_IRQ_MODE_RISING, _usbd_plug_hdr, (void *)0);
	pin_clock_enable(USBD_PIN_PLUG_DEC, RT_TRUE);
	pin_irq_enable(USBD_PIN_PLUG_DEC, RT_TRUE);
	if(PIN_STA_HIGH == pin_read(USBD_PIN_PLUG_DEC))
	{
		rt_event_send(&m_usbd_event_module, USBD_EVENT_STA_UPDATE);
	}
	pin_clock_enable(USBD_PIN_PLUG_DEC, RT_FALSE);
	
	return 0;
}
INIT_DEVICE_EXPORT(usbd_device_init);

static int _usbd_app_init(void)
{
	if(RT_EOK != rt_thread_init(&m_usbd_thread_sta_update, "usbd", _usbd_task_sta_update, (void *)0, (void *)m_usbd_stk_sta_update, USBD_STK_STA_UPDATE, USBD_PRIO_STA_UPDATE, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_usbd_thread_sta_update))
	{
		while(1);
	}
	
	return 0;
}
INIT_APP_EXPORT(_usbd_app_init);



extern PCD_HandleTypeDef hpcd;
void OTG_FS_IRQHandler(void)
{
	rt_interrupt_enter();
	HAL_PCD_IRQHandler(&hpcd);
	rt_interrupt_leave();
}



rt_uint8_t usbd_send(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint8_t sta = RT_FALSE;
	
	if((rt_uint8_t *)0 == pdata)
	{
		return RT_FALSE;
	}
	if(0 == data_len)
	{
		return RT_TRUE;
	}

	_usbd_module_pend();

	if((RT_TRUE == m_usbd_sta) && ((void *)0 != USBD_Device.pClassData))
	{
		while(((USBD_CDC_HandleTypeDef *)(USBD_Device.pClassData))->TxState);
		USBD_CDC_SetTxBuffer(&USBD_Device, (rt_uint8_t *)(void *)pdata, data_len);
		USBD_CDC_TransmitPacket(&USBD_Device);
		while(((USBD_CDC_HandleTypeDef *)(USBD_Device.pClassData))->TxState);

		sta = RT_TRUE;
	}
	
	_usbd_module_post();

	return sta;
}

void usbd_set_recv_hdr(USBD_FUN_RECV_HDR recv_hdr)
{
	rt_base_t level;

	level = rt_hw_interrupt_disable();
	m_usbd_fun_recv_hdr = recv_hdr;
	rt_hw_interrupt_enable(level);
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
