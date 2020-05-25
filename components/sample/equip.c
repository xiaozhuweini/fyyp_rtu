#include "equip.h"
#include "drv_lpm.h"
#include "drv_tevent.h"



static rt_uint16_t			m_equip_pin[EQUIP_NUM_EQUIP];
static rt_uint32_t			m_equip_run_time[EQUIP_NUM_EQUIP];

static struct rt_event		m_equip_event;

static struct rt_thread		m_equip_thread_time_tick;
static rt_uint8_t			m_equip_stk_time_tick[EQUIP_STK_TIME_TICK];



static void _equip_pend(rt_uint32_t event)
{
	if(RT_EOK != rt_event_recv(&m_equip_event, event, RT_EVENT_FLAG_AND + RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)0))
	{
		while(1);
	}
}

static void _equip_post(rt_uint32_t event)
{
	if(RT_EOK != rt_event_send(&m_equip_event, event))
	{
		while(1);
	}
}

static rt_uint8_t _equip_get_run_sta(rt_uint8_t equip)
{
	rt_uint8_t sta;

	_equip_pend(EQUIP_EVENT_RUN_STA << equip);
	
	pin_clock_enable(m_equip_pin[equip], RT_TRUE);
	pin_mode(m_equip_pin[equip], PIN_MODE_I_NOPULL);
	
	sta = (PIN_STA_HIGH == pin_read(m_equip_pin[equip])) ? RT_FALSE : RT_TRUE;
	
	pin_mode(m_equip_pin[equip], PIN_MODE_ANALOG);
	pin_clock_enable(m_equip_pin[equip], RT_FALSE);
	
	_equip_post(EQUIP_EVENT_RUN_STA << equip);

	return sta;
}

static void _equip_task_time_tick(void *parg)
{
	rt_uint8_t i;

	while(1)
	{
		tevent_pend(TEVENT_EVENT_SEC);
		lpm_cpu_ref(RT_TRUE);

		for(i = 0; i < EQUIP_NUM_EQUIP; i++)
		{
			if(RT_TRUE == _equip_get_run_sta(i))
			{
				_equip_pend(EQUIP_EVENT_RUN_TIME << i);
				m_equip_run_time[i]++;
				_equip_post(EQUIP_EVENT_RUN_TIME << i);
			}
		}
		
		lpm_cpu_ref(RT_FALSE);
	}
}

static int _equip_component_init(void)
{
	rt_uint8_t i = 0;
	
	m_equip_pin[i++] = EQUIP_PIN_SB_1;
	m_equip_pin[i++] = EQUIP_PIN_SB_2;
	m_equip_pin[i++] = EQUIP_PIN_SB_3;
	m_equip_pin[i++] = EQUIP_PIN_SB_4;

	for(i = 0; i < EQUIP_NUM_EQUIP; i++)
	{
		m_equip_run_time[i] = 0;
	}

	if(RT_EOK != rt_event_init(&m_equip_event, "equip", RT_IPC_FLAG_PRIO))
	{
		while(1);
	}
	if(RT_EOK != rt_event_send(&m_equip_event, EQUIP_EVENT_INIT_VAL))
	{
		while(1);
	}
	
	return 0;
}
INIT_COMPONENT_EXPORT(_equip_component_init);

static int _equip_app_init(void)
{
	if(RT_EOK != rt_thread_init(&m_equip_thread_time_tick, "equip", _equip_task_time_tick, (void *)0, (void *)m_equip_stk_time_tick, EQUIP_STK_TIME_TICK, EQUIP_PRIO_TIME_TICK, 2))
	{
		while(1);
	}
	if(RT_EOK != rt_thread_startup(&m_equip_thread_time_tick))
	{
		while(1);
	}
	
	return 0;
}
INIT_APP_EXPORT(_equip_app_init);



rt_uint32_t equip_get_run_sta(void)
{
	rt_uint32_t	run_sta = 0;
	rt_uint8_t	i;

	for(i = 0; i < EQUIP_NUM_EQUIP; i++)
	{
		if(RT_TRUE == _equip_get_run_sta(i))
		{
			run_sta += (1 << i);
		}
	}

	return run_sta;
}

rt_uint32_t equip_get_run_time(rt_uint8_t equip, rt_uint8_t clred)
{
	rt_uint32_t run_time;
	
	if(equip >= EQUIP_NUM_EQUIP)
	{
		return 0;
	}

	_equip_pend(EQUIP_EVENT_RUN_TIME << equip);
	run_time = m_equip_run_time[equip];
	if(RT_TRUE == clred)
	{
		m_equip_run_time[equip] = 0;
	}
	_equip_post(EQUIP_EVENT_RUN_TIME << equip);

	return run_time;
}
