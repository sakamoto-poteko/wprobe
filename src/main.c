#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <signal.h>
#include <unistd.h>
#include <syslog.h>

#include <sqlite3.h>

#include "wprobed_global.h"

int parse_arguments(int argc, char **argv, WProbedArgs *args)
{
    int c;
    opterr = 0;
    while ((c = getopt (argc, argv, "fDc:d:")) != -1)
        switch (c)
        {
            case 'f':
                args->frontend = true;
                break;
            case 'D':
                args->debug = true;
            case 'c':
                args->configfile = strdup(optarg);
                break;
            case 'd':
                args->dbpath = strdup(optarg);
                break;
            case '?':
                if (optopt == 'c' || optopt == 'd')
                    syslog(LOG_EMERG, "Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    syslog(LOG_EMERG, "Unknown option `-%c'.\n", optopt);
                else
                    syslog(LOG_EMERG, "Unknown option character `\\x%x'.\n", optopt);
                // abort(); goes next
            default:
                abort();
        }
    return 0;
}

void sigint_got(int sig)
{
    (void)sig;
    syslog(LOG_INFO, "SIGINT catched, exiting...");

    sqlite3_close(__global.db); // ... with a NULL pointer argument is a harmless no-op.
    exit(0);
}

int main(int argc, char **argv)
{
    int result = 0;
    WProbedArgs args = {0};

    // syslog
    setlogmask(LOG_UPTO(LOG_NOTICE));
    openlog("wprobed",  LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);

    parse_arguments(argc, argv, &args);

    sigset(SIGINT, sigint_got);

    if (args.debug)
        setlogmask(LOG_UPTO(LOG_DEBUG));

    // Daemonization
    if (!args.frontend) {
        if (daemon(0, 0) != 0) {    // Daemonization failed
            syslog(LOG_EMERG, "Damonization failed: %s", strerror(errno));
            abort();
        }
    }

    // Open DB
    const char *dbpath = args.dbpath ? args.dbpath : "/var/wprobed/db.sqlite3";
    result = sqlite3_open_v2(dbpath, &__global.db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                             NULL);
    if (result != SQLITE_OK) {
        syslog(LOG_EMERG, "Failed to open database in %s", dbpath);
        abort();
    }
}


WProbedGlobal __global;
