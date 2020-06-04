/*占用的存储空间
**EEPROM		:1200--1299
**FLASH			:1048576--1150975(ram_100KB)
*/



#ifndef __PTCL_H__
#define __PTCL_H__



#include "rtthread.h"
#include "user_priority.h"



//软件版本
#define PTCL_SOFTWARE_EDITION			"fyyp_rtu_0"

//基地址
#define PTCL_BASE_ADDR_PARAM			1200							//参数基地址
#define PTCL_BASE_ADDR_RAM				1048576							//内存数据地址

//配置信息
//#define PTCL_AUTO_REPORT_ENCRYPT							//自报数据加密
#define PTCL_NUM_CENTER					4					//中心站数量
#define PTCL_NUM_COMM_PRIO				2					//通信优先级数量
#define PTCL_NUM_AUTO_REPORT			5					//自报数据容量
#define PTCL_BYTES_ACK_REPORT			230					//应答上报数据空间
#define PTCL_BYTES_TC_BUF				950					//tc接收缓冲区

//内存信息
#define PTCL_NUM_CENTER_RAM				100					//中心站内存容量
#define PTCL_BYTES_RAM_DATA				256					//内存帧空间
#define PTCL_BYTES_RAM_FEATURE			7					//内存数据特征空间

//信道类型
#define PTCL_COMM_TYPE_NONE				0					//禁用
#define PTCL_COMM_TYPE_SMS				1					//sms
#define PTCL_COMM_TYPE_IP				2					//ip
#define PTCL_COMM_TYPE_BD				3					//北斗
#define PTCL_COMM_TYPE_LORA				4					//lora
#define PTCL_COMM_TYPE_PSTN				5					//pstn
#define PTCL_COMM_TYPE_CDB				6					//超短波
#define PTCL_COMM_TYPE_TC				7					//透传
#define PTCL_COMM_TYPE_BT				8					//蓝牙
#define PTCL_NUM_COMM_TYPE				9					//信道数量

//协议类型
#define PTCL_PTCL_TYPE_SL427			0
#define PTCL_PTCL_TYPE_SL651			1
#define PTCL_PTCL_TYPE_MYSELF			2
#define PTCL_PTCL_TYPE_SMT_CFG			3
#define PTCL_PTCL_TYPE_SMT_MNG			4
#define PTCL_PTCL_TYPE_XMODEM			5
#define PTCL_PTCL_TYPE_HJ212			6
#define PTCL_NUM_PTCL_TYPE				7

//任务栈
#define PTCL_STK_AUTO_REPORT_MANAGE		768					//自报管理任务栈
#define PTCL_STK_AUTO_REPORT			640					//自报任务栈
#define PTCL_STK_DTU_RECV_HANDLER		640					//dtu接收处理任务栈
#define PTCL_STK_TC_RECV_HANDLER		640					//tc收处理任务栈
#define PTCL_STK_TC_BUF_HANDLER			512					//tc缓冲区处理任务栈
#define PTCL_STK_SHELL_RECV_HANDLER		640					//壳子接收处理任务栈
#define PTCL_STK_BT_RECV_HANDLER		640
#ifndef PTCL_PRIO_AUTO_REPORT_MANAGE
#define PTCL_PRIO_AUTO_REPORT_MANAGE	28
#endif
#ifndef PTCL_PRIO_AUTO_REPORT
#define PTCL_PRIO_AUTO_REPORT			29
#endif
#ifndef PTCL_PRIO_DTU_RECV_HANDLER
#define PTCL_PRIO_DTU_RECV_HANDLER		3
#endif
#ifndef PTCL_PRIO_TC_RECV_HANDLER
#define PTCL_PRIO_TC_RECV_HANDLER		3
#endif
#ifndef PTCL_PRIO_TC_BUF_HANDLER
#define PTCL_PRIO_TC_BUF_HANDLER		2
#endif
#ifndef PTCL_PRIO_SHELL_RECV_HANDLER
#define PTCL_PRIO_SHELL_RECV_HANDLER	3
#endif
#ifndef PTCL_PRIO_BT_RECV_HANDLER
#define PTCL_PRIO_BT_RECV_HANDLER		4
#endif

