/*
 * server_util.h
 *
 *  Created on: Jun 5, 2015
 *      Author: jimm
 */

#ifndef SERVER_UTIL_H_
#define SERVER_UTIL_H_

#include "common/common_typedef.h"
#include <string>

using namespace std;

class CServerUtil
{
public:
	static string MakeFixedLengthRandomString(uint8_t nLength);
};



#endif /* SERVER_UTIL_H_ */
