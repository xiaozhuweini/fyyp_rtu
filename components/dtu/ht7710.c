/*使用说明
**1、宏电DTU配置的是：【空闲下线】【唤醒上线】【电话、数据唤醒】【宏电心跳包间隔0】；
**2、推荐使用模式：【DTU配置为DTU_MODE_ONLINE】【DTU心跳包为0】,特点如下：
**		-DTU驱动不会下线，发送数据时就不用连接IP，连接IP其实就是设置宏电IP地址并重启，需要漫长的时间；
**		-DTU驱动不会发送心跳包，宏电在空闲状态下可以自主下线，这样即实现了掉线模式；
**		-如需要实现在线，将DTU心跳包设置不为0即可；
**		-此配置模式不能实现宏电掉电功能，需要将DTU配置为DTU_MODE_PWR模式，这适合在数据发送间隔较大的情况，如1小时
**3、掉线任务其实就只是将连接状态清0，DTU_STA_IP状态清除，所以执行掉线任务时，宏电DTU并未真正掉线，不过宏电DTU后面会自主掉线的，不选择关闭IP的方式断开连接，是因为电话唤醒功能，有电话的时候宏电DTU会直接上线，但是如果关闭IP后，来了电话就无法上线了；
*/

/*宏电配置
**使用宏电配置软件m2mtoolsbox进行配置，宏电DTU默认波特率是57600，宏电配的线RX、TX与DTU上标的RX、TX是交叉连接的。
**1、波特率配置为9600；
**2、启用RDP协议，即选择En；
**3、启动RDP下行协议，即选择GPRS+SMS；
**4、服务中心号码改为0001；
**5、调试信息关闭；
**6、清空所有通道的自定义注册包、自定义心跳包和通道服务号码。
*/



/*计时，秒
**
*/
#define DTU_TIME_PWR_OFF			30u						//断电时间
#define DTU_TIME_PWR_ON				50u						//上电时间
#define DTU_TIME_STA_OFF			3u						//关机时间
#define DTU_TIME_STA_ON				10u						//开机时间
#define DTU_TIME_SET_PARAM			3u						//设置参数指令超时时间
#define DTU_TIME_SAVE_PARAM			10u						//保存参数指令超时时间
#define DTU_TIME_RESET_START		10u						//开始复位超时时间
#define DTU_TIME_RESET_END			60u						//结束复位超时时间
#define DTU_TIME_SMS_SEND			15u						//SMS发送超时时间
#define DTU_TIME_IP_SEND			1u						//IP发送超时时间
#define DTU_TIME_BOOT_SMS			10u						//BOOT_SMS完成后的延时时间
#define DTU_TIME_QUERY_STA			3u						//查询状态超时时间

/*字节空间
**
*/
#define DTU_BYTES_AT_SEND			65u						//AT发送指令

/*事件标志组
**
*/
#define DTU_FLAG_NONE_YES			0x01u
#define DTU_FLAG_NONE_NO			0x02u
#define DTU_FLAG_SET_OK				0x04u
#define DTU_FLAG_SET_ERROR			0x08u
#define DTU_FLAG_SAVE_OK			0x10u
#define DTU_FLAG_SAVE_ERROR			0x20u
#define DTU_FLAG_RESET_OK			0x40u
#define DTU_FLAG_RESET_ERROR		0x80u
#define DTU_FLAG_SMS_OK				0x100u
#define DTU_FLAG_SMS_ERROR			0x200u
#define DTU_FLAG_IP_OK				0x400u
#define DTU_FLAG_IP_ERROR			0x800u
#define DTU_FLAG_MODULE_OK			0x1000u
#define DTU_FLAG_MODULE_ERROR		0x2000u
#define DTU_FLAG_CSQ				0x4000u



static INT8U			m_atSend[DTU_BYTES_AT_SEND];		//AT发送指令空间
static INT8U			m_atSendLen;						//AT发送指令长度



/*帧结构
**
*/
#define HT_PACKET_HEAD				0x7Du		//包头
#define HT_PACKET_END				0x7Fu		//包尾
#define HT_BYTES_HEAD				3u			//包头字节数
#define HT_BYTES_LEN				2u			//长度字节数
#define HT_BYTES_AFN				2u			//功能码字节数
#define HT_BYTES_CRC				1u			//校验码字节数
#define HT_BYTES_END				3u			//包尾字节数
#define HT_BYTES_BASIC_FRAME		11u			//基本帧结构字节数HT_BYTES_HEAD + HT_BYTES_LEN + HT_BYTES_AFN + HT_BYTES_CRC + HT_BYTES_END
#define HT_BYTES_SMS_ADDR			32u			//短信内容中号码的字节数

/*功能码
**
*/
#define HT_AFNU_OPEN_RDP			0x00u		//临时打开RDP
#define HT_AFNU_QUERY_PARAM			0x01u		//查询参数
#define HT_AFNU_SET_PARAM			0x02u		//设置参数
#define HT_AFNU_SAVE_PARAM			0x03u		//保存参数
#define HT_AFNU_RESET				0x04u		//复位
#define HT_AFNU_SEND_GPRS			0x05u		//发送GPRS
#define HT_AFNU_SEND_SMS			0x06u		//发送SMS
#define HT_AFNU_QUERY_STA			0x07u		//查询状态
#define HT_AFND_OPEN_RDP			0x80u		//临时打开RDP
#define HT_AFND_QUERY_PARAM			0x81u		//查询参数
#define HT_AFND_SET_PARAM			0x82u		//设置参数
#define HT_AFND_SAVE_PARAM			0x83u		//保存参数
#define HT_AFND_RESET				0x84u		//复位
#define HT_AFND_SEND_GPRS			0x85u		//发送GPRS
#define HT_AFND_SEND_SMS			0x86u		//发送SMS
#define HT_AFND_QUERY_STA			0x87u		//查询状态

/*参数ID
**
*/
#define HT_IDH_PARAM_RTU			0x03u		//RTU参数
#define HT_IDH_PARAM_SMS			0x04u		//短信参数
#define HT_IDH_PARAM_RUN			0x05u		//运行参数
#define HT_IDH_PARAM_CH1			0xC8u		//通道1参数
#define HT_IDH_PARAM_CH2			0xC9u		//通道2参数
#define HT_IDH_PARAM_CH3			0xCAu		//通道3参数
#define HT_IDH_PARAM_CH4			0xCBu		//通道4参数
#define HT_IDH_PARAM_STA			0xF0u		//状态参数

