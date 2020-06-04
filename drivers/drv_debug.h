#ifndef __DRV_DEBUG_H__
#define __DRV_DEBUG_H__



#include "rtthread.h"



#define DEBUG_BYTES_INFO_BUF			128

#define DEBUG_INFO_TYPE_DTU				0
#define DEBUG_INFO_TYPE_AT_DATA			1
#define DEBUG_INFO_TYPE_SAMPLE			2
#define DEBUG_INFO_TYPE_HTTP			3
#define DEBUG_INFO_TYPE_FATFS			4
#define DEBUG_INFO_TYPE_BT				5
#define DEBUG_INFO_TYPE_DS18B20			6
#define DEBUG_INFO_TYPE_HJ212			7
#define DEBUG_INFO_TYPE_TEST			8
#define DEBUG_NUM_INFO_TYPE				31

#define DEBUG_EVENT_INFO_TYPE_ALL		0x7fffffff
#define DEBUG_EVENT_INFO_BUF_PEND		0x80000000

#define DEBUG_INFO_OUTPUT_STR(info_type, info)                                \
do                                                                            \
{                                                                             \
    if (RT_TRUE == debug_get_info_type_sta(info_type))                        \
        debug_info_output_str info;                                           \
}                                                                             \
while (0)

#define DEBUG_INFO_OUTPUT(info_type, info)                                    \
do                                                                            \
{                                                                             \
    if (RT_TRUE == debug_get_info_type_sta(info_type))                        \
        debug_info_output info;                                               \
}                                                                             \
while (0)

#define DEBUG_INFO_OUTPUT_HEX(info_type, info)                                \
do                                                                            \
{                                                                             \
    if (RT_TRUE == debug_get_info_type_sta(info_type))                        \
        debug_info_output_hex info;                                           \
}                                                                             \
while (0)



rt_uint8_t debug_get_info_type_sta(rt_uint8_t info_type);
void debug_set_info_type_sta(rt_uint8_t info_type, rt_uint8_t sta);

void debug_close_all(void);

void debug_info_output_str(const char *fmt, ...);

void debug_info_output(rt_uint8_t const *pdata, rt_uint16_t data_len);

void debug_info_output_hex(rt_uint8_t const *pdata, rt_uint16_t data_len);



#endif

