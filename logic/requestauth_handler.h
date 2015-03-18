/*
 * requestauth_handler.h
 *
 *  Created on: Mar 10, 2015
 *      Author: jimm
 */

#ifndef QUESTAUTH_HANDLER_H_
#define QUESTAUTH_HANDLER_H_

#include "../../common/common_object.h"
#include "../../frame/frame_impl.h"
#include "../../frame/redis_session.h"
#include "../../include/control_head.h"
#include "../../include/account_msg.h"
#include "../../include/msg_head.h"
#include <string>

using namespace std;
using namespace FRAME;

class CRequstAuthHandler : public CBaseObject
{
	struct UserSession
	{
		UserSession()
		{
			m_nAuthCode = 0;
			m_nAuthCodeExpireTime = 0;
		}
		ControlHead			m_stCtlHead;
		MsgHeadCS			m_stMsgHeadCS;
		CRequestAuthReq		m_stAuthRegistPhoneReq;
		uint32_t			m_nAuthCode;
		int64_t				m_nAuthCodeExpireTime;
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

	int32_t GetAuthCodeByPhone(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize);

	int32_t OnSessionAccountIsExist(int32_t nResult, void *pReply, void *pSession);

	int32_t OnSessionGetRegistPhoneInfo(int32_t nResult, void *pReply, void *pSession);

	int32_t OnSessionGetRegistAddrInfo(int32_t nResult, void *pReply, void *pSession);

	int32_t OnRedisSessionTimeout(void *pTimerData);
};


#endif /* QUESTAUTH_HANDLER_H_ */