#define HT_IDL_DATA_MAX				0x07u		//最大数据包
#define HT_IDL_DATA_TIME			0x08u		//数据包间隔时间
#define HT_IDL_SMS_TYPE				0x02u		//短信编码方式
#define HT_IDL_ONLINE_TYPE			0x03u		//上线方式
#define HT_IDL_WAKE_TYPE			0x04u		//唤醒方式
#define HT_IDL_OFFLINE_TIME			0x06u		//下线时间
#define HT_IDL_OFFLINE_TYPE			0x07u		//下线方式
#define HT_IDL_CSQ_MIN				0x08u		//最低信号强度
#define HT_IDL_CONN_TIMES			0x0Au		//重连接次数
#define HT_IDL_CONN_PERIOD			0x0Bu		//重连时间间隔
#define HT_IDL_RUN_MODE				0x0Cu		//运行模式
#define HT_IDL_CONN_TYPE			0x01u		//连接方式
#define HT_IDL_IP_ADDR				0x02u		//IP地址
#define HT_IDL_IP_PORT				0x03u		//IP端口
#define HT_IDL_DOMAIN_NAME			0x04u		//域名
#define HT_IDL_HEART_PERIOD			0x06u		//心跳包周期
#define HT_IDL_SERVICE_SMS			0x0Cu		//通道的服务号码
#define HT_IDL_CSQ_VALUE			0x04u		//信号强度
#define HT_IDL_MODULE_STA			0x01u		//模块状态
#define HT_IDL_SMS_SEND_STA			0x0Eu		//短信发送状态

/*参数长度
**
*/
#define HT_LEN_DATA_MAX				2u			//最大数据包
#define HT_LEN_DATA_TIME			2u			//数据包间隔时间
#define HT_LEN_SMS_TYPE				1u			//短信编码方式
#define HT_LEN_ONLINE_TYPE			1u			//上线方式
#define HT_LEN_WAKE_TYPE			1u			//唤醒方式
#define HT_LEN_OFFLINE_TIME			2u			//下线时间
#define HT_LEN_OFFLINE_TYPE			1u			//下线方式
#define HT_LEN_CSQ_MIN				1u			//拨号最低信号强度
#define HT_LEN_CONN_TIMES			1u			//重连接次数
#define HT_LEN_CONN_PERIOD			2u			//重连接时间
#define HT_LEN_RUN_MODE				1u			//运行模式
#define HT_LEN_HEART_PERIOD			2u			//心跳包时间间隔
#define HT_LEN_CONN_TYPE			1u			//连接方式
#define HT_LEN_IP_ADDR				4u			//IP地址
#define HT_LEN_IP_PORT				2u			//端口号

/*短信编码方式
**
*/
#define HT_SMS_TYPE_7BIT			0u			//7BIT
#define HT_SMS_TYPE_8BIT			1u			//8BIT
#define HT_SMS_TYPE_UCS2			2u			//UCS2

/*上线方式
**
*/
#define HT_ONLINE_TYPE_AUTO			0u			//自动上线
#define HT_ONLINE_TYPE_WAKE			1u			//等待唤醒

/*唤醒方式
**
*/
#define HT_WAKE_TYPE_SMS			1u			//SMS唤醒
#define HT_WAKE_TYPE_CALL			2u			//CALL唤醒
#define HT_WAKE_TYPE_DATA			4u			//DATA唤醒

/*下线方式
**
*/
#define HT_OFFLINE_TYPE_IDLE		0u			//空闲下线
#define HT_OFFLINE_TYPE_TIME		1u			//定时下线

/*运行模式
**
*/
#define HT_RUN_MODE_MUL				0u			//多通道
#define HT_RUN_MODE_MULB			1u			//多通道主备

/*连接方式
**
*/
#define HT_CONN_TYPE_UDP			0u			//UDP
#define HT_CONN_TYPE_TCP			1u			//TCP

/*基本配置
**
*/
#define HT_RTU_DATA_MAX				1024u		//最大数据包
#define HT_RTU_DATA_TIME			500u		//数据包间隔时间，毫秒
#define HT_RUN_OFFLINE_TIME			60u			//下线时间，秒
#define HT_RUN_CSQ_MIN				5u			//拨号最低信号强度
#define HT_RUN_CONN_TIMES			3u			//重连接次数
#define HT_RUN_CONN_PERIOD			30u			//重连接间隔
#define HT_CH_HEART_PERIOD			0u			//心跳包间隔



/*最大数据包
**
*/
static INT8U CMD_DataMax(INT8U *pCMD, INT16U dataMax)
{
	INT8U cmdLen = 0u;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	pCMD[cmdLen++] = HT_IDH_PARAM_RTU;
	pCMD[cmdLen++] = HT_IDL_DATA_MAX;
	pCMD[cmdLen++] = (INT8U)(HT_LEN_DATA_MAX >> 8u);
	pCMD[cmdLen++] = (INT8U)HT_LEN_DATA_MAX;
	pCMD[cmdLen++] = (INT8U)dataMax;
	pCMD[cmdLen++] = (INT8U)(dataMax >> 8u);
	
	return cmdLen;
}

/*数据包间隔时间
**
*/
static INT8U CMD_DataTime(INT8U *pCMD, INT16U dataTime)
{
	INT8U cmdLen = 0u;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	pCMD[cmdLen++] = HT_IDH_PARAM_RTU;
	pCMD[cmdLen++] = HT_IDL_DATA_TIME;
	pCMD[cmdLen++] = (INT8U)(HT_LEN_DATA_TIME >> 8u);
	pCMD[cmdLen++] = (INT8U)HT_LEN_DATA_TIME;
	pCMD[cmdLen++] = (INT8U)dataTime;
	pCMD[cmdLen++] = (INT8U)(dataTime >> 8u);
	
	return cmdLen;
}

