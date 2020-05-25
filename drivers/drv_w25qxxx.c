/*说明
**支持MODE0(CS跳变沿时SCK为低)和MODE3(CS跳变沿时SCK为高)两种模式
**在MODE0下，CS变低后，MISO(bit7)马上有效，第一个时钟沿(上升沿)芯片采集MOSI(bit7)，第二个时钟沿(下降沿)，MISO(bit6)有效
**在MODE3下，CS变低后，第一个时钟沿(下降沿)MISO(bit7)有效，第二个时钟沿(上升沿)芯片采集MOSI(bit7)，第三个时钟沿(下降沿)，MISO(bit6)有效
*/



#include "drv_w25qxxx.h"
#include "drv_mempool.h"
#include "stm32f4xx.h"



static struct rt_semaphore m_w25qxxx_sem;



static void _w25qxxx_pend(void)
{
	rt_err_t err;

	err = rt_sem_take(&m_w25qxxx_sem, RT_WAITING_FOREVER);
	while(RT_EOK != err);
}

static void _w25qxxx_post(void)
{
	rt_err_t err;

	err = rt_sem_release(&m_w25qxxx_sem);
	while(RT_EOK != err);
}

static void _w25qxxx_open(void)
{
	SPI_HandleTypeDef spi_handle;
	
	pin_clock_enable(W25QXXX_PIN_CS, RT_TRUE);
	pin_mode(W25QXXX_PIN_CS, PIN_MODE_O_PP_NOPULL);
	pin_write(W25QXXX_PIN_CS, PIN_STA_HIGH);

	pin_clock_enable(W25QXXX_PIN_SCK, RT_TRUE);
	pin_mode_ex(W25QXXX_PIN_SCK, PIN_MODE_AF_PP_PULLUP, GPIO_AF5_SPI2);

	pin_clock_enable(W25QXXX_PIN_MOSI, RT_TRUE);
	pin_mode_ex(W25QXXX_PIN_MOSI, PIN_MODE_AF_PP_PULLUP, GPIO_AF5_SPI2);

	pin_clock_enable(W25QXXX_PIN_MISO, RT_TRUE);
	pin_mode_ex(W25QXXX_PIN_MISO, PIN_MODE_AF_PP_PULLUP, GPIO_AF5_SPI2);

	__HAL_RCC_SPI2_CLK_ENABLE();

	spi_handle.Instance					= SPI2;
	spi_handle.Init.Mode				= SPI_MODE_MASTER;
	spi_handle.Init.Direction			= SPI_DIRECTION_2LINES;
	spi_handle.Init.DataSize			= SPI_DATASIZE_8BIT;
	spi_handle.Init.CLKPolarity			= SPI_POLARITY_LOW;
	spi_handle.Init.CLKPhase			= SPI_PHASE_1EDGE;
	spi_handle.Init.NSS					= SPI_NSS_SOFT;
	spi_handle.Init.BaudRatePrescaler	= SPI_BAUDRATEPRESCALER_2;
	spi_handle.Init.FirstBit			= SPI_FIRSTBIT_MSB;
	spi_handle.Init.TIMode				= SPI_TIMODE_DISABLE;
	spi_handle.Init.CRCCalculation		= SPI_CRCCALCULATION_DISABLE;
	spi_handle.Init.CRCPolynomial		= 3;
	if(HAL_OK != HAL_SPI_Init(&spi_handle))
	{
		while(1);
	}
	SET_BIT(SPI2->CR1, SPI_CR1_SPE);
}

static void _w25qxxx_close(void)
{
	CLEAR_BIT(SPI2->CR1, SPI_CR1_SPE);
	__HAL_RCC_SPI2_CLK_DISABLE();
	__HAL_RCC_SPI2_FORCE_RESET();
	__HAL_RCC_SPI2_RELEASE_RESET();
	
	pin_write(W25QXXX_PIN_CS, PIN_STA_HIGH);
	pin_clock_enable(W25QXXX_PIN_CS, RT_FALSE);

	pin_mode(W25QXXX_PIN_SCK, PIN_MODE_ANALOG);
	pin_clock_enable(W25QXXX_PIN_SCK, RT_FALSE);

	pin_mode(W25QXXX_PIN_MOSI, PIN_MODE_ANALOG);
	pin_clock_enable(W25QXXX_PIN_MOSI, RT_FALSE);

	pin_mode(W25QXXX_PIN_MISO, PIN_MODE_ANALOG);
	pin_clock_enable(W25QXXX_PIN_MISO, RT_FALSE);
}

