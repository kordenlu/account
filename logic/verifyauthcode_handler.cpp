/*
 * verifyauthcode_handler.cpp
 *
 *  Created on: Mar 16, 2015
 *      Author: jimm
 */

#include "verifyauthcode_handler.h"
#include "../../common/common_datetime.h"
#include "../../common/common_api.h"
#include "../../frame/frame.h"
#include "../../frame/server_helper.h"
#include "../../frame/redissession_bank.h"
#include "../../logger/logger.h"
#include "../../include/cachekey_define.h"
#include "../../include/control_head.h"
#include "../../include/typedef.h"
#include "../config/msgdispatch_config.h"
#include "../config/string_config.h"
#include "../server_typedef.h"
#include "../bank/redis_bank.h"

using namespace LOGGER;
using namespace FRAME;

int32_t CVerifyAuthCodeHandler::VerifyAuthCode(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize)
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

	CVerifyAuthCodeReq *pVerifyAuthCodeReq = dynamic_cast<CVerifyAuthCodeReq *>(pMsgBody);
	if(pVerifyAuthCodeReq == NULL)
	{
		return 0;
	}

	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pSession = pRedisSessionBank->CreateSession(this, static_cast<RedisReply>(&CVerifyAuthCodeHandler::OnSessionGetRegistPhoneInfo),
			static_cast<TimerProc>(&CVerifyAuthCodeHandler::OnRedisSessionTimeout));
	UserSession *pSessionData = new(pSession->GetSessionData()) UserSession();
	pSessionData->m_stCtlHead = *pControlHead;
	pSessionData->m_stMsgHeadCS = *pMsgHeadCS;
	pSessionData->m_stVerifyAuthCodeReq = *pVerifyAuthCodeReq;

	RegistPhoneInfo *pConfigRegistPhoneInfo = (RegistPhoneInfo *)g_Frame.GetConfig(REGIST_PHONE_INFO);

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRedisChannel = pRedisBank->GetRedisChannel(pConfigRegistPhoneInfo->string);
	pRedisChannel->HMGet(pSession, (char *)(pVerifyAuthCodeReq->m_strPhone.c_str()), "%s %s", pConfigRegistPhoneInfo->auth_code,
			pConfigRegistPhoneInfo->auth_code_expire_time);

	return 0;
}

int32_t CVerifyAuthCodeHandler::OnSessionGetRegistPhoneInfo(int32_t nResult, void *pReply, void *pSession)
{
	redisReply *pRedisReply = (redisReply *)pReply;
	RedisSession *pRedisSession = (RedisSession *)pSession;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	CStringConfig *pStringConfig = (CStringConfig *)g_Frame.GetConfig(CONFIG_STRING);

	CMsgDispatchConfig *pMsgDispatchConfig = (CMsgDispatchConfig *)g_Frame.GetConfig(CONFIG_MSGDISPATCH);
	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRespChannel = pRedisBank->GetRedisChannel(pMsgDispatchConfig->GetChannelKey(MSGID_REQUESTAUTH_RESP));
	if(pRespChannel == NULL)
	{
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_REQUESTAUTH_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	CVerifyAuthCodeResp stVerifyAuthCodeResp;
	stVerifyAuthCodeResp.m_nResult = CVerifyAuthCodeResp::enmResult_OK;

	int64_t nAuthCode = 0;
	int64_t nAuthCodeExpireTime = 0;

	bool bIsReturn = false;
	do
	{
		if(pRedisReply->type == REDIS_REPLY_ERROR)
		{
			stVerifyAuthCodeResp.m_nResult = CVerifyAuthCodeResp::enmResult_Unknown;
			bIsReturn = true;
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

		int64_t nCurTime = CDateTime::CurrentDateTime().Seconds();
		if(nCurTime > nAuthCodeExpireTime)
		{
			stVerifyAuthCodeResp.m_nResult = CVerifyAuthCodeResp::enmResult_AuthCodeExpire;
			bIsReturn = true;
			break;
		}

		if(nAuthCode != pUserSession->m_stVerifyAuthCodeReq.m_nAuthCode)
		{
			stVerifyAuthCodeResp.m_nResult = CVerifyAuthCodeResp::enmResult_AuthCodeWrong;
			bIsReturn = true;
			break;
		}
	}while(0);

	if(bIsReturn)
	{
		MsgHeadCS stMsgHeadCS;
		stMsgHeadCS.m_nMsgID = MSGID_VERIFYAUTHCODE_RESP;
		stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
		stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
		stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

		stVerifyAuthCodeResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stVerifyAuthCodeResp.m_nResult);

		uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stVerifyAuthCodeResp, arrRespBuf, sizeof(arrRespBuf));
		pRespChannel->Publish(NULL, (char *)arrRespBuf, nTotalSize);

		g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stVerifyAuthCodeResp, "send ");

		pRedisSessionBank->DestroySession(pRedisSession);
	}
	else
	{
		pRedisSession->SetHandleRedisReply(static_cast<RedisReply>(&CVerifyAuthCodeHandler::OnSessionGetGobalUin));
		pRedisSession->SetTimerProc(static_cast<TimerProc>(&CVerifyAuthCodeHandler::OnRedisSessionTimeout), 60 * MS_PER_SECOND);

		GlobalUin *pConfigGlobalUin = (GlobalUin *)g_Frame.GetConfig(GLOBAL_UIN);

		CRedisChannel *pGobalUinChannl = pRedisBank->GetRedisChannel(pConfigGlobalUin->string);
		pGobalUinChannl->Incr(pRedisSession, NULL);
	}

	return 0;
}

