#include "drv_debug.h"
#include "usbd_cdc_interface.h"
#include "drv_mempool.h"



static struct rt_event m_debug_event_module;



static void _debug_info_buf_pend(void)
{
	if(RT_EOK != rt_event_recv(&m_debug_event_module, DEBUG_EVENT_INFO_BUF_PEND, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}
}

static void _debug_info_buf_post(void)
{
	if(RT_EOK != rt_event_send(&m_debug_event_module, DEBUG_EVENT_INFO_BUF_PEND))
	{
		while(1);
	}
}

static int _debug_device_init(void)
{
	if(RT_EOK != rt_event_init(&m_debug_event_module, "debug", RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	if(RT_EOK != rt_event_send(&m_debug_event_module, DEBUG_EVENT_INFO_BUF_PEND))
	{
		while(1);
	}

	return 0;
}
INIT_DEVICE_EXPORT(_debug_device_init);



rt_uint8_t debug_get_info_type_sta(rt_uint8_t info_type)
{
	if(info_type >= DEBUG_NUM_INFO_TYPE)
	{
		return RT_FALSE;
	}

	if(RT_EOK == rt_event_recv(&m_debug_event_module, 1 << info_type, RT_EVENT_FLAG_AND, RT_WAITING_NO, (rt_uint32_t *)0))
	{
		return RT_TRUE;
	}
	else
	{
		return RT_FALSE;
	}
}

void debug_set_info_type_sta(rt_uint8_t info_type, rt_uint8_t sta)
{
	if(info_type >= DEBUG_NUM_INFO_TYPE)
	{
		return;
	}

	if(RT_TRUE == sta)
	{
		rt_event_send(&m_debug_event_module, 1 << info_type);
	}
	else
	{
		rt_event_recv(&m_debug_event_module, 1 << info_type, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, RT_WAITING_NO, (rt_uint32_t *)0);
	}
}

void debug_close_all(void)
{
	rt_event_recv(&m_debug_event_module, DEBUG_EVENT_INFO_TYPE_ALL, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, RT_WAITING_NO, (rt_uint32_t *)0);
}

void debug_info_output_str(const char *fmt, ...)
{
    va_list			args;
    rt_size_t		length;
    static char		rt_log_buf[DEBUG_BYTES_INFO_BUF];

    va_start(args, fmt);
    _debug_info_buf_pend();
    /* the return value of vsnprintf is the number of bytes that would be
     * written to buffer had if the size of the buffer been sufficiently
     * large excluding the terminating null byte. If the output string
     * would be larger than the rt_log_buf, we have to adjust the output
     * length. */
    length = rt_vsnprintf(rt_log_buf, sizeof(rt_log_buf) - 1, fmt, args);
    if (length > DEBUG_BYTES_INFO_BUF - 1)
    {
        length = DEBUG_BYTES_INFO_BUF - 1;
    }
    usbd_send((rt_uint8_t *)rt_log_buf, length);
    _debug_info_buf_post();
    va_end(args);
}

void debug_info_output(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	if((rt_uint8_t *)0 == pdata)
	{
		return;
	}
	if(0 == data_len)
	{
		return;
	}
	
	usbd_send(pdata, data_len);
}

void debug_info_output_hex(rt_uint8_t const *pdata, rt_uint16_t data_len)
{
	rt_uint8_t	*pmem;
	rt_uint16_t	mem_len;
	
	if((rt_uint8_t *)0 == pdata)
	{
		return;
	}
	if(0 == data_len)
	{
		return;
	}

	pmem = (rt_uint8_t *)mempool_req(data_len * 2 + 1, RT_WAITING_NO);
	if((rt_uint8_t *)0 == pmem)
	{
		while(data_len)
		{
			data_len--;
			debug_info_output_str("%02X", *pdata++);
		}
	}
	else
	{
		mem_len = 0;
		while(data_len)
		{
			data_len--;
			mem_len += rt_sprintf((char *)(pmem + mem_len), "%02X", *pdata++);
		}
		usbd_send(pmem, mem_len);
		
		rt_mp_free((void *)pmem);
	}
}

