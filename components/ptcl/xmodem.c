#include "xmodem.h"
#include "rthw.h"
#include "drv_w25qxxx.h"
#include "drv_rtcee.h"
#include "drv_mempool.h"
#include "stm32f4xx.h"
#include "drv_lpm.h"
#include "string.h"



static XM_FILE_TRANS_INFO		m_xm_file_trans_info, *m_xm_file_trans_info_ptr;
static rt_ubase_t				m_xm_msgpool_file_trans;
static struct rt_mailbox		m_xm_mb_file_trans;

static rt_ubase_t				m_xm_msgpool_recv_data;
static struct rt_mailbox		m_xm_mb_recv_data;

static struct rt_thread			m_xm_thread_file_trans;
static rt_uint8_t				m_xm_stk_file_trans[XM_STK_FILE_TRANS];



static rt_uint8_t _xm_crc_value(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint8_t crc_value = 0;

	while(data_len)
	{
		data_len--;
		crc_value += *pdata++;
	}

	return crc_value;
}

static PTCL_REPORT_DATA *_xm_encode_cmd(rt_uint8_t const *pdata, rt_uint16_t data_len, XM_FUN_SHELL fun_shell)
{
	PTCL_REPORT_DATA *report_data_ptr;
	
	report_data_ptr = ptcl_report_data_req(data_len, RT_WAITING_FOREVER);
	if((PTCL_REPORT_DATA *)0 != report_data_ptr)
	{
		memcpy((void *)report_data_ptr->pdata, (void *)pdata, data_len);
		report_data_ptr->data_len		= data_len;
		report_data_ptr->data_id		= 0;
		report_data_ptr->fun_csq_update	= (void *)0;
		report_data_ptr->need_reply		= RT_FALSE;
		report_data_ptr->fcb_value		= 0;
		report_data_ptr->ptcl_type		= PTCL_PTCL_TYPE_XMODEM;

		if((XM_FUN_SHELL)0 != fun_shell)
		{
			fun_shell(&report_data_ptr);
		}
	}

	return report_data_ptr;
}

static void _xm_firmware_erase(rt_uint32_t file_len)
{
	rt_uint32_t addr = 0;

	while(addr < file_len)
	{
		w25qxxx_block_erase_ex(addr + XM_FIRMWARE_STORE_ADDR);
		addr += W25QXXX_BYTES_PER_BLOCK;
	}
}