static rt_uint8_t _w25qxxx_exchange_byte(rt_uint8_t wbyte)
{
	while((SPI2->SR & SPI_FLAG_TXE) != SPI_FLAG_TXE);
	*(__IO uint8_t *)&SPI2->DR = wbyte;
	while((SPI2->SR & SPI_FLAG_RXNE) != SPI_FLAG_RXNE);
	wbyte = SPI2->DR;

	return wbyte;
}

static rt_uint8_t _w25qxxx_read_status(rt_uint8_t status)
{
	pin_write(W25QXXX_PIN_CS, PIN_STA_LOW);
	_w25qxxx_exchange_byte(status);
	status = _w25qxxx_exchange_byte(0);
	pin_write(W25QXXX_PIN_CS, PIN_STA_HIGH);
	
	return status;
}

static rt_uint8_t _w25qxxx_is_ready(void)
{
	rt_uint8_t	status;
	rt_uint32_t	i = 0;
	
	do
	{
		status = _w25qxxx_read_status(W25QXXX_CMD_READ_STATUS_1);
		if(0 == (status & W25QXXX_CMD_IS_BUSY))
		{
			return RT_TRUE;
		}
		for(status = 0; status < 20; status++);
		i++;
	} while(i < 0x10000);
	
	return RT_FALSE;
}

#ifdef W25QXXX_256
static rt_uint8_t _w25qxxx_is_4b(void)
{
	if(W25QXXX_CMD_IS_4B & _w25qxxx_read_status(W25QXXX_CMD_READ_STATUS_3))
	{
		return RT_TRUE;
	}
	else
	{
		return RT_FALSE;
	}
}
#endif

static rt_uint8_t _w25qxxx_enter_pwrdown(void)
{
	if(RT_FALSE == _w25qxxx_is_ready())
	{
		return W25QXXX_ERR_STATUS;
	}
	
	pin_write(W25QXXX_PIN_CS, PIN_STA_LOW);
	_w25qxxx_exchange_byte(W25QXXX_CMD_ENTER_PWR_DOWN);
	pin_write(W25QXXX_PIN_CS, PIN_STA_HIGH);
	
	rt_thread_delay(W25QXXX_TICKS_ENTER_PWR_DOWN);

	return W25QXXX_ERR_NONE;
}

static void _w25qxxx_exit_pwrdown(void)
{
	pin_write(W25QXXX_PIN_CS, PIN_STA_LOW);
	_w25qxxx_exchange_byte(W25QXXX_CMD_EXIT_PWR_DOWN);
	pin_write(W25QXXX_PIN_CS, PIN_STA_HIGH);
	
	rt_thread_delay(W25QXXX_TICKS_EXIT_PWR_DOWN);
}

#ifdef W25QXXX_256
static rt_uint8_t _w25qxxx_enter_4b(void)
{
	if(RT_TRUE == _w25qxxx_is_4b())
	{
		return W25QXXX_ERR_NONE;
	}
	
	if(RT_FALSE == _w25qxxx_is_ready())
	{
		return W25QXXX_ERR_STATUS;
	}
	
	pin_write(W25QXXX_PIN_CS, PIN_STA_LOW);
	_w25qxxx_exchange_byte(W25QXXX_CMD_ENTER_4B);
	pin_write(W25QXXX_PIN_CS, PIN_STA_HIGH);

	return W25QXXX_ERR_NONE;
}
#endif

