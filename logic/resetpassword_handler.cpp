/*
 * resetpassword_handler.cpp
 *
 *  Created on: May 11, 2015
 *      Author: jimm
 */

#include "resetpassword_handler.h"
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
#include "config/regist_config.h"
#include "server_typedef.h"
#include "bank/redis_bank.h"

using namespace LOGGER;
using namespace FRAME;

int32_t CResetPasswordHandler::GetAuthCodeByPhone(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize)
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

	CResetPasswordReq *pResetPasswordReq = dynamic_cast<CResetPasswordReq *>(pMsgBody);
	if(pResetPasswordReq == NULL)
	{
		return 0;
	}

	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pSession = pRedisSessionBank->CreateSession(this, static_cast<RedisReply>(&CResetPasswordHandler::OnSessionAccountIsExist),
			static_cast<TimerProc>(&CResetPasswordHandler::OnRedisSessionTimeout));
	UserSession *pSessionData = new(pSession->GetSessionData()) UserSession();
	pSessionData->m_stCtlHead = *pControlHead;
	pSessionData->m_stMsgHeadCS = *pMsgHeadCS;
	pSessionData->m_stResetPasswordReq = *pResetPasswordReq;

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pAccountNameChannel = pRedisBank->GetRedisChannel(AccountInfo::servername, pResetPasswordReq->m_strPhone.c_str());
	pAccountNameChannel->HExists(pSession, CServerHelper::MakeRedisKey(AccountInfo::keyname, pResetPasswordReq->m_strPhone.c_str()), AccountInfo::uin);

	return 0;
}

int32_t CResetPasswordHandler::OnSessionAccountIsExist(int32_t nResult, void *pReply, void *pSession)
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
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_RESETPASSWORD_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	CResetPasswordResp stResetPasswordResp;
	stResetPasswordResp.m_nResult = CResetPasswordResp::enmResult_OK;

	bool bAccountIsExist = true;
	do
	{
		if(pRedisReply->type == REDIS_REPLY_ERROR)
		{
			stResetPasswordResp.m_nResult = CResetPasswordResp::enmResult_Unknown;
			bAccountIsExist = false;
			break;
		}

		if(pRedisReply->type == REDIS_REPLY_NIL)
		{
			bAccountIsExist = false;
			break;
		}
	}while(0);

	if(!bAccountIsExist)
	{
		MsgHeadCS stMsgHeadCS;
		stMsgHeadCS.m_nMsgID = MSGID_RESETPASSWORD_RESP;
		stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
		stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
		stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

		stResetPasswordResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stResetPasswordResp.m_nResult);

		uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stResetPasswordResp, arrRespBuf, sizeof(arrRespBuf));
		pRespChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pUserSession->m_stCtlHead.m_nGateID), (char *)arrRespBuf, nTotalSize);

		g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stResetPasswordResp, "send ");

		pRedisSessionBank->DestroySession(pRedisSession);
	}
	else
	{
		pRedisSession->SetHandleRedisReply(static_cast<RedisReply>(&CResetPasswordHandler::OnSessionGetResetPasswordInfo));
		pRedisSession->SetTimerProc(static_cast<TimerProc>(&CResetPasswordHandler::OnRedisSessionTimeout), 60 * MS_PER_SECOND);

		CRedisChannel *pGetResetPasswordChannel = pRedisBank->GetRedisChannel(ResetPassword::servername, pUserSession->m_stResetPasswordReq.m_strPhone.c_str());
		pGetResetPasswordChannel->HMGet(pRedisSession, CServerHelper::MakeRedisKey(ResetPassword::keyname, pUserSession->m_stResetPasswordReq.m_strPhone.c_str()),
				"%s %s %s %s", ResetPassword::reset_count, ResetPassword::last_reset_date, ResetPassword::auth_code,
				ResetPassword::auth_code_expire_time);
	}
	return 0;
}

