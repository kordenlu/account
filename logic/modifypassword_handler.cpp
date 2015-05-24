/*
 * modifypassword_handler.cpp
 *
 *  Created on: May 11, 2015
 *      Author: jimm
 */

#include "modifypassword_handler.h"
#include "common/common_datetime.h"
#include "common/common_api.h"
#include "frame/frame.h"
#include "frame/server_helper.h"
#include "frame/redissession_bank.h"
#include "frame/cachekey_define.h"
#include "logger/logger.h"
#include "include/control_head.h"
#include "include/typedef.h"
#include "config/server_config.h"
#include "config/string_config.h"
#include "server_typedef.h"
#include "bank/redis_bank.h"

using namespace LOGGER;
using namespace FRAME;

int32_t CModifyPasswordHandler::ModifyPassword(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize)
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

	if((pControlHead->m_nUin == 0) || (pControlHead->m_nUin != pMsgHeadCS->m_nSrcUin))
	{
		CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
		CRedisChannel *pClientRespChannel = pRedisBank->GetRedisChannel(pControlHead->m_nGateRedisAddress, pControlHead->m_nGateRedisPort);

		return CServerHelper::KickUser(pControlHead, pMsgHeadCS, pClientRespChannel, KickReason_NotLogined);
	}

	CModifyPasswordReq *pModifyPasswordReq = dynamic_cast<CModifyPasswordReq *>(pMsgBody);
	if(pModifyPasswordReq == NULL)
	{
		return 0;
	}

	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pSession = pRedisSessionBank->CreateSession(this, static_cast<RedisReply>(&CModifyPasswordHandler::OnSessionGetAccountName),
			static_cast<TimerProc>(&CModifyPasswordHandler::OnRedisSessionTimeout));
	UserSession *pSessionData = new(pSession->GetSessionData()) UserSession();
	pSessionData->m_stCtlHead = *pControlHead;
	pSessionData->m_stMsgHeadCS = *pMsgHeadCS;
	pSessionData->m_stModifyPasswordReq = *pModifyPasswordReq;

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pAccountNameChannel = pRedisBank->GetRedisChannel(UserBaseInfo::servername, pMsgHeadCS->m_nSrcUin);
	pAccountNameChannel->HMGet(pSession, CServerHelper::MakeRedisKey(UserBaseInfo::keyname, pMsgHeadCS->m_nSrcUin), "%s", UserBaseInfo::accountname);

	return 0;
}

int32_t CModifyPasswordHandler::OnSessionGetAccountName(int32_t nResult, void *pReply, void *pSession)
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
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_MODIFYPASSWORD_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	CModifyPasswordResp stModifyPasswordResp;
	stModifyPasswordResp.m_nResult = CModifyPasswordResp::enmResult_OK;

	bool bIsFailed = false;
	do
	{
		if(pRedisReply->type == REDIS_REPLY_ERROR)
		{
			stModifyPasswordResp.m_nResult = CModifyPasswordResp::enmResult_Unknown;
			bIsFailed = true;
			break;
		}

		if(pRedisReply->type == REDIS_REPLY_ARRAY)
		{
			redisReply *pReplyElement = pRedisReply->element[0];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				pUserSession->m_strAccountName = string(pReplyElement->str);
			}
			else
			{
				stModifyPasswordResp.m_nResult = CModifyPasswordResp::enmResult_Unknown;
				bIsFailed = true;
				break;
			}
		}
		else
		{
			bIsFailed = true;
			break;
		}
	}while(0);

	if(bIsFailed)
	{
		MsgHeadCS stMsgHeadCS;
		stMsgHeadCS.m_nMsgID = MSGID_MODIFYPASSWORD_RESP;
		stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
		stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
		stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

		stModifyPasswordResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stModifyPasswordResp.m_nResult);

		uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stModifyPasswordResp, arrRespBuf, sizeof(arrRespBuf));
		pRespChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pUserSession->m_stCtlHead.m_nGateID), (char *)arrRespBuf, nTotalSize);

		g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stModifyPasswordResp, "send ");

		pRedisSessionBank->DestroySession(pRedisSession);
	}
	else
	{
		pRedisSession->SetHandleRedisReply(static_cast<RedisReply>(&CModifyPasswordHandler::OnSessionGetAccountInfo));
		pRedisSession->SetTimerProc(static_cast<TimerProc>(&CModifyPasswordHandler::OnRedisSessionTimeout), 60 * MS_PER_SECOND);

		CRedisChannel *pAccountInfoChannl = pRedisBank->GetRedisChannel(AccountInfo::servername, pUserSession->m_strAccountName.c_str());
		pAccountInfoChannl->HMGet(pRedisSession, CServerHelper::MakeRedisKey(AccountInfo::keyname, pUserSession->m_strAccountName.c_str()), "%s",
				AccountInfo::password);
	}

	return 0;
}

