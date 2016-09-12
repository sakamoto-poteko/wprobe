/* Copyright (C) - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 *
 * Proprietary and confidential
 *
 * Written by Afa Cheng <afa@afa.moe>, 2016
 */

#include <string>
#include <vector>
#include <map>
#include <set>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cinttypes>

#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <linux/if_packet.h>

#include <linux/nl80211.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>


#include "wprobed_global.h"
#include "iface_init.h"

typedef std::map<std::string, std::uint64_t> iface_mac_map;

static iface_mac_map get_all_ifaces()
{
    std::map<std::string, std::uint64_t> ret;
    ifaddrs *ifaddr;

    if (getifaddrs(&ifaddr) == -1) {
        syslog(LOG_ERR, "failed to get retrieve interfaces: %s", std::strerror(errno));
        exit_with_error();
    }

    for (ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        if (ifa->ifa_addr->sa_family == AF_PACKET) {
            sockaddr_ll *addr = (sockaddr_ll *)ifa->ifa_addr;
            uint64_t mac, he_mac;
#ifdef __LITTLE_ENDIAN
            mac = *(uint64_t *)(addr->sll_addr) & 0x00FFFFFFFFFFFFUL;
            he_mac = be64toh(mac) >> 16;
#else
#error "Big-endian not supported yet"
#endif
            syslog(LOG_DEBUG, "iface %s, MAC %012" PRIX64, ifa->ifa_name, he_mac);

            ret[ifa->ifa_name] = mac;
        }
    }

    freeifaddrs(ifaddr);

    return ret;
}

static void setup_wifaces(iface_mac_map all_ifaces)
{
    iface_mac_map wifaces;
    std::map<std::string, std::string> physMonMap;
    std::set<std::string> phys;

    int err;
    nl_sock *sk = nl_socket_alloc();

    if (!sk) {
        syslog(LOG_ERR, "failed to allocate netlink socket");
        exit_with_error();
    }


    err = genl_connect(sk);
    if (err < 0) {
        syslog(LOG_ERR, "failed to connect netlink socket: %s", nl_geterror(err));
        exit_with_error();
    }

    int eid = genl_ctrl_resolve(sk, "nl80211");
    if (eid < 0) {
        syslog(LOG_ERR, "failed to resolve `nl80211': %s", nl_geterror(eid));
        exit_with_error();
    }

    for (auto &iface : all_ifaces) {
        nl80211_commands cmd = NL80211_CMD_GET_INTERFACE;
        int ifidx = if_nametoindex(iface.first.c_str());
        int flags = 0;

        nl_msg *msg = nlmsg_alloc();
        if (!msg) {
            syslog(LOG_ERR, "failed to allockate netlink message");
            exit_with_error();
        }

        void *user_hdr;
        user_hdr = genlmsg_put(msg, 0, 0, eid, 0, flags, cmd, 0);
        if (!user_hdr) {
            syslog(LOG_ERR, "failed to put netlink message");
            exit_with_error();
        }

        void *locals[] = { &wifaces, &physMonMap, &phys, &all_ifaces };

        NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ifidx);
        nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM, [](struct nl_msg *msg, void *arg) -> int {
            auto *pWifaces = (iface_mac_map *)((void**)arg)[0];
            auto pPhyMonMap = (std::map<std::string, std::string> *)((void**)arg)[1];
            auto pPhys = (std::set<std::string> *)((void**)arg)[2];
            auto pAllIfaces = (iface_mac_map *)((void**)arg)[3];

            nlmsghdr *ret_hdr = nlmsg_hdr(msg);
            nlattr *tb_msg[NL80211_ATTR_MAX + 1];

            genlmsghdr *gnlh = (genlmsghdr *)nlmsg_data(ret_hdr);

            nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
                      genlmsg_attrlen(gnlh, 0), NULL);

            if (!tb_msg[NL80211_ATTR_IFTYPE] || !tb_msg[NL80211_ATTR_IFNAME]) {
                return NL_STOP;
            }

            std::string ifname = nla_get_string(tb_msg[NL80211_ATTR_IFNAME]);
            char cphyname[IFNAMSIZ], buf[255];
            cphyname[0] = 0;

            snprintf(buf, sizeof(buf) - 1, "/sys/class/net/%s/phy80211/name", ifname.c_str());
            int f = open(buf, O_RDONLY);
            if (f < 0) {
                syslog(LOG_ERR, "Could not open file %s: %s", buf, strerror(errno));
                return NL_STOP;
            }

            int rv = read(f, cphyname, sizeof(cphyname) - 1);
            if (rv < 0) {
                syslog(LOG_ERR, "Could not read file %s: %s", buf, strerror(errno));
                return NL_STOP;
            }
            close(f);
            std::string phyname(cphyname);

            int type = nla_get_u32(tb_msg[NL80211_ATTR_IFTYPE]);

            pPhys->insert(phyname);
            if (type == NL80211_IFTYPE_MONITOR)
                (*pPhyMonMap)[phyname] = ifname;

            uint64_t mac = (*pAllIfaces)[ifname];
            (*pWifaces)[ifname] = mac;

            return NL_OK;
        }, locals);

        err = nl_send_auto_complete(sk, msg);
        if (err < 0) {
            syslog(LOG_ERR, "failed to send netlink message: %s", nl_geterror(err));
//            nlmsg_free(msg);
//            continue;
            exit_with_error();
        }

        err = nl_recvmsgs_default(sk);
        if (err < 0) {
            if (err == -NLE_NODEV)
                continue;
            syslog(LOG_ERR, "failed to recv netlink message: %s", nl_geterror(err));
            continue;
//            exit_with_error();
        }

        continue;
nla_put_failure:
            syslog(LOG_ERR, "failed to put nla message");
            exit_with_error();
    }

    for (auto &ifmap : wifaces) {
        uint64_t he_mac;
#ifdef __LITTLE_ENDIAN
        he_mac = be64toh(ifmap.second) >> 16;
#else
#endif
        syslog(LOG_DEBUG, "detected wifi interface %s with MAC %012" PRIX64, ifmap.first.c_str(), he_mac);
    }
    for (auto &phy : phys) {
        syslog(LOG_DEBUG, "detected wiphy %s", phy.c_str());
    }
    for (auto &physmon : physMonMap) {
        syslog(LOG_DEBUG, "%s has monitor interface %s", physmon.first.c_str(), physmon.second.c_str());
    }



}

bool initialize_wifaces()
{
    iface_mac_map ifs = get_all_ifaces();
    setup_wifaces(ifs);
    return true;
}
