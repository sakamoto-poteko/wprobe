/* Copyright (C) - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 *
 * Proprietary and confidential
 *
 * Written by Afa Cheng <afa@afa.moe>, 2016
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <ctime>

#include <string>
#include <sstream>
#include <fstream>

#include <pthread.h>
#include <syslog.h>

#include <unistd.h>
#include <sys/timerfd.h>

#include <sqlite3.h>
#include <curl/curl.h>

#include "wprobed_global.h"
#include "configuration.h"
#include "service.h"
#include "MacEventUploader.h"
#include "rapidjson/include/rapidjson/document.h"

static bool provision_device()
{
    CURL *curlProvisionRequest;
    curlProvisionRequest = curl_easy_init();
    if (!curlProvisionRequest) {
        syslog(LOG_ERR, "failed to create provision request");
        return false;
    }

    std::stringstream provisionss;
    provisionss << __global.upstreamBaseUrl << "/api/devman/provisioning/" << __global.SN;
    std::string endpoint = provisionss.str();

    std::string response;
    curl_easy_setopt(curlProvisionRequest, CURLOPT_URL, endpoint.c_str());
    curl_easy_setopt(curlProvisionRequest, CURLOPT_WRITEFUNCTION, static_cast<size_t (*)(char *, size_t, size_t, void *)>(
                         [](char *c, size_t s, size_t n, void *str) -> size_t {
        std::string *resp = (std::string *)str;
        ssize_t oldlen = resp->size();
        ssize_t len = s * n;

        resp->resize(oldlen + len);
        std::copy(c, c + len, resp->begin() + oldlen);

        return len;
    }));
    curl_easy_setopt(curlProvisionRequest, CURLOPT_WRITEDATA, &response);
    CURLcode provisionReqRes = curl_easy_perform(curlProvisionRequest);
    if (provisionReqRes != CURLE_OK) {
        syslog(LOG_ERR, "failed to request provisioning: %s", curl_easy_strerror(provisionReqRes));
        curl_easy_cleanup(curlProvisionRequest);
        return false;
    }
    curl_easy_cleanup(curlProvisionRequest);

    // Token, WirelessConfig, WProbedConfig
    rapidjson::Document doc;
    doc.Parse(response.c_str());

    if (!doc.IsObject())
        return false;

    if (doc.HasMember("wirelessConfig") && doc["wirelessConfig"].IsString()) {
        const char *wcf_path = "/etc/config/wireless";
        FILE *wcf = fopen(wcf_path, "w+");
        if (!wcf) {
            syslog(LOG_ERR, "failed to open %s: %s", wcf_path, std::strerror(errno));
            return false;
        }
        std::string wirelessConfig = doc["wirelessConfig"].GetString();
        fwrite(wirelessConfig.c_str(), 1, wirelessConfig.size(), wcf);
        fclose(wcf);
    }

    if (doc.HasMember("wprobedConfig") && doc["wprobedConfig"].IsString()) {
        const char *wpc_path = "/etc/wprobed/wprobed.cfg";
        FILE *wpc = fopen(wpc_path, "w+");
        if (!wpc) {
            syslog(LOG_ERR, "failed to open %s: %s", wpc_path, std::strerror(errno));
            return false;
        }
        std::string wprobedConfig = doc["wprobedConfig"].GetString();
        fwrite(wprobedConfig.c_str(), 1, wprobedConfig.size(), wpc);
        fclose(wpc);
    }


    if (doc.HasMember("token") && doc["token"].IsString()) {
        __global.token = doc["token"].GetString();
        __global.provisioned = true;
    } else {
        return false;
    }

    if (!write_device_token(__global.token))
        return false;


    std::stringstream provisionackss;
    provisionackss << __global.upstreamBaseUrl << "/api/devman/provisioning/" << __global.SN;
    endpoint = provisionackss.str();

    CURL *curlProvisionAckRequest = curl_easy_init();
    if (!curlProvisionAckRequest) {
        syslog(LOG_ERR, "failed to create provision ack request");
        return false;
    }
    curl_easy_setopt(curlProvisionAckRequest, CURLOPT_URL, endpoint.c_str());
    curl_easy_setopt(curlProvisionAckRequest, CURLOPT_WRITEFUNCTION,
                     static_cast<size_t (*)(char *, size_t, size_t, void *)>(
                         [](char *, size_t s, size_t n, void *) -> size_t {
        return s * n;
    }));
    curl_easy_setopt(curlProvisionAckRequest, CURLOPT_PUT, 1L);
    CURLcode provisionAckReq = curl_easy_perform(curlProvisionAckRequest);
    if (provisionAckReq != CURLE_OK) {
        syslog(LOG_ERR, "failed to ack provisioning: %s", curl_easy_strerror(provisionAckReq));
        return false;
    }

    return true;
}

static void *upstream_work_thread(void *)
{
    syslog(LOG_INFO, "upstream thread running");

    MacEventUploader macEventUploader(__global.db, __global.upstreamBaseUrl, "");

    int fd; // timerfd
    fd = timerfd_create(CLOCK_REALTIME, 0);
    if (fd == -1) {
        syslog(LOG_ERR, "failed to create upstream timer");
        exit_with_error();
    }

    itimerspec timerspec;
    timerspec.it_interval.tv_sec    = __global.upstreamInterval;
    timerspec.it_interval.tv_nsec   = 0;
    timerspec.it_value.tv_sec       = 0;
    timerspec.it_value.tv_nsec      = 1;    // fire immediately after started

    if (timerfd_settime(fd, 0, &timerspec, NULL) == -1) {
        syslog(LOG_ERR, "failed to set upstream timer");
        exit_with_error();
    }

    uint64_t buf;
    int result;
    do {
        result = read(fd, &buf, sizeof(buf));
        if (result == -1) { // failed
            if (errno == EINTR && __global.stop)    // stop got
                break;
            else {
                syslog(LOG_ERR, "upstream timer interrupted");
                exit_with_error();
            }
        }

        // timer fired
        if (!__global.provisioned) {
            provision_device();
        } else {
            macEventUploader.upload();
        }
    } while (true);

    pthread_exit(0);
    return 0;
}


void start_upstream()
{
    pthread_t thread;
    int result;
    result = pthread_create(&thread, NULL, upstream_work_thread, NULL);

    if (result) {
        syslog(LOG_ERR, "failed to create worker thread for upstream");
        exit_with_error();
    }
    syslog(LOG_INFO, "upstream thread created");
}
