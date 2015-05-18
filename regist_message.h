/*
 * regist_message.h
 *
 *  Created on: Mar 10, 2015
 *      Author: jimm
 */

#ifndef REGIST_MESSAGE_H_
#define REGIST_MESSAGE_H_

#include "frame/frame.h"
#include "include/msg_head.h"
#include "include/control_head.h"
#include "include/account_msg.h"
#include "logic/requestauth_handler.h"
#include "logic/verifyauthcode_handler.h"
#include "logic/registbaseinfo_handler.h"
#include "logic/userlogin_handler.h"
#include "logic/updatedevicetoken_handler.h"
#include "logic/resetpassword_handler.h"
#include "logic/verifyresetpw_handler.h"
#include "logic/modifypassword_handler.h"

using namespace FRAME;

MSGMAP_BEGIN(msgmap)
ON_PROC_PCH_PMH_PMB_PU8_I32(MSGID_REQUESTAUTH_REQ, ControlHead, MsgHeadCS, CRequestAuthReq, CRequstAuthHandler, CRequstAuthHandler::GetAuthCodeByPhone);
ON_PROC_PCH_PMH_PMB_PU8_I32(MSGID_VERIFYAUTHCODE_REQ, ControlHead, MsgHeadCS, CVerifyAuthCodeReq, CVerifyAuthCodeHandler, CVerifyAuthCodeHandler::VerifyAuthCode);
ON_PROC_PCH_PMH_PMB_PU8_I32(MSGID_REGISTBASEINFO_REQ, ControlHead, MsgHeadCS, CRegistBaseInfoReq, CRegistBaseInfoHandler, CRegistBaseInfoHandler::RegistBaseInfo);
ON_PROC_PCH_PMH_PMB_PU8_I32(MSGID_USERLOGIN_REQ, ControlHead, MsgHeadCS, CUserLoginReq, CUserLoginHandler, CUserLoginHandler::UserLogin);
ON_PROC_PCH_PMH_PMB_PU8_I32(MSGID_UPDATEDEVICETOKEN_REQ, ControlHead, MsgHeadCS, CUpdateDeviceTokenReq, CUpdateDeviceTokenHandler, CUpdateDeviceTokenHandler::UpdateDeviceToken);
ON_PROC_PCH_PMH_PMB_PU8_I32(MSGID_RESETPASSWORD_REQ, ControlHead, MsgHeadCS, CResetPasswordReq, CResetPasswordHandler, CResetPasswordHandler::GetAuthCodeByPhone);
ON_PROC_PCH_PMH_PMB_PU8_I32(MSGID_VERIFYRESETPW_REQ, ControlHead, MsgHeadCS, CVerifyResetPWReq, CVerifyResetPWHandler, CVerifyResetPWHandler::VerifyAuthCode);
ON_PROC_PCH_PMH_PMB_PU8_I32(MSGID_MODIFYPASSWORD_REQ, ControlHead, MsgHeadCS, CModifyPasswordReq, CModifyPasswordHandler, CModifyPasswordHandler::ModifyPassword);
MSGMAP_END(msgmap)

#endif /* REGIST_MESSAGE_H_ */
