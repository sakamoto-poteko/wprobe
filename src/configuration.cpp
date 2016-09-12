/* Copyright (C) - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 *
 * Proprietary and confidential
 *
 * Written by Afa Cheng <afa@afa.moe>, 2016
 */

#include <sstream>

#include <syslog.h>
#include <libconfig.h++>

#include "wprobed_global.h"

#include "configuration.h"

static void set_config_default()
{
    __global.macCleanUpInterval = 500;
    __global.macRetiringTime    = 60;
    __global.upstreamBaseUrl    = "http://api.wprobed.com";
    __global.upstreamInterval   = 60;
}

void parse_config(const char *configpath)
{
    set_config_default();

    libconfig::Config config;

    try {
        config.readFile(configpath);

        const libconfig::Setting &root = config.getRoot();

        if (root.exists("upstream")) {
            // Upstream section
            const libconfig::Setting &upstream = root["upstream"];

            std::uint32_t upstreamInterval;
            if (upstream.lookupValue("interval", upstreamInterval) && upstreamInterval)
                __global.upstreamInterval = upstreamInterval;

            std::string upstreamBaseUrl;
            if (upstream.lookupValue("baseurl", upstreamBaseUrl))
                __global.upstreamBaseUrl = upstreamBaseUrl;
        }

        if (root.exists("probing")) {
            // Probing section
            const libconfig::Setting &probing = root["probing"];

            std::uint32_t cleanupInterval;
            if (probing.lookupValue("cleanupInterval", cleanupInterval) && cleanupInterval)
                __global.macCleanUpInterval = cleanupInterval;

            std::uint32_t retiringTime;
            if (probing.lookupValue("retiringTime", retiringTime) && retiringTime)
                __global.macRetiringTime = retiringTime;

            if (probing.exists("ifaces")) {
                const libconfig::Setting &probingIfaces = probing["ifaces"];
                if (probingIfaces.isArray()) {
                    std::vector<std::string> ifaces;
                    for (int i = 0; i < probingIfaces.getLength(); ++i) {
                        try {
                            std::string iface(probingIfaces[i].c_str());
                            if (!iface.empty())
                                ifaces.push_back(iface);
                        } catch (libconfig::SettingTypeException) {}
                    }
                    __global.ifaces = ifaces;
                }
            }

        }


    } catch (libconfig::ParseException pe) {
        syslog(LOG_ERR, "failed to parse config %s: %s", configpath, pe.getError());
        return;
    } catch (libconfig::FileIOException) {
        syslog(LOG_ERR, "failed to load config %s", configpath);
        return;
    }
}

void log_config()
{
    syslog(LOG_DEBUG, "upstream: interval=%d, baseurl=`%s'",
           __global.upstreamInterval, __global.upstreamBaseUrl.c_str());
    std::stringstream ss;
    ss << "[ ";
    for (const std::string &s : __global.ifaces) {
        ss << s << " ";
    }
    ss << "]";
    std::string sss = ss.str();
    syslog(LOG_DEBUG, "probing: cleanupInterval=%d, retiringTime=%d, ifaces=%s",
           __global.macCleanUpInterval, __global.macRetiringTime, sss.c_str());
}
