/*
 * requestauth_handler.cpp
 *
 *  Created on: Mar 10, 2015
 *      Author: jimm
 */

#include "requestauth_handler.h"
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

int32_t CRequstAuthHandler::GetAuthCodeByPhone(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize)
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

	CRequestAuthReq *pAuthRegistPhoneReq = dynamic_cast<CRequestAuthReq *>(pMsgBody);
	if(pAuthRegistPhoneReq == NULL)
	{
		return 0;
	}

	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pSession = pRedisSessionBank->CreateSession(this, static_cast<RedisReply>(&CRequstAuthHandler::OnSessionAccountIsExist),
			static_cast<TimerProc>(&CRequstAuthHandler::OnRedisSessionTimeout));
	UserSession *pSessionData = new(pSession->GetSessionData()) UserSession();
	pSessionData->m_stCtlHead = *pControlHead;
	pSessionData->m_stMsgHeadCS = *pMsgHeadCS;
	pSessionData->m_stAuthRegistPhoneReq = *pAuthRegistPhoneReq;

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pAccountNameChannel = pRedisBank->GetRedisChannel(AccountInfo::servername, pAuthRegistPhoneReq->m_strPhone.c_str());
	pAccountNameChannel->HExists(pSession, CServerHelper::MakeRedisKey(AccountInfo::keyname, pAuthRegistPhoneReq->m_strPhone.c_str()), AccountInfo::uin);

	return 0;
}

int32_t CRequstAuthHandler::OnSessionAccountIsExist(int32_t nResult, void *pReply, void *pSession)
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
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_REQUESTAUTH_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	CRequestAuthResp stAuthRegistPhoneResp;
	stAuthRegistPhoneResp.m_nResult = CRequestAuthResp::enmResult_OK;

	bool bIsReturn = false;
	do
	{
		if(pRedisReply->type == REDIS_REPLY_ERROR)
		{
			stAuthRegistPhoneResp.m_nResult = CRequestAuthResp::enmResult_Unknown;
			bIsReturn = true;
			break;
		}

		if(pRedisReply->type == REDIS_REPLY_INTEGER)
		{
			if(pRedisReply->integer != 0)
			{
				stAuthRegistPhoneResp.m_nResult = CRequestAuthResp::enmResult_AccountExist;
				bIsReturn = true;
				break;
			}
		}
	}while(0);

	if(bIsReturn)
	{
		MsgHeadCS stMsgHeadCS;
		stMsgHeadCS.m_nMsgID = MSGID_REQUESTAUTH_RESP;
		stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
		stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
		stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

		stAuthRegistPhoneResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stAuthRegistPhoneResp.m_nResult);

		uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stAuthRegistPhoneResp, arrRespBuf, sizeof(arrRespBuf));
		pRespChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pUserSession->m_stCtlHead.m_nGateID), (char *)arrRespBuf, nTotalSize);

		g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stAuthRegistPhoneResp, "send ");

		pRedisSessionBank->DestroySession(pRedisSession);
	}
	else
	{
		pRedisSession->SetHandleRedisReply(static_cast<RedisReply>(&CRequstAuthHandler::OnSessionGetRegistPhoneInfo));
		pRedisSession->SetTimerProc(static_cast<TimerProc>(&CRequstAuthHandler::OnRedisSessionTimeout), 60 * MS_PER_SECOND);

		CRedisChannel *pGetRegistPhoneInfoChannel = pRedisBank->GetRedisChannel(RegistPhoneInfo::servername, pUserSession->m_stAuthRegistPhoneReq.m_strPhone.c_str());
		pGetRegistPhoneInfoChannel->HMGet(pRedisSession, CServerHelper::MakeRedisKey(RegistPhoneInfo::keyname, pUserSession->m_stAuthRegistPhoneReq.m_strPhone.c_str()),
				"%s %s %s %s",
				RegistPhoneInfo::regist_count, RegistPhoneInfo::last_regist_date, RegistPhoneInfo::auth_code,
				RegistPhoneInfo::auth_code_expire_time);
	}
	return 0;
}

