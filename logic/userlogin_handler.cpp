/*
 * userlogin_handler.cpp
 *
 *  Created on: Mar 17, 2015
 *      Author: jimm
 */

#include "userlogin_handler.h"
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
#include "bank/redis_bank.h"
#include "server_typedef.h"
#include "server_util.h"

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

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pAccountNameChannel = pRedisBank->GetRedisChannel(AccountInfo::servername, pUserLoginReq->m_strAccountName.c_str());
	pAccountNameChannel->HMGet(pSession, CServerHelper::MakeRedisKey(AccountInfo::keyname, pUserLoginReq->m_strAccountName.c_str()),
			"%s %s %s %s", AccountInfo::uin, AccountInfo::password, AccountInfo::status, AccountInfo::accountid);

	return 0;
}

int32_t CUserLoginHandler::OnSessionGetAccountInfo(int32_t nResult, void *pReply, void *pSession)
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
		else
		{
			stUserLoginResp.m_nResult = CUserLoginResp::enmResult_Unknown;
			bIsReturn = true;
			break;
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
		pRespChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pUserSession->m_stCtlHead.m_nGateID), (char *)arrRespBuf, nTotalSize);

		g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stUserLoginResp, "send ");

		pRedisSessionBank->DestroySession(pRedisSession);
	}
	else
	{
		pRedisSession->SetHandleRedisReply(static_cast<RedisReply>(&CUserLoginHandler::OnSessionGetUserRelationInfo));
		pRedisSession->SetTimerProc(static_cast<TimerProc>(&CUserLoginHandler::OnRedisSessionTimeout), 60 * MS_PER_SECOND);

		pUserSession->m_nUin = nUin;
		pUserSession->m_strAccountID = strAccountID;

		CRedisChannel *pRelationCount = pRedisBank->GetRedisChannel(UserFollowers::servername, pUserSession->m_stMsgHeadCS.m_nSrcUin);
		pRelationCount->Multi();
		pRelationCount->ZCard(NULL, CServerHelper::MakeRedisKey(UserFollowers::keyname, pUserSession->m_stMsgHeadCS.m_nSrcUin));
		pRelationCount->ZCard(NULL, CServerHelper::MakeRedisKey(UserFans::keyname, pUserSession->m_stMsgHeadCS.m_nSrcUin));
		pRelationCount->ZCard(NULL, CServerHelper::MakeRedisKey(UserLookMe::keyname, pUserSession->m_stMsgHeadCS.m_nSrcUin));
		pRelationCount->Exec(pRedisSession);
	}

	return 0;
}

int32_t CUserLoginHandler::OnSessionGetUserRelationInfo(int32_t nResult, void *pReply, void *pSession)
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
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_USERLOGIN_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	uint8_t arrRespBuf[MAX_MSG_SIZE];

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
			redisReply *pFollowersCount = pRedisReply->element[0];
			if(pFollowersCount->type == REDIS_REPLY_INTEGER)
			{
				pUserSession->m_nFollowersCount = pFollowersCount->integer;
			}

			redisReply *pFansCount = pRedisReply->element[1];
			if(pFansCount->type == REDIS_REPLY_INTEGER)
			{
				pUserSession->m_nFansCount = pFansCount->integer;
			}

			redisReply *pLookMeCount = pRedisReply->element[2];
			if(pLookMeCount->type == REDIS_REPLY_INTEGER)
			{
				pUserSession->m_nLookMeCount = pLookMeCount->integer;
			}
		}
		else
		{
			stUserLoginResp.m_nResult = CUserLoginResp::enmResult_Unknown;
			bIsReturn = true;
			break;
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
		pRespChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pUserSession->m_stCtlHead.m_nGateID), (char *)arrRespBuf, nTotalSize);

		g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stUserLoginResp, "send ");

		pRedisSessionBank->DestroySession(pRedisSession);
	}
	else
	{
		pRedisSession->SetHandleRedisReply(static_cast<RedisReply>(&CUserLoginHandler::OnSessionGetUserSessionKey));
		pRedisSession->SetTimerProc(static_cast<TimerProc>(&CUserLoginHandler::OnRedisSessionTimeout), 60 * MS_PER_SECOND);

		CRedisChannel *pGetUserSessionKeyChannel = pRedisBank->GetRedisChannel(UserSessionKey::servername, pUserSession->m_nUin);
		pGetUserSessionKeyChannel->HMGet(pRedisSession, CServerHelper::MakeRedisKey(UserSessionKey::keyname, pUserSession->m_nUin),
				"%s %s", UserSessionKey::tokenkey, UserSessionKey::datakey);
	}

	return 0;
}

