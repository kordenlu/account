/*
 * subscribe_channel.h
 *
 *  Created on: Mar 10, 2015
 *      Author: jimm
 */

#ifndef SUBSCRIBE_CHANNEL_H_
#define SUBSCRIBE_CHANNEL_H_

#include "../../frame/frame_impl.h"

using namespace FRAME;

class CSubscribeChannel : public IRedisReplyHandler
{
public:
	virtual int32_t GetSize();

	virtual int32_t OnRedisReply(int32_t nResult, void *pReply, CBaseObject *pParam);
};


#endif /* SUBSCRIBE_CHANNEL_H_ */
