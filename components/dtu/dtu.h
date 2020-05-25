/*占用的存储空间
**eeprom		:0--1199
*/



#ifndef __DTU_H__
#define __DTU_H__



#include "rtthread.h"
#include "user_priority.h"



//模块选择，只能选择一个
//#define DTU_MODULE_SIM800C
#define DTU_MODULE_EC20
//#define DTU_MODULE_HT7710

//sms模式
//#define DTU_SMS_MODE_TEXT
#define DTU_SMS_MODE_PDU

//参数基地址
#define DTU_BASE_ADDR_PARAM				0

//配置信息
#define DTU_NUM_RECV_DATA				8							//接收数据数量
#define DTU_NUM_IP_CH					5							//ip通道数量
#define DTU_NUM_SMS_CH					5							//sms通道数量
#define DTU_BYTES_IP_ADDR				29							//ip地址空间
#define DTU_BYTES_SMS_ADDR				19							//sms地址空间
#define DTU_BYTES_APN					14							//接入点空间
#define DTU_BYTES_HEART					100							//心跳包空间
#define DTU_BYTES_RECV_BUF				1000						//数据接收缓冲区
#define DTU_BOOT_TIMES_MAX				2							//boot次数限制
#define DTU_TIME_IP_IDLE_L				600							//下线时间长，秒
#define DTU_TIME_IP_IDLE_S				10							//下线时间短，秒
#define DTU_TIME_SMS_IDLE_L				600							//掉电时间长，秒
#define DTU_TIME_SMS_IDLE_S				10							//掉电时间短，秒
#define DTU_NUM_SUPER_PHONE				30							//权限号码数量
#define DTU_NUM_PREVILIGE_PHONE			10							//权限号码里面特权号码的数量
#define DTU_TIME_TELEPHONE_MAX			600							//最长电话时间，秒
#define DTU_TIME_TTS_MAX				15							//最长tts时间，秒
#define DTU_TIME_DIAL_PHONE				20							//拨打电话时间，秒
#define DTU_RECV_BUF_TIMEOUT			100							//接收缓冲区超时时间，毫秒
#define DTU_NUM_AT_DATA					2
#define DTU_NUM_HTTP_DATA				2							//http协议数据接收数量

//任务
#define DTU_STK_TIME_TICK				640							//计时任务栈
#define DTU_STK_TELEPHONE_HANDLER		640							//电话处理任务栈
#define DTU_STK_RECV_BUF_HANDLER		512							//接收处理任务栈
#define DTU_STK_DIAL_PHONE				640							//拨打电话任务
#define DTU_STK_AT_DATA_HANDLER			512
#ifndef DTU_PRIO_TIME_TICK
#define DTU_PRIO_TIME_TICK				30
#endif
#ifndef DTU_PRIO_TELEPHONE_HANDLER
#define DTU_PRIO_TELEPHONE_HANDLER		2
#endif
#ifndef DTU_PRIO_RECV_BUF_HANDLER
#define DTU_PRIO_RECV_BUF_HANDLER		2
#endif
#ifndef DTU_PRIO_DIAL_PHONE
#define DTU_PRIO_DIAL_PHONE				4
#endif
#ifndef DTU_PRIO_AT_DATA_HANDLER
#define DTU_PRIO_AT_DATA_HANDLER		3
#endif

//运行模式
#define DTU_RUN_MODE_PWR				0							//断电模式
#define DTU_RUN_MODE_OFFLINE			1							//下线模式
#define DTU_RUN_MODE_ONLINE				2							//在线模式
#define DTU_NUM_RUN_MODE				3

//信道类型
#define DTU_COMM_TYPE_SMS				0							//sms
#define DTU_COMM_TYPE_IP				1							//ip

//发送数据类型
#define DTU_PACKET_TYPE_DATA			0							//数据包
#define DTU_PACKET_TYPE_HEART			1							//心跳包

//状态
#define DTU_STA_PWR						0x01						//供电状态
#define DTU_STA_DTU						0x02						//开机状态
#define DTU_STA_SMS						0x04						//短信状态
#define DTU_STA_IP						0x08						//连网状态
#define DTU_STA_HW_OPEN					0x10						//硬件打开状态

//参数事件标志
#define DTU_PARAM_ALL					0xffffffff
#define DTU_PARAM_IP_ADDR				0x01
#define DTU_PARAM_SMS_ADDR				0x100
#define DTU_PARAM_IP_TYPE				0x10000
#define DTU_PARAM_HEART_PERIOD			0x20000
#define DTU_PARAM_RUN_MODE				0x40000
#define DTU_PARAM_SUPER_PHONE			0x80000
#define DTU_PARAM_IP_CH					0x100000
#define DTU_PARAM_SMS_CH				0x200000
#define DTU_PARAM_APN					0x400000

//事件m_dtu_event_module_ex
#define DTU_EVENT_HANG_UP_ME			0x01
#define DTU_EVENT_HANG_UP_SHE			0x02
#define DTU_EVENT_ATA_OK				0x04
#define DTU_EVENT_ATA_ERROR				0x08
#define DTU_EVENT_TTS_STOP_ME			0x10
#define DTU_EVENT_TTS_STOP_SHE			0x20
#define DTU_EVENT_TTS_START_OK			0x40
#define DTU_EVENT_TTS_START_ERROR		0x80
#define DTU_EVENT_RECV_BUF				0x100
#define DTU_EVENT_RECV_BUF_FULL			0x200
#define DTU_EVENT_NEED_CONN_CH			0x1000000
#define DTU_EVENT_SEND_IMMEDIATELY		0x10000

//tts模式
#define DTU_TTS_DATA_TYPE_UCS2			1
#define DTU_TTS_DATA_TYPE_GBK			2



