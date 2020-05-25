#ifndef __USER_PRIORITY_H__
#define __USER_PRIORITY_H__



//usb
#define USBD_PRIO_STA_UPDATE			5

//dtu
#define DTU_PRIO_TIME_TICK				30
#define DTU_PRIO_TELEPHONE_HANDLER		2
#define DTU_PRIO_RECV_BUF_HANDLER		2
#define DTU_PRIO_DIAL_PHONE				4
#define DTU_PRIO_AT_DATA_HANDLER		3

//ptcl
#define PTCL_PRIO_AUTO_REPORT_MANAGE	28
#define PTCL_PRIO_AUTO_REPORT			29
#define PTCL_PRIO_DTU_RECV_HANDLER		3
#define PTCL_PRIO_TC_RECV_HANDLER		3
#define PTCL_PRIO_TC_BUF_HANDLER		2
#define PTCL_PRIO_SHELL_RECV_HANDLER	3
#define PTCL_PRIO_BT_RECV_HANDLER		4

//xmodem
#define XM_PRIO_FILE_TRANS				5

//smt_manage
#define SMTMNG_PRIO_TIME_TICK			27

//http
#define HTTP_PRIO_GET_REQ				4

//sample
#define SAMPLE_PRIO_DATA_STORE			30
#define SAMPLE_PRIO_QUERY_DATA_EX		24

//bluetooth
#define BT_PRIO_INT_HDR					2
#define BT_PRIO_RECV_HDR				3

//hj212
#define HJ212_PRIO_REPORT_RTD			25
#define HJ212_PRIO_REPORT_MIN			26
#define HJ212_PRIO_TIME_CALI_REQ		26
#define HJ212_PRIO_DATA_HDR				6
#define HJ212_PRIO_TIME_TICK			5

//equip
#define EQUIP_PRIO_TIME_TICK			27

#if 0
//warn_manage
#define WM_PRIO_WARN_BASE				10
#define WM_PRIO_TIME_TICK				25
#define WM_PRIO_KEY_DETECT				2

//sl427
#define SL427_PRIO_REPORT				26

//pwr_manage
#define PWRM_PRIO_RECV_BUF_HANDLER		2

//lora
#define LORA_PRIO_RECV_BUF_HANDLER		2

//bx5k
#define BX5K_PRIO_SCREEN_UPDATE			30

//±±¶·
#define BD_PRIO_TIME_TICK				27
#define BD_PRIO_RECV_BUF_HANDLER		2
#define BD_PRIO_BUF_DATA_HANDLER		5
#define BD_PRIO_SEND_TEST				21
#endif



#endif

