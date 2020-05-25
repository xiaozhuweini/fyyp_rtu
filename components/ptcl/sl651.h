/*占用的存储空间
**EEPROM		:1700--1806
**flash			:524588--524787
*/



#ifndef __SL651_H__
#define __SL651_H__



#include "rtthread.h"
#include "drv_rtcee.h"
#include "ptcl.h"



//参数基地址
#define SL651_PARAM_BASE_ADDR				1700
#define SL651_WARN_INFO_ADDR				524588

//参数存储位置(107bytes)
#define SL651_POS_RTU_ADDR					(SL651_PARAM_BASE_ADDR + 0)		//  5个字节，地址
#define SL651_POS_PW						(SL651_PARAM_BASE_ADDR + 5)		//  2个字节，密码
#define SL651_POS_SN						(SL651_PARAM_BASE_ADDR + 7)		//  2个字节，流水号
#define SL651_POS_RTU_TYPE					(SL651_PARAM_BASE_ADDR + 9)		//  1个字节，测站分类码
#define SL651_POS_ELEMENT					(SL651_PARAM_BASE_ADDR + 10)	//  8个字节，监测要素
#define SL651_POS_WORK_MODE					(SL651_PARAM_BASE_ADDR + 18)	//  1个字节，工作模式
#define SL651_POS_REPORT_PERIOD_HOUR		(SL651_PARAM_BASE_ADDR + 19)	//  1个字节，定时报间隔
#define SL651_POS_REPORT_PERIOD_MIN			(SL651_PARAM_BASE_ADDR + 20)	//  1个字节，加报间隔
#define SL651_POS_ERC						(SL651_PARAM_BASE_ADDR + 21)	// 64个字节，事件记录
#define SL651_POS_DEVICE_ID					(SL651_PARAM_BASE_ADDR + 85)	// 20个字节，设备识别号
#define SL651_POS_RANDOM_SAMPLE_PERIOD		(SL651_PARAM_BASE_ADDR + 105)	//  2个字节，随机采集间隔

//配置信息
#define SL651_BYTES_REPORT_DATA				100		//上报数据空间
#define SL651_BYTES_MANUAL_DATA				50		//人工置数数据空间
#define SL651_BYTES_DEVICE_ID				19		//设备识别码数据空间
#define SL651_DATA_POINT_PER_PACKET			20
#define SL651_BYTES_WARN_INFO_MAX			24
#define SL651_WARN_INFO_VALID				0xa55a
#define SL651_NUM_WARN_INFO					4
#define SL651_BYTES_WARN_INFO_IN_FLASH		50
#define SL651_DEF_WARN_TIMES				3

//任务
#define SL651_STK_DATA_DOWNLOAD				640		//数据下载任务栈
#define SL651_STK_RANDOM_SAMPLE				640		//人工置数任务栈
#define SL651_STK_REPORT_TIME				640		//定时报任务栈
#ifndef SL651_PRIO_DATA_DOWNLOAD
#define SL651_PRIO_DATA_DOWNLOAD			20
#endif
#ifndef SL651_PRIO_RANDOM_SAMPLE
#define SL651_PRIO_RANDOM_SAMPLE			20
#endif
#ifndef SL651_PRIO_REPORT_TIME
#define SL651_PRIO_REPORT_TIME				21
#endif

