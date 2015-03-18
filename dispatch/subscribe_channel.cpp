/*
 * subscribe_channel.cpp
 *
 *  Created on: Mar 10, 2015
 *      Author: jimm
 */

#include "subscribe_channel.h"
#include "../../hiredis/hiredis.h"
#include "../server_typedef.h"
#include "../../logger/logger.h"
#include "../../frame/frame.h"
#include "../../common/common_codeengine.h"
#include <string.h>

using namespace FRAME;
using namespace LOGGER;

int32_t CSubscribeChannel::OnRedisReply(int32_t nResult, void *pReply, void *pSession)
{
	redisReply *pRedisReply = (redisReply *)pReply;
	if(nResult != 0)
	{
		WRITE_WARN_LOG(SERVER_NAME, "recv redis reply error!{%s}\n", pRedisReply->str);
		return 0;
	}

	if(pRedisReply->type == REDIS_REPLY_ARRAY)
	{
		redisReply *pReplyElement = pRedisReply->element[2];
		if(pReplyElement->type == REDIS_REPLY_STRING)
		{
			uint8_t *pBuf = (uint8_t *)pReplyElement->str;
			int32_t nBufSize = pReplyElement->len;
			if(nBufSize <= 0)
			{
				WRITE_WARN_LOG(SERVER_NAME, "bufsize is wrong!\n");
				return 0;
			}

			uint32_t nOffset = sizeof(uint16_t);
			uint8_t nControlHeadSize = 0;
			CCodeEngine::Decode(pBuf, nBufSize, nOffset, nControlHeadSize);

			uint16_t nMsgID = 0;
			nOffset = (nControlHeadSize + sizeof(uint16_t));
			CCodeEngine::Decode(pBuf, nBufSize, nOffset, nMsgID);

			int32_t nRet = g_Frame.FrameCallBack(nMsgID, pBuf, nBufSize);
			if(nRet == -1)
			{
				WRITE_WARN_LOG(SERVER_NAME, "frame call back return fail!\n");
			}
			else if(nRet == 1)
			{
				WRITE_WARN_LOG(SERVER_NAME, "it's not found msg handler!{msgid=0x%04x}\n", nMsgID);
			}
			else if(nRet == 2)
			{
				WRITE_WARN_LOG(SERVER_NAME, "decode msg failed!{msgid=0x%04x}\n", nMsgID);
			}
		}
	}

	return 0;
}
