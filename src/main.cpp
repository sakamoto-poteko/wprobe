/* Copyright (C) Afa.L Cheng - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 *
 * Proprietary and confidential
 *
 * Written by Afa Cheng <afa@afa.moe>, 2016
 */

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cctype>
#include <cstdlib>

#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <sqlite3.h>

#include "wprobed_global.h"
#include "service.h"

void exit_with_error()
{
    syslog(LOG_ERR, "error occured, exiting...");
    exit(255);
}

static void print_usage()
{
    // TODO: add
}

static void print_version()
{
    // TODO: add
}

static int parse_arguments(int argc, char **argv, WProbedArgs *args)
{
    int c;
    opterr = 0;
    while ((c = getopt(argc, argv, "hvfDc:d:")) != -1)
        switch (c) {
            case 'h':
                print_usage();
                exit(0);
                break;
            case 'v':
                print_version();
                exit(0);
                break;
            case 'f':
                args->frontend = true;
                break;
            case 'D':
                args->debug = true;
                break;
            case 'c':
                args->configfile = strdup(optarg);
                break;
            case 'd':
                args->dbpath = strdup(optarg);
                break;
            case '?':
                if (optopt == 'c' || optopt == 'd')
                    syslog(LOG_ERR, "Option -%c requires an argument.\n", optopt);
                else if (std::isprint(optopt))
                    syslog(LOG_ERR, "Unknown option `-%c'.\n", optopt);
                else
                    syslog(LOG_ERR, "Unknown option character `\\x%x'.\n", optopt);
                print_usage();
                // abort(); goes next
            default:
               exit_with_error();
        }
    return 0;
}

static void sigint_handler(int sig)
{
    (void)sig;
    syslog(LOG_INFO, "SIGINT caught, exiting...");

    sqlite3_close(__global.db); // ... with a NULL pointer argument is a harmless no-op.
    std::exit(0);
}

static void log_arguments(WProbedArgs *args)
{
    syslog(LOG_DEBUG, "%s%sdb path: %s, config: %s",
           args->frontend ? "front end, " : "",
           args->debug ? "debug, " : "",
           args->dbpath ? args->dbpath : "default",
           args->configfile ? args->configfile : "default");
}

static void initialize_db()
{
    const char *sql_create_table =
            u8"CREATE TABLE IF NOT EXISTS `MAC_EVENT` ("
            u8"`ID` INTEGER PRIMARY KEY AUTOINCREMENT,"
            u8"`MAC`    INTEGER,"
            u8"`IFACE`  VCHAR(16),"
            u8"`TIMESTAMP`  INTEGER)";
    char *errstr = 0;
    int result;

    result = sqlite3_exec(__global.db, sql_create_table, NULL, NULL, &errstr);
    if (result != SQLITE_OK) {
        syslog(LOG_ERR, "failed to initialize db: %s", sqlite3_errstr(result));
        exit_with_error();
    }
    if (errstr) {
        syslog(LOG_ERR, "failed to initialize db: %s", errstr);
        sqlite3_free(errstr);
        exit_with_error();
    }
}

static void parse_config(const char *configpath)
{
    (void)configpath;
    // Set defaults
    __global.macCleanUpInterval = 500;  // Cleanup every 5000 packets
    __global.macRetiringTime    = 60;   // 60s

    // TODO: parse config
}

int main(int argc, char **argv)
{
    int result = 0;
    WProbedArgs args = {0};

    // syslog
    setlogmask(LOG_UPTO(LOG_NOTICE));
    openlog("wprobed",  LOG_CONS | LOG_PID | LOG_NDELAY | LOG_PERROR, LOG_DAEMON);
    syslog(LOG_INFO, "starting ...");

    parse_arguments(argc, argv, &args);
    if (args.debug)
        setlogmask(LOG_UPTO(LOG_DEBUG));
    log_arguments(&args);

    parse_config(args.configfile);

    syslog(LOG_INFO, "starting wprobed, version " WPROBED_VERSION);
    signal(SIGINT, sigint_handler);

    // Daemonization
    if (!args.frontend) {
        if (daemon(0, 0) != 0) {    // Daemonization failed
            syslog(LOG_ERR, "daemonization failed: %s", std::strerror(errno));
            exit_with_error();
        } else {
            syslog(LOG_DEBUG, "daemonized");
        }
    }

    // Open DB
    const char *dbpath = args.dbpath ? args.dbpath : "/etc/wprobed/db.sqlite3";
    result = sqlite3_open_v2(dbpath, &__global.db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                             NULL);

    if (result != SQLITE_OK) {
        syslog(LOG_ERR, "failed to open database in %s: %s", dbpath, sqlite3_errstr(result));
        exit_with_error();
    } else {
        syslog(LOG_DEBUG, "database %s opened", dbpath);
    }

    // Initialize DB
    initialize_db();

    start_probing();

    for (pthread_t thread : __global.threads) {
        pthread_join(thread, NULL);
    }

    syslog(LOG_ERR, "a worker thread terminated");
    exit_with_error();

    // Should never exit from here.
    return 255;
}


WProbedGlobal __global;
