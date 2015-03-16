/*
 * regist_message.h
 *
 *  Created on: Mar 10, 2015
 *      Author: jimm
 */

#ifndef REGIST_MESSAGE_H_
#define REGIST_MESSAGE_H_

#include "../frame/frame.h"
#include "../include/msg_head.h"
#include "../include/control_head.h"
#include "../include/account_msg.h"
#include "logic/requestauth_handler.h"
#include "logic/verifyauthcode_handler.h"
#include "logic/registbaseinfo_handler.h"

using namespace FRAME;

MSGMAP_BEGIN(msgmap)
ON_PROC_PCH_PMH_PMB_PU8_I32(MSGID_REQUESTAUTH_REQ, ControlHead, MsgHeadCS, CRequestAuthReq, CRequstAuthHandler, CRequstAuthHandler::GetAuthCodeByPhone);
ON_PROC_PCH_PMH_PMB_PU8_I32(MSGID_VERIFYAUTHCODE_REQ, ControlHead, MsgHeadCS, CVerifyAuthCodeReq, CVerifyAuthCodeHandler, CVerifyAuthCodeHandler::VerifyAuthCode);
ON_PROC_PCH_PMH_PMB_PU8_I32(MSGID_REGISTBASEINFO_REQ, ControlHead, MsgHeadCS, CRegistBaseInfoReq, CRegistBaseInfoHandler, CRegistBaseInfoHandler::RegistBaseInfo);
MSGMAP_END(msgmap)

#endif /* REGIST_MESSAGE_H_ */
