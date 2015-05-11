/*
 * main.cpp
 *
 *  Created on: Mar 9, 2015
 *      Author: jimm
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include "../logger/logger.h"
#include "../frame/frame_impl.h"
#include "../frame/frame.h"
#include "../frame/cmd_thread.h"
#include "../include/cachekey_define.h"
#include "regist_message.h"
#include "dispatch/msgparser_factory.h"
#include "dispatch/msg_handler.h"
#include "dispatch/checkconn_handler.h"
#include "config/server_config.h"
#include "config/redis_config.h"
#include "server_typedef.h"

using namespace LOGGER;
using namespace FRAME;

//注册到配置管理器
REGIST_CONFIG(REGIST_PHONEINFO, RegistPhoneInfo)
REGIST_CONFIG(REGIST_ADDRINFO, RegistAddrInfo)
REGIST_CONFIG(USER_BASEINFO, UserBaseInfo)
REGIST_CONFIG(USER_SESSIONINFO, UserSessionInfo)
REGIST_CONFIG(ACCOUNT_INFO, AccountInfo)
REGIST_CONFIG(USER_UNREADMSGLIST, UserUnreadMsgList)

class Initer : public IInitFrame
{
public:
	virtual int32_t InitFrame(CNetHandler *pNetHandler)
	{
		CCheckConnHandler *pCheckConnHandler = new CCheckConnHandler();
		int32_t nTimerIndex = 0;
		g_Frame.CreateTimer(static_cast<TimerProc>(&CCheckConnHandler::CheckConnStatus),
				pCheckConnHandler, NULL, 10 * MS_PER_SECOND, true, nTimerIndex);

		return 0;
	}
};

int32_t main()
{
	srand((int)time(0));

	//启动日志线程
	CLogger::Start();

//	CCmdThread *pCmdThread = new CCmdThread(1, 1, "127.0.0.1", 5178);
//	pCmdThread->Start();
	g_Frame.Start(SERVER_NAME, 1, 1, "127.0.0.1", 5178, new Initer());

	while(true)
	{
		Delay(50 * US_PER_SECOND);
	}

//	//创建网络事件处理器
//	CNetHandler *pNetHandler = new CNetHandler();
//	pNetHandler->CreateReactor();
//
//	g_Frame.AddRunner(pNetHandler);
//
//	if(g_Frame.Init(SERVER_NAME) != 0)
//	{
//		return 0;
//	}
//
//	InitNetAndTimer(pNetHandler);
//
//	while(true)
//	{
//		g_Frame.Run();
//	}
//
//	g_Frame.Uninit();

	return 0;
}