/*短信编码方式
**
*/
static INT8U CMD_SmsType(INT8U *pCMD, INT8U smsType)
{
	INT8U cmdLen = 0u;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	pCMD[cmdLen++] = HT_IDH_PARAM_SMS;
	pCMD[cmdLen++] = HT_IDL_SMS_TYPE;
	pCMD[cmdLen++] = (INT8U)(HT_LEN_SMS_TYPE >> 8u);
	pCMD[cmdLen++] = (INT8U)HT_LEN_SMS_TYPE;
	pCMD[cmdLen++] = smsType;
	
	return cmdLen;
}

/*上线方式
**
*/
static INT8U CMD_OnlineType(INT8U *pCMD, INT8U onlineType)
{
	INT8U cmdLen = 0u;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	pCMD[cmdLen++] = HT_IDH_PARAM_RUN;
	pCMD[cmdLen++] = HT_IDL_ONLINE_TYPE;
	pCMD[cmdLen++] = (INT8U)(HT_LEN_ONLINE_TYPE >> 8u);
	pCMD[cmdLen++] = (INT8U)HT_LEN_ONLINE_TYPE;
	pCMD[cmdLen++] = onlineType;
	
	return cmdLen;
}

/*唤醒方式
**
*/
static INT8U CMD_WakeType(INT8U *pCMD, INT8U wakeType)
{
	INT8U cmdLen = 0u;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	pCMD[cmdLen++] = HT_IDH_PARAM_RUN;
	pCMD[cmdLen++] = HT_IDL_WAKE_TYPE;
	pCMD[cmdLen++] = (INT8U)(HT_LEN_WAKE_TYPE >> 8u);
	pCMD[cmdLen++] = (INT8U)HT_LEN_WAKE_TYPE;
	pCMD[cmdLen++] = wakeType;
	
	return cmdLen;
}

/*下线时间
**
*/
static INT8U CMD_OfflineTime(INT8U *pCMD, INT16U offlineTime)
{
	INT8U cmdLen = 0u;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	pCMD[cmdLen++] = HT_IDH_PARAM_RUN;
	pCMD[cmdLen++] = HT_IDL_OFFLINE_TIME;
	pCMD[cmdLen++] = (INT8U)(HT_LEN_OFFLINE_TIME >> 8u);
	pCMD[cmdLen++] = (INT8U)HT_LEN_OFFLINE_TIME;
	pCMD[cmdLen++] = (INT8U)offlineTime;
	pCMD[cmdLen++] = (INT8U)(offlineTime >> 8u);
	
	return cmdLen;
}

/*下线方式
**
*/
static INT8U CMD_OfflineType(INT8U *pCMD, INT8U offlineType)
{
	INT8U cmdLen = 0u;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	pCMD[cmdLen++] = HT_IDH_PARAM_RUN;
	pCMD[cmdLen++] = HT_IDL_OFFLINE_TYPE;
	pCMD[cmdLen++] = (INT8U)(HT_LEN_OFFLINE_TYPE >> 8u);
	pCMD[cmdLen++] = (INT8U)HT_LEN_OFFLINE_TYPE;
	pCMD[cmdLen++] = offlineType;
	
	return cmdLen;
}

/*拨号最低信号强度
**
*/
static INT8U CMD_CsqMin(INT8U *pCMD, INT8U csqValue)
{
	INT8U cmdLen = 0u;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	pCMD[cmdLen++] = HT_IDH_PARAM_RUN;
	pCMD[cmdLen++] = HT_IDL_CSQ_MIN;
	pCMD[cmdLen++] = (INT8U)(HT_LEN_CSQ_MIN >> 8u);
	pCMD[cmdLen++] = (INT8U)HT_LEN_CSQ_MIN;
	pCMD[cmdLen++] = csqValue;
	
	return cmdLen;
}

/*重连接次数
**
*/
static INT8U CMD_ConnTimes(INT8U *pCMD, INT8U connTimes)
{
	INT8U cmdLen = 0u;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	pCMD[cmdLen++] = HT_IDH_PARAM_RUN;
	pCMD[cmdLen++] = HT_IDL_CONN_TIMES;
	pCMD[cmdLen++] = (INT8U)(HT_LEN_CONN_TIMES >> 8u);
	pCMD[cmdLen++] = (INT8U)HT_LEN_CONN_TIMES;
	pCMD[cmdLen++] = connTimes;
	
	return cmdLen;
}

/*重连接时间
**
*/
static INT8U CMD_ConnPeriod(INT8U *pCMD, INT16U connPeriod)
{
	INT8U cmdLen = 0u;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	pCMD[cmdLen++] = HT_IDH_PARAM_RUN;
	pCMD[cmdLen++] = HT_IDL_CONN_PERIOD;
	pCMD[cmdLen++] = (INT8U)(HT_LEN_CONN_PERIOD >> 8u);
	pCMD[cmdLen++] = (INT8U)HT_LEN_CONN_PERIOD;
	pCMD[cmdLen++] = (INT8U)connPeriod;
	pCMD[cmdLen++] = (INT8U)(connPeriod >> 8u);
	
	return cmdLen;
}

/*运行模式
**
*/
static INT8U CMD_RunMode(INT8U *pCMD, INT8U runMode)
{
	INT8U cmdLen = 0u;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	pCMD[cmdLen++] = HT_IDH_PARAM_RUN;
	pCMD[cmdLen++] = HT_IDL_RUN_MODE;
	pCMD[cmdLen++] = (INT8U)(HT_LEN_RUN_MODE >> 8u);
	pCMD[cmdLen++] = (INT8U)HT_LEN_RUN_MODE;
	pCMD[cmdLen++] = runMode;
	
	return cmdLen;
}

/*心跳包间隔
**
*/
static INT8U CMD_HeartPeriod(INT8U *pCMD, INT8U ipCh, INT16U heartPeriod)
{
	INT8U cmdLen = 0u;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	pCMD[cmdLen++] = HT_IDH_PARAM_CH1 + ipCh;
	pCMD[cmdLen++] = HT_IDL_HEART_PERIOD;
	pCMD[cmdLen++] = (INT8U)(HT_LEN_HEART_PERIOD >> 8u);
	pCMD[cmdLen++] = (INT8U)HT_LEN_HEART_PERIOD;
	pCMD[cmdLen++] = (INT8U)heartPeriod;
	pCMD[cmdLen++] = (INT8U)(heartPeriod >> 8u);
	
	return cmdLen;
}

