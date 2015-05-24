/*
 * verifyresetpw_handler.cpp
 *
 *  Created on: May 11, 2015
 *      Author: jimm
 */

#include "verifyresetpw_handler.h"
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
#include <string.h>

using namespace LOGGER;
using namespace FRAME;

int32_t CVerifyResetPWHandler::VerifyAuthCode(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize)
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

	CVerifyResetPWReq *pVerifyResetPWReq = dynamic_cast<CVerifyResetPWReq *>(pMsgBody);
	if(pVerifyResetPWReq == NULL)
	{
		return 0;
	}

	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pSession = pRedisSessionBank->CreateSession(this, static_cast<RedisReply>(&CVerifyResetPWHandler::OnSessionGetResetPasswordInfo),
			static_cast<TimerProc>(&CVerifyResetPWHandler::OnRedisSessionTimeout));
	UserSession *pSessionData = new(pSession->GetSessionData()) UserSession();
	pSessionData->m_stCtlHead = *pControlHead;
	pSessionData->m_stMsgHeadCS = *pMsgHeadCS;
	pSessionData->m_stVerifyResetPWReq = *pVerifyResetPWReq;

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRedisChannel = pRedisBank->GetRedisChannel(ResetPassword::servername, pVerifyResetPWReq->m_strPhone.c_str());
	pRedisChannel->HMGet(pSession, CServerHelper::MakeRedisKey(ResetPassword::keyname, pVerifyResetPWReq->m_strPhone.c_str()), "%s %s",
			ResetPassword::auth_code, ResetPassword::auth_code_expire_time);

	return 0;
}

int32_t CVerifyResetPWHandler::OnSessionGetResetPasswordInfo(int32_t nResult, void *pReply, void *pSession)
{
	redisReply *pRedisReply = (redisReply *)pReply;
	RedisSession *pRedisSession = (RedisSession *)pSession;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	CStringConfig *pStringConfig = (CStringConfig *)g_Frame.GetConfig(CONFIG_STRING);

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRespChannel = pRedisBank->GetRedisChannel(pUserSession->m_stCtlHead.m_nGateRedisAddress, pUserSession->m_stCtlHead.m_nGateRedisPort);
	if(pRespChannel == NULL)
	{
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_VERIFYRESETPW_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	CVerifyResetPWResp stVerifyResetPWResp;
	stVerifyResetPWResp.m_nResult = CVerifyResetPWResp::enmResult_OK;

	int64_t nAuthCode = 0;
	int64_t nAuthCodeExpireTime = 0;

	bool bIsFailed = false;
	do
	{
		if(pRedisReply->type == REDIS_REPLY_ERROR)
		{
			stVerifyResetPWResp.m_nResult = CVerifyResetPWResp::enmResult_Unknown;
			bIsFailed = true;
			break;
		}

		if(pRedisReply->type == REDIS_REPLY_ARRAY)
		{
			redisReply *pReplyElement = pRedisReply->element[0];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				nAuthCode = atoi(pReplyElement->str);
			}

			pReplyElement = pRedisReply->element[1];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				nAuthCodeExpireTime = atoi(pReplyElement->str);
			}
		}
		else
		{
			bIsFailed = true;
			break;
		}

		int64_t nCurTime = CDateTime::CurrentDateTime().Seconds();
		if(nCurTime > nAuthCodeExpireTime)
		{
			stVerifyResetPWResp.m_nResult = CVerifyResetPWResp::enmResult_AuthCodeExpire;
			bIsFailed = true;
			break;
		}

		if(nAuthCode != pUserSession->m_stVerifyResetPWReq.m_nAuthCode)
		{
			stVerifyResetPWResp.m_nResult = CVerifyResetPWResp::enmResult_AuthCodeWrong;
			bIsFailed = true;
			break;
		}
	}while(0);

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_VERIFYRESETPW_RESP;
	stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
	stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

	if(bIsFailed)
	{
		stVerifyResetPWResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stVerifyResetPWResp.m_nResult);
	}
	else
	{
		CRedisChannel *pAccountInfoChannel = pRedisBank->GetRedisChannel(AccountInfo::servername,
				pUserSession->m_stVerifyResetPWReq.m_strPhone.c_str());
		pAccountInfoChannel->HMSet(NULL, CServerHelper::MakeRedisKey(AccountInfo::keyname, pUserSession->m_stVerifyResetPWReq.m_strPhone.c_str()),
				"%s %s", AccountInfo::password, pUserSession->m_stVerifyResetPWReq.m_strPassword.c_str());
	}

	uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stVerifyResetPWResp, arrRespBuf, sizeof(arrRespBuf));
	pRespChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pUserSession->m_stCtlHead.m_nGateID), (char *)arrRespBuf, nTotalSize);

	g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stVerifyResetPWResp, "send ");

	pRedisSessionBank->DestroySession(pRedisSession);
	return 0;
}

int32_t CVerifyResetPWHandler::OnRedisSessionTimeout(void *pTimerData)
{
	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pRedisSession = (RedisSession *)pTimerData;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRespChannel = pRedisBank->GetRedisChannel(pUserSession->m_stCtlHead.m_nGateRedisAddress, pUserSession->m_stCtlHead.m_nGateRedisPort);
	if(pRespChannel == NULL)
	{
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_VERIFYRESETPW_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	CStringConfig *pStringConfig = (CStringConfig *)g_Frame.GetConfig(CONFIG_STRING);

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_VERIFYRESETPW_RESP;
	stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
	stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

	CVerifyResetPWResp stVerifyResetPWResp;
	stVerifyResetPWResp.m_nResult = CVerifyResetPWResp::enmResult_Unknown;
	stVerifyResetPWResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stVerifyResetPWResp.m_nResult);

	uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stVerifyResetPWResp, arrRespBuf, sizeof(arrRespBuf));
	pRespChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pUserSession->m_stCtlHead.m_nGateID), (char *)arrRespBuf, nTotalSize);

	g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stVerifyResetPWResp, "send ");

	pRedisSessionBank->DestroySession(pRedisSession);
	return 0;
}