int32_t CRequstAuthHandler::OnSessionGetRegistPhoneInfo(int32_t nResult, void *pReply, void *pSession)
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
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_REQUESTAUTH_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	CRegistConfig *pRegistConfig = (CRegistConfig *)g_Frame.GetConfig(CONFIG_REGIST);

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	CRequestAuthResp stAuthRegistPhoneResp;
	stAuthRegistPhoneResp.m_nResult = CRequestAuthResp::enmResult_OK;

	int64_t nAuthCode = 0;
	int64_t nAuthCodeExpireTime = 0;

	bool bIsReturn = false;
	do
	{
		if(pRedisReply->type == REDIS_REPLY_ERROR)
		{
			stAuthRegistPhoneResp.m_nResult = CRequestAuthResp::enmResult_Unknown;
			bIsReturn = true;
			break;
		}

		int64_t nRegistCount = 0;
		int64_t nLastRegistDate = 0;
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
				nLastRegistDate = atoi(pReplyElement->str);
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
			stAuthRegistPhoneResp.m_nResult = CRequestAuthResp::enmResult_Unknown;
			bIsReturn = true;
			break;
		}

		int32_t nCurDate = CDateTime::CurrentDateTime().Date();
		if(nCurDate == nLastRegistDate)
		{
			if(nRegistCount > pRegistConfig->GetPhoneRegistCount())
			{
				stAuthRegistPhoneResp.m_nResult = CRequestAuthResp::enmResult_PhoneAuthLimit;
				bIsReturn = true;
				break;
			}
		}

		CRedisChannel *pPhoneInfo = pRedisBank->GetRedisChannel(RegistPhoneInfo::servername, pUserSession->m_stAuthRegistPhoneReq.m_strPhone.c_str());
		if(nCurDate == nLastRegistDate)
		{
			pPhoneInfo->HIncrBy(NULL, CServerHelper::MakeRedisKey(RegistPhoneInfo::keyname, pUserSession->m_stAuthRegistPhoneReq.m_strPhone.c_str()),
					RegistPhoneInfo::regist_count, 1);
		}
		else
		{
			pPhoneInfo->HMSet(NULL, CServerHelper::MakeRedisKey(RegistPhoneInfo::keyname, pUserSession->m_stAuthRegistPhoneReq.m_strPhone.c_str()),
					"%s %d %s %d %s %d",
					RegistPhoneInfo::regist_count, 1, RegistPhoneInfo::last_regist_date, nCurDate,
					RegistPhoneInfo::auth_code_expire_time, 0);
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
			pPhoneInfo->HMSet(NULL, CServerHelper::MakeRedisKey(RegistPhoneInfo::keyname, pUserSession->m_stAuthRegistPhoneReq.m_strPhone.c_str()),
					"%s %d %s %d", RegistPhoneInfo::auth_code, nAuthCode, RegistPhoneInfo::auth_code_expire_time, nAuthCodeExpireTime);
		}
	}while(0);

	if(bIsReturn)
	{
		MsgHeadCS stMsgHeadCS;
		stMsgHeadCS.m_nMsgID = MSGID_REQUESTAUTH_RESP;
		stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
		stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
		stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

		stAuthRegistPhoneResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stAuthRegistPhoneResp.m_nResult);

		uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stAuthRegistPhoneResp, arrRespBuf, sizeof(arrRespBuf));
		pRespChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pUserSession->m_stCtlHead.m_nGateID), (char *)arrRespBuf, nTotalSize);

		g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stAuthRegistPhoneResp, "send ");

		pRedisSessionBank->DestroySession(pRedisSession);
	}
	else
	{
		pRedisSession->SetHandleRedisReply(static_cast<RedisReply>(&CRequstAuthHandler::OnSessionGetRegistAddrInfo));
		pRedisSession->SetTimerProc(static_cast<TimerProc>(&CRequstAuthHandler::OnRedisSessionTimeout), 60 * MS_PER_SECOND);
		pUserSession->m_nAuthCode = nAuthCode;
		pUserSession->m_nAuthCodeExpireTime = nAuthCodeExpireTime;

		CRedisChannel *pAddrInfoChannel = pRedisBank->GetRedisChannel(RegistAddrInfo::servername, pUserSession->m_stCtlHead.m_nClientAddress);
		pAddrInfoChannel->HMGet(pRedisSession, CServerHelper::MakeRedisKey(RegistAddrInfo::keyname, inet_ntoa_f(pUserSession->m_stCtlHead.m_nClientAddress)),
				"%s %s", RegistAddrInfo::regist_count, RegistAddrInfo::last_regist_date);
	}

	return 0;
}

