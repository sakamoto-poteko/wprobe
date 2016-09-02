/* Copyright (C) - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 *
 * Proprietary and confidential
 *
 * Written by Afa Cheng <afa@afa.moe>, 2016
 */

#include <cstdlib>
#include <cstdint>
#include <cerrno>
#include <cstring>
#include <cinttypes>
#include <ctime>

#include <vector>

#include <pcap.h>
#include <syslog.h>
#include <pthread.h>
#include <endian.h>

#include <sqlite3.h>

#include "wprobed_global.h"
#include "service.h"
#include "MacTimestampMap.h"

struct ieee80211_radiotap_header {
        u_int8_t        we_dont_care_1;
        u_int8_t        we_dont_care_2;
        u_int16_t       len;    // All we care
        u_int32_t       we_dont_care3;
} __attribute__((__packed__));



struct pcap_loop_handler_arg {
    pcap_t *pcap;
    char *iface;
    sqlite3_stmt *insertstmt;

    uint32_t    packetCount;
    MacTimestampMap macTimestampMap;
};

static void pcap_loop_handler(u_char *user, const struct pcap_pkthdr *header, const u_char *packet)
{
    pcap_loop_handler_arg *arg = (pcap_loop_handler_arg *)user;
    const char *dberr = "";
    (void)header;

    if (__global.stop)
        pcap_breakloop(arg->pcap);

    // Timestamp map cleanup
    if (arg->packetCount++ % __global.macCleanUpInterval == 0)
        arg->macTimestampMap.cleanup();

    ieee80211_radiotap_header *hdr = (ieee80211_radiotap_header *)packet;
    int len = hdr->len;

    uint64_t addr2; // MAC is addr2 in probe frame

#ifdef __LITTLE_ENDIAN
    addr2 = *(uint64_t *)(packet + len + 10) & 0x00FFFFFFFFFFFFUL;
    uint64_t be_mac = htobe64(addr2) >> 16;

#else
#error "Big-endian not supported yet"
#endif

    std::time_t now;
    std::time(&now);
    std::time_t lastSeen = arg->macTimestampMap[addr2];
    arg->macTimestampMap[addr2] = now;
    double diff = std::difftime(now, lastSeen);

    if (diff < __global.macRetiringTime) {
        syslog(LOG_DEBUG, "probe request from %012" PRIX64 " on iface %s, last seen recently, not recording",
               be_mac, arg->iface);
        return;
    }

    syslog(LOG_DEBUG, "probe request from %012" PRIX64 " on iface %s", be_mac, arg->iface);

    // (`MAC`, `IFACE`, `TIMESTAMP`)
    int result = 0;
    sqlite3_stmt *insertstmt = arg->insertstmt;

    result = sqlite3_bind_int64(insertstmt, 1, (int64_t)addr2);
    if (result != SQLITE_OK) {
        dberr = "bind MAC";
        goto sqlite_err;
    }

    result = sqlite3_bind_text(insertstmt, 2, arg->iface, -1, SQLITE_STATIC);
    if (result != SQLITE_OK) {
        dberr = "bind IFACE";
        goto sqlite_err;
    }

    std::time_t timestamp;
    std::time(&timestamp);
    result = sqlite3_bind_int64(insertstmt, 3, timestamp);
    if (result != SQLITE_OK) {
        dberr = "bind TIMESTAMP";
        goto sqlite_err;
    }

    result = sqlite3_step(insertstmt);
    if (result != SQLITE_DONE) {
        dberr = "step";
        result = sqlite3_reset(insertstmt);
        goto sqlite_err;
    }

    result = sqlite3_reset(insertstmt);
    if (result != SQLITE_OK) {
        dberr = "reset";
        goto sqlite_err;
    }

    return;

sqlite_err:
    syslog(LOG_ERR, "db error %s: %s", dberr, sqlite3_errstr(result));    // Can continue
}

static void *pcap_loop_thread(void *inarg)
{
    pcap_loop_handler_arg *arg = (pcap_loop_handler_arg *)inarg;

    syslog(LOG_INFO, "probing thread for `%s' running", arg->iface);

    int ret = pcap_loop(arg->pcap, -1, pcap_loop_handler, (u_char *)inarg);
    if (ret != -2) {    // An error occured. Not returned due to break call;
        syslog(LOG_ERR, "an error occured in packet capturing: %s", pcap_geterr(arg->pcap));
        exit_with_error();
    } else {
        syslog(LOG_ERR, "stop got. Exiting probing thread...");
        pthread_exit(0);
    }

    // Never reaches here
    return 0;
}

void start_probing()
{
    int ifaces_len = 1;
    char *ifaces[] = {"wlan0"};

    bool atLeastOneCreated = false;

    for (int i = 0; i < ifaces_len; ++i) {
        int result;

        // PCAP for probing
        char *errbuf = new char[PCAP_ERRBUF_SIZE];
        if (!errbuf) {
            syslog(LOG_ERR, "cannot allocate pcap error buf");
            exit_with_error();
        }


        // 256 is enough for wifi capturing
        pcap_t* pcap = pcap_open_live(ifaces[i], 256, 1, 0, errbuf);

        if (!pcap) {
            syslog(LOG_ERR, "cannot open device `%s' for capturing: %s", ifaces[i], errbuf);
            exit_with_error();
        }

        bpf_program bpf;
        if (pcap_compile(pcap, &bpf, "type mgt subtype probe-req", 0, PCAP_NETMASK_UNKNOWN) == -1) {
            syslog(LOG_ERR, "BPF compilation failed: %s", pcap_geterr(pcap));
            exit_with_error();
        }

        if (pcap_setfilter(pcap, &bpf) == -1) {
            syslog(LOG_ERR, "BPF filter set failed on device `%s': %s", ifaces[i], pcap_geterr(pcap));
            exit_with_error();
        }

        pcap_freecode(&bpf);


        // DB for probing
        const char *sql = u8"INSERT INTO `MAC_EVENT` (`MAC`, `IFACE`, `TIMESTAMP`) VALUES (?, ?, ?)";
        sqlite3_stmt *sqlstmt = 0;
        result = sqlite3_prepare_v2(__global.db, sql, std::strlen(sql), &sqlstmt, NULL);
        if (result != SQLITE_OK || sqlstmt == NULL) {
            syslog(LOG_ERR, "database error: %s", sqlite3_errstr(result));
            exit_with_error();
        }

        pcap_loop_handler_arg *arg = new pcap_loop_handler_arg;
        arg->pcap = pcap; arg->iface = ifaces[i]; arg->insertstmt = sqlstmt;
        arg->packetCount = 0;

        pthread_t thread;
        result = pthread_create(&thread, NULL, pcap_loop_thread, arg);
        if (result) {
            syslog(LOG_ERR, "failed to create worker thread: %s", std::strerror(errno));
            continue;   // Proceed to next interface
        }
        __global.threads.push_back(thread);
        atLeastOneCreated = true;
    }
    if (!atLeastOneCreated) {
        syslog(LOG_ERR, "no probing threads created");
        exit_with_error();
    }
    syslog(LOG_INFO, "finished creating all probing threads");
}