static rt_uint8_t _xm_file_trans_to_device(XM_FILE_TRANS_INFO const *trans_info_ptr, rt_uint8_t (*file_data_write)(rt_uint32_t addr, rt_uint8_t const *pdata, rt_uint16_t data_len))
{
	rt_uint8_t			*pmem, timeout;
	rt_uint16_t			i, bytes_per_trans, mem_len, packet_num;
	rt_uint32_t			tmp, file_len, start_addr;
	PTCL_REPORT_DATA	*report_data_ptr;
	PTCL_RECV_DATA		*recv_data_ptr;
	
	while(1)
	{
		//每次传输的数据量、通信超时时间
		if(PTCL_COMM_TYPE_TC == trans_info_ptr->comm_type)
		{
			bytes_per_trans	= XM_BYTES_PER_TRANS_MIN;
			timeout			= XM_TRANS_TIMEOUT_MIN;
		}
		else
		{
			bytes_per_trans	= XM_BYTES_PER_TRANS_MAX;
			timeout			= XM_TRANS_TIMEOUT_MAX;
		}
		//生成SOH指令
		pmem = (rt_uint8_t *)mempool_req(3, RT_WAITING_FOREVER);
		if((rt_uint8_t *)0 == pmem)
		{
			break;
		}
		mem_len = 0;
		pmem[mem_len++] = XMCS_KW_SOH;
		pmem[mem_len++] = (rt_uint8_t)(bytes_per_trans >> 8);
		pmem[mem_len++] = (rt_uint8_t)bytes_per_trans;
		report_data_ptr = _xm_encode_cmd(pmem, mem_len, trans_info_ptr->fun_shell);
		rt_mp_free((void *)pmem);
		if((PTCL_REPORT_DATA *)0 == report_data_ptr)
		{
			break;
		}
		//握手
		for(mem_len = 0; mem_len < XM_TRANS_TRY_TIMES; mem_len++)
		{
			while(RT_EOK == rt_mb_recv(&m_xm_mb_recv_data, (rt_ubase_t *)&recv_data_ptr, RT_WAITING_NO))
			{
				rt_mp_free((void *)recv_data_ptr);
			}
			if(RT_TRUE == ptcl_report_data_send(report_data_ptr, trans_info_ptr->comm_type, trans_info_ptr->ch, 0, 0, (rt_uint16_t *)0))
			{
				if(RT_EOK == rt_mb_recv(&m_xm_mb_recv_data, (rt_ubase_t *)&recv_data_ptr, timeout * RT_TICK_PER_SECOND))
				{
					while(1)
					{
						i = 0;
						if(trans_info_ptr->comm_type != recv_data_ptr->comm_type)
						{
							break;
						}
						if(trans_info_ptr->ch != recv_data_ptr->ch)
						{
							break;
						}
						if(5 != recv_data_ptr->data_len)
						{
							break;
						}
						if(XMCS_KW_SOH != recv_data_ptr->pdata[i++])
						{
							break;
						}
						file_len = recv_data_ptr->pdata[i++];
						file_len <<= 8;
						file_len += recv_data_ptr->pdata[i++];
						file_len <<= 8;
						file_len += recv_data_ptr->pdata[i++];
						file_len <<= 8;
						file_len += recv_data_ptr->pdata[i];
						
						rt_mp_free((void *)recv_data_ptr);
						recv_data_ptr = (PTCL_RECV_DATA *)0;
						
						break;
					}
					if((PTCL_RECV_DATA *)0 == recv_data_ptr)
					{
						break;
					}
					else
					{
						rt_mp_free((void *)recv_data_ptr);
					}
				}
			}
		}
		rt_mp_free((void *)report_data_ptr);
		//握手失败
		if(mem_len >= XM_TRANS_TRY_TIMES)
		{
			break;
		}
		//文件大小验证
		if(XM_FILE_TYPE_FIRMWARE == trans_info_ptr->file_type)
		{
			if((0 == file_len) || ((file_len + XM_BYTES_FIRMWARE_INFO) > XM_BYTES_FIRMWARE_MAX))
			{
				break;
			}
			w25qxxx_open_ex();
			_xm_firmware_erase(file_len + XM_BYTES_FIRMWARE_INFO);
			start_addr = XM_FIRMWARE_STORE_ADDR + XM_BYTES_FIRMWARE_INFO;
		}
		else if(XM_FILE_TYPE_PARAM == trans_info_ptr->file_type)
		{
			if(XM_BYTES_PARAM != file_len)
			{
				break;
			}
			start_addr = XM_PARAM_STORE_ADDR;
		}
		else
		{
			break;
		}
		//包序号
		tmp			= 0;
		packet_num	= 0;
		//接收文件
		while(tmp < file_len)
		{
			//生成REQ指令
			pmem = (rt_uint8_t *)mempool_req(3, RT_WAITING_FOREVER);
			if((rt_uint8_t *)0 == pmem)
			{
				break;
			}
			mem_len = 0;
			pmem[mem_len++] = XMCS_KW_REQ;
			pmem[mem_len++] = (rt_uint8_t)(packet_num >> 8);
			pmem[mem_len++] = (rt_uint8_t)packet_num;
			report_data_ptr = _xm_encode_cmd(pmem, mem_len, trans_info_ptr->fun_shell);
			rt_mp_free((void *)pmem);
			if((PTCL_REPORT_DATA *)0 == report_data_ptr)
			{
				break;
			}
			//传输数据
			for(mem_len = 0; mem_len < XM_TRANS_TRY_TIMES; mem_len++)
			{
				while(RT_EOK == rt_mb_recv(&m_xm_mb_recv_data, (rt_ubase_t *)&recv_data_ptr, RT_WAITING_NO))
				{
					rt_mp_free((void *)recv_data_ptr);
				}
				if(RT_TRUE == ptcl_report_data_send(report_data_ptr, trans_info_ptr->comm_type, trans_info_ptr->ch, 0, 0, (rt_uint16_t *)0))
				{
					if(RT_EOK == rt_mb_recv(&m_xm_mb_recv_data, (rt_ubase_t *)&recv_data_ptr, timeout * RT_TICK_PER_SECOND))
					{
						while(1)
						{
							i = 0;
							if(trans_info_ptr->comm_type != recv_data_ptr->comm_type)
							{
								break;
							}
							if(trans_info_ptr->ch != recv_data_ptr->ch)
							{
								break;
							}
							if((bytes_per_trans + 4) != recv_data_ptr->data_len)
							{
								break;
							}
							if(XMCS_KW_REQ != recv_data_ptr->pdata[i++])
							{
								break;
							}
							if((rt_uint8_t)(packet_num >> 8) != recv_data_ptr->pdata[i++])
							{
								break;
							}
							if((rt_uint8_t)packet_num != recv_data_ptr->pdata[i++])
							{
								break;
							}
							if((tmp + bytes_per_trans) < file_len)
							{
								file_data_write(start_addr + tmp, recv_data_ptr->pdata + i, bytes_per_trans);
								tmp += bytes_per_trans;
							}
							else
							{
								file_data_write(start_addr + tmp, recv_data_ptr->pdata + i, file_len - tmp);
								tmp = file_len;
							}
							packet_num++;
							
							rt_mp_free((void *)recv_data_ptr);
							recv_data_ptr = (PTCL_RECV_DATA *)0;
							
							break;
						}
						if((PTCL_RECV_DATA *)0 == recv_data_ptr)
						{
							break;
						}
						else
						{
							rt_mp_free((void *)recv_data_ptr);
						}
					}
				}
			}
			//释放指令
			rt_mp_free((void *)report_data_ptr);
			//数据传输失败
			if(mem_len >= XM_TRANS_TRY_TIMES)
			{
				break;
			}
		}
		//接收完成
		if(tmp >= file_len)
		{
			//生成EOT指令
			timeout = XMCS_KW_EOT;
			report_data_ptr = _xm_encode_cmd(&timeout, 1, trans_info_ptr->fun_shell);
			if((PTCL_REPORT_DATA *)0 != report_data_ptr)
			{
				ptcl_report_data_send(report_data_ptr, trans_info_ptr->comm_type, trans_info_ptr->ch, 0, 0, (rt_uint16_t *)0);
				rt_mp_free((void *)report_data_ptr);
			}

			if(XM_FILE_TYPE_FIRMWARE == trans_info_ptr->file_type)
			{
				pmem = (rt_uint8_t *)mempool_req(XM_BYTES_FIRMWARE_INFO, RT_WAITING_FOREVER);
				*(rt_uint32_t *)pmem							= XM_FIRMWARE_IS_VALID;
				*(rt_uint32_t *)(pmem + sizeof(rt_uint32_t))	= file_len;
				file_data_write(XM_FIRMWARE_STORE_ADDR, pmem, XM_BYTES_FIRMWARE_INFO);
				rt_mp_free((void *)pmem);
				w25qxxx_close_ex();
				HAL_NVIC_SystemReset();
			}
			else if(XM_FILE_TYPE_PARAM == trans_info_ptr->file_type)
			{
//				//固态存储
//				sample_clear_old_data();
			}

			return RT_TRUE;
		}

		if(XM_FILE_TYPE_FIRMWARE == trans_info_ptr->file_type)
		{
			w25qxxx_close_ex();
		}
		
		break;
	}

	//生成CAN指令
	timeout = XMCS_KW_CAN;
	report_data_ptr = _xm_encode_cmd(&timeout, 1, trans_info_ptr->fun_shell);
	if((PTCL_REPORT_DATA *)0 != report_data_ptr)
	{
		ptcl_report_data_send(report_data_ptr, trans_info_ptr->comm_type, trans_info_ptr->ch, 0, 0, (rt_uint16_t *)0);
		rt_mp_free((void *)report_data_ptr);
	}

	return RT_FALSE;
}