int32_t CUserLoginHandler::OnSessionGetUserSessionKey(int32_t nResult, void *pReply, void *pSession)
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
		WRITE_WARN_LOG(SERVER_NAME, "it's not found redis channel by msgid!{msgid=%d, srcuin=%u, dstuin=%u}\n", MSGID_USERLOGIN_RESP,
				pUserSession->m_stMsgHeadCS.m_nSrcUin, pUserSession->m_stMsgHeadCS.m_nDstUin);
		pRedisSessionBank->DestroySession(pRedisSession);
		return 0;
	}

	bool bIsFind = true;
	do
	{
		if(pRedisReply->type == REDIS_REPLY_ERROR)
		{
			bIsFind = false;
			break;
		}

		if(pRedisReply->type == REDIS_REPLY_ARRAY)
		{
			int32_t nIndex = 0;
			redisReply *pReplyElement = pRedisReply->element[nIndex++];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				pUserSession->m_strTokenKey = pReplyElement->str;
			}
			else
			{
				bIsFind = false;
				break;
			}

			pReplyElement = pRedisReply->element[nIndex++];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				pUserSession->m_strDataKey = pReplyElement->str;
			}
			else
			{
				bIsFind = false;
				break;
			}
		}
		else
		{
			bIsFind = false;
			break;
		}
	}while(0);

	if(!bIsFind)
	{
		pUserSession->m_strTokenKey = CServerUtil::MakeFixedLengthRandomString(8);
		pUserSession->m_strDataKey = CServerUtil::MakeFixedLengthRandomString(8);
		pUserSession->m_bMakeKey = true;
	}

	pRedisSession->SetHandleRedisReply(static_cast<RedisReply>(&CUserLoginHandler::OnSessionGetUserBaseInfo));
	pRedisSession->SetTimerProc(static_cast<TimerProc>(&CUserLoginHandler::OnRedisSessionTimeout), 60 * MS_PER_SECOND);

	CRedisChannel *pGetUserBaseInfoChannel = pRedisBank->GetRedisChannel(UserBaseInfo::servername, pUserSession->m_nUin);
	pGetUserBaseInfoChannel->HMGet(pRedisSession, CServerHelper::MakeRedisKey(UserBaseInfo::keyname, pUserSession->m_nUin),
			"%s %s %s %s %s %s %s", UserBaseInfo::nickname, UserBaseInfo::gender, UserBaseInfo::headimage, UserBaseInfo::version,
			UserBaseInfo::createtopics_count, UserBaseInfo::jointopics_count, UserBaseInfo::followbusline_count);

	return 0;
}

