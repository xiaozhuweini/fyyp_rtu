#ifndef __DRV_FYYP_H__
#define __DRV_FYYP_H__



#include "rtthread.h"



#define FYYP_MEMBER_ADDR_OFFSET(type, member)			((unsigned long)(&((type *)0)->member))



rt_uint8_t fyyp_hex_to_bcd(rt_uint8_t hex);
rt_uint8_t fyyp_bcd_to_hex(rt_uint8_t bcd);
rt_uint8_t fyyp_is_bcdcode(rt_uint8_t const *pdata, rt_uint16_t data_len);
rt_uint32_t fyyp_str_to_hex(rt_uint8_t const *pdata, rt_uint8_t data_len);
rt_uint8_t fyyp_is_number(rt_uint8_t const *pdata, rt_uint16_t data_len);
rt_uint8_t fyyp_ip_str_to_bcd(rt_uint8_t *pdst, rt_uint8_t const *psrc, rt_uint8_t src_len);
rt_uint8_t fyyp_sms_str_to_bcd(rt_uint8_t *pdst, rt_uint8_t const *psrc, rt_uint8_t src_len);
rt_uint8_t fyyp_char_to_hex(rt_uint8_t c);
rt_uint16_t fyyp_str_to_array(rt_uint8_t *pdst, rt_uint8_t const *psrc, rt_uint16_t src_len);
rt_uint8_t fyyp_str_to_float(float *pfloat, rt_uint8_t const *pdata, rt_uint8_t data_len);
rt_uint8_t *fyyp_mem_find(rt_uint8_t const *pmem1, rt_uint16_t len1, rt_uint8_t const *pmem2, rt_uint16_t len2);
rt_uint8_t fyyp_str_to_int(rt_int32_t *pval, rt_uint8_t const *pdata, rt_uint8_t data_len);



#endif