static rt_uint8_t _xm_file_trans_from_device(XM_FILE_TRANS_INFO const *trans_info_ptr, rt_uint8_t (*file_data_read)(rt_uint32_t addr, rt_uint8_t *pdata, rt_uint16_t data_len))
{
	rt_uint8_t			*pmem, timeout;
	rt_uint16_t			mem_len, bytes_per_trans, i, packet_num;
	rt_uint32_t			tmp, file_len, start_addr;
	PTCL_RECV_DATA		*recv_data_ptr;
	PTCL_REPORT_DATA	*report_data_ptr;

	timeout = (PTCL_COMM_TYPE_TC == trans_info_ptr->comm_type) ? XM_TRANS_TIMEOUT_MIN : XM_TRANS_TIMEOUT_MAX;
	//等SOH
	for(mem_len = 0; mem_len < XM_TRANS_TRY_TIMES; mem_len++)
	{
		if(RT_EOK != rt_mb_recv(&m_xm_mb_recv_data, (rt_ubase_t *)&recv_data_ptr, timeout * RT_TICK_PER_SECOND))
		{
			return RT_FALSE;
		}
		while(1)
		{
			i = 0;
			if(trans_info_ptr->comm_type != recv_data_ptr->comm_type)
			{
				break;
			}
			if(trans_info_ptr->ch != recv_data_ptr->ch)
			{
				break;
			}
			if(3 != recv_data_ptr->data_len)
			{
				break;
			}
			if(XMCS_KW_SOH != recv_data_ptr->pdata[i++])
			{
				break;
			}
			bytes_per_trans = recv_data_ptr->pdata[i++];
			bytes_per_trans <<= 8;
			bytes_per_trans += recv_data_ptr->pdata[i];

			rt_mp_free((void *)recv_data_ptr);
			recv_data_ptr = (PTCL_RECV_DATA *)0;
			
			break;
		}
		if((PTCL_RECV_DATA *)0 == recv_data_ptr)
		{
			break;
		}
		else
		{
			rt_mp_free((void *)recv_data_ptr);
		}
	}
	if(mem_len >= XM_TRANS_TRY_TIMES)
	{
		return RT_FALSE;
	}
	//发送SOH
	if(XM_FILE_TYPE_PARAM == trans_info_ptr->file_type)
	{
		file_len	= XM_BYTES_PARAM;
		start_addr	= 0;
	}
	else
	{
		return RT_FALSE;
	}
	pmem = (rt_uint8_t *)mempool_req(5, RT_WAITING_FOREVER);
	if((rt_uint8_t *)0 == pmem)
	{
		return RT_FALSE;
	}
	mem_len = 0;
	pmem[mem_len++] = XMCS_KW_SOH;
	pmem[mem_len++] = (rt_uint8_t)(file_len >> 24);
	pmem[mem_len++] = (rt_uint8_t)(file_len >> 16);
	pmem[mem_len++] = (rt_uint8_t)(file_len >> 8);
	pmem[mem_len++] = (rt_uint8_t)file_len;
	report_data_ptr = _xm_encode_cmd(pmem, mem_len, trans_info_ptr->fun_shell);
	rt_mp_free((void *)pmem);
	if((PTCL_REPORT_DATA *)0 == report_data_ptr)
	{
		return RT_FALSE;
	}
	for(mem_len = 0; mem_len < XM_TRANS_TRY_TIMES; mem_len++)
	{
		if(RT_TRUE == ptcl_report_data_send(report_data_ptr, trans_info_ptr->comm_type, trans_info_ptr->ch, 0, 0, (rt_uint16_t *)0))
		{
			break;
		}
	}
	rt_mp_free((void *)report_data_ptr);
	if(mem_len >= XM_TRANS_TRY_TIMES)
	{
		return RT_FALSE;
	}
	//等REQ、EOT、CAN
	while(RT_EOK == rt_mb_recv(&m_xm_mb_recv_data, (rt_ubase_t *)&recv_data_ptr, timeout * RT_TICK_PER_SECOND))
	{
		while(1)
		{
			i = 0;
			if(trans_info_ptr->comm_type != recv_data_ptr->comm_type)
			{
				break;
			}
			if(trans_info_ptr->ch != recv_data_ptr->ch)
			{
				break;
			}
			if(0 == recv_data_ptr->data_len)
			{
				break;
			}
			switch(recv_data_ptr->pdata[i++])
			{
			default:
				break;
			case XMCS_KW_CAN:
				break;
			case XMCS_KW_EOT:
				if(i == recv_data_ptr->data_len)
				{
					rt_mp_free((void *)recv_data_ptr);
					return RT_TRUE;
				}
				break;
			case XMCS_KW_REQ:
				while(1)
				{
					if((i + 2) != recv_data_ptr->data_len)
					{
						break;
					}
					packet_num = recv_data_ptr->pdata[i++];
					packet_num <<= 8;
					packet_num += recv_data_ptr->pdata[i];
					tmp = packet_num;
					tmp *= bytes_per_trans;
					if(tmp >= file_len)
					{
						break;
					}
					pmem = (rt_uint8_t *)mempool_req(bytes_per_trans + 4, RT_WAITING_FOREVER);
					if((rt_uint8_t *)0 == pmem)
					{
						break;
					}
					mem_len = 0;
					pmem[mem_len++] = XMCS_KW_REQ;
					pmem[mem_len++] = (rt_uint8_t)(packet_num >> 8);
					pmem[mem_len++] = (rt_uint8_t)packet_num;
					if((tmp + bytes_per_trans) <= file_len)
					{
						file_data_read(start_addr + tmp, pmem + mem_len, bytes_per_trans);
					}
					else
					{
						rt_memset((void *)(pmem + mem_len), XMCS_KW_EOF, bytes_per_trans);
						file_data_read(start_addr + tmp, pmem + mem_len, file_len - tmp);
					}
					mem_len += bytes_per_trans;
					pmem[mem_len++] = _xm_crc_value(pmem + 3, bytes_per_trans);
					report_data_ptr = _xm_encode_cmd(pmem, mem_len, trans_info_ptr->fun_shell);
					rt_mp_free((void *)pmem);
					if((PTCL_REPORT_DATA *)0 == report_data_ptr)
					{
						break;
					}
					for(mem_len = 0; mem_len < XM_TRANS_TRY_TIMES; mem_len++)
					{
						if(RT_TRUE == ptcl_report_data_send(report_data_ptr, trans_info_ptr->comm_type, trans_info_ptr->ch, 0, 0, (rt_uint16_t *)0))
						{
							break;
						}
					}
					rt_mp_free((void *)report_data_ptr);
					if(mem_len >= XM_TRANS_TRY_TIMES)
					{
						break;
					}
					rt_mp_free((void *)recv_data_ptr);
					recv_data_ptr = (PTCL_RECV_DATA *)0;
					
					break;
				}
				break;
			}
			
			break;
		}

		if((PTCL_RECV_DATA *)0 != recv_data_ptr)
		{
			rt_mp_free((void *)recv_data_ptr);
			break;
		}
	}

	return RT_FALSE;
}