/*设置指令
**
*/
static INT8U CMD_ParamSet(INT8U *pCMD, INT8U cmdNum)
{
	INT8U cmdLen = 0u, posLen;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	//包头
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	//长度
	posLen = cmdLen;
	pCMD[cmdLen++] = 0u;
	pCMD[cmdLen++] = 0u;
	//功能码
	pCMD[cmdLen++] = HT_AFNU_SET_PARAM;
	pCMD[cmdLen++] = 0u;
	switch(cmdNum)
	{
	case 0u:
		//最大数据包6
		cmdLen += CMD_DataMax(&pCMD[cmdLen], HT_RTU_DATA_MAX);
		//数据包间隔时间6
		cmdLen += CMD_DataTime(&pCMD[cmdLen], HT_RTU_DATA_TIME);
		//短信编码方式5
		cmdLen += CMD_SmsType(&pCMD[cmdLen], HT_SMS_TYPE_8BIT);
		//上线方式5
		cmdLen += CMD_OnlineType(&pCMD[cmdLen], HT_ONLINE_TYPE_WAKE);
		//唤醒方式5
		cmdLen += CMD_WakeType(&pCMD[cmdLen], HT_WAKE_TYPE_DATA);
		//下线时间6
		cmdLen += CMD_OfflineTime(&pCMD[cmdLen], HT_RUN_OFFLINE_TIME);
		//下线方式5
		cmdLen += CMD_OfflineType(&pCMD[cmdLen], HT_OFFLINE_TYPE_IDLE);
		//拨号最低信号强度5
		cmdLen += CMD_CsqMin(&pCMD[cmdLen], HT_RUN_CSQ_MIN);
		//重连接次数5
		cmdLen += CMD_ConnTimes(&pCMD[cmdLen], HT_RUN_CONN_TIMES);
		break;
	case 1u:
		//重连接时间6
		cmdLen += CMD_ConnPeriod(&pCMD[cmdLen], HT_RUN_CONN_PERIOD);
		//运行模式5
		cmdLen += CMD_RunMode(&pCMD[cmdLen], HT_RUN_MODE_MUL);
		//心跳包间隔24
		cmdLen += CMD_HeartPeriod(&pCMD[cmdLen], 0u, HT_CH_HEART_PERIOD);
		cmdLen += CMD_HeartPeriod(&pCMD[cmdLen], 1u, HT_CH_HEART_PERIOD);
		cmdLen += CMD_HeartPeriod(&pCMD[cmdLen], 2u, HT_CH_HEART_PERIOD);
		cmdLen += CMD_HeartPeriod(&pCMD[cmdLen], 3u, HT_CH_HEART_PERIOD);
		break;
	default:
		return 0u;
	}
	//包尾
	pCMD[cmdLen++] = 0u;
	pCMD[cmdLen++] = HT_PACKET_END;
	pCMD[cmdLen++] = HT_PACKET_END;
	pCMD[cmdLen++] = HT_PACKET_END;
	//长度
	pCMD[posLen++] = (INT8U)(cmdLen >> 8u);
	pCMD[posLen++] = (INT8U)cmdLen;
	
	return cmdLen;
}

/*保存参数
**
*/
static INT8U CMD_SaveParam(INT8U *pCMD)
{
	INT8U cmdLen = 0u, posLen;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	//包头
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	//长度
	posLen = cmdLen;
	pCMD[cmdLen++] = 0u;
	pCMD[cmdLen++] = 0u;
	//功能码
	pCMD[cmdLen++] = HT_AFNU_SAVE_PARAM;
	pCMD[cmdLen++] = 0u;
	//包尾
	pCMD[cmdLen++] = 0u;
	pCMD[cmdLen++] = HT_PACKET_END;
	pCMD[cmdLen++] = HT_PACKET_END;
	pCMD[cmdLen++] = HT_PACKET_END;
	//长度
	pCMD[posLen++] = (INT8U)(cmdLen >> 8u);
	pCMD[posLen++] = (INT8U)cmdLen;
	
	return cmdLen;
}

/*连接某IP通道
**
*/
static INT8U CMD_ConnIP(INT8U *pCMD, INT8U ipCh, DTU_IP_ADDR const *pIpAddr, INT8U isUDP)
{
	INT8U	cmdLen = 0u, posLen, pos;
	INT16U	port;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	if((DTU_IP_ADDR *)0u == pIpAddr)
	{
		return 0u;
	}
	
	//包头
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	//长度
	posLen = cmdLen;
	pCMD[cmdLen++] = 0u;
	pCMD[cmdLen++] = 0u;
	//功能码
	pCMD[cmdLen++] = HT_AFNU_SET_PARAM;
	pCMD[cmdLen++] = 0u;
	//数据域
	//连接方式
	pCMD[cmdLen++] = HT_IDH_PARAM_CH1 + ipCh;
	pCMD[cmdLen++] = HT_IDL_CONN_TYPE;
	pCMD[cmdLen++] = (INT8U)(HT_LEN_CONN_TYPE >> 8u);
	pCMD[cmdLen++] = (INT8U)HT_LEN_CONN_TYPE;
	pCMD[cmdLen++] = (OS_TRUE == isUDP) ? HT_CONN_TYPE_UDP : HT_CONN_TYPE_TCP;
	//IP地址
	pCMD[cmdLen++] = HT_IDH_PARAM_CH1 + ipCh;
	pCMD[cmdLen++] = HT_IDL_IP_ADDR;
	pCMD[cmdLen++] = (INT8U)(HT_LEN_IP_ADDR >> 8u);
	pCMD[cmdLen++] = (INT8U)HT_LEN_IP_ADDR;
	pCMD[cmdLen++] = 0u;
	pCMD[cmdLen++] = 0u;
	pCMD[cmdLen++] = 0u;
	pCMD[cmdLen++] = 0u;
	//域名和端口号
	for(pos = 0u; pos < pIpAddr->addrLen; pos++)
	{
		if(':' == pIpAddr->addrData[pos])
		{
			break;
		}
	}
	pCMD[cmdLen++] = HT_IDH_PARAM_CH1 + ipCh;
	pCMD[cmdLen++] = HT_IDL_DOMAIN_NAME;
	pCMD[cmdLen++] = (INT8U)(pos >> 8u);
	pCMD[cmdLen++] = (INT8U)pos;
	for(port = 0u; port < pos; port++)
	{
		pCMD[cmdLen++] = pIpAddr->addrData[port];
	}
	pos++;
	port = 0u;
	for(; pos < pIpAddr->addrLen; pos++)
	{
		if((pIpAddr->addrData[pos] >= '0') && (pIpAddr->addrData[pos] <= '9'))
		{
			port *= 10u;
			port += (pIpAddr->addrData[pos] - '0');
		}
		else
		{
			port = 0u;
			break;
		}
	}
	pCMD[cmdLen++] = HT_IDH_PARAM_CH1 + ipCh;
	pCMD[cmdLen++] = HT_IDL_IP_PORT;
	pCMD[cmdLen++] = (INT8U)(HT_LEN_IP_PORT >> 8u);
	pCMD[cmdLen++] = (INT8U)HT_LEN_IP_PORT;
	pCMD[cmdLen++] = (INT8U)port;
	pCMD[cmdLen++] = (INT8U)(port >> 8u);
	//包尾
	pCMD[cmdLen++] = 0u;
	pCMD[cmdLen++] = HT_PACKET_END;
	pCMD[cmdLen++] = HT_PACKET_END;
	pCMD[cmdLen++] = HT_PACKET_END;
	//长度
	pCMD[posLen++] = (INT8U)(cmdLen >> 8u);
	pCMD[posLen++] = (INT8U)cmdLen;
	
	return cmdLen;
}

