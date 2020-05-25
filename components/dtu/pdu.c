#include "stdio.h"
#include "drv_fyyp.h"



//8个byte编成7个byte，编码前8个byte是56bit，编码后7个byte也是56个bit
static rt_uint16_t _pdu_7bit_encoder(rt_uint8_t *pdst, rt_uint8_t const *psrc, rt_uint16_t src_len)
{
	rt_uint16_t	dst_len = 0, i = 0;
	rt_uint8_t	bit_num = 0;

	while(i < src_len)
	{
		pdst[dst_len] = psrc[i] >> bit_num;
		if((i + 1) < src_len)
		{
			pdst[dst_len] += (psrc[i + 1] << (7 - bit_num));
		}
		dst_len++;
		i++;
		bit_num++;
		if(7 == bit_num)
		{
			bit_num = 0;
			i++;
		}
	}

	return dst_len;
}

//7个byte编成8个byte
static rt_uint16_t _pdu_7bit_decoder(rt_uint8_t *pdst, rt_uint8_t const *psrc, rt_uint16_t src_len)
{
	rt_uint16_t	dst_len = 0, i = 0;
	rt_uint8_t	bit_num = 0, fill_value = 0;

	while(i < src_len)
	{
		pdst[dst_len]	= psrc[i] << bit_num;
		pdst[dst_len]	+= fill_value;
		pdst[dst_len++]	&= 0x7f;
		fill_value = psrc[i++] >> (7 - bit_num);
		bit_num++;
		if(7 == bit_num)
		{
			bit_num = 0;
			pdst[dst_len++] = fill_value;
			fill_value = 0;
		}
	}

	return dst_len;
}

//psrc的所有数据最高位全部为0就采用7-bit编码，否则就采用8-bit编码
static rt_uint16_t _pdu_encoder(rt_uint8_t *pdst, DTU_SMS_ADDR const *sms_addr_ptr, rt_uint8_t const *psrc, rt_uint8_t src_len)
{
	rt_uint16_t	dst_len = 0;
	rt_uint8_t	i, *pmem;

	if(0 == sms_addr_ptr->addr_len)
	{
		return 0;
	}
	if(0 == src_len)
	{
		return 0;
	}
	
	//使用本机设置的短信中心号码
	pdst[dst_len++] = '0';
	pdst[dst_len++] = '0';
	//文件头字节和信息类型，是固定的
	pdst[dst_len++] = '1';
	pdst[dst_len++] = '1';
	pdst[dst_len++] = '0';
	pdst[dst_len++] = '0';
	//收信方地址长度
	dst_len += sprintf((char *)(pdst + dst_len), "%02X", sms_addr_ptr->addr_len);
	//收信方是国内电话
	pdst[dst_len++] = '8';
	pdst[dst_len++] = '1';
	//收信方地址
	for(i = 0; i < sms_addr_ptr->addr_len;)
	{
		if((i + 1) < sms_addr_ptr->addr_len)
		{
			pdst[dst_len++] = sms_addr_ptr->addr_data[i + 1];
		}
		else
		{
			pdst[dst_len++] = 'F';
		}
		pdst[dst_len++] = sms_addr_ptr->addr_data[i];
		i += 2;
	}
	//00 协议标识(TP-PID) 是普通GSM 类型，点到点方式
	pdst[dst_len++] = '0';
	pdst[dst_len++] = '0';
	//编码方式
	for(i = 0; i < src_len; i++)
	{
		if(psrc[i] & 0x80)
		{
			break;
		}
	}
	pdst[dst_len++] = '0';
	if(i < src_len)
	{
		//8bit
		pdst[dst_len++] = '4';
	}
	else
	{
		//7bit
		pdst[dst_len++] = '0';
	}
	//信息时效性
	pdst[dst_len++] = '0';
	pdst[dst_len++] = '1';
	//短信长度，这个长度是原短信内容的长度，不是编码后的长度
	dst_len += sprintf((char *)(pdst + dst_len), "%02X", src_len);
	//短信内容
	if(i >= src_len)
	{
		pmem = (rt_uint8_t *)mempool_req(src_len, RT_WAITING_FOREVER);
		if((rt_uint8_t *)0 == pmem)
		{
			return 0;
		}
		src_len = _pdu_7bit_encoder(pmem, psrc, src_len);
		for(i = 0; i < src_len; i++)
		{
			dst_len += sprintf((char *)(pdst + dst_len), "%02X", pmem[i]);
		}
		rt_mp_free((void *)pmem);
	}
	else
	{
		for(i = 0; i < src_len; i++)
		{
			dst_len += sprintf((char *)(pdst + dst_len), "%02X", psrc[i]);
		}
	}
	
	return dst_len;
}