int32_t CResetPasswordHandler::OnSessionGetResetPasswordInfo(int32_t nResult, void *pReply, void *pSession)
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
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_RESETPASSWORD_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	CRegistConfig *pRegistConfig = (CRegistConfig *)g_Frame.GetConfig(CONFIG_REGIST);

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	CResetPasswordResp stResetPasswordResp;
	stResetPasswordResp.m_nResult = CResetPasswordResp::enmResult_OK;

	int64_t nAuthCode = 0;
	int64_t nAuthCodeExpireTime = 0;

	bool bIsFailed = false;
	do
	{
		if(pRedisReply->type == REDIS_REPLY_ERROR)
		{
			stResetPasswordResp.m_nResult = CResetPasswordResp::enmResult_Unknown;
			bIsFailed = true;
			break;
		}

		int64_t nResetCount = 0;
		int64_t nLastResetDate = 0;
		if(pRedisReply->type == REDIS_REPLY_ARRAY)
		{
			redisReply *pReplyElement = pRedisReply->element[0];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				nResetCount = atoi(pReplyElement->str);
			}

			pReplyElement = pRedisReply->element[1];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				nLastResetDate = atoi(pReplyElement->str);
			}

			pReplyElement = pRedisReply->element[2];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				nAuthCode = atoi(pReplyElement->str);
			}

			pReplyElement = pRedisReply->element[3];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				nAuthCodeExpireTime = atoi(pReplyElement->str);
			}
		}
		else
		{
			stResetPasswordResp.m_nResult = CResetPasswordResp::enmResult_Unknown;
			bIsFailed = true;
			break;
		}

		int32_t nCurDate = CDateTime::CurrentDateTime().Date();
		if(nCurDate == nLastResetDate)
		{
			if(nResetCount > pRegistConfig->GetPhoneRegistCount())
			{
				stResetPasswordResp.m_nResult = CResetPasswordResp::enmResult_PhoneAuthLimit;
				bIsFailed = true;
				break;
			}
		}

		CRedisChannel *pResetPasswordChannel = pRedisBank->GetRedisChannel(ResetPassword::servername, pUserSession->m_stResetPasswordReq.m_strPhone.c_str());
		if(nCurDate == nLastResetDate)
		{
			pResetPasswordChannel->HIncrBy(NULL, CServerHelper::MakeRedisKey(ResetPassword::keyname, pUserSession->m_stResetPasswordReq.m_strPhone.c_str()),
					ResetPassword::reset_count, 1);
		}
		else
		{
			pResetPasswordChannel->HMSet(NULL, CServerHelper::MakeRedisKey(ResetPassword::keyname, pUserSession->m_stResetPasswordReq.m_strPhone.c_str()),
					"%s %d %s %d %s %d", ResetPassword::reset_count, 1, ResetPassword::last_reset_date, nCurDate,
					ResetPassword::auth_code_expire_time, 0);
		}

		int64_t nCurTime = CDateTime::CurrentDateTime().Seconds();
		if(nCurTime > nAuthCodeExpireTime)
		{
			nAuthCode = Random(999999);
			if(nAuthCode < 100000)
			{
				nAuthCode += 100000;
			}

			nAuthCodeExpireTime = nCurTime + pRegistConfig->GetAuthCodeLiveTime();
			pResetPasswordChannel->HMSet(NULL, CServerHelper::MakeRedisKey(ResetPassword::keyname, pUserSession->m_stResetPasswordReq.m_strPhone.c_str()),
					"%s %d %s %d", ResetPassword::auth_code, nAuthCode, ResetPassword::auth_code_expire_time, nAuthCodeExpireTime);
		}
	}while(0);

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_RESETPASSWORD_RESP;
	stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
	stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

	if(bIsFailed)
	{
		stResetPasswordResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stResetPasswordResp.m_nResult);
	}
	else
	{
		CRegistConfig *pRegistConfig = (CRegistConfig *)g_Frame.GetConfig(CONFIG_REGIST);
		char szSmsContent[2048];
		int nContentLen = sprintf(szSmsContent, "mobile=%s&authcode=%ld&content=", pUserSession->m_stResetPasswordReq.m_strPhone.c_str(),
				nAuthCode);
		nContentLen += sprintf(szSmsContent + nContentLen, "%s", pRegistConfig->GetAuthCodeMessage());
		CRedisChannel *pPushSms = pRedisBank->GetRedisChannel(PushSms::servername, pUserSession->m_stResetPasswordReq.m_strPhone.c_str());
		pPushSms->RPush(NULL, CServerHelper::MakeRedisKey(PushSms::keyname), szSmsContent, nContentLen);
	}

	uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stResetPasswordResp, arrRespBuf, sizeof(arrRespBuf));
	pRespChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pUserSession->m_stCtlHead.m_nGateID), (char *)arrRespBuf, nTotalSize);

	g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stResetPasswordResp, "send ");

	pRedisSessionBank->DestroySession(pRedisSession);

	return 0;
}


int32_t CResetPasswordHandler::OnRedisSessionTimeout(void *pTimerData)
{
	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pRedisSession = (RedisSession *)pTimerData;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRespChannel = pRedisBank->GetRedisChannel(pUserSession->m_stCtlHead.m_nGateRedisAddress, pUserSession->m_stCtlHead.m_nGateRedisPort);
	if(pRespChannel == NULL)
	{
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_RESETPASSWORD_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	CStringConfig *pStringConfig = (CStringConfig *)g_Frame.GetConfig(CONFIG_STRING);

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_RESETPASSWORD_RESP;
	stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
	stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

	CResetPasswordResp stResetPasswordResp;
	stResetPasswordResp.m_nResult = CResetPasswordResp::enmResult_Unknown;
	stResetPasswordResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stResetPasswordResp.m_nResult);

	uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stResetPasswordResp, arrRespBuf, sizeof(arrRespBuf));
	pRespChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pUserSession->m_stCtlHead.m_nGateID), (char *)arrRespBuf, nTotalSize);

	g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stResetPasswordResp, "send ");

	pRedisSessionBank->DestroySession(pRedisSession);
	return 0;
}




