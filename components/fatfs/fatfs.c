#include "fatfs.h"
#include "diskio.h"
#include "ff.h"
#include "drv_rtcee.h"
#include "drv_mempool.h"
#include "drv_debug.h"



static struct rt_event		m_fatfs_event_module;
static rt_uint8_t			m_fatfs_hw_ref_times		= 0;
static rt_uint8_t			m_fatfs_sd_type				= FATFS_SD_TYPE_NONE;



static void _fatfs_event_pend(rt_uint32_t event_pend)
{
	if(RT_EOK != rt_event_recv(&m_fatfs_event_module, event_pend, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}
}

static void _fatfs_event_post(rt_uint32_t event_pend)
{
	if(RT_EOK != rt_event_send(&m_fatfs_event_module, event_pend))
	{
		while(1);
	}
}

static void _fatfs_hw_open(void)
{
	SPI_HandleTypeDef spi_handle;
	
	_fatfs_event_pend(FATFS_EVENT_REF_TIMES);
	if(0 == m_fatfs_hw_ref_times)
	{
		//CS
		pin_clock_enable(FATFS_HW_PIN_CS, RT_TRUE);
		pin_mode(FATFS_HW_PIN_CS, PIN_MODE_O_PP_NOPULL);
		pin_write(FATFS_HW_PIN_CS, PIN_STA_HIGH);
		//SCK
		pin_clock_enable(FATFS_HW_PIN_SCK, RT_TRUE);
		pin_mode_ex(FATFS_HW_PIN_SCK, PIN_MODE_AF_PP_PULLUP, FATFS_HW_PIN_AF_INDEX);
		//MISO
		pin_clock_enable(FATFS_HW_PIN_MISO, RT_TRUE);
		pin_mode_ex(FATFS_HW_PIN_MISO, PIN_MODE_AF_PP_PULLUP, FATFS_HW_PIN_AF_INDEX);
		//MOSI
		pin_clock_enable(FATFS_HW_PIN_MOSI, RT_TRUE);
		pin_mode_ex(FATFS_HW_PIN_MOSI, PIN_MODE_AF_PP_PULLUP, FATFS_HW_PIN_AF_INDEX);
		//SPI外设
		FATFS_HW_SPI_CLK_EN();
		spi_handle.Instance					= FATFS_HW_SPIX;
		spi_handle.Init.Mode				= SPI_MODE_MASTER;
		spi_handle.Init.Direction			= SPI_DIRECTION_2LINES;
		spi_handle.Init.DataSize			= SPI_DATASIZE_8BIT;
		spi_handle.Init.CLKPolarity			= SPI_POLARITY_LOW;
		spi_handle.Init.CLKPhase			= SPI_PHASE_1EDGE;
		spi_handle.Init.NSS					= SPI_NSS_SOFT;
		spi_handle.Init.BaudRatePrescaler	= SPI_BAUDRATEPRESCALER_4;
		spi_handle.Init.FirstBit			= SPI_FIRSTBIT_MSB;
		spi_handle.Init.TIMode				= SPI_TIMODE_DISABLE;
		spi_handle.Init.CRCCalculation		= SPI_CRCCALCULATION_DISABLE;
		spi_handle.Init.CRCPolynomial		= 3;
		if(HAL_OK != HAL_SPI_Init(&spi_handle))
		{
			while(1);
		}
		SET_BIT(FATFS_HW_SPIX->CR1, SPI_CR1_SPE);

#ifndef FATFS_HW_PWR_ALWAYS_ON
		//PWREN
		pin_clock_enable(FATFS_HW_PIN_PWREN, RT_TRUE);
		pin_mode(FATFS_HW_PIN_PWREN, PIN_MODE_O_PP_NOPULL);
		pin_write(FATFS_HW_PIN_PWREN, PIN_STA_HIGH);
		rt_thread_delay(RT_TICK_PER_SECOND / 50);
#endif
	}
	m_fatfs_hw_ref_times++;
	_fatfs_event_post(FATFS_EVENT_REF_TIMES);
}

static void _fatfs_hw_close(void)
{
	_fatfs_event_pend(FATFS_EVENT_REF_TIMES);
	if(m_fatfs_hw_ref_times)
	{
		m_fatfs_hw_ref_times--;
	}
	if(0 == m_fatfs_hw_ref_times)
	{
#ifndef FATFS_HW_PWR_ALWAYS_ON
		//PWREN
		pin_write(FATFS_HW_PIN_PWREN, PIN_STA_LOW);
		pin_mode(FATFS_HW_PIN_PWREN, PIN_MODE_ANALOG);
		pin_clock_enable(FATFS_HW_PIN_PWREN, RT_FALSE);
		m_fatfs_sd_type = FATFS_SD_TYPE_NONE;
		//CS
		pin_write(FATFS_HW_PIN_CS, PIN_STA_HIGH);
		pin_mode(FATFS_HW_PIN_CS, PIN_MODE_ANALOG);
		pin_clock_enable(FATFS_HW_PIN_CS, RT_FALSE);
#else
		//CS
		pin_write(FATFS_HW_PIN_CS, PIN_STA_HIGH);
		pin_clock_enable(FATFS_HW_PIN_CS, RT_FALSE);
#endif

		//SPI外设
		CLEAR_BIT(FATFS_HW_SPIX->CR1, SPI_CR1_SPE);
		FATFS_HW_SPI_CLK_DIS();
		//SCK
		pin_mode(FATFS_HW_PIN_SCK, PIN_MODE_ANALOG);
		pin_clock_enable(FATFS_HW_PIN_SCK, RT_FALSE);
		//MISO
		pin_mode(FATFS_HW_PIN_MISO, PIN_MODE_ANALOG);
		pin_clock_enable(FATFS_HW_PIN_MISO, RT_FALSE);
		//MOSI
		pin_mode(FATFS_HW_PIN_MOSI, PIN_MODE_ANALOG);
		pin_clock_enable(FATFS_HW_PIN_MOSI, RT_FALSE);
	}
	_fatfs_event_post(FATFS_EVENT_REF_TIMES);
}

static rt_uint8_t _fatfs_hw_exchange_byte(rt_uint8_t wbyte)
{
	while((FATFS_HW_SPIX->SR & SPI_FLAG_TXE) != SPI_FLAG_TXE);
	*(__IO uint8_t *)&FATFS_HW_SPIX->DR = wbyte;
	while((FATFS_HW_SPIX->SR & SPI_FLAG_RXNE) != SPI_FLAG_RXNE);
	wbyte = FATFS_HW_SPIX->DR;

	return wbyte;
}

//设置SPI时钟速率在100KHZ～400KHZ，SD卡在供电后，SCLK在这个速率下输出至少74个周期的脉冲
//此时CS、DI必须为高电平
static void _fatfs_hw_sck_slow(void)
{
	CLEAR_BIT(FATFS_HW_SPIX->CR1, SPI_CR1_SPE);
	CLEAR_BIT(FATFS_HW_SPIX->CR1, SPI_CR1_BR_Msk);
	SET_BIT(FATFS_HW_SPIX->CR1, SPI_BAUDRATEPRESCALER_256);
	SET_BIT(FATFS_HW_SPIX->CR1, SPI_CR1_SPE);
}

//设置SPI时钟速率在正常速率，最高不能超过25MHZ
static void _fatfs_hw_sck_fast(void)
{
	CLEAR_BIT(FATFS_HW_SPIX->CR1, SPI_CR1_SPE);
	CLEAR_BIT(FATFS_HW_SPIX->CR1, SPI_CR1_BR_Msk);
	SET_BIT(FATFS_HW_SPIX->CR1, SPI_BAUDRATEPRESCALER_4);
	SET_BIT(FATFS_HW_SPIX->CR1, SPI_CR1_SPE);
}

static void _fatfs_pend(void)
{
	_fatfs_hw_open();
	_fatfs_event_pend(FATFS_EVENT_MODULE_PEND);
}

static void _fatfs_post(void)
{
	_fatfs_event_post(FATFS_EVENT_MODULE_PEND);
	_fatfs_hw_close();
}

static rt_uint8_t _fatfs_sd_is_rdy(rt_uint32_t ms)
{
	ms *= RT_TICK_PER_SECOND;
	ms /= 1000;

	do
	{
		if(0xff == _fatfs_hw_exchange_byte(0xff))
		{
			return RT_TRUE;
		}
		rt_thread_delay(1);
		if(ms)
		{
			ms--;
		}
	} while(ms);

	return RT_FALSE;
}

static void _fatfs_sd_dis(void)
{
	pin_write(FATFS_HW_PIN_CS, PIN_STA_HIGH);
	_fatfs_hw_exchange_byte(0xff);
}

static rt_uint8_t _fatfs_sd_en(void)
{
	pin_write(FATFS_HW_PIN_CS, PIN_STA_LOW);
	_fatfs_hw_exchange_byte(0xff);
	
	if(RT_TRUE == _fatfs_sd_is_rdy(1000))
	{
		return FATFS_SD_ERR_NONE;
	}
	
	_fatfs_sd_dis();
	
	return FATFS_SD_ERR_NOT_RDY;
}

static rt_uint8_t _fatfs_sd_recv_data_block(rt_uint8_t *pdata, rt_uint16_t data_len)
{
	rt_uint32_t	ms = 400;
	rt_uint8_t	token;

	ms *= RT_TICK_PER_SECOND;
	ms /= 1000;

	do
	{
		token = _fatfs_hw_exchange_byte(0xff);
		if(0xff != token)
		{
			break;
		}
		rt_thread_delay(1);
		if(ms)
		{
			ms--;
		}
	} while(ms);

	//未收到token
	if(0xff == token)
	{
		return FATFS_SD_ERR_TOKEN_NONE;
	}
	//错误的token
	if(FATFS_SD_DATA_TOKEN_BLK_RD != token)
	{
		return FATFS_SD_ERR_TOKEN_ERR;
	}
	//接收数据和CRC
	while(data_len--)
	{
		*pdata++ = _fatfs_hw_exchange_byte(0xff);
	}
	_fatfs_hw_exchange_byte(0xff);
	_fatfs_hw_exchange_byte(0xff);

	return FATFS_SD_ERR_NONE;
}

static rt_uint8_t _fatfs_sd_send_data_block(rt_uint8_t const *pdata, rt_uint8_t token)
{
	rt_uint16_t cnt = 512;
	
	//每发送一个内存块后，DO上会有一个BUSY信号(低电平)，需等BUSY信号消失后再发送新的内存块
	if(RT_FALSE == _fatfs_sd_is_rdy(1000))
	{
		return FATFS_SD_ERR_NOT_RDY;
	}
	//发送data packet(token+data+crc)的token部分
	_fatfs_hw_exchange_byte(token);
	//如果是结束多块写操作，data packet只包含token部分
	if(FATFS_SD_DATA_TOKEN_STOP_TRAN != token)
	{
		//发送data packet(token+data+crc)的data部分，固定为512字节
		while(cnt--)
		{
			_fatfs_hw_exchange_byte(*pdata++);
		}
		//发送data packet(token+data+crc)的crc部分
		_fatfs_hw_exchange_byte(0xff);
		_fatfs_hw_exchange_byte(0xff);
		//接收Data Response，该应答紧挨着刚发送的data packet，0x05表示data accepted
		token = _fatfs_hw_exchange_byte(0xff);
		if(0x05 != (token & 0x1f))
		{
			return FATFS_SD_ERR_DATA_REJECT;
		}
	}
	
	return FATFS_SD_ERR_NONE;
}

static rt_uint8_t _fatfs_sd_send_cmd(rt_uint8_t cmd, rt_uint32_t arg)
{
	rt_uint8_t resp;

	if(0x80 & cmd)
	{
		cmd &= 0x7f;
		resp = _fatfs_sd_send_cmd(FATFS_SD_CMD_55, 0);
		if(resp > 1)
		{
			return resp;
		}
	}
	//如果不是CMD12(结束多块读)，则失能SD卡再使能SD卡
	if(FATFS_SD_CMD_12 != cmd)
	{
		_fatfs_sd_dis();
		if(FATFS_SD_ERR_NONE != _fatfs_sd_en())
		{
			return 0xff;
		}
	}
	//发送CMD
	_fatfs_hw_exchange_byte((rt_uint8_t)(cmd | 0x40));
	_fatfs_hw_exchange_byte((rt_uint8_t)(arg >> 24));
	_fatfs_hw_exchange_byte((rt_uint8_t)(arg >> 16));
	_fatfs_hw_exchange_byte((rt_uint8_t)(arg >> 8));
	_fatfs_hw_exchange_byte((rt_uint8_t)(arg));
	if(FATFS_SD_CMD_0 == cmd)
	{
		_fatfs_hw_exchange_byte(0x95);
	}
	else if(FATFS_SD_CMD_8 == cmd)
	{
		_fatfs_hw_exchange_byte(0x87);
	}
	else
	{
		_fatfs_hw_exchange_byte(0xff);
	}
	//如果是CMD12，则第一个字节是stuff byte，不是回执，需丢弃
	if(FATFS_SD_CMD_12 == cmd)
	{
		_fatfs_hw_exchange_byte(0xff);
	}
	//最多10个字节的时钟周期之内就会收到应答
	for(cmd = 0; cmd < 10; cmd++)
	{
		resp = _fatfs_hw_exchange_byte(0xff);
		//R1 Response的最高位默认是0
		if(0 == (0x80 & resp))
		{
			break;
		}
	}
	
	return resp;
}

static int _fatfs_device_init(void)
{
	if(RT_EOK != rt_event_init(&m_fatfs_event_module, "fatfs", RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	if(RT_EOK != rt_event_send(&m_fatfs_event_module, FATFS_EVENT_INIT_VALUE))
	{
		while(1);
	}

#ifdef FATFS_HW_PWR_ALWAYS_ON
	//CS
	pin_clock_enable(FATFS_HW_PIN_CS, RT_TRUE);
	pin_mode(FATFS_HW_PIN_CS, PIN_MODE_O_PP_NOPULL);
	pin_write(FATFS_HW_PIN_CS, PIN_STA_HIGH);
	pin_clock_enable(FATFS_HW_PIN_CS, RT_FALSE);
	//PWREN
	pin_clock_enable(FATFS_HW_PIN_PWREN, RT_TRUE);
	pin_mode(FATFS_HW_PIN_PWREN, PIN_MODE_O_PP_NOPULL);
	pin_write(FATFS_HW_PIN_PWREN, PIN_STA_HIGH);
	pin_clock_enable(FATFS_HW_PIN_PWREN, RT_FALSE);
	rt_thread_delay(RT_TICK_PER_SECOND / 50);
#endif
	
	return 0;
}
INIT_DEVICE_EXPORT(_fatfs_device_init);



//Get Drive Status
DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	if(pdrv)
	{
		return STA_NOINIT;
	}
	
	if(FATFS_SD_TYPE_NONE != m_fatfs_sd_type)
	{
		return 0;
	}
	
	return STA_NOINIT;
}

//Inidialize a Drive
DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	rt_uint8_t	resp, ocr[4];
	rt_uint32_t	ms;
	
	if(pdrv)
	{
		return STA_NOINIT;
	}

	DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]卡初始化"));
	//设置SCLK为100K至400K之间
	_fatfs_hw_sck_slow();
	//CS和DI置高，SCLK上给至少74个时钟沿
	pin_write(FATFS_HW_PIN_CS, PIN_STA_HIGH);
	for(resp = 0; resp < 11; resp++)
	{
		_fatfs_hw_exchange_byte(0xff);
	}
	//CMD0，复位SD卡，进入Idle State
	if(1 != _fatfs_sd_send_cmd(FATFS_SD_CMD_0, 0))
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]卡初始化,cmd0失败"));
		m_fatfs_sd_type = FATFS_SD_TYPE_NONE;
		goto __exit;
	}
	//SD_TYPE_MMC_V3或者SD_TYPE_SC_V1
	if(1 != _fatfs_sd_send_cmd(FATFS_SD_CMD_8, 0x1aa))
	{
		//FATFS_SD_TYPE_SC_V1
		if(_fatfs_sd_send_cmd(FATFS_SD_ACMD_41, 0) <= 1)
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]卡初始化,sd_type_sc_v1"));
			m_fatfs_sd_type = FATFS_SD_TYPE_SC_V1;
			resp = FATFS_SD_ACMD_41;
		}
		//FATFS_SD_TYPE_MMC_V3
		else
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]卡初始化,sd_type_mmc_v3"));
			m_fatfs_sd_type = FATFS_SD_TYPE_MMC_V3;
			resp = FATFS_SD_CMD_1;
		}
		//等待初始化完成
		ms = 1000;
		ms *= RT_TICK_PER_SECOND;
		ms /= 1000;
		do
		{
			if(0 == _fatfs_sd_send_cmd(resp, 0))
			{
				break;
			}
			rt_thread_delay(1);
			if(ms)
			{
				ms--;
			}
		} while(ms);
		//初始化失败或者设置块长度为512失败
		if(!ms || _fatfs_sd_send_cmd(FATFS_SD_CMD_16, 512))
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]卡初始化,设置块长度512失败"));
			m_fatfs_sd_type = FATFS_SD_TYPE_NONE;
		}
	}
	//SD_TYPE_SC_V2或者SD_TYPE_HC_XC
	else
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]卡初始化,sd_type_sc_v2或sd_type_hc_xc"));
		//判断供电电压
		for(resp = 0; resp < 4; resp++)
		{
			ocr[resp] = _fatfs_hw_exchange_byte(0xff);
		}
		if((1 != ocr[2]) || (0xaa != ocr[3]))
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]卡初始化,ocr值错误"));
			m_fatfs_sd_type = FATFS_SD_TYPE_NONE;
			goto __exit;
		}
		//等待初始化完成
		ms = 1000;
		ms *= RT_TICK_PER_SECOND;
		ms /= 1000;
		do
		{
			if(0 == _fatfs_sd_send_cmd(FATFS_SD_ACMD_41, 0x40000000))
			{
				break;
			}
			rt_thread_delay(1);
			if(ms)
			{
				ms--;
			}
		} while(ms);
		//初始化失败或者读取OCR失败
		if(!ms || _fatfs_sd_send_cmd(FATFS_SD_CMD_58, 0))
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]卡初始化,acmd41或cmd58失败"));
			m_fatfs_sd_type = FATFS_SD_TYPE_NONE;
			goto __exit;
		}
		for(resp = 0; resp < 4; resp++)
		{
			ocr[resp] = _fatfs_hw_exchange_byte(0xff);
		}
		//FATFS_SD_TYPE_HC_XC
		if(ocr[0] & 0x40)
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]卡初始化,sd_type_hc_xc"));
			m_fatfs_sd_type = FATFS_SD_TYPE_HC_XC;
		}
		//FATFS_SD_TYPE_SC_V2
		else
		{
			DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]卡初始化,sd_type_sc_v2"));
			//设置块长度为512
			if(_fatfs_sd_send_cmd(FATFS_SD_CMD_16, 512))
			{
				DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]卡初始化,设置块长度512失败"));
				m_fatfs_sd_type = FATFS_SD_TYPE_NONE;
				goto __exit;
			}
			m_fatfs_sd_type = FATFS_SD_TYPE_SC_V2;
		}
	}