static rt_uint8_t _w25qxxx_data_read(rt_uint32_t addr, rt_uint8_t *pdata, rt_uint16_t data_len)
{
	if(RT_FALSE == _w25qxxx_is_ready())
	{
		return W25QXXX_ERR_STATUS;
	}
	pin_write(W25QXXX_PIN_CS, PIN_STA_LOW);
	_w25qxxx_exchange_byte(W25QXXX_CMD_READ_DATA);
#ifdef W25QXXX_256
	_w25qxxx_exchange_byte((rt_uint8_t)(addr >> 24));
#endif
	_w25qxxx_exchange_byte((rt_uint8_t)(addr >> 16));
	_w25qxxx_exchange_byte((rt_uint8_t)(addr >> 8));
	_w25qxxx_exchange_byte((rt_uint8_t)addr);
	while(data_len)
	{
		data_len--;
		*pdata++ = _w25qxxx_exchange_byte(0);
	}
	pin_write(W25QXXX_PIN_CS, PIN_STA_HIGH);
	
	return W25QXXX_ERR_NONE;
}

static void _w25qxxx_write_enable(void)
{
	pin_write(W25QXXX_PIN_CS, PIN_STA_LOW);
	_w25qxxx_exchange_byte(W25QXXX_CMD_WRITE_ENABLE);
	pin_write(W25QXXX_PIN_CS, PIN_STA_HIGH);
}

static rt_uint8_t _w25qxxx_data_program(rt_uint32_t addr, rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint16_t i, wpos = 0, wlen;
	
	while(wpos < data_len)
	{
		//本次编程字节数
		wlen = addr % W25QXXX_BYTES_PER_PAGE;
		wlen = W25QXXX_BYTES_PER_PAGE - wlen;
		if((wpos + wlen) > data_len)
		{
			wlen = data_len - wpos;
		}
		
		if(RT_FALSE == _w25qxxx_is_ready())
		{
			return W25QXXX_ERR_STATUS;
		}
		_w25qxxx_write_enable();
		pin_write(W25QXXX_PIN_CS, PIN_STA_LOW);
		_w25qxxx_exchange_byte(W25QXXX_CMD_PAGE_PROGRAM);
#ifdef W25QXXX_256
		_w25qxxx_exchange_byte((rt_uint8_t)(addr >> 24));
#endif
		_w25qxxx_exchange_byte((rt_uint8_t)(addr >> 16));
		_w25qxxx_exchange_byte((rt_uint8_t)(addr >> 8));
		_w25qxxx_exchange_byte((rt_uint8_t)addr);
		for(i = 0; i < wlen; i++)
		{
			_w25qxxx_exchange_byte(pdata[wpos + i]);
		}
		pin_write(W25QXXX_PIN_CS, PIN_STA_HIGH);
		wpos += wlen;
		addr += wlen;
		if(addr >= W25QXXX_BYTES_CHIP)
		{
			addr = 0;
		}
	}
	
	return W25QXXX_ERR_NONE;
}

static rt_uint8_t _w25qxxx_erase(rt_uint32_t addr, rt_uint8_t cmd)
{
	if(RT_FALSE == _w25qxxx_is_ready())
	{
		return W25QXXX_ERR_STATUS;
	}
	_w25qxxx_write_enable();
	pin_write(W25QXXX_PIN_CS, PIN_STA_LOW);
	_w25qxxx_exchange_byte(cmd);
#ifdef W25QXXX_256
	_w25qxxx_exchange_byte((rt_uint8_t)(addr >> 24));
#endif
	_w25qxxx_exchange_byte((rt_uint8_t)(addr >> 16));
	_w25qxxx_exchange_byte((rt_uint8_t)(addr >> 8));
	_w25qxxx_exchange_byte((rt_uint8_t)addr);
	pin_write(W25QXXX_PIN_CS, PIN_STA_HIGH);
	
	return W25QXXX_ERR_NONE;
}



static int _w25qxxx_device_init(void)
{
	if(RT_EOK != rt_sem_init(&m_w25qxxx_sem, "w25q", 1, RT_IPC_FLAG_PRIO))
	{
		while(1);
	}

	_w25qxxx_open();
#ifdef W25QXXX_256
	_w25qxxx_exit_pwrdown();
	_w25qxxx_enter_4b();
#endif
	_w25qxxx_enter_pwrdown();
	_w25qxxx_close();

	return 0;
}
INIT_DEVICE_EXPORT(_w25qxxx_device_init);



