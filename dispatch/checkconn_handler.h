/*
 * checkconn_handler.h
 *
 *  Created on: Mar 7, 2015
 *      Author: jimm
 */

#ifndef CHECKCONN_HANDLER_H_
#define CHECKCONN_HANDLER_H_

#include "../../common/common_object.h"
#include "../../frame/frame_timer.h"

using namespace FRAME;

class CCheckConnHandler : public CBaseObject
{
public:
	CCheckConnHandler()
	{
		Init();
	}

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

	int32_t CheckConnStatus(CTimer *pTimer);
};


#endif /* CHECKCONN_HANDLER_H_ */
