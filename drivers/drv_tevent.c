#include "drv_tevent.h"



static struct rt_event m_tevent_event;



static int _tevent_prev_init(void)
{
	if(RT_EOK != rt_event_init(&m_tevent_event, "tevent", RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	
	return 0;
}
INIT_PREV_EXPORT(_tevent_prev_init);



void tevent_pend(rt_uint32_t tevent)
{
	if(RT_EOK != rt_event_recv(&m_tevent_event, tevent, RT_EVENT_FLAG_AND, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}
	rt_event_recv(&m_tevent_event, tevent, RT_EVENT_FLAG_OR + RT_EVENT_FLAG_CLEAR, RT_WAITING_NO, (rt_uint32_t *)0);
}

void tevent_post(rt_uint32_t tevent)
{
	if(RT_EOK != rt_event_send(&m_tevent_event, tevent))
	{
		while(1);
	}
}