//传输状态：0-传输中、1-传输成功、2-传输正常但文件太大
static rt_uint8_t _xm_file_trans_from_platform(XM_FILE_TRANS_INFO const *trans_info_ptr, rt_uint8_t (*file_data_write)(rt_uint32_t addr, rt_uint8_t const *pdata, rt_uint16_t data_len))
{
	rt_uint8_t			*pmem, timeout, trans_sta = 0;
	rt_uint16_t			i, bytes_per_trans, mem_len, packet_num;
	rt_uint32_t			tmp, file_len, start_addr;
	PTCL_REPORT_DATA	*report_data_ptr;
	PTCL_RECV_DATA		*recv_data_ptr;
	
	while(1)
	{
		//每次传输的数据量、通信超时时间
		if(PTCL_COMM_TYPE_TC == trans_info_ptr->comm_type)
		{
			bytes_per_trans	= XM_BYTES_PER_TRANS_PLATFORM;
			timeout			= XM_TRANS_TIMEOUT_MIN;
		}
		else
		{
			bytes_per_trans	= XM_BYTES_PER_TRANS_PLATFORM;
			timeout			= XM_TRANS_TIMEOUT_MAX;
		}
		//文件类型验证
		if(XM_FILE_TYPE_FIRMWARE == trans_info_ptr->file_type)
		{
			w25qxxx_open_ex();
			_xm_firmware_erase(XM_BYTES_FIRMWARE_MAX);
			start_addr = XM_FIRMWARE_STORE_ADDR + XM_BYTES_FIRMWARE_INFO;
		}
		else
		{
			break;
		}
		//文件长度、包序号
		file_len	= 0;
		packet_num	= 1;
		//接收文件
		do
		{
			//生成REQ指令
			pmem = (rt_uint8_t *)mempool_req(3, RT_WAITING_FOREVER);
			if((rt_uint8_t *)0 == pmem)
			{
				break;
			}
			mem_len = 0;
			pmem[mem_len++] = XMCS_KW_REQ;
			pmem[mem_len++] = (rt_uint8_t)(packet_num >> 8);
			pmem[mem_len++] = (rt_uint8_t)packet_num;
			report_data_ptr = _xm_encode_cmd(pmem, mem_len, trans_info_ptr->fun_shell);
			rt_mp_free((void *)pmem);
			if((PTCL_REPORT_DATA *)0 == report_data_ptr)
			{
				break;
			}
			//传输数据
			for(mem_len = 0; mem_len < XM_TRANS_TRY_TIMES; mem_len++)
			{
				while(RT_EOK == rt_mb_recv(&m_xm_mb_recv_data, (rt_ubase_t *)&recv_data_ptr, RT_WAITING_NO))
				{
					rt_mp_free((void *)recv_data_ptr);
				}
				if(RT_TRUE == ptcl_report_data_send(report_data_ptr, trans_info_ptr->comm_type, trans_info_ptr->ch, 0, 0, (rt_uint16_t *)0))
				{
					if(RT_EOK == rt_mb_recv(&m_xm_mb_recv_data, (rt_ubase_t *)&recv_data_ptr, timeout * RT_TICK_PER_SECOND))
					{
						while(1)
						{
							i = 0;
							if(trans_info_ptr->comm_type != recv_data_ptr->comm_type)
							{
								break;
							}
							if(trans_info_ptr->ch != recv_data_ptr->ch)
							{
								break;
							}
							if((bytes_per_trans + 4) != recv_data_ptr->data_len)
							{
								break;
							}
							if(XMCS_KW_REQ_EX != recv_data_ptr->pdata[i++])
							{
								break;
							}
							if((rt_uint8_t)(packet_num >> 8) != recv_data_ptr->pdata[i++])
							{
								break;
							}
							if((rt_uint8_t)packet_num != recv_data_ptr->pdata[i++])
							{
								break;
							}
							//判断是否最后一包并得出本包数据长度
							if(XMCS_KW_EOF == recv_data_ptr->pdata[i + bytes_per_trans - 1])
							{
								trans_sta = 1;
								for(tmp = 0; tmp < bytes_per_trans; tmp++)
								{
									if(XMCS_KW_EOF == recv_data_ptr->pdata[i + tmp])
									{
										break;
									}
								}
							}
							else
							{
								tmp = bytes_per_trans;
							}
							//判断文件长度是否超过限制
							if((file_len + tmp + XM_BYTES_FIRMWARE_INFO) > XM_BYTES_FIRMWARE_MAX)
							{
								trans_sta = 2;
								rt_mp_free((void *)recv_data_ptr);
								recv_data_ptr = (PTCL_RECV_DATA *)0;
								break;
							}
							//写入数据
							file_data_write(start_addr + file_len, recv_data_ptr->pdata + i, tmp);
							file_len += tmp;
							packet_num++;
							
							rt_mp_free((void *)recv_data_ptr);
							recv_data_ptr = (PTCL_RECV_DATA *)0;
							
							break;
						}
						if((PTCL_RECV_DATA *)0 == recv_data_ptr)
						{
							break;
						}
						else
						{
							rt_mp_free((void *)recv_data_ptr);
						}
					}
				}
			}
			//释放指令
			rt_mp_free((void *)report_data_ptr);
			//数据传输失败
			if(mem_len >= XM_TRANS_TRY_TIMES)
			{
				break;
			}
		} while(0 == trans_sta);
		//接收完成
		if(1 == trans_sta)
		{
			//生成EOT指令
			timeout = XMCS_KW_EOT;
			report_data_ptr = _xm_encode_cmd(&timeout, 1, trans_info_ptr->fun_shell);
			if((PTCL_REPORT_DATA *)0 != report_data_ptr)
			{
				ptcl_report_data_send(report_data_ptr, trans_info_ptr->comm_type, trans_info_ptr->ch, 0, 0, (rt_uint16_t *)0);
				rt_mp_free((void *)report_data_ptr);
			}

			if(XM_FILE_TYPE_FIRMWARE == trans_info_ptr->file_type)
			{
				pmem = (rt_uint8_t *)mempool_req(XM_BYTES_FIRMWARE_INFO, RT_WAITING_FOREVER);
				*(rt_uint32_t *)pmem							= XM_FIRMWARE_IS_VALID;
				*(rt_uint32_t *)(pmem + sizeof(rt_uint32_t))	= file_len;
				file_data_write(XM_FIRMWARE_STORE_ADDR, pmem, XM_BYTES_FIRMWARE_INFO);
				rt_mp_free((void *)pmem);
				w25qxxx_close_ex();
				HAL_NVIC_SystemReset();
			}

			return RT_TRUE;
		}

		if(XM_FILE_TYPE_FIRMWARE == trans_info_ptr->file_type)
		{
			w25qxxx_close_ex();
		}
		
		break;
	}

	//生成CAN指令
	timeout = XMCS_KW_CAN;
	report_data_ptr = _xm_encode_cmd(&timeout, 1, trans_info_ptr->fun_shell);
	if((PTCL_REPORT_DATA *)0 != report_data_ptr)
	{
		ptcl_report_data_send(report_data_ptr, trans_info_ptr->comm_type, trans_info_ptr->ch, 0, 0, (rt_uint16_t *)0);
		rt_mp_free((void *)report_data_ptr);
	}

	return RT_FALSE;
}

