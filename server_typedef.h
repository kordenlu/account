/*
 * server_typedef.h
 *
 *  Created on: 2014年1月23日
 *      Author: jimm
 */

#ifndef SERVER_TYPEDEF_H_
#define SERVER_TYPEDEF_H_

#include "common/common_typedef.h"

#define SERVER_NAME			"account"

#define CONFIG_SERVER		"server_config.xml"
#define CONFIG_REDIS		"redis_config.xml"
#define CONFIG_MSGDISPATCH	"msgdispatch_config.xml"
#define CONFIG_STRING		"string_config.xml"
#define CONFIG_REGIST		"regist_config.xml"

#define BANK_REDIS			"redis_bank"

#define HEARTBEAT_MISS		3		//can miss heartbeat
#define HEARTBEAT_INTERVAL	60		//second

typedef int32_t			ServerID;
enum
{
	enmInvalidServerID		= 0,
};

enum
{
	enmMaxConfigFileNameSize	= 1024,
	enmMaxPacketTokenSize		= 64,
	enmIPV4NetAddressSize		= 16,
	enmMaxNetNodeCount			= 32,
	enmMaxRedisCount			= 128,
};

#endif /* SERVER_TYPEDEF_H_ */
