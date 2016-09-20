/* Copyright (C) - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 *
 * Proprietary and confidential
 *
 * Written by Afa Cheng <afa@afa.moe>, 2016
 */

#include <cinttypes>
#include <sstream>

#include <syslog.h>
#include <libconfig.h>

#include "wprobed_global.h"

#include "configuration.h"

static void set_config_default()
{
    __global.provisioned        = false;
    __global.token              = "";
    __global.macCleanUpInterval = 500;
    __global.macRetiringTime    = 60;
    __global.upstreamBaseUrl    = "http://api.wprobed.com";
    __global.upstreamInterval   = 60;
    __global.SN                 = get_sn();
}

void parse_config(const char *configpath)
{
    set_config_default();
    __global.configpath = configpath;

    config_t config;
    config_init(&config);

    if (config_read_file(&config, configpath) != CONFIG_TRUE) {
        syslog(LOG_ERR, "failed to parse config %s: %s", configpath, config_error_text(&config));
        return;
    }

    int upstream_interval;
    if (config_lookup_int(&config, "upstream.interval", &upstream_interval) == CONFIG_TRUE) {
        __global.upstreamInterval = upstream_interval;
        syslog(LOG_DEBUG, "config: upstream.interval: %d", upstream_interval);
    }

    const char *upstream_baseurl;
    if (config_lookup_string(&config, "upstream.baseurl", &upstream_baseurl) == CONFIG_TRUE) {
        __global.upstreamBaseUrl = std::string(upstream_baseurl);
        syslog(LOG_DEBUG, "config: upstream.baseurl: %s", upstream_baseurl);
    }

    int probing_cleanupInterval;
    if (config_lookup_int(&config, "probing.cleanupInterval", &probing_cleanupInterval) == CONFIG_TRUE) {
        __global.macCleanUpInterval = probing_cleanupInterval;
        syslog(LOG_DEBUG, "config: probing.cleanupInterval: %d", probing_cleanupInterval);
    }

    int probing_retiringTime;
    if (config_lookup_int(&config, "probing.retiringTime", &probing_retiringTime) == CONFIG_TRUE) {
        __global.macRetiringTime = probing_retiringTime;
        syslog(LOG_DEBUG, "config: probing.retiringTime: %d", probing_retiringTime);
    }

    config_setting_t *probing_ifaces;
    probing_ifaces = config_lookup(&config, "probing.ifaces");
    if (probing_ifaces != NULL) {
        std::vector<std::string> ifaces;
        int sifaces = config_setting_length(probing_ifaces);
        for (int i = 0; i < sifaces; ++i) {
            const char *iface = config_setting_get_string_elem(probing_ifaces, i);
            if (iface == NULL) {
                syslog(LOG_DEBUG, "config: probing.ifaces[%d]: invalid", i);
                continue;
            }

            syslog(LOG_DEBUG, "config: probing.ifaces[%d]: %s", i, iface);
            ifaces.push_back(iface);
        }
        __global.ifaces.swap(ifaces);
    }

    const char *device_token;
    if (config_lookup_string(&config, "device.token", &device_token) == CONFIG_TRUE) {
        __global.token.assign(device_token);
        __global.provisioned = true;
    }

    config_destroy(&config);
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

    syslog(LOG_DEBUG, "device: S/N %" PRIu64 ", %s", __global.SN,
           __global.provisioned ? "provisioned" : "not provisioned yet");
}

uint64_t get_sn()
{
    return 10000;
}

bool write_device_token(std::string token)
{
    bool written = false;
    config_t config;
    config_init(&config);
    if (config_read_file(&config, __global.configpath.c_str()) != CONFIG_TRUE) {
        syslog(LOG_ERR, "failed to load config file for device token");
        return false;
    }


    config_setting_t *setting, *root;
    root = config_root_setting(&config);

    setting = config_setting_get_member(root, "device");
    if (!setting)
        setting = config_setting_add(root, "device", CONFIG_TYPE_GROUP);

    setting = config_setting_get_member(setting, "token");
    if (!setting)
        setting = config_setting_add(setting, "token", CONFIG_TYPE_GROUP);

    if (config_setting_set_string(setting, token.c_str()) == CONFIG_TRUE) {
        if (config_write_file(&config, __global.configpath.c_str())) {
            written = true;
        } else {
            syslog(LOG_ERR, "failed to write config file for device token");
        }
    } else {
        syslog(LOG_ERR, "failed to set device toke in config file");
    }

    config_destroy(&config);
    return written;
}