int32_t CUserLoginHandler::OnSessionGetUserBaseInfo(int32_t nResult, void *pReply, void *pSession)
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
			int32_t nIndex = 0;
			redisReply *pReplyElement = pRedisReply->element[nIndex++];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				stUserLoginResp.m_strNickName = pReplyElement->str;
			}

			pReplyElement = pRedisReply->element[nIndex++];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				stUserLoginResp.m_nGender = atoi(pReplyElement->str);
			}

			pReplyElement = pRedisReply->element[nIndex++];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				stUserLoginResp.m_strHeadImageAddr = pReplyElement->str;
			}

			pReplyElement = pRedisReply->element[nIndex++];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				stUserLoginResp.m_nSelfInfoVersion = atoi(pReplyElement->str);
			}

			pReplyElement = pRedisReply->element[nIndex++];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				stUserLoginResp.m_nCreateTopicsCount = atoi(pReplyElement->str);
			}

			pReplyElement = pRedisReply->element[nIndex++];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				stUserLoginResp.m_nJoinTopicsCount = atoi(pReplyElement->str);
			}

			pReplyElement = pRedisReply->element[nIndex++];
			if(pReplyElement->type != REDIS_REPLY_NIL)
			{
				stUserLoginResp.m_nFollowBuslineCount = atoi(pReplyElement->str);
			}
		}
		else
		{
			stUserLoginResp.m_nResult = CUserLoginResp::enmResult_Unknown;
			bIsReturn = true;
			break;
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
		stUserLoginResp.m_nFollowersCount = pUserSession->m_nFollowersCount;
		stUserLoginResp.m_nFansCount = pUserSession->m_nFansCount;
		stUserLoginResp.m_nLookMeCount = pUserSession->m_nLookMeCount;
		stUserLoginResp.m_strTokenKey = pUserSession->m_strTokenKey;
		stUserLoginResp.m_strDataKey = pUserSession->m_strDataKey;

		pRedisSession->SetHandleRedisReply(static_cast<RedisReply>(&CUserLoginHandler::OnSessionGetUserSessionInfo));
		pRedisSession->SetTimerProc(static_cast<TimerProc>(&CUserLoginHandler::OnRedisSessionTimeout), 60 * MS_PER_SECOND);

		CRedisChannel *pUserSessionChannel = pRedisBank->GetRedisChannel(UserSessionInfo::servername, pUserSession->m_stMsgHeadCS.m_nSrcUin);

		pUserSessionChannel->Multi();

		pUserSessionChannel->HMGet(NULL, CServerHelper::MakeRedisKey(UserSessionInfo::keyname, pUserSession->m_stMsgHeadCS.m_nSrcUin),
				"%s %s %s %s %s %s", UserSessionInfo::clientaddress, UserSessionInfo::clientport, UserSessionInfo::sessionid,
				UserSessionInfo::gateid, UserSessionInfo::gateredisaddress, UserSessionInfo::gateredisport);

		pUserSessionChannel->HMSet(NULL, CServerHelper::MakeRedisKey(UserSessionInfo::keyname, pUserSession->m_stMsgHeadCS.m_nSrcUin),
				"%s %u %s %u %s %d %s %d %s %d %s %u %s %d",
				UserSessionInfo::sessionid, pUserSession->m_stCtlHead.m_nSessionID,
				UserSessionInfo::clientaddress, pUserSession->m_stCtlHead.m_nClientAddress,
				UserSessionInfo::clientport, pUserSession->m_stCtlHead.m_nClientPort,
				UserSessionInfo::gateid, pUserSession->m_stCtlHead.m_nGateID,
				UserSessionInfo::phonetype, pUserSession->m_stCtlHead.m_nPhoneType,
				UserSessionInfo::gateredisaddress, pUserSession->m_stCtlHead.m_nGateRedisAddress,
				UserSessionInfo::gateredisport, pUserSession->m_stCtlHead.m_nGateRedisPort);

		pUserSessionChannel->Expire(NULL, CServerHelper::MakeRedisKey(UserSessionInfo::keyname, pUserSession->m_stMsgHeadCS.m_nSrcUin),
				HEARTBEAT_INTERVAL * (HEARTBEAT_MISS + 1));

		pUserSessionChannel->Exec(pRedisSession);
	}

	uint16_t nTotalSize = CServerHelper::MakeMsg(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stUserLoginResp, arrRespBuf, sizeof(arrRespBuf));
	pRespChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pUserSession->m_stCtlHead.m_nGateID), (char *)arrRespBuf, nTotalSize);

	g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stUserLoginResp, "send ");

	if(bIsReturn)
	{
		pRedisSessionBank->DestroySession(pRedisSession);
	}

	return 0;
}

