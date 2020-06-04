#ifndef __DRV_TEVENT_H__
#define __DRV_TEVENT_H__



#include "rtthread.h"



#define TEVENT_EVENT_SEC		0x01
#define TEVENT_EVENT_MIN		0x02
#define TEVENT_EVENT_HOUR		0x04



void tevent_pend(rt_uint32_t tevent);
void tevent_post(rt_uint32_t tevent);



#endif