static void _xm_task_file_trans(void *parg)
{
	XM_FILE_TRANS_INFO		*file_trans_info_ptr;
	rt_base_t				level;

	while(1)
	{
		if(RT_EOK != rt_mb_recv(&m_xm_mb_file_trans, (rt_ubase_t *)&file_trans_info_ptr, RT_WAITING_FOREVER))
		{
			while(1);
		}

		lpm_cpu_ref(RT_TRUE);

		if((XM_FILE_TRANS_INFO *)0 != file_trans_info_ptr)
		{
			if(XM_DIR_TO_DEVICE == file_trans_info_ptr->trans_dir)
			{
				if(XM_FILE_TYPE_FIRMWARE == file_trans_info_ptr->file_type)
				{
					_xm_file_trans_to_device(file_trans_info_ptr, w25qxxx_program_ex);
				}
				else if(XM_FILE_TYPE_PARAM == file_trans_info_ptr->file_type)
				{
					_xm_file_trans_to_device(file_trans_info_ptr, rtcee_eeprom_write);
				}
			}
			else if(XM_DIR_FROM_DEVICE == file_trans_info_ptr->trans_dir)
			{
				if(XM_FILE_TYPE_PARAM == file_trans_info_ptr->file_type)
				{
					_xm_file_trans_from_device(file_trans_info_ptr, rtcee_eeprom_read);
				}
			}
			else if(XM_DIR_FROM_PLATFORM == file_trans_info_ptr->trans_dir)
			{
				if(XM_FILE_TYPE_FIRMWARE == file_trans_info_ptr->file_type)
				{
					_xm_file_trans_from_platform(file_trans_info_ptr, w25qxxx_program_ex);
				}
			}
			
			level = rt_hw_interrupt_disable();
			m_xm_file_trans_info_ptr = file_trans_info_ptr;
			rt_hw_interrupt_enable(level);
		}

		lpm_cpu_ref(RT_FALSE);
	}
}

