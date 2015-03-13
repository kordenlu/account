/*
 * msg_handler.cpp
 *
 *  Created on: 2014��2��20��
 *      Author: Administrator
 */

#include "../../logger/logger.h"
#include "../../frame/frame_msghandle.h"
#include "../../frame/frame_define.h"
#include "../../frame/frame.h"
#include "../server_typedef.h"
#include "msg_handler.h"
#include "../../include/account_msg.h"

using namespace LOGGER;
using namespace FRAME;

int32_t CMsgHandler::OnOpened(IIOSession *pIoSession)
{
	WRITE_DEBUG_LOG(SERVER_NAME, "new session!{peeraddress=%s, peerport=%d}\n",
			pIoSession->GetPeerAddressStr(), pIoSession->GetPeerPort());

//	MsgHead stMsgHead;
//	stMsgHead.m_nMsgID = 1;
//	stMsgHead.m_nSrcUin = 100;
//	stMsgHead.m_nDstUin = 101;
//
//	CAuthRegistPhoneReq stReq;
//	stReq.m_strPhone = "18616274138";
//
//	g_Frame.SendMsg(NULL, &stMsgHead, &stReq);

	return 0;
}

int32_t CMsgHandler::OnRecved(IIOSession *pIoSession, uint8_t *pBuf, uint32_t nBufSize)
{
	WRITE_DEBUG_LOG(SERVER_NAME, "recv message : [size=%d]:%s\n", nBufSize, (char *)pBuf);

	return 0;
}

int32_t CMsgHandler::OnSent(IIOSession *pIoSession, uint8_t *pBuf, uint32_t nBufSize)
{
	WRITE_DEBUG_LOG(SERVER_NAME, "sent message : [size=%d]:%s\n", nBufSize, (char *)pBuf);

	return 0;
}

int32_t CMsgHandler::OnClosed(IIOSession *pIoSession)
{
	WRITE_DEBUG_LOG(SERVER_NAME, "session closed!{peeraddress=%s, peerport=%d}\n",
			pIoSession->GetPeerAddressStr(), pIoSession->GetPeerPort());

	return 0;
}

int32_t CMsgHandler::OnError(IIOSession *pIoSession)
{
	WRITE_DEBUG_LOG(SERVER_NAME, "session error!{peeraddress=%s, peerport=%d}\n",
			pIoSession->GetPeerAddressStr(), pIoSession->GetPeerPort());

	return 0;
}

int32_t CMsgHandler::OnTimeout(IIOSession *pIoSession)
{
	WRITE_DEBUG_LOG(SERVER_NAME, "session timeout!{peeraddress=%s, peerport=%d}\n",
			pIoSession->GetPeerAddressStr(), pIoSession->GetPeerPort());

	return 0;
}