//帧结构
#define SL651_BYTES_FRAME_SYMBOL			2		//帧标志字节数
#define SL651_BYTES_RTU_ADDR				5		//RTU地址字节数
#define SL651_BYTES_CENTER_ADDR				1		//中心站地址字节数
#define SL651_BYTES_PW						2		//密码字节数
#define SL651_BYTES_AFN						1		//功能码字节数
#define SL651_BYTES_UP_DOWN_LEN				2		//上下行标识及长度字节数
#define SL651_BYTES_START_SYMBOL			1		//正文起始符字节数
#define SL651_BYTES_SN						2		//流水号字节数
#define SL651_BYTES_SEND_TIME				6		//发报时间字节数
#define SL651_BYTES_CRC_VALUE				2		//CRC校验码字节数
#define SL651_BYTES_FRAME_HEAD				14		//帧头字节数
#define SL651_BYTES_FRAME_END				3		//帧尾字节数
#define SL651_BYTES_RTU_TYPE				1		//测站分类码字节数
#define SL651_BYTES_FRAME_OBSERVE_TIME		7		//带标识符的观测时间字节数
#define SL651_BYTES_FRAME_RTU_ADDR			7		//带标识符的RTU地址字节数
#define SL651_BYTES_FRAME_SHANGQING			5		//带标识符的墒情数据字节数(含负数)
#define SL651_BYTES_FRAME_SHANGQING_EX		11		//日墒情数据字节数
#define SL651_BYTES_FRAME_VOLTAGE			4		//带标识符的供电电压数据字节数
#define SL651_BYTES_FRAME_VOLTAGE_EX		5		//带标识符的充电电压数据字节数
#define SL651_BYTES_FRAME_STATUS_ALARM		6		//带标识符的状态及报警信息字节数
#define SL651_BYTES_FRAME_RAIN				5
#define SL651_BYTES_FRAME_WATER_CUR			7
#define SL651_BYTES_FRAME_CSQ_EX			4
#define SL651_BYTES_FRAME_ERR_CODE_EX		6
#define SL651_BYTES_FRAME_WATER_STA_EX		4
#define SL651_BYTES_FRAME_TIME_RANGE		8		//时间范围字节数
#define SL651_BYTES_FRAME_TIME_STEP			5		//时间步长字节数
#define SL651_BYTES_FRAME_ELEMENT_SYMBOL	3		//要素标识符字节数(含扩展FF)
#define SL651_BYTES_FRAME_PACKET_INFO		3		//包总数与包序号字节数
#define SL651_FRAME_DEFAULT_FCB				3		//默认FCB值
#define SL651_FRAME_DEFAULT_PW				0		//缺省密码
#define SL651_FRAME_SYMBOL					0x7e	//帧标志
#define SL651_FRAME_SYMBOL_STX				0x02	//正文起始符，单包
#define SL651_FRAME_SYMBOL_SYN				0x16	//正文起始符，多包
#define SL651_FRAME_SYMBOL_ETX				0x03	//正文结束符，后续无报文
#define SL651_FRAME_SYMBOL_ETB				0x17	//正文结束符，后续有报文
#define SL651_FRAME_SYMBOL_ENQ				0x05	//正文结束符，询问
#define SL651_FRAME_SYMBOL_EOT				0x04	//正文结束符，退出通信
#define SL651_FRAME_SYMBOL_ACK				0x06	//正文结束符，确认，多包发送结束
#define SL651_FRAME_SYMBOL_NAK				0x15	//正文结束符，否认，重发某包
#define SL651_FRAME_SYMBOL_ESC				0x1b	//正文结束符，保持在线
#define SL651_FRAME_INVALID_DATA			0xff	//无效数据
#define SL651_FRAME_SIGN_NEGATIVE			0xff	//负标志
#define SL651_FRAME_DIR_UP					0		//上行标志
#define SL651_FRAME_DIR_DOWN				8		//下行标志
#define SL651_FRAME_SYMBOL_ELEMENT_EX		0xff	//扩展要素标识符标志

//测站分类码
#define SL651_RTU_TYPE_JIANGSHUI			0x50	//降水
#define SL651_RTU_TYPE_HEDAO				0x48	//河道
#define SL651_RTU_TYPE_SHUIKU				0x4b	//水库
#define SL651_RTU_TYPE_ZHABA				0x5a	//闸坝
#define SL651_RTU_TYPE_BENGZHAN				0x44	//泵站
#define SL651_RTU_TYPE_CHAOXI				0x54	//潮汐
#define SL651_RTU_TYPE_SHANGQING			0x4d	//墒情
#define SL651_RTU_TYPE_DIXIASHUI			0x47	//地下水
#define SL651_RTU_TYPE_SHUIZHI				0x51	//水质
#define SL651_RTU_TYPE_QUSHUIKOU			0x49	//取水口
#define SL651_RTU_TYPE_PAISHUIKOU			0x4f	//排水口

