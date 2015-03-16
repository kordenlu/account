/*
 * registbaseinfo_handler.cpp
 *
 *  Created on: Mar 16, 2015
 *      Author: jimm
 */

#include "registbaseinfo_handler.h"
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
	CRedisChannel *pUserBaseInfoChannel = pRedisBank->GetRedisChannel(RK_USER_BASE_INFO);
	pUserBaseInfoChannel->HMSet(NULL, itoa(pMsgHeadCS->m_nSrcUin), "%s %s %s %s %s %s %s %d %s %d", "nickname", pRegistBaseInfoReq->m_strNickName.c_str(),
			"headimage", pRegistBaseInfoReq->m_strHeadImageAddr.c_str(), "birthday", pRegistBaseInfoReq->m_strBirthday.c_str(),
			"age", nAge, "sex", pRegistBaseInfoReq->m_nSex);

	uint8_t arrRespBuf[MAX_MSG_SIZE];

	MsgHeadCS stMsgHeadCS;
	stMsgHeadCS.m_nMsgID = MSGID_REGISTBASEINFO_RESP;
	stMsgHeadCS.m_nSeq = pMsgHeadCS->m_nSeq;
	stMsgHeadCS.m_nSrcUin = pMsgHeadCS->m_nSrcUin;
	stMsgHeadCS.m_nDstUin = pMsgHeadCS->m_nDstUin;

	CRegistBaseInfoResp stRegistBaseInfoResp;
	stRegistBaseInfoResp.m_nResult = CRegistBaseInfoResp::enmResult_OK;

	CMsgDispatchConfig *pMsgDispatchConfig = (CMsgDispatchConfig *)g_Frame.GetConfig(CONFIG_MSGDISPATCH);
	CRedisChannel *pRespChannel = pRedisBank->GetRedisChannel(pMsgDispatchConfig->GetChannelKey(MSGID_REGISTBASEINFO_RESP));
	uint16_t nTotalSize = CServerHelper::MakeMsg(pCtlHead, &stMsgHeadCS, &stRegistBaseInfoResp, arrRespBuf, sizeof(arrRespBuf));
	pRespChannel->Publish(NULL, (char *)arrRespBuf, nTotalSize);

	return 0;
}


