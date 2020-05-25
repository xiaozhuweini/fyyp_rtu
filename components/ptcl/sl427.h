/*占用的存储空间
**EEPROM		:1200--1213
*/



#ifndef __SL427_H__
#define __SL427_H__



#include "rtthread.h"
#include "ptcl.h"
#include "user_priority.h"



//基地址
#define SL427_PARAM_BASE_ADDR			1200

//参数地址(14字节)
#define SL427_POS_RTU_ADDR				(SL427_PARAM_BASE_ADDR + 0)		//11字节，设备地址
#define SL427_POS_REPORT_PERIOD			(SL427_PARAM_BASE_ADDR + 11)	// 2字节，上报间隔
#define SL427_POS_SMS_WARN_RECEIPT		(SL427_PARAM_BASE_ADDR + 13)	// 1字节，短信预警回执

//配置信息
#define SL427_BYTES_SMS_FRAME_HEAD		6			//sms指令帧头最大长度
#define SL427_BYTES_GPRS_FRAME_HEAD		18			//gprs指令帧头长度
#define SL427_BYTES_GPRS_FRAME_END		2			//gprs指令帧尾长度
#define SL427_FRAME_HEAD_1				0x68
#define SL427_FRAME_HEAD_2				0
#define SL427_BYTES_RTU_ADDR			11
#define SL427_BYTES_MAIN_SOFT_EDITION	16
#define SL427_BYTES_PWR_SOFT_EDITION	16
#define SL427_POS_CSQ_IN_REPORT			69
#define SL427_BYTES_WARN_INFO_MAX		500
#define SL427_BYTES_REPORT_DATA			100

//任务
#define SL427_STK_REPORT				640
#ifndef SL427_PRIO_REPORT
#define SL427_PRIO_REPORT				26
#endif

//sms指令
#define SL427_CMD_BJ					0			//发送告警
#define SL427_CMD_JGLH					1			//加管理号
#define SL427_CMD_JSQH					2			//加授权号
#define SL427_CMD_SGLH					3			//删除管理号
#define SL427_CMD_SSQH					4			//删除授权号
#define SL427_CMD_SZDZ					5			//设置设备地址
#define SL427_CMD_SZSB					6			//设置上报间隔
#define SL427_CMD_SZHZ					7			//设置回执
#define SL427_CMD_SZCS					8			//设置告警次数
#define SL427_CMD_SZPL					9			//设置fm频率
#define SL427_CMD_SZBF					10			//设置告警优先级
#define SL427_CMD_SZIP					11			//设置ip
#define SL427_CMD_SZJJ					12			//设置紧急告警内容
#define SL427_CMD_CGLH					13			//查询管理号
#define SL427_CMD_CSQH					14			//查询授权号
#define SL427_CMD_CXDZ					15			//查询设备地址
#define SL427_CMD_CXSB					16			//查询上报间隔
#define SL427_CMD_CXPL					17			//查询fm频率
#define SL427_CMD_CXBF					18			//查询告警优先级
#define SL427_CMD_CXIP					19			//查询ip
#define SL427_CMD_CXCS					20			//告警次数
#define SL427_NUM_CMD					21

//参数
#define SL427_PARAM_RTU_ADDR			0x01
#define SL427_PARAM_REPORT_PERIOD		0x02
#define SL427_PARAM_SMS_WARN_RECEIPT	0x04
#define SL427_PARAM_ALL					0x07

//功能码
#define SL427_AFN_QUERY_SUPER_PHONE		0x01		//查询全部电话号码
#define SL427_AFN_QUERY_SYS_PARAM		0x02		//查询系统参数
#define SL427_AFN_REQUEST_WARN			0x03		//请求报警
#define SL427_AFN_ADD_SUPER_PHONE		0x04		//增加号码
#define SL427_AFN_DEL_SUPER_PHONE		0x05		//删除号码
#define SL427_AFN_MODIFY_SUPER_PHONE	0x06		//修改号码
#define SL427_AFN_SET_SYS_PARAM			0x07		//设置系统参数
#define SL427_AFN_QUERY_TIME			0x08		//查询时间
#define SL427_AFN_SET_TIME				0x09		//设置时间
#define SL427_AFN_SET_PRESET_WARN_INFO	0x0a		//设置预置告警信息
#define SL427_AFN_DIAL_PHONE			0x11		//拨打电话
#define SL427_AFN_HEART					0x1b		//心跳
#define SL427_AFN_AUTO_REPORT			0x1c		//自报
#define SL427_AFN_QUERY_SCREEN_TXT		0x2a		//查询显示屏文本
#define SL427_AFN_REQUEST_WARN_EX		0x13
#define SL427_AFN_REQUEST_AD			0x53		//发送通知
#define SL427_AFN_REQUEST_WEATHER		0x54		//发送天气信息

//报警文本编码
#define SL427_WARN_DATA_TYPE_ASCII		4
#define SL427_WARN_DATA_TYPE_UNICODE	8

//错误码
#define SL427_ERR_AFN					3
#define SL427_ERR_DATA_VALUE			4
#define SL427_ERR_DATA_LEN				5
#define SL427_ERR_DEVICE				6
#define SL427_ERR_ID					7
#define SL427_ERR_WARN_DATA_TYPE		8
#define SL427_ERR_WARN_TOO_LONG			9
#define SL427_ERR_NONE					15

//供电方式
#define SL427_PWR_TYPE_BATT				1
#define SL427_PWR_TYPE_ACDC				2

//报警信息
typedef struct sl427_warn_info
{
	rt_uint8_t			packet_total;
	rt_uint8_t			packet_cur;
	rt_uint8_t			data_type;
	rt_uint8_t			warn_times;
	rt_uint16_t			data_len;
	rt_uint8_t			*pdata;
} SL427_WARN_INFO;



//参数设置读取
void sl427_get_rtu_addr(rt_uint8_t *rtu_addr_ptr);
void sl427_set_rtu_addr(rt_uint8_t const *rtu_addr_ptr, rt_uint8_t addr_len);

rt_uint16_t sl427_get_report_period(void);
void sl427_set_report_period(rt_uint16_t period);

rt_uint8_t sl427_get_sms_warn_receipt(void);
void sl427_set_sms_warn_receipt(rt_uint8_t en);

void sl427_param_restore(void);

//心跳
rt_uint16_t sl427_heart_encoder(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t ch);

//sms协议解码
rt_uint8_t sl427_sms_data_decoder(PTCL_REPORT_DATA *report_data_ptr, PTCL_RECV_DATA const *recv_data_ptr, PTCL_PARAM_INFO **param_info_ptr_ptr);
//gprs协议解码
rt_uint8_t sl427_gprs_data_decoder(PTCL_REPORT_DATA *report_data_ptr, PTCL_RECV_DATA const *recv_data_ptr, PTCL_PARAM_INFO **param_info_ptr_ptr);

//gprs协议解码之lora信道
rt_uint8_t sl427_gprs_data_decoder_lora(PTCL_REPORT_DATA *report_data_ptr, PTCL_RECV_DATA const *recv_data_ptr);



#endif