//功能码
#define SL651_AFN_REPORT_HEART				0x2f	//链路维持报
#define SL651_AFN_REPORT_TEST				0x30	//测试报
#define SL651_AFN_REPORT_AVERAGE			0x31	//均匀时段报
#define SL651_AFN_REPORT_TIMING				0x32	//定时报
#define SL651_AFN_REPORT_ADD				0x33	//加报报
#define SL651_AFN_REPORT_HOUR				0x34	//小时报
#define SL651_AFN_REPORT_MANUAL				0x35	//人工置数
#define SL651_AFN_QUERY_PHOTO				0x36	//查询图片信息
#define SL651_AFN_QUERY_CUR_DATA			0x37	//查询实时数据
#define SL651_AFN_QUERY_OLD_DATA			0x38	//查询历史数据
#define SL651_AFN_QUERY_MANUAL_DATA			0x39	//查询人工置数
#define SL651_AFN_QUERY_ELEMENT_DATA		0x3a	//查询指定要素数据
#define SL651_AFN_SET_BASIC_PARAM			0x40	//修改基本配置
#define SL651_AFN_QUERY_BASIC_PARAM			0x41	//读取基本配置
#define SL651_AFN_SET_RUN_PARAM				0x42	//修改运行配置
#define SL651_AFN_QUERY_RUN_PARAM			0x43	//读取运行配置
#define SL651_AFN_QUERY_MOTO_DATA			0x44	//查询水泵电机数据
#define SL651_AFN_QUERY_SOFT_VER			0x45	//查询软件版本
#define SL651_AFN_QUERY_RTU_STATUS			0x46	//查询遥测站状态及报警信息
#define SL651_AFN_CLEAR_OLD_DATA			0x47	//清空历史数据
#define SL651_AFN_RTU_RESTORE				0x48	//恢复出厂设置
#define SL651_AFN_CHANGE_PW					0x49	//修改密码
#define SL651_AFN_SET_TIME					0x4a	//设置时钟
#define SL651_AFN_SET_IC_STATUS				0x4b	//设置IC卡状态
#define SL651_AFN_CTRL_PUMP					0x4c	//控制水泵开关命令、水泵状态信息自报
#define SL651_AFN_CTRL_VALVE				0x4d	//控制阀门开关命令、阀门状态信息自报
#define SL651_AFN_CTRL_SLUICE				0x4e	//控制闸门开关命令、闸门状态信息自报
#define SL651_AFN_CTRL_FIXED_WATER			0x4f	//水量定值控制命令
#define SL651_AFN_QUERY_ERC					0x50	//查询事件记录
#define SL651_AFN_QUERY_TIME				0x51	//查询时钟

