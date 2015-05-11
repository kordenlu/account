/*
 * regist_config.cpp
 *
 *  Created on: Mar 18, 2015
 *      Author: jimm
 */

#include "regist_config.h"
#include "../../tinyxml/tinyxml.h"
#include "../../logger/logger.h"
#include "../server_typedef.h"

using namespace LOGGER;

//注册到配置管理器
REGIST_CONFIG_SAFE(CONFIG_REGIST, CRegistConfig)

//初始化配置
int32_t CRegistConfig::Init()
{
	return 0;
}

//卸载配置
int32_t CRegistConfig::Uninit()
{
	return 0;
}

int32_t CRegistConfig::Parser(char *pXMLString)
{
	TiXmlDocument doc;
	doc.Parse(pXMLString, 0, TIXML_ENCODING_UTF8);

	//获取根节点
	TiXmlElement *pRoot = doc.RootElement();
	if (NULL == pRoot)
	{
		WRITE_WARN_LOG(SERVER_NAME, "%s is not found redis node!\n", m_szConfigFile);
		return 1;
	}

	TiXmlElement *pRegist = pRoot->FirstChildElement("regist");
	if(NULL == pRegist)
	{
		WRITE_WARN_LOG(SERVER_NAME, "%s is not found regist node!\n", m_szConfigFile);
		return 1;
	}

	const char* pszValue = NULL;

	pszValue = pRegist->Attribute("phone_regist_count", &m_nPhoneRegistCount);
	if(NULL == pszValue)
	{
		WRITE_WARN_LOG(SERVER_NAME, "%s is not found phone_regist_count node!\n", m_szConfigFile);
		return 1;
	}

	pszValue = pRegist->Attribute("address_regist_count", &m_nAddrRegistCount);
	if(NULL == pszValue)
	{
		WRITE_WARN_LOG(SERVER_NAME, "%s is not found address_regist_count node!\n", m_szConfigFile);
		return 1;
	}

	TiXmlElement *pAuth = pRoot->FirstChildElement("auth");
	if(NULL == pAuth)
	{
		WRITE_WARN_LOG(SERVER_NAME, "%s is not found auth node!\n", m_szConfigFile);
		return 1;
	}

	pszValue = pAuth->Attribute("auth_code_live_time", &m_nAuthCodeLiveTime);
	if(NULL == pszValue)
	{
		WRITE_WARN_LOG(SERVER_NAME, "%s is not found auth_code_live_time node!\n", m_szConfigFile);
		return 1;
	}

	pszValue = pAuth->Attribute("message");
	if(NULL == pszValue)
	{
		WRITE_WARN_LOG(SERVER_NAME, "%s is not found message node!\n", m_szConfigFile);
		return 1;
	}

	m_strAuthCodeMessage = string(pszValue);

	return 0;
}