//事件
#define PTCL_EVENT_REPLY_AUTO_REPORT	0x01				//自报回执标志
#define PTCL_EVENT_REPLY_ACK_TC			0x100				//tc信道应答回执标志
#define PTCL_EVENT_REPLY_ACK_IP			0x200				//ip信道应答回执标志
#define PTCL_EVENT_REPLY_TSM_MS			0x400				//tsm和ms远程控制报文发送回执标志
#define PTCL_EVENT_REPLY_TSM_ERR		0x800				//tsm故障回执标志
#define PTCL_EVENT_AUTO_REPORT_PEND		0x1000				//自报数据占用(多中心同时上报时对数据的互斥)
#define PTCL_EVENT_TC_RECV				0x2000				//tc信道收到数据
#define PTCL_EVENT_REPLY_ACK_SHELL		0x4000				//壳子数据回执标志
#define PTCL_EVENT_REPLY_LIST_PEND		0x8000				//回执管理链表占用
#define PTCL_EVENT_REPLY_ACK_BT			0x10000

//超时时间
#define PTCL_REPLY_TIME_IP				20					//ip信道回执超时时间，秒
#define PTCL_REPLY_TIME_TC				5					//tc信道回执超时时间，秒
#define PTCL_REPLY_TIME_BT				5					//bt信道回执超时时间，秒
#define PTCL_TC_BUF_TIMEOUT				100					//tc缓冲区超时时间，毫秒
#define PTCL_REPLY_VALID_TIMEOUT		3					//回执有效期限，秒，用于回执来得太快的情况

//运行状态
#define PTCL_RUN_STA_NORMAL				0					//正常运行
#define PTCL_RUN_STA_RESET				1					//复位
#define PTCL_RUN_STA_RESTORE			2					//重置

//参数
#define PTCL_PARAM_ALL					0xffffffff
#define PTCL_PARAM_CENTER_ADDR			0x01
#define PTCL_PARAM_COMM_TYPE			0x100
#define PTCL_PARAM_RAM_PTR				0x10000
#define PTCL_PARAM_PTCL_TYPE			0x1000000

//参数信息
#define PTCL_PARAM_INFO_TT							0
#define PTCL_PARAM_INFO_CENTER_ADDR					1
#define PTCL_PARAM_INFO_COMM_TYPE					2
#define PTCL_PARAM_INFO_IP_ADDR						3
#define PTCL_PARAM_INFO_SMS_ADDR					4
#define PTCL_PARAM_INFO_IP_TYPE						5
#define PTCL_PARAM_INFO_IP_CH						6
#define PTCL_PARAM_INFO_SMS_CH						7
#define PTCL_PARAM_INFO_DTU_APN						8
#define PTCL_PARAM_INFO_HEART_PERIOD				9
#define PTCL_PARAM_INFO_DTU_MODE					10
#define PTCL_PARAM_INFO_ADD_SUPER_PHONE				11
#define PTCL_PARAM_INFO_DEL_SUPER_PHONE				12
#define PTCL_PARAM_INFO_FM_FREQUENCY				13
#define PTCL_PARAM_INFO_WARN_PRIO					14
#define PTCL_PARAM_INFO_WARN_TIMES					15
#define PTCL_PARAM_INFO_FM_RSSI_THRESHOLD			16
#define PTCL_PARAM_INFO_SL427_RTU_ADDR				17
#define PTCL_PARAM_INFO_SL427_REPORT_PERIOD			18
#define PTCL_PARAM_INFO_SMTMNG_STA_PERIOD			19
#define PTCL_PARAM_INFO_SMTMNG_TIMING_PERIOD		20
#define PTCL_PARAM_INFO_SMTMNG_HEART_PERIOD			21
#define PTCL_PARAM_INFO_LORA_EN						22
#define PTCL_PARAM_INFO_LORA_ADDR					23
#define PTCL_PARAM_INFO_LORA_NET_ID					24
#define PTCL_PARAM_INFO_LORA_AIR_SPEED				25
#define PTCL_PARAM_INFO_LORA_COMM_CH				26
#define PTCL_PARAM_INFO_LORA_TRANS_TYPE				27
#define PTCL_PARAM_INFO_LORA_TRANSFER_EN			28
#define PTCL_PARAM_INFO_LORA_DES_ADDR				29
#define PTCL_PARAM_INFO_LORA_DES_COMM_CH			30
#define PTCL_PARAM_INFO_PTCL_TYPE					31
#define PTCL_PARAM_INFO_AD_CAL_VAL					32
#define PTCL_PARAM_INFO_SAMPLE_HW_RATE				33
#define PTCL_PARAM_INFO_SAMPLE_SENSOR_TYPE			34
#define PTCL_PARAM_INFO_SAMPLE_SENSOR_ADDR			35
#define PTCL_PARAM_INFO_SAMPLE_HW_PORT				36
#define PTCL_PARAM_INFO_SAMPLE_PROTOCOL				37
#define PTCL_PARAM_INFO_SAMPLE_K					38
#define PTCL_PARAM_INFO_SAMPLE_B					39
#define PTCL_PARAM_INFO_SAMPLE_BASE					40
#define PTCL_PARAM_INFO_SAMPLE_OFFSET				41
#define PTCL_PARAM_INFO_SAMPLE_UP					42
#define PTCL_PARAM_INFO_SAMPLE_DOWN					43
#define PTCL_PARAM_INFO_SAMPLE_THRESHOLD			44
#define PTCL_PARAM_INFO_SAMPLE_KOPT					45

