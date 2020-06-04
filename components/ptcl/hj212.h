/*占用的存储空间
**EEPROM		:2050--2299
*/



#ifndef __HJ212_H__
#define __HJ212_H__



#include "rtthread.h"
#include "user_priority.h"
#include "ptcl.h"



#define HJ212_BASE_ADDR_PARAM			2050

//帧格式
#define HJ212_FRAME_BAOTOU				"##"
#define HJ212_FRAME_QN					"QN="
#define HJ212_FRAME_ST					"ST="
#define HJ212_FRAME_CN					"CN="
#define HJ212_FRAME_PW					"PW="
#define HJ212_FRAME_MN					"MN="
#define HJ212_FRAME_FLAG				"Flag="
#define HJ212_FRAME_PNUM				"PNUM="
#define HJ212_FRAME_PNO					"PNO="
#define HJ212_FRAME_CP_START			"CP=&&"
#define HJ212_FRAME_CP_END				"&&"
#define HJ212_FRAME_BAOWEI				"\r\n"
#define HJ212_BYTES_FRAME_BAOTOU		2
#define HJ212_BYTES_FRAME_LEN			4
#define HJ212_BYTES_FRAME_CRC			4
#define HJ212_BYTES_FRAME_BAOWEI		2
#define HJ212_BYTES_FRAME_QN			20
#define HJ212_BYTES_FRAME_ST			5
#define HJ212_BYTES_FRAME_CN			7
#define HJ212_BYTES_FRAME_PW			9
#define HJ212_BYTES_FRAME_MN			27
#define HJ212_BYTES_QN					17
#define HJ212_BYTES_ST					2
#define HJ212_BYTES_CN					4
#define HJ212_BYTES_PW					6
#define HJ212_BYTES_MN					24

//拆分包及应答标志
#define HJ212_FLAG_PTCL_VER				0x04
#define HJ212_FLAG_PACKET_DIV_EN		0x02
#define HJ212_FLAG_ACK_EN				0x01

//相关配置信息
#define HJ212_ST_SYS_COMM				"91"
#define HJ212_BYTES_TIME_CALI_REQ		150
#define HJ212_BYTES_DATA_RTD			230
#define HJ212_BYTES_DATA_MIN			460
#define HJ212_REPORT_REPLY_EN			RT_FALSE
#define HJ212_FILE_NAME_MIN_DATA		"min.txt"
#define HJ212_FILE_NAME_HOUR_DATA		"hour.txt"
#define HJ212_FILE_NAME_DAY_DATA		"day.txt"
#define HJ212_FILE_NAME_DAY_RS_DATA		"day_rs.txt"

//字段名
#define HJ212_FIELD_NAME_SYSTIME		"SystemTime="
#define HJ212_FIELD_NAME_QNRTN			"QnRtn="
#define HJ212_FIELD_NAME_EXERTN			"ExeRtn="
#define HJ212_FIELD_NAME_RTDINTERVAL	"RtdInterval="
#define HJ212_FIELD_NAME_MININTERVAL	"MinInterval="
#define HJ212_FIELD_NAME_RESTARTTIME	"RestartTime="
#define HJ212_FIELD_NAME_DATA_RTD		"-Rtd="
#define HJ212_FIELD_NAME_DATA_MIN		"-Min="
#define HJ212_FIELD_NAME_DATA_MAX		"-Max="
#define HJ212_FIELD_NAME_DATA_AVG		"-Avg="
#define HJ212_FIELD_NAME_DATA_COU		"-Cou="
#define HJ212_FIELD_NAME_DATA_FLAG		"-Flag="
#define HJ212_FIELD_NAME_BEGIN_TIME		"BeginTime="
#define HJ212_FIELD_NAME_END_TIME		"EndTime="
#define HJ212_FIELD_NAME_DATA_TIME		"DataTime="
#define HJ212_FIELD_NAME_NEW_PW			"NewPW="
#define HJ212_FIELD_NAME_OVER_TIME		"OverTime="
#define HJ212_FIELD_NAME_RECOUNT		"ReCount="
#define HJ212_FIELD_NAME_RUN_STA		"SB%d-RS=%c;"
#define HJ212_FIELD_NAME_RUN_TIME		"SB%d-RT=%.1f;"

