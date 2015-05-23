/*
 * registbaseinfo_handler.cpp
 *
 *  Created on: Mar 16, 2015
 *      Author: jimm
 */

#include "registbaseinfo_handler.h"
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

int32_t CRegistBaseInfoHandler::RegistBaseInfo(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize)
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

	CRegistBaseInfoReq *pRegistBaseInfoReq = dynamic_cast<CRegistBaseInfoReq *>(pMsgBody);
	if(pRegistBaseInfoReq == NULL)
	{
		return 0;
	}

	int32_t nCurYear = CDateTime::CurrentDateTime().Year();
	int32_t nBornYear = atoi(pRegistBaseInfoReq->m_strBirthday.c_str());
	int32_t nAge = nCurYear - nBornYear;

	CRedisBank *pRedisBank = (CRedisBank *)g_Frame.GetBank(BANK_REDIS);
	CRedisChannel *pUserBaseInfoChannel = pRedisBank->GetRedisChannel(UserBaseInfo::servername, pMsgHeadCS->m_nSrcUin);
	pUserBaseInfoChannel->HMSet(NULL, CServerHelper::MakeRedisKey(UserBaseInfo::keyname, pMsgHeadCS->m_nSrcUin), "%s %s %s %s %s %s %s %d %s %d",
			UserBaseInfo::nickname, pRegistBaseInfoReq->m_strNickName.c_str(),
			UserBaseInfo::headimage, pRegistBaseInfoReq->m_strHeadImageAddr.c_str(),
			UserBaseInfo::birthday, pRegistBaseInfoReq->m_strBirthday.c_str(),
			UserBaseInfo::age, nAge, UserBaseInfo::gender, pRegistBaseInfoReq->m_nGender);

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_REGISTBASEINFO_RESP;
	stMsgHeadCS.m_nSeq = pMsgHeadCS->m_nSeq;
	stMsgHeadCS.m_nSrcUin = pMsgHeadCS->m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pMsgHeadCS->m_nDstUin;

	CRegistBaseInfoResp stRegistBaseInfoResp;
	stRegistBaseInfoResp.m_nResult = CRegistBaseInfoResp::enmResult_OK;

	CRedisChannel *pRespChannel = pRedisBank->GetRedisChannel(pControlHead->m_nGateRedisAddress, pControlHead->m_nGateRedisPort);
	uint16_t nTotalSize = CServerHelper::MakeMsg(pCtlHead, &stMsgHeadCS, &stRegistBaseInfoResp, arrRespBuf, sizeof(arrRespBuf));
	pRespChannel->RPush(NULL, CServerHelper::MakeRedisKey(ClientResp::keyname, pControlHead->m_nGateID), (char *)arrRespBuf, nTotalSize);

	g_Frame.Dump(pCtlHead, &stMsgHeadCS, &stRegistBaseInfoResp, "send ");
	return 0;
}


