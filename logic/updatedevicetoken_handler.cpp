/*
 * updatedevicetoken_handler.cpp
 *
 *  Created on: May 9, 2015
 *      Author: jimm
 */

#include "updatedevicetoken_handler.h"
#include "common/common_datetime.h"
#include "common/common_api.h"
#include "frame/frame.h"
#include "frame/server_helper.h"
#include "frame/redissession_bank.h"
#include "frame/cachekey_define.h"
#include "logger/logger.h"
#include "include/control_head.h"
#include "include/typedef.h"
#include "config/string_config.h"
#include "server_typedef.h"
#include "bank/redis_bank.h"

using namespace LOGGER;
using namespace FRAME;

int32_t CUpdateDeviceTokenHandler::UpdateDeviceToken(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize)
{
	ControlHead *pControlHead = dynamic_cast<ControlHead *>(pCtlHead);
	if(pControlHead == NULL)
	{
		return 0;
	}

	MsgHeadCS *pMsgHeadCS = dynamic_cast<MsgHeadCS *>(pMsgHead);
	if(pMsgHeadCS == NULL)
	{
		return 0;
	}

	CUpdateDeviceTokenReq *pUpdateDeviceTokenReq = dynamic_cast<CUpdateDeviceTokenReq *>(pMsgBody);
	if(pUpdateDeviceTokenReq == NULL)
	{
		return 0;
	}

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pUserBaseInfoChannel = pRedisBank->GetRedisChannel(UserBaseInfo::servername, pMsgHeadCS->m_nSrcUin);
	pUserBaseInfoChannel->HMSet(NULL, CServerHelper::MakeRedisKey(UserBaseInfo::keyname, pMsgHeadCS->m_nSrcUin), "%s %s",
			UserBaseInfo::devicetoken, pUpdateDeviceTokenReq->m_strDeviceToken.c_str());

	return 0;
}



