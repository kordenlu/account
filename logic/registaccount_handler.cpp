/*
 * registaccount_handler.cpp
 *
 *  Created on: Mar 10, 2015
 *      Author: jimm
 */

#include "registaccount_handler.h"
#include "../../common/common_datetime.h"
#include "../../frame/frame.h"
#include "../../frame/server_helper.h"
#include "../../include/cachekey_define.h"
#include "../../include/control_head.h"
#include "../../include/typedef.h"
#include "../config/msgdispatch_config.h"
#include "../config/string_config.h"
#include "../server_typedef.h"
#include "../bank/redis_bank.h"

using namespace FRAME;

int32_t CRegistAccountHandler::OnRegistPhoneInfo(int32_t nResult, void *pReply, void *pSession)
{
	redisReply *pRedisReply = (redisReply *)pReply;
	if(pRedisReply->type == REDIS_REPLY_ERROR)
	{
		return 0;
	}

	RedisSession *pRedisSession = (RedisSession *)pSession;
	GetRegistSession *pUserSession = (GetRegistSession *)pRedisSession->GetSessionData();

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_AUTHREGISTPHONE_RESP;
	stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
	stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

	CAuthRegistPhoneResp stAuthRegistPhoneResp;
	stAuthRegistPhoneResp.m_nResult = 0;

	int64_t nRegistCount = 0;
	int64_t nLastRegistDay = 0;
	if(pRedisReply->type == REDIS_REPLY_ARRAY)
	{
		redisReply *pReplyElement = pRedisReply->element[0];
		if(pReplyElement->type != REDIS_REPLY_NIL)
		{
			nRegistCount = atoi(pReplyElement->str);
		}

		pReplyElement = pRedisReply->element[1];
		if(pReplyElement->type != REDIS_REPLY_NIL)
		{
			nLastRegistDay = atoi(pReplyElement->str);
		}
	}

	int32_t nToday = CDateTime::CurrentDateTime().Year() * 10000 + CDateTime::CurrentDateTime().Month() * 100 + CDateTime::CurrentDateTime().Day();
	if(nToday == nLastRegistDay)
	{
		if(nRegistCount > 1)
		{
			stAuthRegistPhoneResp.m_nResult = 1;
			CStringConfig *pStringConfig = (CStringConfig *)g_Frame.GetConfig(CONFIG_STRING);
			stAuthRegistPhoneResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stAuthRegistPhoneResp.m_nResult);
		}
	}

	CMsgDispatchConfig *pMsgDispatchConfig = (CMsgDispatchConfig *)g_Frame.GetConfig(CONFIG_MSGDISPATCH);
	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);

	CRedisChannel *pRedisChannel = pRedisBank->GetRedisChannel(pMsgDispatchConfig->GetChannelKey(MSGID_AUTHREGISTPHONE_RESP));
	if(pRedisChannel == NULL)
	{
		delete pRedisSession;
		return 0;
	}

	CRedisChannel *pRegistChannel = pRedisBank->GetRedisChannel(RK_REGIST_PHONE_INFO);
	if(nToday == nLastRegistDay)
	{
		pRegistChannel->HIncrBy(NULL, (char *)(pUserSession->m_stAuthRegistPhoneReq.m_strPhone.c_str()), "%s %d", "regist_count", 1);
	}
	else
	{
		pRegistChannel->HMSet(NULL, (char *)(pUserSession->m_stAuthRegistPhoneReq.m_strPhone.c_str()), "%s %d %s %d", "regist_count", 1, "last_regist_day", nToday);
	}

	uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stAuthRegistPhoneResp, arrRespBuf, sizeof(arrRespBuf));

	pRedisChannel->Publish(NULL, (char *)arrRespBuf, nTotalSize);

	delete pRedisSession;

	return 0;
}

int32_t CRegistAccountHandler::GetAuthCodeByPhone(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize)
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

	CAuthRegistPhoneReq *pAuthRegistPhoneReq = dynamic_cast<CAuthRegistPhoneReq *>(pMsgBody);
	if(pAuthRegistPhoneReq == NULL)
	{
		return 0;
	}

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRedisChannel = pRedisBank->GetRedisChannel(RK_REGIST_PHONE_INFO);

	RedisSession *pSession = new RedisSession(this, static_cast<HandleRedisReply>(&CRegistAccountHandler::OnRegistPhoneInfo));
	GetRegistSession *pSessionData = new(pSession->GetSessionData()) GetRegistSession();
	pSessionData->m_stCtlHead = *pControlHead;
	pSessionData->m_stMsgHeadCS = *pMsgHeadCS;
	pSessionData->m_stAuthRegistPhoneReq = *pAuthRegistPhoneReq;

	pRedisChannel->HMGet(pSession, (char *)(pAuthRegistPhoneReq->m_strPhone.c_str()), "%s %s", "regist_count", "last_regist_day");

	return 0;
}