//要素标识符引导符
#define SL651_BOOT_OBSERVE_TIME				0xf0	//观测时间
#define SL651_BOOT_RTU_CODE					0xf1	//测站编码
#define SL651_BOOT_MANUAL_DATA				0xf2	//人工置数
#define SL651_BOOT_PHOTO_INFO				0xf3	//图片信息
#define SL651_BOOT_WATER_TEMPER				0x03	//瞬时水温
#define SL651_BOOT_TIME_STEP				0x04	//时间步长
#define SL651_BOOT_GROUND_WATER				0x0e	//地下水瞬时埋深
#define SL651_BOOT_SHANGQING_10CM			0x10	//10cm处土壤含水率
#define SL651_BOOT_SHANGQING_20CM			0x11	//20cm处土壤含水率
#define SL651_BOOT_SHANGQING_30CM			0x12	//30cm处土壤含水率
#define SL651_BOOT_SHANGQING_40CM			0x13	//40cm处土壤含水率
#define SL651_BOOT_SHANGQING_50CM			0x14	//50cm处土壤含水率
#define SL651_BOOT_SHANGQING_60CM			0x15	//60cm处土壤含水率
#define SL651_BOOT_SHANGQING_80CM			0x16	//80cm处土壤含水率
#define SL651_BOOT_SHANGQING_100CM			0x17	//100cm处土壤含水率
#define SL651_BOOT_SHANGQING_10CM_EX		0xff10	//10cm处土壤含水率(日数据)
#define SL651_BOOT_SHANGQING_20CM_EX		0xff20	//20cm处土壤含水率(日数据)
#define SL651_BOOT_SHANGQING_30CM_EX		0xff30	//40cm处土壤含水率(日数据)
#define SL651_BOOT_SHANGQING_40CM_EX		0xff40	//40cm处土壤含水率(日数据)
#define SL651_BOOT_RAIN_TOTAL				0x26	//降水量累计值
#define SL651_BOOT_LIUSU_CUR				0x37	//瞬时流速
#define SL651_BOOT_PWR_VOLTAGE				0x38	//电源电压
#define SL651_BOOT_PWR_VOLTAGE_EX			0xff38	//电源电压
#define SL651_BOOT_WATER_CUR				0x39	//瞬时水位
#define SL651_BOOT_WATER_CUR_1				0x3c	//取排水口水位1
#define SL651_BOOT_WATER_CUR_2				0x3d	//取排水口水位2
#define SL651_BOOT_WATER_CUR_3				0x3e	//取排水口水位3
#define SL651_BOOT_WATER_CUR_4				0x3f	//取排水口水位4
#define SL651_BOOT_WATER_CUR_5				0x40	//取排水口水位5
#define SL651_BOOT_WATER_CUR_6				0x41	//取排水口水位6
#define SL651_BOOT_WATER_CUR_7				0x42	//取排水口水位7
#define SL651_BOOT_WATER_CUR_8				0x43	//取排水口水位8
#define SL651_BOOT_STATUS_ALARM				0x45	//状态及报警信息
#define SL651_BOOT_FLOW_CUR_1				0x68	//水表1每小时水量
#define SL651_BOOT_FLOW_CUR_2				0x69	//水表2每小时水量
#define SL651_BOOT_FLOW_CUR_3				0x6a	//水表3每小时水量
#define SL651_BOOT_FLOW_CUR_4				0x6b	//水表4每小时水量
#define SL651_BOOT_FLOW_CUR_5				0x6c	//水表5每小时水量
#define SL651_BOOT_FLOW_CUR_6				0x6d	//水表6每小时水量
#define SL651_BOOT_FLOW_CUR_7				0x6e	//水表7每小时水量
#define SL651_BOOT_FLOW_CUR_8				0x6f	//水表8每小时水量
#define SL651_BOOT_CSQ_EX					0xff85
#define SL651_BOOT_ERR_CODE_EX				0xff81
#define SL651_BOOT_WATER_LIMIT_1_EX			0xff3a
#define SL651_BOOT_WATER_LIMIT_2_EX			0xff3b
#define SL651_BOOT_WATER_STA_EX				0xff3c

//基本参数标识符引导符
#define SL651_BOOT_CENTER_ADDR				0x01	//中心站地址
#define SL651_BOOT_RTU_ADDR					0x02	//RTU地址
#define SL651_BOOT_PW						0x03	//密码
#define SL651_BOOT_CENTER_1_COMM_1			0x04	//中心站1主信道
#define SL651_BOOT_CENTER_1_COMM_2			0x05	//中心站1备信道
#define SL651_BOOT_CENTER_2_COMM_1			0x06	//中心站2主信道
#define SL651_BOOT_CENTER_2_COMM_2			0x07	//中心站2备信道
#define SL651_BOOT_CENTER_3_COMM_1			0x08	//中心站3主信道
#define SL651_BOOT_CENTER_3_COMM_2			0x09	//中心站3备信道
#define SL651_BOOT_CENTER_4_COMM_1			0x0a	//中心站4主信道
#define SL651_BOOT_CENTER_4_COMM_2			0x0b	//中心站4备信道
#define SL651_BOOT_WORK_MODE				0x0c	//工作方式
#define SL651_BOOT_SAMPLE_ELEMENT			0x0d	//采集要素
#define SL651_BOOT_DEVICE_ID				0x0f	//设备识别码

