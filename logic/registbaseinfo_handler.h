/*
 * registbaseinfo_handler.h
 *
 *  Created on: Mar 16, 2015
 *      Author: jimm
 */

#ifndef REGISTBASEINFO_HANDLER_H_
#define REGISTBASEINFO_HANDLER_H_

#include "common/common_object.h"
#include "frame/frame_impl.h"
#include "frame/redis_session.h"
#include "include/control_head.h"
#include "include/account_msg.h"
#include "include/msg_head.h"
#include <string>

using namespace std;
using namespace FRAME;

class CRegistBaseInfoHandler : public CBaseObject
{
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

	int32_t RegistBaseInfo(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize);
};


#endif /* REGISTBASEINFO_HANDLER_H_ */