int32_t CUserLoginHandler::OnSessionGetUserSessionInfo(int32_t nResult, void *pReply, void *pSession)
{
	redisReply *pRedisReply = (redisReply *)pReply;
	RedisSession *pRedisSession = (RedisSession *)pSession;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	ControlHead stCtlHead;

	bool bGetSessionSuccess = true;
	do
	{
		if(pRedisReply->type == REDIS_REPLY_ERROR)
		{
			bGetSessionSuccess = false;
			break;
		}

		if(pRedisReply->type == REDIS_REPLY_ARRAY)
		{
			redisReply *pReplyElement = pRedisReply->element[0];
			if(pReplyElement->type == REDIS_REPLY_ARRAY)
			{
				redisReply *pReplyGetUserSession = pReplyElement->element[0];
				if(pReplyGetUserSession->type != REDIS_REPLY_NIL)
				{
					stCtlHead.m_nClientAddress = atoi(pReplyGetUserSession->str);
				}
				else
				{
					bGetSessionSuccess = false;
					break;
				}

				pReplyGetUserSession = pReplyElement->element[1];
				if(pReplyGetUserSession->type != REDIS_REPLY_NIL)
				{
					stCtlHead.m_nClientPort = atoi(pReplyGetUserSession->str);
				}
				else
				{
					bGetSessionSuccess = false;
					break;
				}

				pReplyGetUserSession = pReplyElement->element[2];
				if(pReplyGetUserSession->type != REDIS_REPLY_NIL)
				{
					stCtlHead.m_nSessionID = atoi(pReplyGetUserSession->str);
				}
				else
				{
					bGetSessionSuccess = false;
					break;
				}

				pReplyGetUserSession = pReplyElement->element[3];
				if(pReplyGetUserSession->type != REDIS_REPLY_NIL)
				{
					stCtlHead.m_nGateID = atoi(pReplyGetUserSession->str);
				}
				else
				{
					bGetSessionSuccess = false;
					break;
				}

				pReplyGetUserSession = pReplyElement->element[4];
				if(pReplyGetUserSession->type != REDIS_REPLY_NIL)
				{
					stCtlHead.m_nGateRedisAddress = atoi(pReplyGetUserSession->str);
				}
				else
				{
					bGetSessionSuccess = false;
					break;
				}

				pReplyGetUserSession = pReplyElement->element[5];
				if(pReplyGetUserSession->type != REDIS_REPLY_NIL)
				{
					stCtlHead.m_nGateRedisPort = atoi(pReplyGetUserSession->str);
				}
				else
				{
					bGetSessionSuccess = false;
					break;
				}
			}

		}
		else
		{
			bGetSessionSuccess = false;
			break;
		}
	}while(0);

	if(bGetSessionSuccess)
	{
		stCtlHead.m_nUin = pUserSession->m_stMsgHeadCS.m_nSrcUin;

		CRedisChannel *pClientRespChannel = pRedisBank->GetRedisChannel(stCtlHead.m_nGateRedisAddress, stCtlHead.m_nGateRedisPort);

		CServerHelper::KickUser(&stCtlHead, &pUserSession->m_stMsgHeadCS, pClientRespChannel, KickReason_AnotherLogin);
	}

	pRedisSession->SetHandleRedisReply(static_cast<RedisReply>(&CUserLoginHandler::OnSessionGetUnreadMsgCount));

	CRedisChannel *pUnreadMsgChannel = pRedisBank->GetRedisChannel(UserUnreadMsgList::servername, pUserSession->m_stMsgHeadCS.m_nSrcUin);

	pUnreadMsgChannel->ZCard(pRedisSession, CServerHelper::MakeRedisKey(UserUnreadMsgList::keyname, pUserSession->m_stMsgHeadCS.m_nSrcUin));

	CRedisChannel *pUserBaseChannel = pRedisBank->GetRedisChannel(UserBaseInfo::servername, pUserSession->m_stMsgHeadCS.m_nSrcUin);
	pUserBaseChannel->HMSet(NULL, CServerHelper::MakeRedisKey(UserBaseInfo::keyname, pUserSession->m_stMsgHeadCS.m_nSrcUin),
			"%s %d %s %s %s %s %s %ld", UserBaseInfo::phonetype, pUserSession->m_stUserLoginReq.m_nPhoneType,
			UserBaseInfo::osversion, pUserSession->m_stUserLoginReq.m_strOSVer.c_str(),
			UserBaseInfo::phonestyle, pUserSession->m_stUserLoginReq.m_strPhoneStyle.c_str(),
			UserBaseInfo::lastlogintime, CDateTime::CurrentDateTime().Seconds());

	CRedisChannel *pUserSessionKeyChannel = pRedisBank->GetRedisChannel(UserSessionKey::servername, pUserSession->m_nUin);
	if(pUserSession->m_bMakeKey)
	{
		pUserSessionKeyChannel->HMSet(NULL, CServerHelper::MakeRedisKey(UserSessionKey::keyname, pUserSession->m_stMsgHeadCS.m_nSrcUin),
				"%s %s %s %s", UserSessionKey::tokenkey, pUserSession->m_strTokenKey.c_str(),
				UserSessionKey::datakey, pUserSession->m_strDataKey.c_str());
	}
	pUserSessionKeyChannel->Expire(NULL, CServerHelper::MakeRedisKey(UserSessionKey::keyname, pUserSession->m_stMsgHeadCS.m_nSrcUin),
			SECOND_PER_WEEK);

	return 0;
}