__exit:
	_fatfs_sd_dis();
	if(FATFS_SD_TYPE_NONE != m_fatfs_sd_type)
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]卡初始化,成功"));
		_fatfs_hw_sck_fast();
	}
	
	return (FATFS_SD_TYPE_NONE != m_fatfs_sd_type) ? 0 : STA_NOINIT;
}

//Read Sector(s)
DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	if(pdrv)
	{
		return RES_PARERR;
	}
	if((BYTE *)0 == buff)
	{
		return RES_PARERR;
	}
	if(0 == count)
	{
		return RES_PARERR;
	}

	if(FATFS_SD_TYPE_HC_XC != m_fatfs_sd_type)
	{
		sector *= 512;
	}
	//读单块
	if(1 == count)
	{
		if(_fatfs_sd_send_cmd(FATFS_SD_CMD_17, sector))
		{
			goto __exit;
		}
		if(FATFS_SD_ERR_NONE != _fatfs_sd_recv_data_block(buff, 512))
		{
			goto __exit;
		}
		count = 0;
	}
	//读多块
	else
	{
		if(_fatfs_sd_send_cmd(FATFS_SD_CMD_18, sector))
		{
			goto __exit;
		}
		
		do
		{
			if(FATFS_SD_ERR_NONE != _fatfs_sd_recv_data_block(buff, 512))
			{
				break;
			}
			buff += 512;
		} while(--count);

		_fatfs_sd_send_cmd(FATFS_SD_CMD_12, 0);
	}