static int _xm_component_init(void)
{
	m_xm_file_trans_info_ptr = &m_xm_file_trans_info;
	if(RT_EOK != rt_mb_init(&m_xm_mb_file_trans, "xmodem", (void *)&m_xm_msgpool_file_trans, 1, RT_IPC_FLAG_PRIO))
	{
		while(1);
	}

	if(RT_EOK != rt_mb_init(&m_xm_mb_recv_data, "xmodem", (void *)&m_xm_msgpool_recv_data, 1, RT_IPC_FLAG_PRIO))
	{
		while(1);
	}

	return 0;
}
INIT_COMPONENT_EXPORT(_xm_component_init);

static int _xm_app_init(void)
{
	if(RT_EOK != rt_thread_init(&m_xm_thread_file_trans, "xmodem", _xm_task_file_trans, (void *)0, (void *)m_xm_stk_file_trans, XM_STK_FILE_TRANS, XM_PRIO_FILE_TRANS, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_xm_thread_file_trans))
	{
		while(1);
	}
	
	return 0;
}
INIT_APP_EXPORT(_xm_app_init);



rt_uint8_t xm_data_decoder(PTCL_RECV_DATA const *recv_data_ptr)
{
	rt_uint16_t		i = 0;
	PTCL_RECV_DATA	*data_ptr;

	if(0 == recv_data_ptr->data_len)
	{
		return RT_FALSE;
	}

	switch(recv_data_ptr->pdata[i++])
	{
	default:
		return RT_FALSE;
	case XMCS_KW_SOH:
		if((3 != recv_data_ptr->data_len) && (5 != recv_data_ptr->data_len))
		{
			return RT_FALSE;
		}
		break;
	case XMCS_KW_REQ:
	case XMCS_KW_REQ_EX:
		if(recv_data_ptr->data_len < (i + XM_BYTES_FRAME_PACKET_INFO))
		{
			return RT_FALSE;
		}
		if((i + XM_BYTES_FRAME_PACKET_INFO) != recv_data_ptr->data_len)
		{
			if((i + XM_BYTES_FRAME_PACKET_INFO + XM_BYTES_FRAME_CRC) >= recv_data_ptr->data_len)
			{
				return RT_FALSE;
			}
			i += XM_BYTES_FRAME_PACKET_INFO;
			if(recv_data_ptr->pdata[recv_data_ptr->data_len - 1] != _xm_crc_value(recv_data_ptr->pdata + i, recv_data_ptr->data_len - 4))
			{
				return RT_FALSE;
			}
		}
		break;
	case XMCS_KW_CAN:
	case XMCS_KW_EOT:
		if(i != recv_data_ptr->data_len)
		{
			return RT_FALSE;
		}
		break;
	}

	data_ptr = ptcl_recv_data_req(recv_data_ptr->data_len, RT_WAITING_FOREVER);
	if((PTCL_RECV_DATA *)0 != data_ptr)
	{
		data_ptr->comm_type	= recv_data_ptr->comm_type;
		data_ptr->ch		= recv_data_ptr->ch;
		data_ptr->data_len	= recv_data_ptr->data_len;
		memcpy((void *)data_ptr->pdata, (void *)recv_data_ptr->pdata, recv_data_ptr->data_len);
		if(RT_EOK != rt_mb_send_wait(&m_xm_mb_recv_data, (rt_ubase_t)data_ptr, RT_WAITING_NO))
		{
			rt_mp_free((void *)data_ptr);
		}
	}

	return RT_TRUE;
}

