/* Copyright (C) - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 *
 * Proprietary and confidential
 *
 * Written by Afa Cheng <afa@afa.moe>, 2016
 */

#ifndef WPROBED_GLOBAL_H
#define WPROBED_GLOBAL_H

struct sqlite;

typedef struct WProbedArgs_t {
    bool frontend;
    bool debug;
    char *configfile;
    char *dbpath;
} WProbedArgs;

typedef struct WProbedGlobal_t {
    struct sqlite *db;
} WProbedGlobal;

extern WProbedGlobal __global;

#endif // WPROBED_GLOBAL_H
