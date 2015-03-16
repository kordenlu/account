/*
 * requestauth_handler.cpp
 *
 *  Created on: Mar 10, 2015
 *      Author: jimm
 */

#include "requestauth_handler.h"
#include "../../common/common_datetime.h"
#include "../../common/common_api.h"
#include "../../frame/frame.h"
#include "../../frame/server_helper.h"
#include "../../frame/redissession_bank.h"
#include "../../include/cachekey_define.h"
#include "../../include/control_head.h"
#include "../../include/typedef.h"
#include "../config/msgdispatch_config.h"
#include "../config/string_config.h"
#include "../server_typedef.h"
#include "../bank/redis_bank.h"

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
			static_cast<TimerProc>(&CRequstAuthHandler::OnTimeoutAccountIsExist));
	UserSession *pSessionData = new(pSession->GetSessionData()) UserSession();
	pSessionData->m_stCtlHead = *pControlHead;
	pSessionData->m_stMsgHeadCS = *pMsgHeadCS;
	pSessionData->m_stAuthRegistPhoneReq = *pAuthRegistPhoneReq;

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pAccountNameChannel = pRedisBank->GetRedisChannel(RK_ACCOUNT_NAME);
	pAccountNameChannel->HExists(pSession, (char *)(pAuthRegistPhoneReq->m_strPhone.c_str()), "%s", "uin");

	return 0;
}

int32_t CRequstAuthHandler::OnSessionAccountIsExist(int32_t nResult, void *pReply, void *pSession)
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
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_REQUESTAUTH_RESP;
	stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
	stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

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
		stAuthRegistPhoneResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stAuthRegistPhoneResp.m_nResult);

		uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stAuthRegistPhoneResp, arrRespBuf, sizeof(arrRespBuf));
		pRespChannel->Publish(NULL, (char *)arrRespBuf, nTotalSize);
	}
	else
	{
		pRedisSession->SetHandleRedisReply(static_cast<RedisReply>(&CRequstAuthHandler::OnSessionGetRegistPhoneInfo));
		pRedisSession->SetTimerProc(static_cast<TimerProc>(&CRequstAuthHandler::OnTimeoutGetRegistPhoneInfo), 60 * MS_PER_SECOND);

		CRedisChannel *pGetRegistPhoneInfoChannel = pRedisBank->GetRedisChannel(RK_REGIST_PHONE_INFO);
		pGetRegistPhoneInfoChannel->HMGet(pRedisSession, (char *)(pUserSession->m_stAuthRegistPhoneReq.m_strPhone.c_str()), "%s %s %s %s",
				"regist_count", "last_regist_date", "auth_code", "auth_code_expire_time");
	}
	return 0;
}

int32_t CRequstAuthHandler::OnTimeoutAccountIsExist(void *pTimerData)
{
	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pRedisSession = (RedisSession *)pTimerData;

	CMsgDispatchConfig *pMsgDispatchConfig = (CMsgDispatchConfig *)g_Frame.GetConfig(CONFIG_MSGDISPATCH);
	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRespChannel = pRedisBank->GetRedisChannel(pMsgDispatchConfig->GetChannelKey(MSGID_REQUESTAUTH_RESP));
	if(pRespChannel == NULL)
	{
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

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
	pRespChannel->Publish(NULL, (char *)arrRespBuf, nTotalSize);

	pRedisSessionBank->DestroySession(pRedisSession);
	return 0;
}

int32_t CRequstAuthHandler::OnSessionGetRegistPhoneInfo(int32_t nResult, void *pReply, void *pSession)
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
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_REQUESTAUTH_RESP;
	stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
	stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

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

		int32_t nCurDate = CDateTime::CurrentDateTime().Date();
		if(nCurDate == nLastRegistDate)
		{
			if(nRegistCount > 10)
			{
				stAuthRegistPhoneResp.m_nResult = CRequestAuthResp::enmResult_PhoneAuthLimit;
				bIsReturn = true;
				break;
			}
		}

		CRedisChannel *pPhoneInfo = pRedisBank->GetRedisChannel(RK_REGIST_PHONE_INFO);
		if(nCurDate == nLastRegistDate)
		{
			pPhoneInfo->HIncrBy(NULL, (char *)(pUserSession->m_stAuthRegistPhoneReq.m_strPhone.c_str()), "%s %d", "regist_count", 1);
		}
		else
		{
			pPhoneInfo->HMSet(NULL, (char *)(pUserSession->m_stAuthRegistPhoneReq.m_strPhone.c_str()), "%s %d %s %d %s %d",
					"regist_count", 1, "last_regist_date", nCurDate, "auth_code_expire_time", 0);
		}

		int64_t nCurTime = CDateTime::CurrentDateTime().Seconds();
		if(nCurTime > nAuthCodeExpireTime)
		{
			nAuthCode = Random(999999);
			if(nAuthCode < 100000)
			{
				nAuthCode += 100000;
			}

			nAuthCodeExpireTime = nCurTime + 600;
			pPhoneInfo->HMSet(NULL, (char *)(pUserSession->m_stAuthRegistPhoneReq.m_strPhone.c_str()), "%s %d %s %d",
					"auth_code", nAuthCode, "auth_code_expire_time", nAuthCodeExpireTime);
		}
	}while(0);

	if(bIsReturn)
	{
		stAuthRegistPhoneResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stAuthRegistPhoneResp.m_nResult);

		uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stAuthRegistPhoneResp, arrRespBuf, sizeof(arrRespBuf));
		pRespChannel->Publish(NULL, (char *)arrRespBuf, nTotalSize);
	}
	else
	{
		pRedisSession->SetHandleRedisReply(static_cast<RedisReply>(&CRequstAuthHandler::OnSessionGetRegistAddrInfo));
		pRedisSession->SetTimerProc(static_cast<TimerProc>(&CRequstAuthHandler::OnTimeoutGetRegistAddrInfo), 60 * MS_PER_SECOND);
		pUserSession->m_nAuthCode = nAuthCode;
		pUserSession->m_nAuthCodeExpireTime = nAuthCodeExpireTime;

		CRedisChannel *pAddrInfoChannel = pRedisBank->GetRedisChannel(RK_REGIST_ADDR_INFO);
		pAddrInfoChannel->HMGet(pRedisSession, inet_ntoa_f(pUserSession->m_stCtlHead.m_nClientAddress), "%s %s", "regist_count", "last_regist_date");
	}

	return 0;
}

