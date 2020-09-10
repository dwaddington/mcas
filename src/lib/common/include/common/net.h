#ifndef __COMMON_NET_H__
#define __COMMON_NET_H__

#include <common/common.h>
#include <string>

namespace common
{

/**
 * Convert ip address to network interface name
 *
 * @param ipaddr IP address string
 *
 * @return Network interface name
 */
std::string get_eth_device_from_ip(const std::string& ipaddr);

}

#endif