/*查询信号强度
**
*/
static INT8U CMD_QueryCSQ(INT8U *pCMD)
{
	INT8U cmdLen = 0u, posLen;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	//包头
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	//长度
	posLen = cmdLen;
	pCMD[cmdLen++] = 0u;
	pCMD[cmdLen++] = 0u;
	//功能码
	pCMD[cmdLen++] = HT_AFNU_QUERY_STA;
	pCMD[cmdLen++] = 0u;
	//数据域
	//信号强度
	pCMD[cmdLen++] = HT_IDH_PARAM_STA;
	pCMD[cmdLen++] = HT_IDL_CSQ_VALUE;
	//包尾
	pCMD[cmdLen++] = 0u;
	pCMD[cmdLen++] = HT_PACKET_END;
	pCMD[cmdLen++] = HT_PACKET_END;
	pCMD[cmdLen++] = HT_PACKET_END;
	//长度
	pCMD[posLen++] = (INT8U)(cmdLen >> 8u);
	pCMD[posLen++] = (INT8U)cmdLen;
	
	return cmdLen;
}

/*查询模块状态
**
*/
static INT8U CMD_ModuleSta(INT8U *pCMD)
{
	INT8U cmdLen = 0u, posLen;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	//包头
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	//长度
	posLen = cmdLen;
	pCMD[cmdLen++] = 0u;
	pCMD[cmdLen++] = 0u;
	//功能码
	pCMD[cmdLen++] = HT_AFNU_QUERY_STA;
	pCMD[cmdLen++] = 0u;
	//数据域
	//模块状态
	pCMD[cmdLen++] = HT_IDH_PARAM_STA;
	pCMD[cmdLen++] = HT_IDL_MODULE_STA;
	//包尾
	pCMD[cmdLen++] = 0u;
	pCMD[cmdLen++] = HT_PACKET_END;
	pCMD[cmdLen++] = HT_PACKET_END;
	pCMD[cmdLen++] = HT_PACKET_END;
	//长度
	pCMD[posLen++] = (INT8U)(cmdLen >> 8u);
	pCMD[posLen++] = (INT8U)cmdLen;
	
	return cmdLen;
}

/*复位
**
*/
static INT8U CMD_Reset(INT8U *pCMD)
{
	INT8U cmdLen = 0u, posLen;
	
	if((INT8U *)0u == pCMD)
	{
		return 0u;
	}
	
	//包头
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	pCMD[cmdLen++] = HT_PACKET_HEAD;
	//长度
	posLen = cmdLen;
	pCMD[cmdLen++] = 0u;
	pCMD[cmdLen++] = 0u;
	//功能码
	pCMD[cmdLen++] = HT_AFNU_RESET;
	pCMD[cmdLen++] = 0u;
	//包尾
	pCMD[cmdLen++] = 0u;
	pCMD[cmdLen++] = HT_PACKET_END;
	pCMD[cmdLen++] = HT_PACKET_END;
	pCMD[cmdLen++] = HT_PACKET_END;
	//长度
	pCMD[posLen++] = (INT8U)(cmdLen >> 8u);
	pCMD[posLen++] = (INT8U)cmdLen;
	
	return cmdLen;
}



/*执行一条指令
**
*/
static INT8U AT_CmdExcute(INT8U const *pCMD, INT16U cmdLen, OS_FLAGS yes, OS_FLAGS no, INT8U errTimes, INT8U timeOutTimes, INT32U timeOut, INT32U waitTime)
{
	INT8U		err;
	OS_FLAGS	flagsRdy;
	
	//参数验证
	if((INT8U *)0u == pCMD)
	{
		return OS_FALSE;
	}
	//执行
	while(OS_TRUE)
	{
		//清除事件标志位
		OSFlagPost(m_pFlagGrpDtu, yes + no, OS_FLAG_CLR, &err);
		while(OS_ERR_NONE != err);
		//发送指令
		SendToModule(pCMD, cmdLen);
		//判断事件标志位
		flagsRdy = OSFlagPend(m_pFlagGrpDtu, yes + no, OS_FLAG_CONSUME + OS_FLAG_WAIT_SET_ANY, timeOut * OS_TICKS_PER_SEC, &err);
		if(flagsRdy & yes)
		{
			return OS_TRUE;
		}
		else if(flagsRdy & no)
		{
			if(errTimes)
			{
				errTimes--;
			}
			if(errTimes)
			{
				OSTimeDly(waitTime * OS_TICKS_PER_SEC);
			}
			else
			{
				return OS_FALSE;
			}
		}
		else
		{
			if(timeOutTimes)
			{
				timeOutTimes--;
			}
			if(0u == timeOutTimes)
			{
				return OS_FALSE;
			}
		}
	}
}

