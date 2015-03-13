/*
 * msg_parser.cpp
 *
 *  Created on: 2014��5��13��
 *      Author: jimm
 */

#include "msg_parser.h"
#include "../server_typedef.h"
#include "../../include/msg_head.h"
#include "../../common/common_codeengine.h"
#include "../../common/common_crypt.h"

static char g_arrSSKey[16] = {'v', 'd', 'c', '$', 'a', 'u', 't', 'h', '@', '1', '7','9','.', 'c', 'o', 'm'};

int32_t CMsgParser::Init()
{
	return 0;
}

int32_t CMsgParser::Uninit()
{
	return 0;
}

int32_t CMsgParser::GetSize()
{
	return sizeof(*this);
}

int32_t CMsgParser::Parser(const uint8_t arrInputBuf[], const uint32_t nInputBufSize, uint8_t arrOutputBuf[], int32_t nOutputBufSize)
{
	uint16_t nTotalSize = 0;
	uint32_t nOffset = 0;
	int32_t nRet = CCodeEngine::Decode((unsigned char *)arrInputBuf, nInputBufSize, nOffset, nTotalSize);
	if(nRet < 0)
	{
		return 0;
	}

	if(nTotalSize > nInputBufSize)
	{
		return 0;
	}

	MsgHeadCS stMsgHeadCS;
	int32_t nHeadSize = stMsgHeadCS.GetSize();
	int32_t nBodySize = CXTEA::Encrypt((char *)&arrInputBuf[nHeadSize], nTotalSize - nHeadSize,
		(char *)&arrOutputBuf[nHeadSize], nOutputBufSize - nHeadSize, g_arrSSKey);
	if(nBodySize <= 0)
	{
		return 0;
	}

	uint16_t nPacketSize = nHeadSize + nBodySize;
	memcpy(arrOutputBuf, arrInputBuf, nHeadSize);

	nOffset = 0;
	CCodeEngine::Encode(arrOutputBuf, nOutputBufSize, nOffset, nPacketSize);

	return nTotalSize;
}

