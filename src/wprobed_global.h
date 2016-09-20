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

#if defined(__mips__)
#define WPROBED_ARCH "mips"
#elif defined(__amd64__)
#define WPROBED_ARCH "amd64"
#else
#define WPROBED_ARCH "unknown"
#warning "This is an untested architecture!"
#endif

#define WPROBED_INTRO_STRING "wprobed " WPROBED_VERSION " for " WPROBED_ARCH " compiled on " __DATE__ " " __TIME__

void exit_with_error();

struct sqlite;

typedef struct WProbedArgs_t {
    bool frontend;
    bool debug;
    std::string configfile;
    std::string dbpath;
} WProbedArgs;

typedef struct WProbedGlobal_t {
    struct sqlite3 *db;
    bool stop;  // the program should stop and exits
    std::vector<pthread_t> threads;
    std::string configpath;

    // Configs
    std::uint64_t   SN;
    bool            provisioned;
    std::string     token;
    std::vector<std::string> ifaces;    // Wireless ifaces
    std::uint32_t   macRetiringTime;
    std::uint32_t   macCleanUpInterval;
    std::uint32_t   upstreamInterval;
    std::string     upstreamBaseUrl;
} WProbedGlobal;

extern WProbedGlobal __global;

#endif // WPROBED_GLOBAL_H
