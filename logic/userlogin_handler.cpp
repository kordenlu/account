/*
 * userlogin_handler.cpp
 *
 *  Created on: Mar 17, 2015
 *      Author: jimm
 */

#include "userlogin_handler.h"
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

int32_t CUserLoginHandler::UserLogin(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize)
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

	CUserLoginReq *pUserLoginReq = dynamic_cast<CUserLoginReq *>(pMsgBody);
	if(pUserLoginReq == NULL)
	{
		return 0;
	}

	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pSession = pRedisSessionBank->CreateSession(this, static_cast<RedisReply>(&CUserLoginHandler::OnSessionGetAccountInfo),
			static_cast<TimerProc>(&CUserLoginHandler::OnRedisSessionTimeout));
	UserSession *pSessionData = new(pSession->GetSessionData()) UserSession();
	pSessionData->m_stCtlHead = *pControlHead;
	pSessionData->m_stMsgHeadCS = *pMsgHeadCS;
	pSessionData->m_stUserLoginReq = *pUserLoginReq;

	AccountInfo *pConfigAccountInfo = (AccountInfo *)g_Frame.GetConfig(ACCOUNT_INFO);

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pAccountNameChannel = pRedisBank->GetRedisChannel(pConfigAccountInfo->string);
	pAccountNameChannel->HMGet(pSession, (char *)(pUserLoginReq->m_strAccountName.c_str()), "%s %s %s %s", pConfigAccountInfo->uin,
			pConfigAccountInfo->password, pConfigAccountInfo->status, pConfigAccountInfo->accountid);

	return 0;
}

int32_t CUserLoginHandler::OnSessionGetAccountInfo(int32_t nResult, void *pReply, void *pSession)
{
	redisReply *pRedisReply = (redisReply *)pReply;
	RedisSession *pRedisSession = (RedisSession *)pSession;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	CStringConfig *pStringConfig = (CStringConfig *)g_Frame.GetConfig(CONFIG_STRING);

	CMsgDispatchConfig *pMsgDispatchConfig = (CMsgDispatchConfig *)g_Frame.GetConfig(CONFIG_MSGDISPATCH);
	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRespChannel = pRedisBank->GetRedisChannel(pMsgDispatchConfig->GetChannelKey(MSGID_USERLOGIN_RESP));
	if(pRespChannel == NULL)
	{
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_USERLOGIN_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	CUserLoginResp stUserLoginResp;
	stUserLoginResp.m_nResult = CUserLoginResp::enmResult_OK;

	uint32_t nUin = 0;
	string strPassword;
	int32_t nAccountStatus = 0;
	string strAccountID;

	bool bIsReturn = false;
	do
	{
		if(pRedisReply->type == REDIS_REPLY_ERROR)
		{
			stUserLoginResp.m_nResult = CUserLoginResp::enmResult_Unknown;
			bIsReturn = true;
			break;
		}

		if(pRedisReply->type == REDIS_REPLY_ARRAY)
		{
			redisReply *pReplyElement = pRedisReply->element[0];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				nUin = atoi(pReplyElement->str);
				pUserSession->m_stMsgHeadCS.m_nSrcUin = nUin;
			}
			else
			{
				stUserLoginResp.m_nResult = CUserLoginResp::enmResult_NotExist;
				bIsReturn = true;
				break;
			}

			pReplyElement = pRedisReply->element[1];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				strPassword = pReplyElement->str;
			}
			else
			{
				stUserLoginResp.m_nResult = CUserLoginResp::enmResult_NotExist;
				bIsReturn = true;
				break;
			}

			pReplyElement = pRedisReply->element[2];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				nAccountStatus = atoi(pReplyElement->str);
			}

			if(strPassword != pUserSession->m_stUserLoginReq.m_strPassword)
			{
				stUserLoginResp.m_nResult = CUserLoginResp::enmResult_LoginWrong;
				bIsReturn = true;
				break;
			}

			pReplyElement = pRedisReply->element[3];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				strAccountID = string(pReplyElement->str);
			}
			else
			{
				stUserLoginResp.m_nResult = CUserLoginResp::enmResult_Unknown;
				bIsReturn = true;
				break;
			}
		}
	}while(0);

	if(bIsReturn)
	{
		MsgHeadCS stMsgHeadCS;
		stMsgHeadCS.m_nMsgID = MSGID_USERLOGIN_RESP;
		stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
		stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
		stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

		stUserLoginResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stUserLoginResp.m_nResult);

		uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stUserLoginResp, arrRespBuf, sizeof(arrRespBuf));
		pRespChannel->RPush(NULL, (char *)arrRespBuf, nTotalSize);

		g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stUserLoginResp, "send ");

		pRedisSessionBank->DestroySession(pRedisSession);
	}
	else
	{
		pRedisSession->SetHandleRedisReply(static_cast<RedisReply>(&CUserLoginHandler::OnSessionGetUserBaseInfo));
		pRedisSession->SetTimerProc(static_cast<TimerProc>(&CUserLoginHandler::OnRedisSessionTimeout), 60 * MS_PER_SECOND);

		pUserSession->m_nUin = nUin;
		pUserSession->m_strAccountID = strAccountID;

		UserBaseInfo *pConfigUserBaseInfo = (UserBaseInfo *)g_Frame.GetConfig(USER_BASE_INFO);

		CRedisChannel *pGetUserBaseInfoChannel = pRedisBank->GetRedisChannel(pConfigUserBaseInfo->string);
		pGetUserBaseInfoChannel->HMGet(pRedisSession, itoa(pUserSession->m_nUin), "%s %s %s %s",
				pConfigUserBaseInfo->nickname, pConfigUserBaseInfo->gender, pConfigUserBaseInfo->headimage, pConfigUserBaseInfo->version);
	}

	return 0;
}

