/*
 * subscribe_channel.h
 *
 *  Created on: Mar 10, 2015
 *      Author: jimm
 */

#ifndef SUBSCRIBE_CHANNEL_H_
#define SUBSCRIBE_CHANNEL_H_

#include "../../common/common_typedef.h"
#include "../../common/common_object.h"

class CSubscribeChannel : public CBaseObject
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
		return sizeof(*this);
	}

	virtual int32_t OnRedisReply(int32_t nResult, void *pReply, void *pSession);
};


#endif /* SUBSCRIBE_CHANNEL_H_ */
