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
#include <fcntl.h>
#include <ifaddrs.h>

#include <sqlite3.h>
#include <curl/curl.h>

#include "wprobed_global.h"
#include "service.h"
#include "configuration.h"

void exit_with_error()
{
    syslog(LOG_ERR, "error occured, exiting...");
    exit(EXIT_FAILURE);
}

static void print_usage()
{
    std::fprintf(stderr,
                 WPROBED_INTRO_STRING "\n"
                 "Usage: wprobed [options]\n"
                 "\n"
                 "    -h        This help text\n"
                 "    -v        Show verbose version info\n"
                 "    -d db     Use database on path db\n"
                 "    -c cfg    Use config file on path cfg\n"
                 "    -D        Debug mode\n"
                 "    -f        Run in front end\n"
                 );
}

static void print_version()
{
    std::fprintf(stderr,
                 WPROBED_INTRO_STRING "\n"
                 "Copyright (c) " WPROBED_COPYRIGHT "\n"
                 "Wireless network user tracking daemon\n\n"
                 "This tool is linked against:\n"
                 "libsqlite3: %s\n"
                 "libcurl: %s\n"
                 ,
                 sqlite3_libversion(),
                 curl_version());
}

static void init_arguments(WProbedArgs *args)
{
    args->configfile.assign("/etc/wprobed/wprobed.cfg");
    args->dbpath.assign("/etc/wprobed/wprobed.sqlite3");
    args->debug = false;
    args->frontend = false;
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
                args->configfile.assign(optarg);
                break;
            case 'd':
                args->dbpath.assign(optarg);
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
           args->dbpath.c_str(),
           args->configfile.c_str());
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
        syslog(LOG_ERR, "failed to initialize db: %s, %s", sqlite3_errstr(result), errstr);
        sqlite3_free(errstr);
        exit_with_error();
    }
}

int main(int argc, char **argv)
{
    int result = 0;
    WProbedArgs args;

    // syslog
    setlogmask(LOG_UPTO(LOG_NOTICE));
    openlog("wprobed",  LOG_CONS | LOG_PID | LOG_NDELAY | LOG_PERROR, LOG_DAEMON);
    syslog(LOG_INFO, "starting ...");

    init_arguments(&args);
    parse_arguments(argc, argv, &args);
    if (args.debug)
        setlogmask(LOG_UPTO(LOG_DEBUG));
    log_arguments(&args);

    // Check if there's already an instance running
    int pid_file = open("/var/run/wprobed.pid", O_CREAT | O_RDWR, 0666);
    if (lockf(pid_file, F_TLOCK, 0) == -1) {
        if (errno == EAGAIN) {
            syslog(LOG_ERR, "A wprobed instance is already running");
            exit_with_error();
        }
    }

    parse_config(args.configfile.c_str());
    log_config();

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
    const char *dbpath = args.dbpath.c_str();
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
    curl_global_init(CURL_GLOBAL_ALL);

    start_probing();
    start_upstream();

    for (pthread_t thread : __global.threads) {
        pthread_join(thread, NULL);
    }

    // should never reache here
    syslog(LOG_INFO, "wprobed stopped");

    return 0;
}


WProbedGlobal __global;