#define PTCL_PARAM_INFO_SAMPLE_STORE_PERIOD			47
#define PTCL_PARAM_INFO_SL651_RTU_ADDR				48
#define PTCL_PARAM_INFO_SL651_PW					49
#define PTCL_PARAM_INFO_SL651_SN					50
#define PTCL_PARAM_INFO_SL651_RTU_TYPE				51
#define PTCL_PARAM_INFO_SL651_REPORT_HOUR			52
#define PTCL_PARAM_INFO_SL651_REPORT_MIN			53
#define PTCL_PARAM_INFO_SL651_RANDOM_PERIOD			54
#define PTCL_PARAM_INFO_CENTER_ADDR_ALL				55
#define PTCL_PARAM_INFO_SL651_CENTER_COMM			56
#define PTCL_PARAM_INFO_DEBUG_INFO_TYPE_STA			57
#define PTCL_PARAM_INFO_BX5K_SCR_UPDATE_PERIOD		58
#define PTCL_PARAM_INFO_BX5K_TIME_CALI_PERIOD		59
#define PTCL_PARAM_INFO_BD_EN						60
#define PTCL_PARAM_INFO_BD_PWR_OFF_TIME				61
#define PTCL_PARAM_INFO_BD_DES_ADDR					62
#define PTCL_PARAM_INFO_SL427_SMS_WARN_RECEIPT		63
#define PTCL_PARAM_INFO_HJ212_ST					64
#define PTCL_PARAM_INFO_HJ212_PW					65
#define PTCL_PARAM_INFO_HJ212_MN					66
#define PTCL_PARAM_INFO_HJ212_OVER_TIME				67
#define PTCL_PARAM_INFO_HJ212_RECOUNT				68
#define PTCL_PARAM_INFO_HJ212_RTD_EN				69
#define PTCL_PARAM_INFO_HJ212_RTD_INTERVAL			70
#define PTCL_PARAM_INFO_HJ212_MIN_INTERVAL			71
#define PTCL_PARAM_INFO_HJ212_RS_EN					72



//上报数据
typedef struct ptcl_report_data
{
	void						*fun_csq_update;							//信号强度更新函数
	rt_uint8_t					*pdata;										//数据
	rt_uint16_t					data_len;									//数据长度
	rt_uint16_t					data_id;									//数据id
	rt_uint8_t					need_reply;									//是否需要回执
	rt_uint8_t					fcb_value;									//fcb值
	rt_uint8_t					ptcl_type;									//协议类型
} PTCL_REPORT_DATA;