//运行参数标识符引导符
#define SL651_BOOT_REPORT_TIMEING			0x20	//定时报时间间隔
#define SL651_BOOT_REPORT_ADD				0x21	//加报时间间隔
#define SL651_BOOT_RAIN_UNIT				0x25	//雨量分辨力
#define SL651_BOOT_WATER_BASE_1				0x28	//水位基值1
#define SL651_BOOT_WATER_BASE_2				0x29	//水位基值2
#define SL651_BOOT_WATER_BASE_3				0x2a	//水位基值3
#define SL651_BOOT_WATER_BASE_4				0x2b	//水位基值4
#define SL651_BOOT_WATER_BASE_5				0x2c	//水位基值5
#define SL651_BOOT_WATER_BASE_6				0x2d	//水位基值6
#define SL651_BOOT_WATER_BASE_7				0x2e	//水位基值7
#define SL651_BOOT_WATER_BASE_8				0x2f	//水位基值8
#define SL651_BOOT_WATER_OFFSET_1			0x30	//水位修正值1
#define SL651_BOOT_WATER_OFFSET_2			0x31	//水位修正值2
#define SL651_BOOT_WATER_OFFSET_3			0x32	//水位修正值3
#define SL651_BOOT_WATER_OFFSET_4			0x33	//水位修正值4
#define SL651_BOOT_WATER_OFFSET_5			0x34	//水位修正值5
#define SL651_BOOT_WATER_OFFSET_6			0x35	//水位修正值6
#define SL651_BOOT_WATER_OFFSET_7			0x36	//水位修正值7
#define SL651_BOOT_WATER_OFFSET_8			0x37	//水位修正值8
#define SL651_BOOT_CLEAR_OLD_DATA			0x97	//清空历史数据
#define SL651_BOOT_RTU_RESTORE				0x98	//恢复出厂设置

//要素数据定义
#define SL651_LEN_OBSERVE_TIME				0xf0	//观测时间
#define SL651_LEN_RTU_CODE					0xf1	//测站编码
#define SL651_LEN_MANUAL_DATA				0xf2	//人工置数
#define SL651_LEN_PHOTO_INFO				0xf3	//图片信息
#define SL651_LEN_TIME_STEP					0x18	//时间步长
#define SL651_LEN_PWR_VOLTAGE				0x12	//电源电压
#define SL651_LEN_PWR_VOLTAGE_EX			0x12	//电源电压
#define SL651_LEN_STATUS_ALARM				0x20	//状态及报警信息
#define SL651_LEN_FLOW_CUR					0x2a	//水表每小时水量
#define SL651_LEN_RAIN_TOTAL				0x19	//降水量累计值
#define SL651_LEN_WATER_CUR					0x23	//水位
#define SL651_LEN_LIUSU_CUR					0x1b	//流速
#define SL651_LEN_GROUND_WATER				0x1a	//地下水瞬时埋深
#define SL651_LEN_SHANGQING					0x11	//墒情
#define SL651_LEN_SHANGQING_EX				0x08	//墒情(日数据)
#define SL651_LEN_CSQ_EX					0x08
#define SL651_LEN_ERR_CODE_EX				0x18
#define SL651_LEN_WATER_STA_EX				0x08

//基本参数数据定义
#define SL651_LEN_CENTER_ADDR				0x20	//中心站地址
#define SL651_LEN_RTU_ADDR					0x28	//RTU地址
#define SL651_LEN_PW						0x10	//密码
#define SL651_LEN_WORK_MODE					0x08	//工作方式
#define SL651_LEN_SAMPLE_ELEMENT			0x40	//采集要素

//运行参数数据定义
#define SL651_LEN_REPORT_TIMEING			0x08	//定时报时间间隔
#define SL651_LEN_REPORT_ADD				0x08	//加报时间间隔
#define SL651_LEN_RAIN_UNIT					0x09	//雨量分辨力
#define SL651_LEN_WATER_BASE				0x23	//水位基值
#define SL651_LEN_WATER_OFFSET				0x1b	//水位修正值
#define SL651_LEN_CLEAR_OLD_DATA			0x00	//清空历史数据
#define SL651_LEN_RTU_RESTORE				0x00	//恢复出厂设置