rt_uint8_t w25qxxx_read(rt_uint32_t addr, rt_uint8_t *pdata, rt_uint16_t data_len)
{
	rt_uint8_t err;
	
	if(addr >= W25QXXX_BYTES_CHIP)
	{
		return W25QXXX_ERR_ARG;
	}
	if((rt_uint8_t *)0 == pdata)
	{
		return W25QXXX_ERR_ARG;
	}
	if(0 == data_len)
	{
		return W25QXXX_ERR_ARG;
	}
	
	_w25qxxx_pend();
	_w25qxxx_open();
	_w25qxxx_exit_pwrdown();
	err = _w25qxxx_data_read(addr, pdata, data_len);
	_w25qxxx_enter_pwrdown();
	_w25qxxx_close();
	_w25qxxx_post();
	
	return err;
}

rt_uint8_t w25qxxx_write(rt_uint32_t addr, rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	static rt_uint8_t	*pmem[W25QXXX_4K_NBLKS];
	rt_uint8_t			*p, need_erase, sta = W25QXXX_ERR_NONE;
	rt_uint16_t			i, wpos = 0, wlen;
	rt_uint32_t			temp_addr;
	
	if(addr >= W25QXXX_BYTES_CHIP)
	{
		return W25QXXX_ERR_ARG;
	}
	if((rt_uint8_t *)0 == pdata)
	{
		return W25QXXX_ERR_ARG;
	}
	if(0 == data_len)
	{
		return W25QXXX_ERR_ARG;
	}
	
	_w25qxxx_pend();
	_w25qxxx_open();
	_w25qxxx_exit_pwrdown();
	//申请空间
	for(i = 0; i < W25QXXX_4K_NBLKS; i++)
	{
		pmem[i] = (rt_uint8_t *)mempool_req(W25QXXX_4K_BLKSIZE, RT_WAITING_FOREVER);
		while((rt_uint8_t *)0 == pmem[i]);
	}
	
	while(wpos < data_len)
	{
		//本次写入字节数
		wlen = addr % W25QXXX_BYTES_PER_SECTOR;
		wlen = W25QXXX_BYTES_PER_SECTOR - wlen;
		if((wpos + wlen) > data_len)
		{
			wlen = data_len - wpos;
		}
		//读出所在sector的数据
		temp_addr = addr;
		temp_addr /= W25QXXX_BYTES_PER_SECTOR;
		temp_addr *= W25QXXX_BYTES_PER_SECTOR;
		for(i = 0; i < W25QXXX_4K_NBLKS; i++)
		{
			if(W25QXXX_ERR_NONE != _w25qxxx_data_read(temp_addr, pmem[i], W25QXXX_4K_BLKSIZE))
			{
				sta = W25QXXX_ERR_DATA_READ;
				goto __exit;
			}
			temp_addr += W25QXXX_4K_BLKSIZE;
		}
		//判断是否需要擦除sector，并填充本次需要编程的数据
		need_erase = RT_FALSE;
		temp_addr = addr % W25QXXX_BYTES_PER_SECTOR;
		for(i = 0; i < wlen; i++)
		{
			p = pmem[temp_addr / W25QXXX_4K_BLKSIZE];
			p += (temp_addr % W25QXXX_4K_BLKSIZE);
			if(0xff != *p)
			{
				need_erase = RT_TRUE;
			}
			*p = pdata[wpos + i];
			temp_addr++;
		}
		//不需要擦除，直接编程
		if(RT_FALSE == need_erase)
		{
			if(W25QXXX_ERR_NONE != _w25qxxx_data_program(addr, pdata + wpos, wlen))
			{
				sta = W25QXXX_ERR_DATA_PROGRAM;
				goto __exit;
			}
		}
		//需要擦除，先擦除再编程
		else
		{
			if(W25QXXX_ERR_NONE != _w25qxxx_erase(addr, W25QXXX_CMD_SECTOR_ERASE))
			{
				sta = W25QXXX_ERR_SECTOR_ERASE;
				goto __exit;
			}
			temp_addr = addr;
			temp_addr /= W25QXXX_BYTES_PER_SECTOR;
			temp_addr *= W25QXXX_BYTES_PER_SECTOR;
			for(i = 0; i < W25QXXX_4K_NBLKS; i++)
			{
				if(W25QXXX_ERR_NONE != _w25qxxx_data_program(temp_addr, pmem[i], W25QXXX_4K_BLKSIZE))
				{
					sta = W25QXXX_ERR_DATA_PROGRAM;
					goto __exit;
				}
				temp_addr += W25QXXX_4K_BLKSIZE;
			}
		}
		wpos += wlen;
		addr += wlen;
		if(addr >= W25QXXX_BYTES_CHIP)
		{
			addr = 0;
		}
	}

__exit:
	//释放空间
	for(i = 0; i < W25QXXX_4K_NBLKS; i++)
	{
		rt_mp_free((void *)pmem[i]);
	}
	_w25qxxx_enter_pwrdown();
	_w25qxxx_close();
	_w25qxxx_post();
	
	return sta;
}