static rt_uint16_t _pdu_decoder(rt_uint8_t *pdst, DTU_SMS_ADDR *sms_addr_ptr, rt_uint8_t const *psrc, rt_uint16_t src_len)
{
	rt_uint16_t	i = 0, addr_len;
	rt_uint8_t	byte, tp_udl, *pmem;

	if(0 == src_len)
	{
		return 0;
	}

	//短信中心号码长度
	if((i + 2) > src_len)
	{
		return 0;
	}
	fyyp_str_to_array(&byte, psrc + i, 2);
	i += 2;
	addr_len = byte;
	addr_len *= 2;
	//短信中心号码内容
	if((i + addr_len) > src_len)
	{
		return 0;
	}
	i += addr_len;
	//基本参数
	if((i + 2) > src_len)
	{
		return 0;
	}
	i += 2;
	//回复地址长度
	if((i + 2) > src_len)
	{
		return 0;
	}
	fyyp_str_to_array(&sms_addr_ptr->addr_len, psrc + i, 2);
	if((0 == sms_addr_ptr->addr_len) || (sms_addr_ptr->addr_len > DTU_BYTES_SMS_ADDR))
	{
		return 0;
	}
	i += 2;
	//地址格式
	if((i + 2) > src_len)
	{
		return 0;
	}
	fyyp_str_to_array(&byte, psrc + i, 2);
	i += 2;
	addr_len = 0;
	if(0x91 == byte)
	{
		byte = sms_addr_ptr->addr_len;
		sms_addr_ptr->addr_len++;
		if(sms_addr_ptr->addr_len > DTU_BYTES_SMS_ADDR)
		{
			return 0;
		}
		sms_addr_ptr->addr_data[addr_len++] = '+';
	}
	else
	{
		byte = sms_addr_ptr->addr_len;
	}
	//地址内容
	if(byte % 2)
	{
		byte++;
	}
	if((i + byte) > src_len)
	{
		return 0;
	}
	while(byte)
	{
		//加上这个长度判断，可解决短信地址中F的问题
		if(addr_len < sms_addr_ptr->addr_len)
		{
			sms_addr_ptr->addr_data[addr_len++] = psrc[i + 1];
		}
		if(addr_len < sms_addr_ptr->addr_len)
		{
			sms_addr_ptr->addr_data[addr_len++] = psrc[i];
		}
		i += 2;
		byte -= 2;
	}
	//协议标识
	if((i + 2) > src_len)
	{
		return 0;
	}
	i += 2;
	//编码方式
	if((i + 2) > src_len)
	{
		return 0;
	}
	fyyp_str_to_array(&byte, psrc + i, 2);
	i += 2;
	byte &= 0x0c;
	//时间戳
	if((i + 14) > src_len)
	{
		return 0;
	}
	i += 14;
	//数据长度(这是实际长度，即编码前的长度，并非后面数据的长度，后面是编码后的数据)
	if((i + 2) > src_len)
	{
		return 0;
	}
	fyyp_str_to_array(&tp_udl, psrc + i, 2);
	addr_len = tp_udl;
	i += 2;
	if(0 == byte)
	{
		//当采用7bit编码时，每8个字节会编码成7个字节，因为这是编码前的实际长度，所以需要处理一下
		addr_len -= (addr_len / 8);
	}
	addr_len *= 2;
	//数据
	if((i + addr_len) != src_len)
	{
		return 0;
	}
	if(0 == byte)
	{
		pmem = (rt_uint8_t *)mempool_req(addr_len / 2, RT_WAITING_FOREVER);
		if((rt_uint8_t *)0 == pmem)
		{
			return 0;
		}
		addr_len = fyyp_str_to_array(pmem, psrc + i, addr_len);
		i = _pdu_7bit_decoder(pdst, pmem, addr_len);
		rt_mp_free((void *)pmem);
	}
	else
	{
		i = fyyp_str_to_array(pdst, psrc + i, addr_len);
	}

	return i;
}

