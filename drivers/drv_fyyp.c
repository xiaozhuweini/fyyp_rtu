#include "drv_fyyp.h"
#include "string.h"



rt_uint8_t fyyp_hex_to_bcd(rt_uint8_t hex)
{
	rt_uint8_t bcd;
	
	bcd = hex / 10;
	bcd <<= 4;
	bcd += hex % 10;
	
	return bcd;
}

rt_uint8_t fyyp_bcd_to_hex(rt_uint8_t bcd)
{
	rt_uint8_t hex;
	
	hex = bcd >> 4;
	hex *= 10;
	hex += bcd & 0x0F;
	
	return hex;
}

rt_uint8_t fyyp_is_bcdcode(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint16_t i;
	
	if((rt_uint8_t *)0 == pdata)
	{
		return RT_FALSE;
	}
	if(0 == data_len)
	{
		return RT_FALSE;
	}
	
	for(i = 0; i < data_len; i++)
	{
		if((pdata[i] >> 4) > 9)
		{
			return RT_FALSE;
		}
		
		if((pdata[i] & 0x0F) > 9)
		{
			return RT_FALSE;
		}
	}
	
	return RT_TRUE;
}

rt_uint32_t fyyp_str_to_hex(rt_uint8_t const *pdata, rt_uint8_t data_len)
{
	rt_uint8_t	i;
	rt_uint32_t	value = 0;
	
	if((rt_uint8_t *)0 == pdata)
	{
		return 0;
	}
	if(0 == data_len)
	{
		return 0;
	}
	
	for(i = 0; i < data_len; i++)
	{
		value *= 10;
		value += (pdata[i] - '0');
	}
	
	return value;
}

rt_uint8_t fyyp_is_number(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint16_t i;
	
	if((rt_uint8_t *)0 == pdata)
	{
		return RT_FALSE;
	}
	if(0 == data_len)
	{
		return RT_FALSE;
	}
	
	for(i = 0; i < data_len; i++)
	{
		if((pdata[i] < '0') || (pdata[i] > '9'))
		{
			return RT_FALSE;
		}
	}
	
	return RT_TRUE;
}

rt_uint8_t fyyp_ip_str_to_bcd(rt_uint8_t *pdst, rt_uint8_t const *psrc, rt_uint8_t src_len)
{
	rt_uint8_t	ip[4], i, pos = 0, num = 0;
	rt_uint16_t	port = 0;

	if((rt_uint8_t *)0 == pdst)
	{
		return 0;
	}
	if((rt_uint8_t *)0 == psrc)
	{
		return 0;
	}
	if(0 == src_len)
	{
		return 0;
	}

	for(i = 0; i < 4; i++)
	{
		ip[i] = 0;
	}
	
	for(i = 0; i < src_len; i++)
	{
		if(4 == num)
		{
			port = fyyp_str_to_hex(psrc + i, src_len - i);
			break;
		}
		if((psrc[i] < '0') || (psrc[i] > '9'))
		{
			if(num < 4)
			{
				ip[num] = fyyp_str_to_hex(psrc + pos, i - pos);
			}
			num++;
			pos = i + 1;
		}
	}

	i = 0;
	pdst[i++] = fyyp_hex_to_bcd(ip[0] / 10);
	pdst[i++] = ((ip[0] % 10) << 4) + ip[1] / 100;
	pdst[i++] = fyyp_hex_to_bcd(ip[1] % 100);
	pdst[i++] = fyyp_hex_to_bcd(ip[2] / 10);
	pdst[i++] = ((ip[2] % 10) << 4) + ip[3] / 100;
	pdst[i++] = fyyp_hex_to_bcd(ip[3] % 100);
	pdst[i++] = fyyp_hex_to_bcd(port / 10000 % 100);
	pdst[i++] = fyyp_hex_to_bcd(port / 100 % 100);
	pdst[i++] = fyyp_hex_to_bcd(port % 100);

	return i;
}