rt_uint8_t w25qxxx_block_erase(rt_uint32_t addr)
{
	rt_uint8_t sta;
	
	if(addr >= W25QXXX_BYTES_CHIP)
	{
		return W25QXXX_ERR_ARG;
	}

	_w25qxxx_pend();
	_w25qxxx_open();
	_w25qxxx_exit_pwrdown();
	sta = _w25qxxx_erase(addr, W25QXXX_CMD_BLOCK_ERASE);
	_w25qxxx_enter_pwrdown();
	_w25qxxx_close();
	_w25qxxx_post();

	return sta;
}

rt_uint8_t w25qxxx_program(rt_uint32_t addr, rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint8_t sta;
	
	if(addr >= W25QXXX_BYTES_CHIP)
	{
		return W25QXXX_ERR_ARG;
	}
	if((rt_uint8_t *)0 == pdata)
	{
		return W25QXXX_ERR_ARG;
	}
	if(0 == data_len)
	{
		return W25QXXX_ERR_ARG;
	}
	
	_w25qxxx_pend();
	_w25qxxx_open();
	_w25qxxx_exit_pwrdown();
	sta = _w25qxxx_data_program(addr, pdata, data_len);
	_w25qxxx_enter_pwrdown();
	_w25qxxx_close();
	_w25qxxx_post();
	
	return sta;
}

void w25qxxx_open_ex(void)
{
	_w25qxxx_pend();
	_w25qxxx_open();
	_w25qxxx_exit_pwrdown();
}

void w25qxxx_close_ex(void)
{
	_w25qxxx_enter_pwrdown();
	_w25qxxx_close();
	_w25qxxx_post();
}

rt_uint8_t w25qxxx_program_ex(rt_uint32_t addr, rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	if(addr >= W25QXXX_BYTES_CHIP)
	{
		return W25QXXX_ERR_ARG;
	}
	if((rt_uint8_t *)0 == pdata)
	{
		return W25QXXX_ERR_ARG;
	}
	if(0 == data_len)
	{
		return W25QXXX_ERR_ARG;
	}
	
	return _w25qxxx_data_program(addr, pdata, data_len);
}

rt_uint8_t w25qxxx_block_erase_ex(rt_uint32_t addr)
{
	if(addr >= W25QXXX_BYTES_CHIP)
	{
		return W25QXXX_ERR_ARG;
	}

	return _w25qxxx_erase(addr, W25QXXX_CMD_BLOCK_ERASE);
}

rt_uint8_t w25qxxx_read_ex(rt_uint32_t addr, rt_uint8_t *pdata, rt_uint16_t data_len)
{
	if(addr >= W25QXXX_BYTES_CHIP)
	{
		return W25QXXX_ERR_ARG;
	}
	if((rt_uint8_t *)0 == pdata)
	{
		return W25QXXX_ERR_ARG;
	}
	if(0 == data_len)
	{
		return W25QXXX_ERR_ARG;
	}
	
	return _w25qxxx_data_read(addr, pdata, data_len);
}