/*断开IP通道
**
*/
static INT8U AT_CloseIP(INT8U ipCh)
{
	//参数验证
	if(ipCh >= DTU_NUM_IP_CH)
	{
		return OS_FALSE;
	}
	
	return OS_TRUE;
}

/*连接IP通道
**
*/
static INT8U AT_ConnIP(INT8U ipCh)
{
	//参数验证
	if(ipCh >= DTU_NUM_IP_CH)
	{
		return OS_FALSE;
	}
	//设置IP地址
	DtuParamPend();
	m_atSendLen = CMD_ConnIP(m_atSend, ipCh, &m_dtuIpAddr[ipCh], (m_dtuIpType & (1u << ipCh)) ? OS_TRUE : OS_FALSE);
	DtuParamPost();
	if(OS_FALSE == AT_CmdExcute(m_atSend, m_atSendLen, DTU_FLAG_SET_OK, DTU_FLAG_SET_ERROR, 3u, 3u, DTU_TIME_SET_PARAM, 2u))
	{
		return OS_FALSE;
	}
	//保存参数
	m_atSendLen = CMD_SaveParam(m_atSend);
	if(OS_FALSE == AT_CmdExcute(m_atSend, m_atSendLen, DTU_FLAG_SAVE_OK, DTU_FLAG_SAVE_ERROR, 3u, 3u, DTU_TIME_SAVE_PARAM, 2u))
	{
		return OS_FALSE;
	}
	//复位
	m_atSendLen = CMD_Reset(m_atSend);
	if(OS_FALSE == AT_CmdExcute(m_atSend, m_atSendLen, DTU_FLAG_RESET_OK, DTU_FLAG_RESET_ERROR, 3u, 3u, DTU_TIME_RESET_START, 2u))
	{
		return OS_FALSE;
	}
	//延时
	OSTimeDly(DTU_TIME_RESET_END * OS_TICKS_PER_SEC);
	
	return OS_TRUE;
}

/*发送数据
**
*/
static INT8U AT_SendIP(INT8U ipCh, INT8U const *pData, INT16U dataLen)
{
	INT16U lenTotal;
	
	//参数验证
	if(ipCh >= DTU_NUM_IP_CH)
	{
		return OS_FALSE;
	}
	if((INT8U *)0u == pData)
	{
		return OS_FALSE;
	}
	if(0u == dataLen)
	{
		return OS_FALSE;
	}
	
	//总长度
	lenTotal = dataLen + HT_BYTES_HEAD + HT_BYTES_LEN + HT_BYTES_AFN + HT_BYTES_CRC + HT_BYTES_END;
	//包头
	m_atSendLen = 0u;
	m_atSend[m_atSendLen++] = HT_PACKET_HEAD;
	m_atSend[m_atSendLen++] = HT_PACKET_HEAD;
	m_atSend[m_atSendLen++] = HT_PACKET_HEAD;
	m_atSend[m_atSendLen++] = (INT8U)(lenTotal >> 8u);
	m_atSend[m_atSendLen++] = (INT8U)lenTotal;
	m_atSend[m_atSendLen++] = HT_AFNU_SEND_GPRS;
	m_atSend[m_atSendLen++] = ipCh + 1u;
	SendToModule(m_atSend, m_atSendLen);
	//数据
	SendToModule(pData, dataLen);
	//包尾
	m_atSendLen = 0u;
	m_atSend[m_atSendLen++] = 0u;
	m_atSend[m_atSendLen++] = HT_PACKET_END;
	m_atSend[m_atSendLen++] = HT_PACKET_END;
	m_atSend[m_atSendLen++] = HT_PACKET_END;
	SendToModule(m_atSend, m_atSendLen);
	
	return OS_TRUE;
}

/*发送数据
**
*/
static INT8U AT_SendSMS(INT8U smsCh, INT8U const *pData, INT16U dataLen)
{
	INT8U			err;
	INT16U			lenTotal;
	OS_FLAGS		flagsRdy;
	DTU_SMS_ADDR	*pSmsAddr;
	
	//参数验证
	if((INT8U *)0u == pData)
	{
		return OS_FALSE;
	}
	if(0u == dataLen)
	{
		return OS_FALSE;
	}
	
	//清除事件标志位
	OSFlagPost(m_pFlagGrpDtu, DTU_FLAG_SMS_OK + DTU_FLAG_SMS_ERROR, OS_FLAG_CLR, &err);
	while(OS_ERR_NONE != err);
	//总长度
	lenTotal = dataLen + HT_BYTES_HEAD + HT_BYTES_LEN + HT_BYTES_AFN + HT_BYTES_CRC + HT_BYTES_END + HT_BYTES_SMS_ADDR;
	//包头
	m_atSendLen = 0u;
	m_atSend[m_atSendLen++] = HT_PACKET_HEAD;
	m_atSend[m_atSendLen++] = HT_PACKET_HEAD;
	m_atSend[m_atSendLen++] = HT_PACKET_HEAD;
	m_atSend[m_atSendLen++] = (INT8U)(lenTotal >> 8u);
	m_atSend[m_atSendLen++] = (INT8U)lenTotal;
	m_atSend[m_atSendLen++] = HT_AFNU_SEND_SMS;
	m_atSend[m_atSendLen++] = 0u;
	if(smsCh < DTU_NUM_SMS_CH)
	{
		pSmsAddr = &m_dtuSmsAddr[smsCh];
	}
	else
	{
		pSmsAddr = &m_dtuSuperPhone;
	}
	DtuParamPend();
	for(err = 0u; err < pSmsAddr->addrLen; err++)
	{
		m_atSend[m_atSendLen++] = pSmsAddr->addrData[err];
	}
	DtuParamPost();
	for(; err < HT_BYTES_SMS_ADDR; err++)
	{
		m_atSend[m_atSendLen++] = 0u;
	}
	SendToModule(m_atSend, m_atSendLen);
	//数据
	SendToModule(pData, dataLen);
	//包尾
	m_atSendLen = 0u;
	m_atSend[m_atSendLen++] = 0u;
	m_atSend[m_atSendLen++] = HT_PACKET_END;
	m_atSend[m_atSendLen++] = HT_PACKET_END;
	m_atSend[m_atSendLen++] = HT_PACKET_END;
	SendToModule(m_atSend, m_atSendLen);
	//判断事件标志位
	flagsRdy = OSFlagPend(m_pFlagGrpDtu, DTU_FLAG_SMS_OK + DTU_FLAG_SMS_ERROR, OS_FLAG_CONSUME + OS_FLAG_WAIT_SET_ANY, DTU_TIME_SMS_SEND * OS_TICKS_PER_SEC, &err);
	if(flagsRdy & DTU_FLAG_SMS_OK)
	{
		return OS_TRUE;
	}
	else if(flagsRdy & DTU_FLAG_SMS_ERROR)
	{
		return OS_FALSE;
	}
	else
	{
		return OS_FALSE;
	}
}