int32_t CModifyPasswordHandler::OnSessionGetAccountInfo(int32_t nResult, void *pReply, void *pSession)
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
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_MODIFYPASSWORD_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	CModifyPasswordResp stModifyPasswordResp;
	stModifyPasswordResp.m_nResult = CModifyPasswordResp::enmResult_Unknown;

	uint32_t nUin = 0;
	string strPassword;
	int32_t nAccountStatus = 0;
	string strAccountID;

	bool bIsFailed = true;
	do
	{
		if(pRedisReply->type == REDIS_REPLY_ERROR)
		{
			break;
		}

		if(pRedisReply->type == REDIS_REPLY_ARRAY)
		{
			redisReply *pReplyElement = pRedisReply->element[0];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				if(pUserSession->m_stModifyPasswordReq.m_strCurPassword != pReplyElement->str)
				{
					stModifyPasswordResp.m_nResult = CModifyPasswordResp::enmResult_CurPasswordWrong;
					break;
				}
				else
				{
					stModifyPasswordResp.m_nResult = CModifyPasswordResp::enmResult_OK;
					bIsFailed = false;
					break;
				}
			}
		}
	}while(0);

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_MODIFYPASSWORD_RESP;
	stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
	stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

	if(bIsFailed)
	{
		stModifyPasswordResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stModifyPasswordResp.m_nResult);
	}
	else
	{
		CRedisChannel *pAccountInfoChannel = pRedisBank->GetRedisChannel(AccountInfo::servername, pUserSession->m_strAccountName.c_str());
		pAccountInfoChannel->HMSet(NULL, CServerHelper::MakeRedisKey(AccountInfo::keyname, pUserSession->m_strAccountName.c_str()), "%s %s",
				AccountInfo::password, pUserSession->m_stModifyPasswordReq.m_strNewPassword.c_str());
	}

	uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stModifyPasswordResp, arrRespBuf, sizeof(arrRespBuf));
	pRespChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pUserSession->m_stCtlHead.m_nGateID), (char *)arrRespBuf, nTotalSize);

	g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stModifyPasswordResp, "send ");

	pRedisSessionBank->DestroySession(pRedisSession);

	return 0;
}

int32_t CModifyPasswordHandler::OnRedisSessionTimeout(void *pTimerData)
{
	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pRedisSession = (RedisSession *)pTimerData;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRespChannel = pRedisBank->GetRedisChannel(pUserSession->m_stCtlHead.m_nGateRedisAddress, pUserSession->m_stCtlHead.m_nGateRedisPort);
	if(pRespChannel == NULL)
	{
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_MODIFYPASSWORD_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	CStringConfig *pStringConfig = (CStringConfig *)g_Frame.GetConfig(CONFIG_STRING);

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_MODIFYPASSWORD_RESP;
	stMsgHeadCS.m_nSeq = pUserSession->m_stMsgHeadCS.m_nSeq;
	stMsgHeadCS.m_nSrcUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pUserSession->m_stMsgHeadCS.m_nDstUin;

	CModifyPasswordResp stModifyPasswordResp;
	stModifyPasswordResp.m_nResult = CModifyPasswordResp::enmResult_Unknown;
	stModifyPasswordResp.m_strTips = pStringConfig->GetString(stMsgHeadCS.m_nMsgID, stModifyPasswordResp.m_nResult);

	uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stModifyPasswordResp, arrRespBuf, sizeof(arrRespBuf));
	pRespChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pUserSession->m_stCtlHead.m_nGateID), (char *)arrRespBuf, nTotalSize);

	g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stModifyPasswordResp, "send ");

	pRedisSessionBank->DestroySession(pRedisSession);
	return 0;
}


