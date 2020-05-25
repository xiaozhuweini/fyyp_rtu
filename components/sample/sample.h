/*存储空间
**EEPROM		:1400--1999
**FLASH			:1150976--11636735(10MB)	11636736--22122495(10MB)
*/



#ifndef __SAMPLE_H__
#define __SAMPLE_H__



#include "rtthread.h"
#include "user_priority.h"
#include "drv_pin.h"



//基地址
#define SAMPLE_BASE_ADDR_PARAM			1400			//参数存储基地址
#define SAMPLE_BASE_ADDR_DATA			1150976			//数据存储基地址
#define SAMPLE_BASE_ADDR_DATA_EX		11636736		//扩展数据存储基地址

//配置信息
#define SAMPLE_NUM_CTRL_BLK				10				//采集控制块数量
#define SAMPLE_PIN_ADC_BATT				PIN_INDEX_46
#define SAMPLE_PIN_ADC_VIN				PIN_INDEX_47
#if 0
#define SAMPLE_TASK_STORE_EN
#endif

//任务
#define SAMPLE_STK_DATA_STORE			640				//数据存储任务栈空间
#define SAMPLE_STK_QUERY_DATA_EX		640
#ifndef SAMPLE_PRIO_DATA_STORE
#define SAMPLE_PRIO_DATA_STORE			30
#endif
#ifndef SAMPLE_PRIO_QUERY_DATA_EX
#define SAMPLE_PRIO_QUERY_DATA_EX		24
#endif

//串口采集接收信息
#define SAMPLE_BYTES_COM_RECV			96				//串口接收字节空间
#define SAMPLE_ERROR_TIMES				2				//采集错误次数
#define SAMPLE_TIME_DATA_NO				1500			//毫秒
#define SAMPLE_TIME_DATA_END			200				//毫秒

//数据存储信息
#define SAMPLE_BYTES_PER_ITEM			11				//一条数据字节数
#define SAMPLE_NUM_TOTAL_ITEM			953250			//数据条总数
#define SAMPLE_BYTES_PER_ITEM_EX		512				//一条数据字节数
#define SAMPLE_NUM_TOTAL_ITEM_EX		20480			//数据条总数
#define SAMPLE_BYTES_ITEM_INFO_EX		9				//数据条信息(年 月 日 时 分 CN LEN)

//传感器类型(低4位,所以最多16种传感器类型)
#define SAMPLE_SENSOR_WUSHUI			0				//污水
#define SAMPLE_SENSOR_COD				1				//化学需氧量
#define SAMPLE_NUM_SENSOR_TYPE			2				//传感器类型总数
#define SAMPLE_IS_SENSOR_TYPE(VAL)		((VAL & 0x0f) < SAMPLE_NUM_SENSOR_TYPE)

//传感器数据种类(高4位)
#define SAMPLE_SENSOR_DATA_CUR			(0 << 4)
#define SAMPLE_SENSOR_DATA_MIN_H		(1 << 4)
#define SAMPLE_SENSOR_DATA_MIN_D		(2 << 4)
#define SAMPLE_SENSOR_DATA_AVG_H		(3 << 4)
#define SAMPLE_SENSOR_DATA_AVG_D		(4 << 4)
#define SAMPLE_SENSOR_DATA_MAX_H		(5 << 4)
#define SAMPLE_SENSOR_DATA_MAX_D		(6 << 4)
#define SAMPLE_SENSOR_DATA_COU_M		(7 << 4)
#define SAMPLE_SENSOR_DATA_COU_H		(8 << 4)
#define SAMPLE_SENSOR_DATA_COU_D		(9 << 4)

//硬件接口
#define SAMPLE_HW_RS485_1				0
#define SAMPLE_HW_RS485_2				1
#define SAMPLE_HW_RS232_1				2
#define SAMPLE_HW_RS232_2				3
#define SAMPLE_HW_RS232_3				4
#define SAMPLE_HW_RS232_4				5
#define SAMPLE_HW_ADC					6
#define SAMPLE_NUM_HW_PORT				7

//协议
#define SAMPLE_PTCL_A					0
#define SAMPLE_NUM_PTCL					1

//事件标志
#define SAMPLE_EVENT_CTRL_BLK			0x01			//控制块
#define SAMPLE_EVENT_HW_PORT			0x10000			//硬件端口用事件
#define SAMPLE_EVENT_DATA_POINTER		0x80000000		//存储数据指针
#define SAMPLE_EVENT_SUPPLY_VOLT		0x40000000
#define SAMPLE_EVENT_DATA_POINTER_EX	0x20000000		//扩展存储数据指针
#define SAMPLE_EVENT_INIT_VALUE			0xe000ffff



//数据定义
typedef struct sample_data_type
{
	rt_uint8_t			sta;							//状态(是否有效)
	float				value;							//数值
} SAMPLE_DATA_TYPE;

//串口接收
typedef struct sample_com_recv
{
	rt_uint32_t			recv_event;						//接收事件
	rt_uint8_t			*pdata;							//数据
	rt_uint16_t			data_len;						//数据长度
} SAMPLE_COM_RECV;

//采集控制块
typedef struct sample_ctrl_blk
{
	rt_uint8_t			sensor_type;					//传感器类型
	rt_uint8_t			sensor_addr;					//传感器地址
	rt_uint8_t			hw_port;						//硬件接口
	rt_uint8_t			protocol;						//协议
	rt_uint32_t			hw_rate;						//硬件速率
	rt_uint16_t			store_period;					//存储间隔
	rt_uint8_t			k_opt;
	float				k;								//1次系数
	float				b;								//0次系数
	float				base;							//基值
	float				offset;							//修正值
	float				up;								//上限
	float				down;							//下限
	float				threshold;						//阈值
} SAMPLE_CTRL_BLK;