rt_uint8_t xm_file_trans_trigger(rt_uint8_t comm_type, rt_uint8_t ch, rt_uint8_t trans_dir, rt_uint8_t file_type, XM_FUN_SHELL fun_shell)
{
	XM_FILE_TRANS_INFO	*file_trans_info_ptr;
	rt_base_t			level;

	if(trans_dir >= XM_NUM_TRANS_DIR)
	{
		return RT_FALSE;
	}
	if(file_type >= XM_NUM_FILE_TYPE)
	{
		return RT_FALSE;
	}

	level = rt_hw_interrupt_disable();
	file_trans_info_ptr = m_xm_file_trans_info_ptr;
	m_xm_file_trans_info_ptr = (XM_FILE_TRANS_INFO *)0;
	rt_hw_interrupt_enable(level);

	if((XM_FILE_TRANS_INFO *)0 == file_trans_info_ptr)
	{
		return RT_FALSE;
	}

	file_trans_info_ptr->comm_type	= comm_type;
	file_trans_info_ptr->ch			= ch;
	file_trans_info_ptr->trans_dir	= trans_dir;
	file_trans_info_ptr->file_type	= file_type;
	file_trans_info_ptr->fun_shell	= fun_shell;

	if(RT_EOK == rt_mb_send_wait(&m_xm_mb_file_trans, (rt_ubase_t)file_trans_info_ptr, RT_WAITING_NO))
	{
		return RT_TRUE;
	}

	level = rt_hw_interrupt_disable();
	m_xm_file_trans_info_ptr = file_trans_info_ptr;
	rt_hw_interrupt_enable(level);

	return RT_FALSE;
}