int32_t CRequstAuthHandler::OnSessionGetRegistAddrInfo(int32_t nResult, void *pReply, void *pSession)
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
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_REQUESTAUTH_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	CRegistConfig *pRegistConfig = (CRegistConfig *)g_Frame.GetConfig(CONFIG_REGIST);

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	CRequestAuthResp stAuthRegistPhoneResp;
	stAuthRegistPhoneResp.m_nResult = CRequestAuthResp::enmResult_OK;

	bool bIsReturn = false;
	do
	{
		if(pRedisReply->type == REDIS_REPLY_ERROR)
		{
			stAuthRegistPhoneResp.m_nResult = CRequestAuthResp::enmResult_Unknown;
			bIsReturn = true;
			break;
		}

		int64_t nRegistCount = 0;
		int64_t nLastRegistDate = 0;
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
				nLastRegistDate = atoi(pReplyElement->str);
			}
		}
		else
		{
			stAuthRegistPhoneResp.m_nResult = CRequestAuthResp::enmResult_Unknown;
			bIsReturn = true;
			break;
		}

		int32_t nCurDate = CDateTime::CurrentDateTime().Date();
		if(nCurDate == nLastRegistDate)
		{
			if(nRegistCount > pRegistConfig->GetAddrRegistCount())
			{
				stAuthRegistPhoneResp.m_nResult = CRequestAuthResp::enmResult_AddrAuthLimit;
				bIsReturn = true;
				break;
			}
		}

		CRedisChannel *pAddrInfo = pRedisBank->GetRedisChannel(RegistAddrInfo::servername, pUserSession->m_stCtlHead.m_nClientAddress);
		if(nCurDate == nLastRegistDate)
		{
			pAddrInfo->HIncrBy(NULL, CServerHelper::MakeRedisKey(RegistAddrInfo::keyname, inet_ntoa_f(pUserSession->m_stCtlHead.m_nClientAddress)),
					RegistAddrInfo::regist_count, 1);
		}
		else
		{
			pAddrInfo->HMSet(NULL, CServerHelper::MakeRedisKey(RegistAddrInfo::keyname, inet_ntoa_f(pUserSession->m_stCtlHead.m_nClientAddress)),
					"%s %d %s %d", RegistAddrInfo::regist_count, 1, RegistAddrInfo::last_regist_date, nCurDate);
		}
	}while(0);

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_REQUESTAUTH_RESP;
	stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
	stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

	if(bIsReturn)
	{
		stAuthRegistPhoneResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stAuthRegistPhoneResp.m_nResult);
	}
	else
	{
		CRegistConfig *pRegistConfig = (CRegistConfig *)g_Frame.GetConfig(CONFIG_REGIST);
		char szSmsContent[2048];
		int nContentLen = sprintf(szSmsContent, "mobile=%s&authcode=%ld&content=", pUserSession->m_stAuthRegistPhoneReq.m_strPhone.c_str(),
				pUserSession->m_nAuthCode);
		nContentLen += sprintf(szSmsContent + nContentLen, "%s", pRegistConfig->GetAuthCodeMessage());
		CRedisChannel *pPushSms = pRedisBank->GetRedisChannel(PushSms::servername, pUserSession->m_stAuthRegistPhoneReq.m_strPhone.c_str());
		pPushSms->RPush(NULL, CServerHelper::MakeRedisKey(PushSms::keyname), szSmsContent, nContentLen);
	}

	uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stAuthRegistPhoneResp, arrRespBuf, sizeof(arrRespBuf));
	pRespChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pUserSession->m_stCtlHead.m_nGateID), (char *)arrRespBuf, nTotalSize);

	g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stAuthRegistPhoneResp, "send ");

	pRedisSessionBank->DestroySession(pRedisSession);
	return 0;
}

int32_t CRequstAuthHandler::OnRedisSessionTimeout(void *pTimerData)
{
	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pRedisSession = (RedisSession *)pTimerData;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRespChannel = pRedisBank->GetRedisChannel(pUserSession->m_stCtlHead.m_nGateRedisAddress, pUserSession->m_stCtlHead.m_nGateRedisPort);
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

	CRequestAuthResp stAuthRegistPhoneResp;
	stAuthRegistPhoneResp.m_nResult = CRequestAuthResp::enmResult_Unknown;
	stAuthRegistPhoneResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stAuthRegistPhoneResp.m_nResult);

	uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stAuthRegistPhoneResp, arrRespBuf, sizeof(arrRespBuf));
	pRespChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pUserSession->m_stCtlHead.m_nGateID), (char *)arrRespBuf, nTotalSize);

	g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stAuthRegistPhoneResp, "send ");

	pRedisSessionBank->DestroySession(pRedisSession);
	return 0;
}

