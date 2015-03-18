/*
 * regist_config.h
 *
 *  Created on: Mar 18, 2015
 *      Author: jimm
 */

#ifndef REGIST_CONFIG_H_
#define REGIST_CONFIG_H_

#include "../../frame/frame_impl.h"
#include "../../frame/frame.h"
#include "../../common/common_api.h"
#include "../server_typedef.h"
#include <string>
#include <string.h>

using namespace std;
using namespace FRAME;

class CRegistConfig : public IConfig
{
public:
	CRegistConfig(const char *pConfigName)
	{
		strcpy(m_szConfigFile, pConfigName);
		m_nPhoneRegistCount = 0;
		m_nAddrRegistCount = 0;
		m_nAuthCodeLiveTime = 0;
	}

	virtual ~CRegistConfig(){};

	//初始化配置
	virtual int32_t Init();
	//卸载配置
	virtual int32_t Uninit();

	int32_t GetPhoneRegistCount()
	{
		return m_nPhoneRegistCount;
	}

	int32_t GetAddrRegistCount()
	{
		return m_nAddrRegistCount;
	}

	int32_t GetAuthCodeLiveTime()
	{
		return m_nAuthCodeLiveTime;
	}

	const char *GetAuthCodeMessage()
	{
		return m_strAuthCodeMessage.c_str();
	}

protected:
	char		m_szConfigFile[enmMaxConfigFileNameSize];
	int32_t		m_nPhoneRegistCount;
	int32_t		m_nAddrRegistCount;
	int32_t		m_nAuthCodeLiveTime;
	string		m_strAuthCodeMessage;
};

#endif /* REGIST_CONFIG_H_ */