int32_t CVerifyAuthCodeHandler::OnSessionGetGobalUin(int32_t nResult, void *pReply, void *pSession)
{
	redisReply *pRedisReply = (redisReply *)pReply;
	RedisSession *pRedisSession = (RedisSession *)pSession;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	CStringConfig *pStringConfig = (CStringConfig *)g_Frame.GetConfig(CONFIG_STRING);

	CMsgDispatchConfig *pMsgDispatchConfig = (CMsgDispatchConfig *)g_Frame.GetConfig(CONFIG_MSGDISPATCH);
	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRespChannel = pRedisBank->GetRedisChannel(pMsgDispatchConfig->GetChannelKey(MSGID_REQUESTAUTH_RESP));
	if(pRespChannel == NULL)
	{
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_REQUESTAUTH_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	CVerifyAuthCodeResp stVerifyAuthCodeResp;
	stVerifyAuthCodeResp.m_nResult = CVerifyAuthCodeResp::enmResult_OK;

	bool bIsReturn = false;
	do
	{
		if(pRedisReply->type == REDIS_REPLY_ERROR)
		{
			stVerifyAuthCodeResp.m_nResult = CVerifyAuthCodeResp::enmResult_Unknown;
			bIsReturn = true;
			break;
		}

		if(pRedisReply->type == REDIS_REPLY_INTEGER)
		{
			stVerifyAuthCodeResp.m_nUin = pRedisReply->integer;
			pUserSession->m_stMsgHeadCS.m_nSrcUin = pRedisReply->integer;
		}
	}while(0);

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_VERIFYAUTHCODE_RESP;
	stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
	stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

	if(bIsReturn)
	{
		stVerifyAuthCodeResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stVerifyAuthCodeResp.m_nResult);
	}
	else
	{
		UserBaseInfo *pConfigUserBaseInfo = (UserBaseInfo *)g_Frame.GetConfig(USER_BASE_INFO);

		CRedisChannel *pUserBaseInfoChannel = pRedisBank->GetRedisChannel(pConfigUserBaseInfo->string);
		pUserBaseInfoChannel->HMSet(NULL, itoa(stVerifyAuthCodeResp.m_nUin), "%s %d %s %s %s %d %s %ld",
				pConfigUserBaseInfo->version, 1,
				pConfigUserBaseInfo->accountname, pUserSession->m_stVerifyAuthCodeReq.m_strPhone.c_str(),
				pConfigUserBaseInfo->uin, stVerifyAuthCodeResp.m_nUin,
				pConfigUserBaseInfo->createtime, CDateTime::CurrentDateTime().Seconds());

		AccountInfo *pConfigAccountInfo = (AccountInfo *)g_Frame.GetConfig(ACCOUNT_INFO);

		CRedisChannel *pAccountNameChannel = pRedisBank->GetRedisChannel(pConfigAccountInfo->string);
		pAccountNameChannel->HMSet(NULL, (char *)pUserSession->m_stVerifyAuthCodeReq.m_strPhone.c_str(), "%s %u %s %s %s %s",
				pConfigAccountInfo->uin, stVerifyAuthCodeResp.m_nUin, pConfigAccountInfo->accountname, pUserSession->m_stVerifyAuthCodeReq.m_strPhone.c_str(),
				pConfigAccountInfo->password, pUserSession->m_stVerifyAuthCodeReq.m_strPassword.c_str());
	}

	uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stVerifyAuthCodeResp, arrRespBuf, sizeof(arrRespBuf));
	pRespChannel->Publish(NULL, (char *)arrRespBuf, nTotalSize);

	g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stVerifyAuthCodeResp, "send ");

	pRedisSessionBank->DestroySession(pRedisSession);

	return 0;
}

int32_t CVerifyAuthCodeHandler::OnRedisSessionTimeout(void *pTimerData)
{
	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pRedisSession = (RedisSession *)pTimerData;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	CMsgDispatchConfig *pMsgDispatchConfig = (CMsgDispatchConfig *)g_Frame.GetConfig(CONFIG_MSGDISPATCH);
	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRespChannel = pRedisBank->GetRedisChannel(pMsgDispatchConfig->GetChannelKey(MSGID_REQUESTAUTH_RESP));
	if(pRespChannel == NULL)
	{
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_REQUESTAUTH_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	CStringConfig *pStringConfig = (CStringConfig *)g_Frame.GetConfig(CONFIG_STRING);

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_REQUESTAUTH_RESP;
	stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
	stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

	CVerifyAuthCodeResp stVerifyAuthCodeResp;
	stVerifyAuthCodeResp.m_nResult = CVerifyAuthCodeResp::enmResult_Unknown;
	stVerifyAuthCodeResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stVerifyAuthCodeResp.m_nResult);

	uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stVerifyAuthCodeResp, arrRespBuf, sizeof(arrRespBuf));
	pRespChannel->Publish(NULL, (char *)arrRespBuf, nTotalSize);

	g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stVerifyAuthCodeResp, "send ");

	pRedisSessionBank->DestroySession(pRedisSession);
	return 0;
}

