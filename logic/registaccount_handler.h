/*
 * registaccount_handler.h
 *
 *  Created on: Mar 10, 2015
 *      Author: jimm
 */

#ifndef REGISTACCOUNT_HANDLER_H_
#define REGISTACCOUNT_HANDLER_H_

#include "../../include/control_head.h"
#include "../../frame/frame_impl.h"
using namespace FRAME;

class CRegistAccountHandler : public IRedisReplyHandler
{
public:
	virtual int32_t OnRedisReply(int32_t nResult, void *pReply, CBaseObject *pParam);

	int32_t GetAuthCodeByPhone(ICtlHead *pCtlHead, IMsgHead *pMsgHead, IMsgBody *pMsgBody, uint8_t *pBuf, int32_t nBufSize);
};


#endif /* REGISTACCOUNT_HANDLER_H_ */