//exertn
#define HJ212_EXERTN_OK					1
#define HJ212_EXERTN_ERR_NO_REASON		2
#define HJ212_EXERTN_ERR_CMD			3
#define HJ212_EXERTN_ERR_COMM			4
#define HJ212_EXERTN_ERR_BUSY			5
#define HJ212_EXERTN_ERR_SYSTEM			6
#define HJ212_EXERTN_ERR_NO_DATA		100

//qnrtn
#define HJ212_QNRTN_EXCUTE				1
#define HJ212_QNRTN_REJECTED			2
#define HJ212_QNRTN_ERR_PW				3
#define HJ212_QNRTN_ERR_MN				4
#define HJ212_QNRTN_ERR_ST				5
#define HJ212_QNRTN_ERR_FLAG			6
#define HJ212_QNRTN_ERR_QN				7
#define HJ212_QNRTN_ERR_CN				8
#define HJ212_QNRTN_ERR_CRC				9
#define HJ212_QNRTN_ERR_UNKNOWN			100

//数据标记
#define HJ212_DATA_FLAG_RUN				'N'
#define HJ212_DATA_FLAG_STOP			'F'
#define HJ212_DATA_FLAG_MAINTAIN		'M'
#define HJ212_DATA_FLAG_MANUAL			'S'
#define HJ212_DATA_FLAG_ERR				'D'
#define HJ212_DATA_FLAG_CALIBRATE		'C'
#define HJ212_DATA_FLAG_LIMIT			'T'
#define HJ212_DATA_FLAG_COMM			'B'

//命令编码
#define HJ212_CN_SET_OT_RECOUNT			1000
#define HJ212_CN_GET_SYS_TIME			1011
#define HJ212_CN_SET_SYS_TIME			1012
#define HJ212_CN_REQ_SYS_TIME			1013
#define HJ212_CN_GET_RTD_INTERVAL		1061
#define HJ212_CN_SET_RTD_INTERVAL		1062
#define HJ212_CN_GET_MIN_INTERVAL		1063
#define HJ212_CN_SET_MIN_INTERVAL		1064
#define HJ212_CN_SET_NEW_PW				1072
#define HJ212_CN_RTD_DATA				2011
#define HJ212_CN_STOP_RTD_DATA			2012
#define HJ212_CN_RS_DATA				2021
#define HJ212_CN_STOP_RS_DATA			2022
#define HJ212_CN_DAY_DATA				2031
#define HJ212_CN_DAY_RS_DATA			2041
#define HJ212_CN_MIN_DATA				2051
#define HJ212_CN_HOUR_DATA				2061
#define HJ212_CN_RESTART_TIME			2081
#define HJ212_CN_GET_QNRTN				9011
#define HJ212_CN_GET_EXERTN				9012
#define HJ212_CN_GET_INFORM_RTN			9013
#define HJ212_CN_GET_DATA_RTN			9014

//监测因子编码
#define HJ212_FACTOR_WUSHUI				"w00000"
#define HJ212_FACTOR_PH					"w01001"
#define HJ212_FACTOR_RONGJIEYANG		"w01009"
#define HJ212_FACTOR_SHUIWEN			"w01010"
#define HJ212_FACTOR_DIANDAOLV			"w01014"
#define HJ212_FACTOR_CO2				"a05001"
#define HJ212_FACTOR_CH4				"a05002"
#define HJ212_FACTOR_O2					"a19001"
#define HJ212_FACTOR_VOCS				"avoc01"
#define HJ212_FACTOR_COD				"w01018"