//信号强度更新函数指针
typedef void (*DTU_FUN_CSQ_UPDATE)(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t csq_value);

//心跳包编码函数指针
typedef rt_uint16_t (*DTU_FUN_HEART_ENCODER)(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t ch);

//接收数据结构
typedef struct dtu_recv_data
{
	rt_uint8_t					comm_type;							//信道类型
	rt_uint8_t					ch;									//通道
	rt_uint16_t					data_len;							//数据长度
	rt_uint8_t					*pdata;								//数据
} DTU_RECV_DATA;

//ip地址数据结构
typedef struct dtu_ip_addr
{
	rt_uint8_t					addr_data[DTU_BYTES_IP_ADDR];		//ip地址，格式必须为1.85.44.234:9602
	rt_uint8_t					addr_len;							//ip地址长度
} DTU_IP_ADDR;

//sms地址数据结构
typedef struct dtu_sms_addr
{
	rt_uint8_t					addr_data[DTU_BYTES_SMS_ADDR];		//sms地址
	rt_uint8_t					addr_len;							//sms地址长度
} DTU_SMS_ADDR;

//apn数据结构
typedef struct dtu_apn
{
	rt_uint8_t					apn_data[DTU_BYTES_APN];			//apn名称
	rt_uint8_t					apn_len;							//apn长度
} DTU_APN;

//tts数据
typedef struct dtu_tts_data
{
	rt_uint8_t					data_type;							//数据类型gbk或者ucs2
	rt_uint16_t					data_len;							//数据长度
	rt_uint8_t					*pdata;								//数据
	void (*fun_tts_over)(void);										//结束通知
} DTU_TTS_DATA;

//参数集,成员位置不允许随意变化
typedef struct dtu_param_set
{
	DTU_IP_ADDR					ip_addr[DTU_NUM_IP_CH];
	DTU_SMS_ADDR				sms_addr[DTU_NUM_SMS_CH];
	rt_uint8_t					ip_ch;
	rt_uint8_t					sms_ch;
	rt_uint8_t					ip_type;
	rt_uint8_t					run_mode[DTU_NUM_IP_CH];
	rt_uint16_t					heart_period[DTU_NUM_IP_CH];
	DTU_SMS_ADDR				super_phone[DTU_NUM_SUPER_PHONE];
	DTU_APN						apn;
} DTU_PARAM_SET;



//参数读写
void dtu_set_ip_addr(DTU_IP_ADDR const *ip_addr_ptr, rt_uint8_t ch);
void dtu_get_ip_addr(DTU_IP_ADDR *ip_addr_ptr, rt_uint8_t ch);

void dtu_set_sms_addr(DTU_SMS_ADDR const *sms_addr_ptr, rt_uint8_t ch);
void dtu_get_sms_addr(DTU_SMS_ADDR *sms_addr_ptr, rt_uint8_t ch);

void dtu_set_ip_type(rt_uint8_t ip_type, rt_uint8_t ch);
rt_uint8_t dtu_get_ip_type(rt_uint8_t ch);

void dtu_set_heart_period(rt_uint16_t heart_period, rt_uint8_t ch);
rt_uint16_t dtu_get_heart_period(rt_uint8_t ch);

void dtu_set_run_mode(rt_uint8_t run_mode, rt_uint8_t ch);
rt_uint8_t dtu_get_run_mode(rt_uint8_t ch);

void dtu_set_super_phone(DTU_SMS_ADDR const *sms_addr_ptr, rt_uint8_t ch);
void dtu_get_super_phone(DTU_SMS_ADDR *sms_addr_ptr, rt_uint8_t ch);
void dtu_del_super_phone(DTU_SMS_ADDR const *sms_addr_ptr);
rt_uint8_t dtu_add_super_phone(DTU_SMS_ADDR const *sms_addr_ptr, rt_uint8_t previlige);
rt_uint8_t dtu_is_previlige_phone(rt_uint8_t ch);

void dtu_set_ip_ch(rt_uint8_t sta, rt_uint8_t ch);
rt_uint8_t dtu_get_ip_ch(rt_uint8_t ch);

void dtu_set_sms_ch(rt_uint8_t sta, rt_uint8_t ch);
rt_uint8_t dtu_get_sms_ch(rt_uint8_t ch);

void dtu_set_apn(DTU_APN const *apn_ptr);
void dtu_get_apn(DTU_APN *apn_ptr);

//参数重置
void dtu_param_restore(void);

//发送数据
rt_uint8_t dtu_send_data(rt_uint8_t comm_type, rt_uint8_t ch, rt_uint8_t const *pdata, rt_uint16_t data_len, DTU_FUN_CSQ_UPDATE fun_csq_update);

//接收数据
DTU_RECV_DATA *dtu_recv_data_pend(void);

DTU_RECV_DATA *dtu_http_data_pend(rt_int32_t timeout);

void dtu_http_data_clear(void);

//保持在线
void dtu_hold_enable(rt_uint8_t comm_type, rt_uint8_t ch, rt_uint8_t en);

//函数挂载
void dtu_fun_mount(DTU_FUN_HEART_ENCODER fun_heart_encoder);

rt_uint8_t dtu_get_conn_sta(rt_uint8_t ch);

//拨打电话
rt_uint8_t dtu_dial_phone(DTU_SMS_ADDR *sms_addr_ptr);

//立即发送
void dtu_ip_send_immediately(rt_uint8_t ch);

rt_uint8_t dtu_get_csq_value(void);

rt_uint8_t dtu_http_send(DTU_IP_ADDR const *ip_addr_ptr, rt_uint8_t const *pdata, rt_uint16_t data_len);



#endif

