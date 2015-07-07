/*
 * userlogin_handler.h
 *
 *  Created on: Mar 17, 2015
 *      Author: jimm
 */

#ifndef USERLOGIN_HANDLER_H_
#define USERLOGIN_HANDLER_H_

#include "common/common_object.h"
#include "frame/frame_impl.h"
#include "frame/redis_session.h"
#include "include/control_head.h"
#include "include/account_msg.h"
#include "include/msg_head.h"
#include <string>

using namespace std;
using namespace FRAME;

class CUserLoginHandler : public CBaseObject
{
	struct UserSession
	{
		UserSession()
		{
			m_nUin = 0;
			m_nFollowersCount = 0;
			m_nFansCount = 0;
			m_nLookMeCount = 0;
			m_bMakeKey = false;
		}
		uint32_t				m_nUin;
		string				m_strAccountID;
		ControlHead			m_stCtlHead;
		MsgHeadCS			m_stMsgHeadCS;
		CUserLoginReq		m_stUserLoginReq;
		uint32_t			m_nFollowersCount;
		uint32_t			m_nFansCount;
		uint32_t			m_nLookMeCount;
		bool				m_bMakeKey;
		string				m_strTokenKey;
		string				m_strDataKey;
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

	int32_t UserLogin(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize);

	int32_t OnSessionGetAccountInfo(int32_t nResult, void *pReply, void *pSession);

	int32_t OnSessionGetUserRelationInfo(int32_t nResult, void *pReply, void *pSession);

	int32_t OnSessionGetUserSessionKey(int32_t nResult, void *pReply, void *pSession);

	int32_t OnSessionGetUserBaseInfo(int32_t nResult, void *pReply, void *pSession);

	int32_t OnSessionGetUserSessionInfo(int32_t nResult, void *pReply, void *pSession);

	int32_t OnSessionGetUnreadMsgCount(int32_t nResult, void *pReply, void *pSession);

	int32_t OnRedisSessionTimeout(void *pTimerData);

};


#endif /* USERLOGIN_HANDLER_H_ */