__exit:
	_fatfs_sd_dis();
	
	return count ? RES_ERROR : RES_OK;
}

//Write Sector(s)
DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	if(pdrv)
	{
		return RES_PARERR;
	}
	if((BYTE *)0 == buff)
	{
		return RES_PARERR;
	}
	if(0 == count)
	{
		return RES_PARERR;
	}

	if(FATFS_SD_TYPE_HC_XC != m_fatfs_sd_type)
	{
		sector *= 512;
	}

	if(1 == count)
	{
		if(_fatfs_sd_send_cmd(FATFS_SD_CMD_24, sector))
		{
			goto __exit;
		}
		if(FATFS_SD_ERR_NONE != _fatfs_sd_send_data_block(buff, FATFS_SD_DATA_TOKEN_SINGLE_WR))
		{
			goto __exit;
		}
		count = 0;
	}
	else
	{
		if(FATFS_SD_TYPE_MMC_V3 != m_fatfs_sd_type)
		{
			_fatfs_sd_send_cmd(FATFS_SD_ACMD_23, count);
		}
		if(_fatfs_sd_send_cmd(FATFS_SD_CMD_25, sector))
		{
			goto __exit;
		}
		do
		{
			if(FATFS_SD_ERR_NONE != _fatfs_sd_send_data_block(buff, FATFS_SD_DATA_TOKEN_MULTI_WR))
			{
				break;
			}
			buff += 512;
		} while(--count);
		if(FATFS_SD_ERR_NONE != _fatfs_sd_send_data_block(buff, FATFS_SD_DATA_TOKEN_STOP_TRAN))
		{
			count = 1;
		}
	}

