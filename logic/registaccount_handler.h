/*
 * registaccount_handler.h
 *
 *  Created on: Mar 10, 2015
 *      Author: jimm
 */

#ifndef REGISTACCOUNT_HANDLER_H_
#define REGISTACCOUNT_HANDLER_H_

#include "../../common/common_object.h"
#include "../../frame/frame_impl.h"
#include "../../frame/redis_session.h"
#include "../../include/control_head.h"
#include "../../include/account_msg.h"
#include "../../include/msg_head.h"
#include <string>

using namespace std;
using namespace FRAME;

class CRegistAccountHandler : public CBaseObject
{
	struct GetRegistSession
	{
		ControlHead			m_stCtlHead;
		MsgHeadCS			m_stMsgHeadCS;
		CAuthRegistPhoneReq	m_stAuthRegistPhoneReq;
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

	virtual int32_t OnRegistPhoneInfo(int32_t nResult, void *pReply, void *pSession);

	int32_t GetAuthCodeByPhone(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize);
};


#endif /* REGISTACCOUNT_HANDLER_H_ */
