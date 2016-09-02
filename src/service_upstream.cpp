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
#include <cerrno>
#include <ctime>

#include <string>
#include <sstream>

#include <pthread.h>
#include <syslog.h>

#include <unistd.h>
#include <sys/timerfd.h>

#include <sqlite3.h>
#include <curl/curl.h>

#include "wprobed_global.h"
#include "service.h"
#include "MacEventUploader.h"


static void *upstream_work_thread(void *)
{
    syslog(LOG_INFO, "upstream thread running");

    MacEventUploader macEventUploader(__global.db, __global.upstreamBaseUrl, "");
    // FIXME: fix devid arg

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
        macEventUploader.upload();
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
