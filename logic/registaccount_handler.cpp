/*
 * registaccount_handler.cpp
 *
 *  Created on: Mar 10, 2015
 *      Author: jimm
 */

#include "registaccount_handler.h"
#include "../../frame/redis_agent.h"
#include "../../frame/redis_glue.h"
#include "../../frame/frame.h"
#include "../../frame/server_helper.h"
#include "../../include/account_msg.h"
#include "../../include/msg_head.h"
#include "../../include/cachekey_define.h"
#include "../../include/control_head.h"
#include "../../include/typedef.h"
#include "../server_typedef.h"
#include "../bank/redis_bank.h"

using namespace FRAME;

int32_t CRegistAccountHandler::OnRedisReply(int32_t nResult, void *pReply, CBaseObject *pParam)
{
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
	CRedisChannel *pRedisChannel = pRedisBank->GetRedisChannel(RDSKEY_CHANNEL_CLIENT_RESP);
	if(pRedisChannel == NULL)
	{
		return 0;
	}

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_AUTHREGISTPHONE_RESP;
	stMsgHeadCS.m_nSeq = pMsgHeadCS->m_nSeq;
	stMsgHeadCS.m_nSrcUin = pMsgHeadCS->m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pMsgHeadCS->m_nDstUin;

	CAuthRegistPhoneResp stAuthRegistPhoneResp;
	stAuthRegistPhoneResp.m_nResult = 0;

	uint16_t nTotalSize = CServerHelper::MakeMsg(pCtlHead, &stMsgHeadCS, &stAuthRegistPhoneResp, arrRespBuf, sizeof(arrRespBuf));

	pRedisChannel->Publish((char *)arrRespBuf, nTotalSize);

	return 0;
}