//数据指针
typedef struct sample_data_pointer
{
	rt_uint8_t			sta;							//状态(是否覆盖)
	rt_uint32_t			p;								//指向位置
} SAMPLE_DATA_POINTER;

//扩展数据查询信息
typedef struct sample_query_info_ex
{
	rt_uint16_t			cn;
	rt_uint32_t			unix_low;
	rt_uint32_t			unix_high;
	rt_mailbox_t		pmb;
} SAMPLE_QUERY_INFO_EX;

//参数
typedef struct sample_param_set
{
	SAMPLE_DATA_POINTER	data_pointer;
	SAMPLE_CTRL_BLK		ctrl_blk[SAMPLE_NUM_CTRL_BLK];
	SAMPLE_DATA_POINTER	data_pointer_ex;
} SAMPLE_PARAM_SET;

//数据编码函数(数据查询时用)
typedef rt_uint16_t (*SAMPLE_FUN_DATA_FORMAT)(rt_uint8_t *pdst, rt_uint8_t sensor_type, rt_uint8_t sta, float value);



//参数设置、读取
void sample_set_sensor_type(rt_uint8_t ctrl_num, rt_uint8_t sensor_type);
rt_uint8_t sample_get_sensor_type(rt_uint8_t ctrl_num);

void sample_set_sensor_addr(rt_uint8_t ctrl_num, rt_uint8_t sensor_addr);
rt_uint8_t sample_get_sensor_addr(rt_uint8_t ctrl_num);

void sample_set_hw_port(rt_uint8_t ctrl_num, rt_uint8_t hw_port);
rt_uint8_t sample_get_hw_port(rt_uint8_t ctrl_num);

void sample_set_protocol(rt_uint8_t ctrl_num, rt_uint8_t protocol);
rt_uint8_t sample_get_protocol(rt_uint8_t ctrl_num);

void sample_set_hw_rate(rt_uint8_t ctrl_num, rt_uint32_t hw_rate);
rt_uint32_t sample_get_hw_rate(rt_uint8_t ctrl_num);

void sample_set_store_period(rt_uint8_t ctrl_num, rt_uint16_t store_period);
rt_uint16_t sample_get_store_period(rt_uint8_t ctrl_num);

void sample_set_kopt(rt_uint8_t ctrl_num, rt_uint8_t kopt);
rt_uint8_t sample_get_kopt(rt_uint8_t ctrl_num);

void sample_set_k(rt_uint8_t ctrl_num, float value);
float sample_get_k(rt_uint8_t ctrl_num);

void sample_set_b(rt_uint8_t ctrl_num, float value);
float sample_get_b(rt_uint8_t ctrl_num);

void sample_set_base(rt_uint8_t ctrl_num, float value);
float sample_get_base(rt_uint8_t ctrl_num);

void sample_set_offset(rt_uint8_t ctrl_num, float value);
float sample_get_offset(rt_uint8_t ctrl_num);

void sample_set_up(rt_uint8_t ctrl_num, float value);
float sample_get_up(rt_uint8_t ctrl_num);

void sample_set_down(rt_uint8_t ctrl_num, float value);
float sample_get_down(rt_uint8_t ctrl_num);

void sample_set_threshold(rt_uint8_t ctrl_num, float value);
float sample_get_threshold(rt_uint8_t ctrl_num);

//恢复出厂
void sample_param_restore(void);

//是否存在
rt_uint8_t sample_sensor_type_exist(rt_uint8_t sensor_type);

//得到控制块
rt_uint8_t sample_get_ctrl_num_by_sensor_type(rt_uint8_t sensor_type);

//数据采集
SAMPLE_DATA_TYPE sample_get_cur_data(rt_uint8_t sensor_type);

//数据清空
void sample_clear_old_data(void);

//数据查询
rt_uint16_t sample_query_data(rt_uint8_t *pdata, rt_uint8_t sensor_type, rt_uint32_t unix_low, rt_uint32_t unix_high, rt_uint32_t unix_step, SAMPLE_FUN_DATA_FORMAT fun_data_format);
rt_uint8_t sample_query_data_ex(rt_uint16_t cn, rt_uint32_t unix_low, rt_uint32_t unix_high, rt_mailbox_t pmb);

//供电电压，0.01V
rt_uint16_t sample_supply_volt(rt_uint16_t adc_pin);

//存储数据
void sample_store_data(struct tm const *ptime, rt_uint8_t sensor_type, SAMPLE_DATA_TYPE const *pdata);
void sample_store_data_ex(struct tm const *ptime, rt_uint16_t cn, rt_uint16_t data_len, rt_uint8_t const *pdata);

void sample_query_min(rt_uint8_t sensor_type, rt_uint32_t unix_low, rt_uint32_t unix_high, rt_uint32_t unix_step, SAMPLE_DATA_TYPE *pval);
void sample_query_max(rt_uint8_t sensor_type, rt_uint32_t unix_low, rt_uint32_t unix_high, rt_uint32_t unix_step, SAMPLE_DATA_TYPE *pval);
void sample_query_avg(rt_uint8_t sensor_type, rt_uint32_t unix_low, rt_uint32_t unix_high, rt_uint32_t unix_step, SAMPLE_DATA_TYPE *pval);
void sample_query_cou(rt_uint8_t sensor_type, rt_uint32_t unix_low, rt_uint32_t unix_high, rt_uint32_t unix_step, SAMPLE_DATA_TYPE *pval);



#endif