__exit:
	_fatfs_sd_dis();
	
	return count ? RES_ERROR : RES_OK;
}

//Miscellaneous Functions
DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT		res = RES_ERROR;
	BYTE		n, csd[16];
	DWORD		st, ed;
	
	if(pdrv)
	{
		return RES_PARERR;
	}

	switch(cmd)
	{
	default:
		return RES_PARERR;
	case CTRL_SYNC:
		if(FATFS_SD_ERR_NONE == _fatfs_sd_en())
		{
			res = RES_OK;
		}
		break;
	case GET_SECTOR_COUNT:
		if(RES_OK == disk_ioctl(pdrv, MMC_GET_CSD, csd))
		{
			if(1 == (csd[0] >> 6))
			{
				st = csd[9] + ((WORD)csd[8] << 8) + ((DWORD)(csd[7] & 63) << 16) + 1;
				*(DWORD*)buff = st << 10;
			}
			else
			{
				n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
				st = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
				*(DWORD*)buff = st << (n - 9);
			}
			
			res = RES_OK;
		}
		break;
	case GET_BLOCK_SIZE:
		if((FATFS_SD_TYPE_SC_V2 == m_fatfs_sd_type) || (FATFS_SD_TYPE_HC_XC == m_fatfs_sd_type))
		{
			if(0 == _fatfs_sd_send_cmd(FATFS_SD_ACMD_13, 0))
			{
				_fatfs_hw_exchange_byte(0xff);
				if(FATFS_SD_ERR_NONE == _fatfs_sd_recv_data_block(csd, 16))
				{
					for (n = 64 - 16; n; n--)
					{
						_fatfs_hw_exchange_byte(0xff);
					}
					*(DWORD*)buff = 16 << (csd[10] >> 4);
					res = RES_OK;
				}
			}
		}
		else
		{
			if(RES_OK == disk_ioctl(pdrv, MMC_GET_CSD, csd))
			{
				if(FATFS_SD_TYPE_SC_V1 == m_fatfs_sd_type)
				{
					*(DWORD*)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
				}
				else
				{
					*(DWORD*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
				}
				
				res = RES_OK;
			}
		}
		break;
	//FatFs does not check result of this command
	case CTRL_TRIM:
		if((FATFS_SD_TYPE_HC_XC == m_fatfs_sd_type) || (FATFS_SD_TYPE_SC_V2 == m_fatfs_sd_type) || (FATFS_SD_TYPE_SC_V1 == m_fatfs_sd_type))
		{
			if(RES_OK == disk_ioctl(pdrv, MMC_GET_CSD, csd))
			{
				if((csd[0] >> 6) || (csd[10] & 0x40))
				{
					st = *(DWORD *)buff;
					ed = *((DWORD *)buff + 1);
					if(FATFS_SD_TYPE_HC_XC != m_fatfs_sd_type)
					{
						st *= 512;
						ed *= 512;
					}
					if(0 == _fatfs_sd_send_cmd(FATFS_SD_CMD_32, st))
					{
						if(0 == _fatfs_sd_send_cmd(FATFS_SD_CMD_33, ed))
						{
							if(0 == _fatfs_sd_send_cmd(FATFS_SD_CMD_38, 0))
							{
								if(RT_TRUE == _fatfs_sd_is_rdy(30000))
								{
									res = RES_OK;
								}
							}
						}
					}
				}
			}
		}
		break;
	case MMC_GET_CSD:
		if(0 == _fatfs_sd_send_cmd(FATFS_SD_CMD_9, 0))
		{
			if(FATFS_SD_ERR_NONE == _fatfs_sd_recv_data_block((rt_uint8_t *)buff, 16))
			{
				res = RES_OK;
			}
		}
		break;
	}

	_fatfs_sd_dis();
	
	return res;
}