/*短信初始化
**
*/
static INT8U AT_BootSMS(void)
{
	INT8U i = 0u;
	
	//设置参数
	while(OS_TRUE)
	{
		m_atSendLen = CMD_ParamSet(m_atSend, i++);
		if(0u == m_atSendLen)
		{
			break;
		}
		if(OS_FALSE == AT_CmdExcute(m_atSend, m_atSendLen, DTU_FLAG_SET_OK, DTU_FLAG_SET_ERROR, 3u, 3u, DTU_TIME_SET_PARAM, 2u))
		{
			return OS_FALSE;
		}
	}
	
	//保存参数
	m_atSendLen = CMD_SaveParam(m_atSend);
	if(OS_FALSE == AT_CmdExcute(m_atSend, m_atSendLen, DTU_FLAG_SAVE_OK, DTU_FLAG_SAVE_ERROR, 3u, 3u, DTU_TIME_SAVE_PARAM, 2u))
	{
		return OS_FALSE;
	}
	
	//延时
	OSTimeDly(DTU_TIME_BOOT_SMS * OS_TICKS_PER_SEC);
	
	return OS_TRUE;
}

/*连网初始化
**
*/
static INT8U AT_BootIP(void)
{
	INT8U sta;
	
	m_atSendLen = CMD_ModuleSta(m_atSend);
	sta = AT_CmdExcute(m_atSend, m_atSendLen, DTU_FLAG_MODULE_OK, DTU_FLAG_MODULE_ERROR, 3u, 3u, DTU_TIME_QUERY_STA, 2u);
	
	return sta;
}

/*信号强度更新
**
*/
static INT8U AT_CsqUpdate(void)
{
	INT8U sta;
	
	m_atSendLen = CMD_QueryCSQ(m_atSend);
	sta = AT_CmdExcute(m_atSend, m_atSendLen, DTU_FLAG_CSQ, DTU_FLAG_NONE_NO, 3u, 3u, DTU_TIME_QUERY_STA, 2u);
	
	return sta;
}

/*关闭网络
**
*/
static INT8U AT_CloseNET(void)
{
	return OS_TRUE;
}

/*挂电话
**
*/
static INT8U AT_HangUp(void)
{
	return OS_TRUE;
}



/*默认的心跳包组包函数
**
*/
static INT16U HeartEncoder(INT8U *pData, INT8U ipCh, INT16U lenLimit)
{
	INT16U i = 0u;
	
	if((INT8U *)0u == pData)
	{
		return 0u;
	}
	
	if(ipCh >= DTU_NUM_IP_CH)
	{
		return 0u;
	}
	
	if(lenLimit < 10u)
	{
		return 0u;
	}
	
	pData[i++] = 'H';
	pData[i++] = 'T';
	pData[i++] = '7';
	pData[i++] = '7';
	pData[i++] = '1';
	pData[i++] = '0';
	pData[i++] = '-';
	pData[i++] = 'C';
	pData[i++] = 'H';
	pData[i++] = '0' + ipCh;
	
	return i;
}

/*SMS数据转换
**
*/
static void SmsDataConv(DTU_RECV_DATA *pRecvData)
{
	INT8U					i, valid;
	static DTU_SMS_ADDR		smsAddr;
	
	//接收数据指针为空
	if((DTU_RECV_DATA *)0u == pRecvData)
	{
		return;
	}
	//非SMS信道接收来的数据
	if(DTU_COMM_TYPE_SMS != pRecvData->commType)
	{
		return;
	}
	//数据长度最多只容纳短信号码
	if(pRecvData->dataLen <= HT_BYTES_SMS_ADDR)
	{
		pRecvData->dataLen = 0u;
		return;
	}
	//获取短信号码
	smsAddr.addrLen = 0u;
	for(i = 0u; i < HT_BYTES_SMS_ADDR; i++)
	{
		if(0u == pRecvData->pData[i])
		{
			break;
		}
		else if(smsAddr.addrLen < DTU_BYTES_SMS_ADDR)
		{
			smsAddr.addrData[smsAddr.addrLen++] = pRecvData->pData[i];
		}
		else
		{
			break;
		}
	}
	//比较短信号码
	valid = OS_FALSE;
	DtuParamPend();
	if(OS_TRUE == ComparePhone(&smsAddr, &m_dtuSuperPhone))
	{
		valid = OS_TRUE;
		pRecvData->ch = DTU_NUM_SMS_CH;
	}
	else
	{
		for(i = 0u; i < DTU_NUM_SMS_CH; i++)
		{
			if(m_dtuSmsCh & (1u << i))
			{
				if(OS_TRUE == ComparePhone(&smsAddr, &m_dtuSmsAddr[i]))
				{
					valid = OS_TRUE;
					pRecvData->ch = i;
					break;
				}
			}
		}
	}
	DtuParamPost();
	//根据比较结果进行相应的操作
	if(OS_FALSE == valid)
	{
		pRecvData->dataLen = 0u;
	}
	else
	{
		pRecvData->dataLen -= HT_BYTES_SMS_ADDR;
		pRecvData->pData += HT_BYTES_SMS_ADDR;
	}
}