//监测要素
#define SL651_BYTES_ELEMENT					8		//监测要素字节数
#define SL651_ELEMENT_QIYA					0		//气压
#define SL651_ELEMENT_DIWEN					1		//地温
#define SL651_ELEMENT_SHIDU					2		//湿度
#define SL651_ELEMENT_QIWEN					3		//气温
#define SL651_ELEMENT_FENGSU				4		//风速
#define SL651_ELEMENT_FENGXIANG				5		//风向
#define SL651_ELEMENT_ZHENGFA				6		//蒸发量
#define SL651_ELEMENT_JIANGSHUI				7		//降水量
#define SL651_ELEMENT_SHUIWEI_1				8		//水位1
#define SL651_ELEMENT_SHUIWEI_2				9		//水位2
#define SL651_ELEMENT_SHUIWEI_3				10		//水位3
#define SL651_ELEMENT_SHUIWEI_4				11		//水位4
#define SL651_ELEMENT_SHUIWEI_5				12		//水位5
#define SL651_ELEMENT_SHUIWEI_6				13		//水位6
#define SL651_ELEMENT_SHUIWEI_7				14		//水位7
#define SL651_ELEMENT_SHUIWEI_8				15		//水位8
#define SL651_ELEMENT_SHUIYA				16		//水压
#define SL651_ELEMENT_LIULIANG				17		//流量
#define SL651_ELEMENT_LIUSU					18		//流速
#define SL651_ELEMENT_SHUILIANG				19		//水量
#define SL651_ELEMENT_ZHAMEN				20		//闸门
#define SL651_ELEMENT_BOLANG				21		//波浪
#define SL651_ELEMENT_TUPIAN				22		//图片
#define SL651_ELEMENT_MAISHEN				23		//地下水埋深
#define SL651_ELEMENT_SHUIBIAO_1			24		//水表1
#define SL651_ELEMENT_SHUIBIAO_2			25		//水表2
#define SL651_ELEMENT_SHUIBIAO_3			26		//水表3
#define SL651_ELEMENT_SHUIBIAO_4			27		//水表4
#define SL651_ELEMENT_SHUIBIAO_5			28		//水表5
#define SL651_ELEMENT_SHUIBIAO_6			29		//水表6
#define SL651_ELEMENT_SHUIBIAO_7			30		//水表7
#define SL651_ELEMENT_SHUIBIAO_8			31		//水表8
#define SL651_ELEMENT_SHANGQING_10CM		32		//10CM墒情
#define SL651_ELEMENT_SHANGQING_20CM		33		//20CM墒情
#define SL651_ELEMENT_SHANGQING_30CM		34		//30CM墒情
#define SL651_ELEMENT_SHANGQING_40CM		35		//40CM墒情
#define SL651_ELEMENT_SHANGQING_50CM		36		//50CM墒情
#define SL651_ELEMENT_SHANGQING_60CM		37		//60CM墒情
#define SL651_ELEMENT_SHANGQING_80CM		38		//80CM墒情
#define SL651_ELEMENT_SHANGQING_100CM		39		//100CM墒情
#define SL651_ELEMENT_SHUIWEN				40		//水温
#define SL651_ELEMENT_ANDAN					41		//氨氮
#define SL651_ELEMENT_GAOMENGSUAN			42		//高锰酸盐指数
#define SL651_ELEMENT_YANGHUA				43		//氧化还原电位
#define SL651_ELEMENT_ZHUODU				44		//浊度
#define SL651_ELEMENT_DIANDAOLV				45		//电导率
#define SL651_ELEMENT_RONGJIEYANG			46		//溶解氧
#define SL651_ELEMENT_PH					47		//PH值
#define SL651_ELEMENT_GE					48		//镉
#define SL651_ELEMENT_ZONGGONG				49		//总汞
#define SL651_ELEMENT_SHEN					50		//砷
#define SL651_ELEMENT_XI					51		//硒
#define SL651_ELEMENT_XIN					52		//锌
#define SL651_ELEMENT_ZONGLIN				53		//总磷
#define SL651_ELEMENT_ZONGDAN				54		//总氮
#define SL651_ELEMENT_ZONGTAN				55		//总有机碳
#define SL651_ELEMENT_QIAN					56		//铅
#define SL651_ELEMENT_TONG					57		//铜
#define SL651_ELEMENT_YELVSU_A				58		//叶绿素a
#define SL651_NUM_ELEMENT					59		//监测要素数量