//任务
#define HJ212_STK_REPORT_RTD			640
#define HJ212_STK_REPORT_MIN			720
#define HJ212_STK_TIME_CALI_REQ			640
#define HJ212_STK_DATA_HDR				720
#define HJ212_STK_TIME_TICK				512
#ifndef HJ212_PRIO_REPORT_RTD
#define HJ212_PRIO_REPORT_RTD			25
#endif
#ifndef HJ212_PRIO_REPORT_MIN
#define HJ212_PRIO_REPORT_MIN			26
#endif
#ifndef HJ212_PRIO_TIME_CALI_REQ
#define HJ212_PRIO_TIME_CALI_REQ		26
#endif
#ifndef HJ212_PRIO_DATA_HDR
#define HJ212_PRIO_DATA_HDR				6
#endif
#ifndef HJ212_PRIO_TIME_TICK
#define HJ212_PRIO_TIME_TICK			5
#endif

//参数默认值
#define HJ212_DEF_VAL_ST				"21"
#define HJ212_DEF_VAL_PW				"123456"
#define HJ212_DEF_VAL_MN				"00112233445566778899AABB"
#define HJ212_DEF_VAL_OT				15
#define HJ212_DEF_VAL_RECOUNT			3
#define HJ212_DEF_VAL_RTD_INTERVAL		300
#define HJ212_DEF_VAL_MIN_INTERVAL		10
#define HJ212_DEF_CRC_VAL				0xa55a

//事件
#define HJ212_EVENT_PARAM_ST			0x01
#define HJ212_EVENT_PARAM_PW			0x02
#define HJ212_EVENT_PARAM_MN			0x04
#define HJ212_EVENT_PARAM_OT			0x08
#define HJ212_EVENT_PARAM_RECOUNT		0x10
#define HJ212_EVENT_PARAM_RTD_EN		0x20
#define HJ212_EVENT_PARAM_RTD_INTERVAL	0x40
#define HJ212_EVENT_PARAM_MIN_INTERVAL	0x80
#define HJ212_EVENT_PARAM_RS_EN			0x100
#define HJ212_EVENT_PARAM_ALL			0x1ff
#define HJ212_EVENT_TIME_CALI_REQ		0x200
#define HJ212_EVENT_REPORT_RTD			0x400
#define HJ212_EVENT_INIT_VALUE			0x1ff

//监测因子数据小数位数
#define HJ212_BIT_FACTOR_WUSHUI			2
#define HJ212_BIT_FACTOR_COD			1

//参数集合
typedef struct hj212_param_set
{
	rt_uint8_t			st[HJ212_BYTES_ST];
	rt_uint8_t			pw[HJ212_BYTES_PW];
	rt_uint8_t			mn[HJ212_BYTES_MN];
	rt_uint16_t			over_time;
	rt_uint8_t			recount;
	rt_uint8_t			rtd_en;
	rt_uint16_t			rtd_interval;
	rt_uint16_t			min_interval;
	rt_uint8_t			rs_en;
} HJ212_PARAM_SET;



void hj212_get_st(rt_uint8_t *pdata);
void hj212_set_st(rt_uint8_t const *pdata);
void hj212_get_pw(rt_uint8_t *pdata);
void hj212_set_pw(rt_uint8_t const *pdata);
void hj212_get_mn(rt_uint8_t *pdata);
void hj212_set_mn(rt_uint8_t const *pdata);
rt_uint16_t hj212_get_over_time(void);
void hj212_set_over_time(rt_uint16_t over_time);
rt_uint8_t hj212_get_recount(void);
void hj212_set_recount(rt_uint8_t recount);
rt_uint8_t hj212_get_rtd_en(void);
void hj212_set_rtd_en(rt_uint8_t rtd_en);
rt_uint16_t hj212_get_rtd_interval(void);
void hj212_set_rtd_interval(rt_uint16_t rtd_interval);
rt_uint16_t hj212_get_min_interval(void);
void hj212_set_min_interval(rt_uint16_t min_interval);
rt_uint8_t hj212_get_rs_en(void);
void hj212_set_rs_en(rt_uint8_t en);
void hj212_param_restore(void);
rt_uint16_t hj212_heart_encoder(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t ch);
void hj212_time_cali_req(void);
rt_uint8_t hj212_data_decoder(PTCL_REPORT_DATA *report_data_p, PTCL_RECV_DATA const *recv_data_p, PTCL_PARAM_INFO **param_info_pp);



#endif