/*DTU应答接收
**
*/
void DtuRecvHandler(INT8U recvData)
{
	static INT8U			step = 0u, afnH, afnL;
	static INT16U			dataLen, recvCount;
	static DTU_RECV_DATA	*pRecvData;
	
	switch(step)
	{
	case 0u:
		if(HT_PACKET_HEAD == recvData)
		{
			step = 1u;
		}
		break;
	case 1u:
		if(HT_PACKET_HEAD == recvData)
		{
			step = 2u;
		}
		else
		{
			step = 0u;
		}
		break;
	case 2u:
		if(HT_PACKET_HEAD == recvData)
		{
			step = 3u;
		}
		else
		{
			step = 0u;
		}
		break;
	case 3u:
		dataLen = recvData;
		dataLen <<= 8u;
		step = 4u;
		break;
	case 4u:
		dataLen += recvData;
		step = 5u;
		break;
	case 5u:
		afnH = recvData;
		step = 6u;
		break;
	case 6u:
		afnL = recvData;
		switch(afnH)
		{
		//设置参数
		case HT_AFND_SET_PARAM:
			if((0u == afnL) && (HT_BYTES_BASIC_FRAME == dataLen))
			{
				OSFlagPost(m_pFlagGrpDtu, DTU_FLAG_SET_OK, OS_FLAG_SET, &step);
			}
			else
			{
				OSFlagPost(m_pFlagGrpDtu, DTU_FLAG_SET_ERROR, OS_FLAG_SET, &step);
			}
			step = 0u;
			break;
		//保存参数
		case HT_AFND_SAVE_PARAM:
			if((0u == afnL) && (HT_BYTES_BASIC_FRAME == dataLen))
			{
				OSFlagPost(m_pFlagGrpDtu, DTU_FLAG_SAVE_OK, OS_FLAG_SET, &step);
			}
			else
			{
				OSFlagPost(m_pFlagGrpDtu, DTU_FLAG_SAVE_ERROR, OS_FLAG_SET, &step);
			}
			step = 0u;
			break;
		//复位
		case HT_AFND_RESET:
			if((0u == afnL) && (HT_BYTES_BASIC_FRAME == dataLen))
			{
				OSFlagPost(m_pFlagGrpDtu, DTU_FLAG_RESET_OK, OS_FLAG_SET, &step);
			}
			else
			{
				OSFlagPost(m_pFlagGrpDtu, DTU_FLAG_RESET_ERROR, OS_FLAG_SET, &step);
			}
			step = 0u;
			break;
		//发送GPRS
		case HT_AFND_SEND_GPRS:
			if((afnL > 0u) && (afnL < 5u) && (dataLen > HT_BYTES_BASIC_FRAME))
			{
				afnL--;
				dataLen -= HT_BYTES_BASIC_FRAME;
				pRecvData = RecvDataReq(dataLen);
				if((DTU_RECV_DATA *)0u == pRecvData)
				{
					step = 0u;
				}
				else
				{
					pRecvData->commType		= DTU_COMM_TYPE_IP;
					pRecvData->ch			= afnL;
					pRecvData->dataLen		= dataLen;
					recvCount = 0u;
					step = 7u;
				}
			}
			else
			{
				step = 0u;
			}
			break;
		//发送SMS
		case HT_AFND_SEND_SMS:
			if((0u == afnL) && (dataLen > HT_BYTES_BASIC_FRAME))
			{
				dataLen -= HT_BYTES_BASIC_FRAME;
				pRecvData = RecvDataReq(dataLen);
				if((DTU_RECV_DATA *)0u == pRecvData)
				{
					step = 0u;
				}
				else
				{
					pRecvData->commType		= DTU_COMM_TYPE_SMS;
					pRecvData->ch			= DTU_NUM_SMS_CH;
					pRecvData->dataLen		= dataLen;
					recvCount = 0u;
					step = 7u;
				}
			}
			else
			{
				step = 0u;
			}
			break;
		//查询状态
		case HT_AFND_QUERY_STA:
			if((0u == afnL) && (dataLen > HT_BYTES_BASIC_FRAME))
			{
				step = 8u;
			}
			else
			{
				step = 0u;
			}
			break;
		default:
			step = 0u;
			break;
		}
		break;
	//GPRS、SMS数据接收
	case 7u:
		pRecvData->pData[recvCount++] = recvData;
		if(recvCount >= pRecvData->dataLen)
		{
			if(OS_ERR_NONE != OSQPost(m_pQRecvData, (void *)pRecvData))
			{
				OSMemPut(pRecvData->pMem, (void *)pRecvData);
			}
			step = 0u;
		}
		break;
	case 8u:
		if(HT_IDH_PARAM_STA == recvData)
		{
			step = 9u;
		}
		else
		{
			step = 0u;
		}
		break;
	case 9u:
		afnL = recvData;
		step = 10u;
		break;
	case 10u:
		dataLen = recvData;
		dataLen <<= 8u;
		step = 11u;
		break;
	case 11u:
		dataLen += recvData;
		step = 12u;
		break;
	case 12u:
		switch(afnL)
		{
		//信号强度
		case HT_IDL_CSQ_VALUE:
			if(1u == dataLen)
			{
				m_csqValue = recvData;
				OSFlagPost(m_pFlagGrpDtu, DTU_FLAG_CSQ, OS_FLAG_SET, &step);
			}
			step = 0u;
			break;
		//SMS发送状态
		case HT_IDL_SMS_SEND_STA:
			if(1u == dataLen)
			{
				if(1u == recvData)
				{
					OSFlagPost(m_pFlagGrpDtu, DTU_FLAG_SMS_OK, OS_FLAG_SET, &step);
				}
				else
				{
					OSFlagPost(m_pFlagGrpDtu, DTU_FLAG_SMS_ERROR, OS_FLAG_SET, &step);
				}
			}
			step = 0u;
			break;
		//模块状态
		case HT_IDL_MODULE_STA:
			if(1u == dataLen)
			{
				if(1u == recvData)
				{
					OSFlagPost(m_pFlagGrpDtu, DTU_FLAG_MODULE_OK, OS_FLAG_SET, &step);
				}
				else
				{
					OSFlagPost(m_pFlagGrpDtu, DTU_FLAG_MODULE_ERROR, OS_FLAG_SET, &step);
				}
			}
			step = 0u;
			break;
		default:
			step = 0u;
			break;
		}
		break;
	}
}