int32_t CUserLoginHandler::OnSessionGetUserBaseInfo(int32_t nResult, void *pReply, void *pSession)
{
	redisReply *pRedisReply = (redisReply *)pReply;
	RedisSession *pRedisSession = (RedisSession *)pSession;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	CStringConfig *pStringConfig = (CStringConfig *)g_Frame.GetConfig(CONFIG_STRING);

	CMsgDispatchConfig *pMsgDispatchConfig = (CMsgDispatchConfig *)g_Frame.GetConfig(CONFIG_MSGDISPATCH);
	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRespChannel = pRedisBank->GetRedisChannel(pMsgDispatchConfig->GetChannelKey(MSGID_USERLOGIN_RESP));
	if(pRespChannel == NULL)
	{
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_USERLOGIN_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_USERLOGIN_RESP;
	stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
	stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

	CUserLoginResp stUserLoginResp;
	stUserLoginResp.m_nResult = CUserLoginResp::enmResult_OK;

	bool bIsReturn = false;
	do
	{
		if(pRedisReply->type == REDIS_REPLY_ERROR)
		{
			stUserLoginResp.m_nResult = CUserLoginResp::enmResult_Unknown;
			bIsReturn = true;
			break;
		}

		if(pRedisReply->type == REDIS_REPLY_ARRAY)
		{
			redisReply *pReplyElement = pRedisReply->element[0];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				stUserLoginResp.m_strNickName = pReplyElement->str;
			}
//			else
//			{
//				stUserLoginResp.m_nResult = CUserLoginResp::enmResult_Unknown;
//				bIsReturn = true;
//				break;
//			}

			pReplyElement = pRedisReply->element[1];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				stUserLoginResp.m_nGender = atoi(pReplyElement->str);
			}
//			else
//			{
//				stUserLoginResp.m_nResult = CUserLoginResp::enmResult_Unknown;
//				bIsReturn = true;
//				break;
//			}

			pReplyElement = pRedisReply->element[2];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				stUserLoginResp.m_strHeadImageAddr = pReplyElement->str;
			}
//			else
//			{
//				stUserLoginResp.m_nResult = CUserLoginResp::enmResult_Unknown;
//				bIsReturn = true;
//				break;
//			}

			pReplyElement = pRedisReply->element[3];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				stUserLoginResp.m_nSelfInfoVersion = atoi(pReplyElement->str);
			}
//			else
//			{
//				stUserLoginResp.m_nResult = CUserLoginResp::enmResult_Unknown;
//				bIsReturn = true;
//				break;
//			}
		}
	}while(0);

	if(bIsReturn)
	{
		stUserLoginResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stUserLoginResp.m_nResult);
	}
	else
	{
		stUserLoginResp.m_nUin = pUserSession->m_nUin;
		stUserLoginResp.m_strAccountID = pUserSession->m_strAccountID;
	}

	uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stUserLoginResp, arrRespBuf, sizeof(arrRespBuf));
	pRespChannel->RPush(NULL, (char *)arrRespBuf, nTotalSize);

	g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stUserLoginResp, "send ");

	pRedisSessionBank->DestroySession(pRedisSession);

	return 0;
}

int32_t CUserLoginHandler::OnRedisSessionTimeout(void *pTimerData)
{
	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pRedisSession = (RedisSession *)pTimerData;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	CMsgDispatchConfig *pMsgDispatchConfig = (CMsgDispatchConfig *)g_Frame.GetConfig(CONFIG_MSGDISPATCH);
	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRespChannel = pRedisBank->GetRedisChannel(pMsgDispatchConfig->GetChannelKey(MSGID_USERLOGIN_RESP));
	if(pRespChannel == NULL)
	{
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_USERLOGIN_RESP,
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

	CUserLoginResp stUserLoginResp;
	stUserLoginResp.m_nResult = CUserLoginResp::enmResult_Unknown;
	stUserLoginResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stUserLoginResp.m_nResult);

	uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stUserLoginResp, arrRespBuf, sizeof(arrRespBuf));
	pRespChannel->RPush(NULL, (char *)arrRespBuf, nTotalSize);

	g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stUserLoginResp, "send ");

	pRedisSessionBank->DestroySession(pRedisSession);
	return 0;
}