int32_t CRequstAuthHandler::OnTimeoutGetRegistPhoneInfo(void *pTimerData)
{
	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pRedisSession = (RedisSession *)pTimerData;

	CMsgDispatchConfig *pMsgDispatchConfig = (CMsgDispatchConfig *)g_Frame.GetConfig(CONFIG_MSGDISPATCH);
	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRespChannel = pRedisBank->GetRedisChannel(pMsgDispatchConfig->GetChannelKey(MSGID_REQUESTAUTH_RESP));
	if(pRespChannel == NULL)
	{
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

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
	pRespChannel->Publish(NULL, (char *)arrRespBuf, nTotalSize);

	pRedisSessionBank->DestroySession(pRedisSession);
	return 0;
}

int32_t CRequstAuthHandler::OnSessionGetRegistAddrInfo(int32_t nResult, void *pReply, void *pSession)
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
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_REQUESTAUTH_RESP;
	stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
	stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

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

		int32_t nCurDate = CDateTime::CurrentDateTime().Date();
		if(nCurDate == nLastRegistDate)
		{
			if(nRegistCount > 1000)
			{
				stAuthRegistPhoneResp.m_nResult = CRequestAuthResp::enmResult_AddrAuthLimit;
				bIsReturn = true;
				break;
			}
		}

		CRedisChannel *pAddrInfo = pRedisBank->GetRedisChannel(RK_REGIST_ADDR_INFO);
		if(nCurDate == nLastRegistDate)
		{
			pAddrInfo->HIncrBy(NULL, inet_ntoa_f(pUserSession->m_stCtlHead.m_nClientAddress), "%s %d", "regist_count", 1);
		}
		else
		{
			pAddrInfo->HMSet(NULL, inet_ntoa_f(pUserSession->m_stCtlHead.m_nClientAddress), "%s %d %s %d", "regist_count", 1, "last_regist_date", nCurDate);
		}
	}while(0);

	if(bIsReturn)
	{
		stAuthRegistPhoneResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stAuthRegistPhoneResp.m_nResult);
	}
//	else
//	{
//		CRedisChannel *pAddrInfo = pRedisBank->GetRedisChannel(RK_REGIST_PHONE_INFO);
//	}

	uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stAuthRegistPhoneResp, arrRespBuf, sizeof(arrRespBuf));
	pRespChannel->Publish(NULL, (char *)arrRespBuf, nTotalSize);

	pRedisSessionBank->DestroySession(pRedisSession);
	return 0;
}

int32_t CRequstAuthHandler::OnTimeoutGetRegistAddrInfo(void *pTimerData)
{
	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pRedisSession = (RedisSession *)pTimerData;

	CMsgDispatchConfig *pMsgDispatchConfig = (CMsgDispatchConfig *)g_Frame.GetConfig(CONFIG_MSGDISPATCH);
	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRespChannel = pRedisBank->GetRedisChannel(pMsgDispatchConfig->GetChannelKey(MSGID_REQUESTAUTH_RESP));
	if(pRespChannel == NULL)
	{
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

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
	pRespChannel->Publish(NULL, (char *)arrRespBuf, nTotalSize);

	pRedisSessionBank->DestroySession(pRedisSession);

	return 0;
}


