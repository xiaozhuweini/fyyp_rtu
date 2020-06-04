/*占用的存储空间
EEPROM			dtu			(0--1199)
				ptcl		(1200--1299)
				smt_manage	(1300--1399)
				sample		(1400--1999)
				drv_ads		(2000--2049)
				hj212		(2050--2299)

FLASH			固件			(0--1048575)1MB
				ptcl		(1048576--1150975)100KB
				sample		(1150976--11636735)10MB (11636736--22122495)10MB
*/



#include "rtthread.h"
#include "drv_lpm.h"
#include "drv_rtcee.h"
#include "drv_tevent.h"



int main(void)
{
	rt_uint16_t tick_sec = 0;

	lpm_cpu_ref(RT_FALSE);
	while(1)
	{
		rtcee_rtc_sec_pend();

		if(++tick_sec >= 3600)
		{
			tick_sec = 0;
		}

		//1秒钟
		tevent_post(TEVENT_EVENT_SEC);
		//1分钟
		if(1 == tick_sec % 60)
		{
			tevent_post(TEVENT_EVENT_MIN);
		}
		//1小时
		if(2 == tick_sec)
		{
			tevent_post(TEVENT_EVENT_HOUR);
		}
	}
}