//状态和报警信息
#define SL651_NUM_RTU_STA					32		//状态信息数量
#define SL651_STA_AC_OFF					0		//交流电充电状态
#define SL651_STA_DC_LACK					1		//蓄电池电压状态
#define SL651_STA_WATER_LEVEL_OVER			2		//水位超限报警状态
#define SL651_STA_FLOW_OVER					3		//流量超限报警状态
#define SL651_STA_WATER_QUALITY_OVER		4		//水质超限报警状态
#define SL651_STA_FLOW_SENSOR_WRONG			5		//流量仪表状态
#define SL651_STA_WATER_SENSOR_WRONG		6		//水位仪表状态
#define SL651_STA_DOOR_CLOSE				7		//终端箱门状态
#define SL651_STA_FLASH_WRONG				8		//存储器状态
#define SL651_STA_IC_EFFECTIVE				9		//IC卡功能状态
#define SL651_STA_PUMP_OFF					10		//水泵工作状态
#define SL651_STA_WATER_OVER				11		//剩余水量报警

//工作模式
#define SL651_MODE_REPORT					1		//自报模式
#define SL651_MODE_REPLY					2		//自报确认模式
#define SL651_MODE_QA						3		//查询应答模式
#define SL651_MODE_DEBUG					4		//调试模式

//事件记录
#define SL651_NUM_ERC_TYPE					32		//事件个数
#define SL651_ERC_CLEAR_OLD_DATA			0		//历史数据初始化记录
#define SL651_ERC_PARAM_CHANGE				1		//参数变更记录
#define SL651_ERC_STATUS_CHANGE				2		//状态量变位记录
#define SL651_ERC_SENSOR_WRONG				3		//仪表故障记录
#define SL651_ERC_PW_CHANGE					4		//密码修改记录
#define SL651_ERC_RTU_WRONG					5		//终端故障记录
#define SL651_ERC_AC_OFF					6		//交流失电记录
#define SL651_ERC_DC_LACK					7		//蓄电池电压低告警记录
#define SL651_ERC_DOOR_OPEN					8		//终端箱门非法打开记录
#define SL651_ERC_PUMP_WRONG				9		//水泵故障记录
#define SL651_ERC_WATER_OVER				10		//剩余水量越限告警记录
#define SL651_ERC_WATER_LEVEL_OVER			11		//水位超限告警记录
#define SL651_ERC_WATER_PRESSURE_OVER		12		//水压超限告警记录
#define SL651_ERC_WATER_QUALITY_OVER		13		//水质参数超限告警记录
#define SL651_ERC_DATA_WRONG				14		//数据出错记录
#define SL651_ERC_SEND_DATA					15		//发报文记录
#define SL651_ERC_RECV_DATA					16		//收报文记录
#define SL651_ERC_SEND_FAIL					17		//发报文出错记录

//参数互斥
#define SL651_EVENT_PARAM_RTU_ADDR			0x01
#define SL651_EVENT_PARAM_PW				0x02
#define SL651_EVENT_PARAM_SN				0x04
#define SL651_EVENT_PARAM_RTU_TYPE			0x08
#define SL651_EVENT_PARAM_ELEMENT			0x10
#define SL651_EVENT_PARAM_WORK_MODE			0x20
#define SL651_EVENT_PARAM_REPORT_HOUR		0x40
#define SL651_EVENT_PARAM_REPORT_MIN		0x80
#define SL651_EVENT_PARAM_ERC				0x100
#define SL651_EVENT_PARAM_DEVICE_ID			0x200
#define SL651_EVENT_PARAM_RANDOM_PERIOD		0x400
#define SL651_EVENT_PARAM_ALL				0x7ff



