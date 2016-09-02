/* Copyright (C) Afa.L Cheng - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 *
 * Proprietary and confidential
 *
 * Written by Afa Cheng <afa@afa.moe>, 2016
 */

#ifndef WPROBED_GLOBAL_H
#define WPROBED_GLOBAL_H

#include <cstdint>
#include <vector>
#include <string>
#include <pthread.h>

#define WPROBED_VERSION "0.1.0"
#define WPROBED_COPYRIGHT "Afa Cheng <afa@heckphi.com>"

void exit_with_error();

struct sqlite;

typedef struct WProbedArgs_t {
    bool frontend;
    bool debug;
    char *configfile;
    char *dbpath;
} WProbedArgs;

typedef struct WProbedGlobal_t {
    struct sqlite3 *db;
    std::vector<pthread_t> threads;
    std::vector<std::string> ifaces;    // Wireless ifaces
    bool stop;  // the program should stop and exits

    // Configs
    std::uint32_t macRetiringTime;
    std::uint32_t macCleanUpInterval;
    std::uint32_t upstreamInterval;
    const char *upstreamBaseUrl;
} WProbedGlobal;

extern WProbedGlobal __global;

#endif // WPROBED_GLOBAL_H