//bit31:25		年(0-127)(从1980开始)
//bit24:21		月(1-12)
//bit20:16		日(1-31)
//bit15:11		时(0-23)
//bit10:5		分(0-59)
//bit4:0		秒(0-29)
DWORD get_fattime(void)
{
	struct tm	time;
	DWORD		unix = 0;

	time = rtcee_rtc_get_calendar();
	if(time.tm_year > 1980)
	{
		unix += ((time.tm_year - 1980) << 25);
	}
	unix += ((time.tm_mon + 1) << 21);
	unix += (time.tm_mday << 16);
	unix += (time.tm_hour << 11);
	unix += (time.tm_min << 5);

	return unix;
}

rt_uint8_t fatfs_mkfs(void)
{
	void		*pbuf;
	FRESULT		res;
	rt_uint8_t	err;

	DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]格式化文件系统"));
	
	_fatfs_pend();

	//申请工作区
	pbuf = mempool_req(FF_MAX_SS, RT_WAITING_NO);
	if((void *)0 == pbuf)
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]格式化文件系统，申请工作区失败"));
		err = FATFS_ERR_MEM_MKFS;
		goto __exit;
	}
	//格式化
	res = f_mkfs("", FM_ANY, 0, pbuf, FF_MAX_SS);
	if(FR_OK != res)
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]格式化文件系统，[f_mkfs]执行失败，错误码[%d]", res));
		err = FATFS_ERR_MKFS;
		goto __exit;
	}
	DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]格式化文件系统，[f_mkfs]执行成功"));
	err = FATFS_ERR_NONE;