//数据下载
typedef struct sl651_data_download
{
	rt_uint8_t			comm_type;
	rt_uint8_t			ch;
	rt_uint8_t			center_addr;
	rt_uint16_t			element_boot;
	rt_uint8_t			element_len;
	rt_uint32_t			unix_low;
	rt_uint32_t			unix_high;
	rt_uint32_t			unix_step;
} SL651_DATA_DOWNLOAD;

//人工置数
typedef struct sl651_manual_data
{
	rt_uint8_t			data[SL651_BYTES_MANUAL_DATA];
	rt_uint16_t			data_len;
} SL651_MANUAL_DATA;

//设备识别码
typedef struct sl651_device_id
{
	rt_uint8_t			data[SL651_BYTES_DEVICE_ID];
	rt_uint8_t			data_len;
} SL651_DEVICE_ID;

//告警信息
typedef struct sl651_warn_info
{
	rt_uint16_t			valid;
	rt_uint16_t			data_len;
	rt_uint8_t			data[SL651_BYTES_WARN_INFO_MAX];
} SL651_WARN_INFO;



//参数配置
void sl651_get_rtu_addr(rt_uint8_t *rtu_addr_ptr);
void sl651_set_rtu_addr(rt_uint8_t const *rtu_addr_ptr);

rt_uint16_t sl651_get_pw(void);
void sl651_set_pw(rt_uint16_t pw);

rt_uint16_t sl651_get_sn(void);
void sl651_set_sn(rt_uint16_t sn);

rt_uint8_t sl651_get_rtu_type(void);
void sl651_set_rtu_type(rt_uint8_t rtu_type);

#if 0
void sl651_get_element(rt_uint8_t *pelement);
void sl651_set_element(rt_uint8_t const *pelement);

rt_uint8_t sl651_get_work_mode(void);
void sl651_set_work_mode(rt_uint8_t work_mode);

rt_uint8_t sl651_get_device_id(rt_uint8_t *pid);
void sl651_set_device_id(rt_uint8_t const *pid, rt_uint8_t id_len);
#endif

rt_uint8_t sl651_get_report_hour(void);
void sl651_set_report_hour(rt_uint8_t period);

rt_uint8_t sl651_get_report_min(void);
void sl651_set_report_min(rt_uint8_t period);

rt_uint16_t sl651_get_random_period(void);
void sl651_set_random_period(rt_uint16_t period);

void sl651_set_center_comm(rt_uint8_t center, rt_uint8_t prio, rt_uint8_t const *pdata, rt_uint8_t data_len);

rt_uint16_t sl651_get_warn_info(rt_uint8_t *pdata, rt_uint8_t warn_level);
void sl651_set_warn_info(rt_uint8_t const *pdata, rt_uint16_t data_len, rt_uint8_t warn_level);



//参数重置
void sl651_param_restore(void);

//数据合法性检测函数
rt_uint8_t sl651_data_valid(rt_uint8_t const *pdata, rt_uint16_t data_len, rt_uint16_t data_id, rt_uint8_t reply_sta, rt_uint8_t fcb_value);

//数据更新函数
rt_uint8_t sl651_data_update(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t center_addr, struct tm time);

//心跳包编码函数
rt_uint16_t sl651_heart_encoder(rt_uint8_t *pdata, rt_uint16_t data_len, rt_uint8_t ch);

//数据解包
rt_uint8_t sl651_data_decoder(PTCL_REPORT_DATA *report_data_ptr, PTCL_RECV_DATA const *recv_data_ptr, PTCL_PARAM_INFO **param_info_ptr_ptr);

rt_uint8_t sl651_report_decoder(PTCL_REPORT_DATA *report_data_ptr, PTCL_RECV_DATA const *recv_data_ptr);



#endif