//回执数据
typedef struct ptcl_reply_data
{
	rt_uint8_t					comm_type;									//信道类型
	rt_uint8_t					ch;											//来源中心
	rt_uint16_t					data_id;									//数据id
	rt_uint32_t					event;										//事件标志
	struct ptcl_reply_data		*prev;
	struct ptcl_reply_data		*next;
} PTCL_REPLY_DATA;

//接收数据
typedef struct ptcl_recv_data
{
	rt_uint8_t					comm_type;									//信道类型
	rt_uint8_t					ch;											//通道
	rt_uint16_t					data_len;									//数据长度
	rt_uint8_t					*pdata;										//数据
} PTCL_RECV_DATA;

//内存索引
typedef struct ptcl_ram_ptr
{
	rt_uint8_t					in;											//入索引
	rt_uint8_t					out;										//出索引
} PTCL_RAM_PTR;

//中心站信息
typedef struct ptcl_center_info
{
	rt_uint8_t					center_addr;								//中心站地址
	rt_uint8_t					ptcl_type;									//协议类型
	rt_uint8_t					comm_type[PTCL_NUM_COMM_PRIO];				//通信类型
	PTCL_RAM_PTR				ram_ptr;									//内存指针
} PTCL_CENTER_INFO;

//参数集合
typedef struct ptcl_param_set
{
	PTCL_CENTER_INFO			center_info[PTCL_NUM_CENTER];
} PTCL_PARAM_SET;

//参数配置节点信息
typedef struct ptcl_param_info
{
	rt_uint8_t					param_type;
	rt_uint16_t					data_len;
	rt_uint8_t					*pdata;
	struct ptcl_param_info		*next;
} PTCL_PARAM_INFO;



//参数读写
void ptcl_set_center_addr(rt_uint8_t center_addr, rt_uint8_t center);
rt_uint8_t ptcl_get_center_addr(rt_uint8_t center);

void ptcl_set_comm_type(rt_uint8_t comm_type, rt_uint8_t center, rt_uint8_t prio);
rt_uint8_t ptcl_get_comm_type(rt_uint8_t center, rt_uint8_t prio);

void ptcl_set_ram_ptr(PTCL_RAM_PTR const *ram_ptr, rt_uint8_t center);
void ptcl_get_ram_ptr(PTCL_RAM_PTR *ram_ptr, rt_uint8_t center);

void ptcl_set_ptcl_type(rt_uint8_t ptcl_type, rt_uint8_t center);
rt_uint8_t ptcl_get_ptcl_type(rt_uint8_t center);

//参数恢复出厂
void ptcl_param_restore(void);

//上报数据申请
PTCL_REPORT_DATA *ptcl_report_data_req(rt_uint16_t bytes_req, rt_int32_t ticks);

//接收数据申请
PTCL_RECV_DATA *ptcl_recv_data_req(rt_uint16_t bytes_req, rt_int32_t ticks);

//参数信息申请
PTCL_PARAM_INFO *ptcl_param_info_req(rt_uint16_t bytes_req, rt_int32_t ticks);

//参数信息添加
void ptcl_param_info_add(PTCL_PARAM_INFO **param_info_ptr_ptr, PTCL_PARAM_INFO *param_info_ptr);

//上报数据
rt_uint8_t ptcl_report_data_send(PTCL_REPORT_DATA *report_data_ptr, rt_uint8_t comm_type, rt_uint8_t ch, rt_uint8_t center_addr, rt_uint32_t event_reply, rt_uint16_t *reply_info_ptr);

//回执释放
void ptcl_reply_post(rt_uint8_t comm_type, rt_uint8_t ch, rt_uint16_t data_id, rt_uint16_t reply_info);

//自报数据post
rt_uint8_t ptcl_auto_report_post(PTCL_REPORT_DATA *report_data_ptr);

//运行状态
rt_uint8_t ptcl_run_sta_set(rt_uint8_t sta);

//是否有壳子
rt_uint8_t ptcl_have_shell(void);

//壳子数据post
rt_uint8_t ptcl_shell_data_post(PTCL_RECV_DATA *shell_data_ptr);



#endif