int32_t CUserLoginHandler::OnSessionGetUnreadMsgCount(int32_t nResult, void *pReply, void *pSession)
{
	redisReply *pRedisReply = (redisReply *)pReply;
	RedisSession *pRedisSession = (RedisSession *)pSession;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	bool bIsSyncNoti = true;
	do
	{
		if(pRedisReply->type == REDIS_REPLY_ERROR)
		{
			bIsSyncNoti = false;
			break;
		}

		if(pRedisReply->type == REDIS_REPLY_INTEGER)
		{
			if(pRedisReply->integer <= 0)
			{
				bIsSyncNoti = false;
				break;
			}
		}
		else
		{
			bIsSyncNoti = false;
			break;
		}
	}while(0);

	if(bIsSyncNoti)
	{
		CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
		CRedisChannel *pPushClientChannel = pRedisBank->GetRedisChannel(pUserSession->m_stCtlHead.m_nGateRedisAddress, pUserSession->m_stCtlHead.m_nGateRedisPort);

		CServerHelper::SendSyncNoti(pPushClientChannel, &pUserSession->m_stCtlHead, pUserSession->m_stMsgHeadCS.m_nSrcUin);
	}

	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	pRedisSessionBank->DestroySession(pRedisSession);

	return 0;
}

int32_t CUserLoginHandler::OnRedisSessionTimeout(void *pTimerData)
{
	CRedisSessionBank *pRedisSessionBank = (CRedisSessionBank *)g_Frame.GetBank(BANK_REDIS_SESSION);
	RedisSession *pRedisSession = (RedisSession *)pTimerData;
	UserSession *pUserSession = (UserSession *)pRedisSession->GetSessionData();

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pRespChannel = pRedisBank->GetRedisChannel(pUserSession->m_stCtlHead.m_nGateRedisAddress, pUserSession->m_stCtlHead.m_nGateRedisPort);
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
	pRespChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pUserSession->m_stCtlHead.m_nGateID), (char *)arrRespBuf, nTotalSize);

	g_Frame.Dump(&pUserSession->m_stCtlHead, &stMsgHeadCS, &stUserLoginResp, "send ");

	pRedisSessionBank->DestroySession(pRedisSession);
	return 0;
}
