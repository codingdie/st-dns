//
// Created by System Administrator on 2020/10/23.
//

#ifndef ST_DNS_LAUCHEDUTILS_H
#define ST_DNS_LAUCHEDUTILS_H

#include "STUtils.h"

namespace st {
    namespace dns {
        namespace service {
            bool start() {
                string result;
                string error;
                shell::exec("scriptDir=$(cd $(dirname $0); pwd)"
                            "&&cp -f  ${scriptDir}/com.codingdie.st.dns.plist /Library/LaunchDaemons/com.codingdie.st.dns.plist"
                            "&&launchctl unload /Library/LaunchDaemons/com.codingdie.st.dns.plist"
                            "&&launchctl load  /Library/LaunchDaemons/com.codingdie.st.dns.plist", result, error)
                return false;
            }

            bool stop() {
                return false;
            }
        }
    }
}
#endif //ST_DNS_LAUCHEDUTILS_H