__exit:
	//释放工作区
	if((void *)0 != pbuf)
	{
		rt_mp_free(pbuf);
	}
	
	_fatfs_post();

	return err;
}

rt_uint8_t fatfs_write(char const *file_name, rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint8_t	err;
	rt_uint32_t	wlen;
	FATFS		*fatfs_ptr	= (FATFS *)0;
	FIL			*file_ptr	= (FIL *)0;
	FRESULT		res;

	if((char *)0 == file_name)
	{
		return FATFS_ERR_ARG;
	}
	if((rt_uint8_t *)0 == pdata)
	{
		return FATFS_ERR_ARG;
	}
	if(0 == data_len)
	{
		return FATFS_ERR_ARG;
	}

	DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]写数据，文件名[%s]，长度[%d]", file_name, data_len));

	_fatfs_pend();
	
	//申请FATFS
	fatfs_ptr = (FATFS *)mempool_req(sizeof(FATFS), RT_WAITING_NO);
	if((FATFS *)0 == fatfs_ptr)
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]写数据，申请FATFS失败"));
		err = FATFS_ERR_MEM_FATFS;
		goto __exit;
	}
	//申请FIL
	file_ptr = (FIL *)mempool_req(sizeof(FIL), RT_WAITING_NO);
	if((FIL *)0 == file_ptr)
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]写数据，申请FIL失败"));
		err = FATFS_ERR_MEM_FIL;
		goto __exit;
	}
	//挂载文件系统
	res = f_mount(fatfs_ptr, file_name, 0);
	if(FR_OK != res)
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]写数据，[f_mount]执行失败，错误码[%d]", res));
		err = FATFS_ERR_MOUNT;
		goto __exit;
	}
	//打开文件
	res = f_open(file_ptr, file_name, FA_OPEN_APPEND + FA_WRITE);
	if(FR_OK != res)
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]写数据，[f_open]执行失败，错误码[%d]", res));
		err = FATFS_ERR_OPEN;
		goto __exit_mount;
	}
	//写数据
	res = f_write(file_ptr, pdata, data_len, &wlen);
	if(FR_OK != res)
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]写数据，[f_write]执行失败，错误码[%d]", res));
		err = FATFS_ERR_WRITE;
		goto __exit_close;
	}
	if(wlen != data_len)
	{
		DEBUG_INFO_OUTPUT_STR(DEBUG_INFO_TYPE_FATFS, ("\r\n[fatfs]写数据，写入长度不一致，待写入[%d]，已写入[%d]", data_len, wlen));
		err = FATFS_ERR_WRLEN;
		goto __exit_close;
	}
	err = FATFS_ERR_NONE;
__exit_close:
	f_close(file_ptr);
__exit_mount:
	f_mount((FATFS *)0, "",  0);
__exit:
	//释放FIL
	if((FIL *)0 != file_ptr)
	{
		rt_mp_free((void *)file_ptr);
	}
	//释放FATFS
	if((FATFS *)0 != fatfs_ptr)
	{
		rt_mp_free((void *)fatfs_ptr);
	}
	
	_fatfs_post();

	return err;
}

