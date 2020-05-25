#ifndef __MYSELF_H__
#define __MYSELF_H__



#include "rtthread.h"
#include "ptcl.h"



//帧结构
#define MY_BYTES_FRAME_HEAD			3
#define MY_BYTES_FRAME_END			4
#define MY_FRAME_HEAD_1				'S'
#define MY_FRAME_HEAD_2				'M'
#define MY_FRAME_SEPARATOR			' '
#define MY_FRAME_SUPER_CRC			'i'
#define MY_FRAME_END				'N'

//参数类型
#define MY_PARAM_TYPE_NONE			0
#define MY_PARAM_TYPE_RESET			1				//"RESET "
#define MY_PARAM_TYPE_RESTORE		2				//"RESTORE "
#define MY_PARAM_TYPE_TT			3				//"TT "
#define MY_PARAM_TYPE_VER			4				//"VER "
#define MY_PARAM_TYPE_CA1			5				//"CA1 "
#define MY_PARAM_TYPE_CA2			6				//"CA2 "
#define MY_PARAM_TYPE_CA3			7				//"CA3 "
#define MY_PARAM_TYPE_CA4			8				//"CA4 "
#define MY_PARAM_TYPE_C1P1			9				//"C1P1 "
#define MY_PARAM_TYPE_C1P2			10				//"C1P2 "
#define MY_PARAM_TYPE_C2P1			11				//"C2P1 "
#define MY_PARAM_TYPE_C2P2			12				//"C2P2 "
#define MY_PARAM_TYPE_C3P1			13				//"C3P1 "
#define MY_PARAM_TYPE_C3P2			14				//"C3P2 "
#define MY_PARAM_TYPE_C4P1			15				//"C4P1 "
#define MY_PARAM_TYPE_C4P2			16				//"C4P2 "
#define MY_PARAM_TYPE_PTCL1			17				//"PTCL1 "
#define MY_PARAM_TYPE_PTCL2			18				//"PTCL2 "
#define MY_PARAM_TYPE_PTCL3			19				//"PTCL3 "
#define MY_PARAM_TYPE_PTCL4			20				//"PTCL4 "
#define MY_PARAM_TYPE_IPADDR1		21				//"IPADDR1 "
#define MY_PARAM_TYPE_IPADDR2		22				//"IPADDR2 "
#define MY_PARAM_TYPE_IPADDR3		23				//"IPADDR3 "
#define MY_PARAM_TYPE_IPADDR4		24				//"IPADDR4 "
#define MY_PARAM_TYPE_IPADDR5		25				//"IPADDR5 "
#define MY_PARAM_TYPE_SMSADDR1		26				//"SMSADDR1 "
#define MY_PARAM_TYPE_SMSADDR2		27				//"SMSADDR2 "
#define MY_PARAM_TYPE_SMSADDR3		28				//"SMSADDR3 "
#define MY_PARAM_TYPE_SMSADDR4		29				//"SMSADDR4 "
#define MY_PARAM_TYPE_SMSADDR5		30				//"SMSADDR5 "
#define MY_PARAM_TYPE_IPTYPE1		31				//"IPTYPE1 "
#define MY_PARAM_TYPE_IPTYPE2		32				//"IPTYPE2 "
#define MY_PARAM_TYPE_IPTYPE3		33				//"IPTYPE3 "
#define MY_PARAM_TYPE_IPTYPE4		34				//"IPTYPE4 "
#define MY_PARAM_TYPE_IPTYPE5		35				//"IPTYPE5 "
#define MY_PARAM_TYPE_HEART1		36				//"HEART1 "
#define MY_PARAM_TYPE_HEART2		37				//"HEART2 "
#define MY_PARAM_TYPE_HEART3		38				//"HEART3 "
#define MY_PARAM_TYPE_HEART4		39				//"HEART4 "
#define MY_PARAM_TYPE_HEART5		40				//"HEART5 "
#define MY_PARAM_TYPE_DTUMODE1		41				//"DTUMODE1 "
#define MY_PARAM_TYPE_DTUMODE2		42				//"DTUMODE2 "
#define MY_PARAM_TYPE_DTUMODE3		43				//"DTUMODE3 "
#define MY_PARAM_TYPE_DTUMODE4		44				//"DTUMODE4 "
#define MY_PARAM_TYPE_DTUMODE5		45				//"DTUMODE5 "
#define MY_PARAM_TYPE_IPCH1			46				//"IPCH1 "
#define MY_PARAM_TYPE_IPCH2			47				//"IPCH2 "
#define MY_PARAM_TYPE_IPCH3			48				//"IPCH3 "
#define MY_PARAM_TYPE_IPCH4			49				//"IPCH4 "
#define MY_PARAM_TYPE_IPCH5			50				//"IPCH5 "
#define MY_PARAM_TYPE_SMSCH1		51				//"SMSCH1 "
#define MY_PARAM_TYPE_SMSCH2		52				//"SMSCH2 "
#define MY_PARAM_TYPE_SMSCH3		53				//"SMSCH3 "
#define MY_PARAM_TYPE_SMSCH4		54				//"SMSCH4 "
#define MY_PARAM_TYPE_SMSCH5		55				//"SMSCH5 "
#define MY_PARAM_TYPE_DTUAPN		56				//"DTUAPN "
#define MY_PARAM_TYPE_FIRMWARE		57				//"FIRMWARE "
#define MY_NUM_PARAM_TYPE			58

//常用字符串
#define MY_STR_QUERY				"? "
#define MY_STR_COMM_NONE			"NONE "
#define MY_STR_COMM_SMS				"SMS "
#define MY_STR_COMM_GPRS			"GPRS "
#define MY_STR_COMM_TC				"TC "
#define MY_STR_ENABLE				"EN "
#define MY_STR_DISABLE				"DIS "
#define MY_STR_TCP					"TCP "
#define MY_STR_UDP					"UDP "
#define MY_STR_DTUMODE_PWR			"PWR "
#define MY_STR_DTUMODE_ONLINE		"ONLINE "
#define MY_STR_DTUMODE_OFFLINE		"OFFLINE "
#define MY_STR_SL427				"SL427 "
#define MY_STR_SL651				"SL651 "
#define MY_STR_HJ212				"HJ212 "



//数据解包
rt_uint8_t my_data_decoder(PTCL_REPORT_DATA *report_data_ptr, PTCL_RECV_DATA const *recv_data_ptr, PTCL_PARAM_INFO **param_info_ptr_ptr);



#endif

