/*
 * verifyauthcode_handler.h
 *
 *  Created on: Mar 16, 2015
 *      Author: jimm
 */

#ifndef VERIFYAUTHCODE_HANDLER_H_
#define VERIFYAUTHCODE_HANDLER_H_

#include "../../common/common_object.h"
#include "../../frame/frame_impl.h"
#include "../../frame/redis_session.h"
#include "../../include/control_head.h"
#include "../../include/account_msg.h"
#include "../../include/msg_head.h"
#include <string>

using namespace std;
using namespace FRAME;

class CVerifyAuthCodeHandler : public CBaseObject
{
	struct UserSession
	{
		UserSession()
		{
		}
		ControlHead			m_stCtlHead;
		MsgHeadCS			m_stMsgHeadCS;
		CVerifyAuthCodeReq	m_stVerifyAuthCodeReq;
	};
public:

	virtual int32_t Init()
	{
		return 0;
	}
	virtual int32_t Uninit()
	{
		return 0;
	}
	virtual int32_t GetSize()
	{
		return 0;
	}

	int32_t VerifyAuthCode(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize);

	int32_t OnSessionGetRegistPhoneInfo(int32_t nResult, void *pReply, void *pSession);

	int32_t OnTimeoutGetRegistPhoneInfo(void *pTimerData);

	int32_t OnSessionGetGobalUin(int32_t nResult, void *pReply, void *pSession);

	int32_t OnTimeoutGetGobalUin(void *pTimerData);
};



#endif /* VERIFYAUTHCODE_HANDLER_H_ */