rt_uint8_t fyyp_sms_str_to_bcd(rt_uint8_t *pdst, rt_uint8_t const *psrc, rt_uint8_t src_len)
{
	rt_uint8_t dst_len = 0, i = 0;

	if((rt_uint8_t *)0 == pdst)
	{
		return 0;
	}
	if((rt_uint8_t *)0 == psrc)
	{
		return 0;
	}
	if(0 == src_len)
	{
		return 0;
	}

	if(src_len % 2)
	{
		pdst[dst_len++] = 0xA0 + (psrc[i++] - '0');
	}
	while(i < src_len)
	{
		pdst[dst_len]	= psrc[i++] - '0';
		pdst[dst_len]	<<= 4;
		pdst[dst_len++]	+= psrc[i++] - '0';
	}

	return dst_len;
}

rt_uint8_t fyyp_char_to_hex(rt_uint8_t c)
{
	rt_uint8_t hex = 0;

	if((c >= '0') && (c <= '9'))
	{
		hex = c - '0';
	}
	else if((c >= 'A') && (c <= 'F'))
	{
		hex = c - 'A' + 10;
	}

	return hex;
}

rt_uint16_t fyyp_str_to_array(rt_uint8_t *pdst, rt_uint8_t const *psrc, rt_uint16_t src_len)
{
	rt_uint16_t dst_len = 0, i = 0;
	
	if((rt_uint8_t *)0 == pdst)
	{
		return 0;
	}

	if((rt_uint8_t *)0 == psrc)
	{
		return 0;
	}

	if((0 == src_len) || (src_len % 2))
	{
		return 0;
	}

	while(i < src_len)
	{
		pdst[dst_len]	= fyyp_char_to_hex(psrc[i++]);
		pdst[dst_len]	<<= 4;
		pdst[dst_len++]	+= fyyp_char_to_hex(psrc[i++]);
	}

	return dst_len;
}

rt_uint8_t fyyp_str_to_float(float *pfloat, rt_uint8_t const *pdata, rt_uint8_t data_len)
{
	rt_uint8_t i = 0, sign, pos;

	if((float *)0 == pfloat)
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

	if('-' == pdata[i])
	{
		sign = RT_TRUE;
		i++;
	}
	else
	{
		sign = RT_FALSE;
	}

	for(pos = i; pos < data_len; pos++)
	{
		if('.' == pdata[pos])
		{
			break;
		}
	}
	if(RT_FALSE == fyyp_is_number(pdata + i, pos - i))
	{
		return RT_FALSE;
	}
	if(pos < data_len)
	{
		if(RT_FALSE == fyyp_is_number(pdata + pos + 1, data_len - pos - 1))
		{
			return RT_FALSE;
		}
		data_len -= (pos + 1);
		*pfloat = (float)fyyp_str_to_hex(pdata + pos + 1, data_len);
		while(data_len)
		{
			data_len--;
			*pfloat /= (float)10;
		}
	}
	else
	{
		*pfloat = (float)0;
	}

	*pfloat += (float)fyyp_str_to_hex(pdata + i, pos - i);
	if(RT_TRUE == sign)
	{
		*pfloat = (float)0 - *pfloat;
	}

	return RT_TRUE;
}

rt_uint8_t *fyyp_mem_find(rt_uint8_t const *pmem1, rt_uint16_t len1, rt_uint8_t const *pmem2, rt_uint16_t len2)
{
	if(0 == len2)
	{
		return (rt_uint8_t *)0;
	}

	while(len1 >= len2)
	{
		if(!memcmp((void *)pmem1, (void *)pmem2, len2))
		{
			return (rt_uint8_t *)pmem1;
		}
		len1--;
		pmem1++;
	}

	return (rt_uint8_t *)0;
}

rt_uint8_t fyyp_str_to_int(rt_int32_t *pval, rt_uint8_t const *pdata, rt_uint8_t data_len)
{
	rt_uint8_t i = 0, sign;

	if((rt_int32_t *)0 == pval)
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

	if('-' == pdata[i])
	{
		sign = RT_TRUE;
		i++;
	}
	else
	{
		sign = RT_FALSE;
	}

	if(RT_FALSE == fyyp_is_number(pdata + i, data_len - i))
	{
		return RT_FALSE;
	}

	*pval = fyyp_str_to_hex(pdata + i, data_len - i);
	if(RT_TRUE == sign)
	{
		*pval = 0 - *pval;
	}

	return RT_TRUE;
}

