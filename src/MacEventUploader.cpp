/* Copyright (C) - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 *
 * Proprietary and confidential
 *
 * Written by Afa Cheng <afa@afa.moe>, 2016
 */

#include <sstream>

#include <syslog.h>

#include "wprobed_global.h"
#include "MacEventUploader.h"


MacEventUploader::MacEventUploader(sqlite3 *db, const std::string &baseUrl, const std::string &devid) :
    _db(db), _baseUrl(baseUrl), _devid(devid), _headers(NULL)
{
    int result;
    result = sqlite3_prepare_v2(_db, u8"SELECT * FROM `MAC_EVENT`", -1, &_selectStmt, NULL);
    if (result != SQLITE_OK || _selectStmt == NULL) {
        syslog(LOG_ERR, "database error: %s", sqlite3_errstr(result));
        exit_with_error();
    }

    _curlUploadRequest = curl_easy_init();
    if (!_curlUploadRequest) {
        syslog(LOG_ERR, "curl init error");
        exit_with_error();
    }

    _endpoint = baseUrl + "/api/DeviceEvents/MacProbe/" + devid;
//    _endpoint = baseUrl;

    _headers = curl_slist_append(_headers, "Content-Type: application/json");
}

MacEventUploader::~MacEventUploader()
{
    curl_slist_free_all(_headers);
    curl_easy_cleanup(_curlUploadRequest);
}

std::vector<MacEventUploader::ProbeRequestRecord> MacEventUploader::readMacEventsFromDb()
{
    std::vector<ProbeRequestRecord> probes;
    do {
        int result = sqlite3_step(_selectStmt);

        if (result == SQLITE_DONE)
            break;

        if (result == SQLITE_ROW) {
            ProbeRequestRecord r;
            r.id = sqlite3_column_int64(_selectStmt, 0);
            r.MAC = (uint64_t)sqlite3_column_int64(_selectStmt, 1);
            r.iface = (char *)sqlite3_column_text(_selectStmt, 2);
            r.timestamp = sqlite3_column_int64(_selectStmt, 3);
            probes.push_back(r);
            continue;
        } else {
            syslog(LOG_ERR, "database error: %s", sqlite3_errstr(result));
            break;
        }
    } while (true);

    sqlite3_reset(_selectStmt);
    return probes;
}

void MacEventUploader::cleanUpDbRecrds(const std::vector<MacEventUploader::ProbeRequestRecord> &records)
{
    if (records.empty())
        return;

    std::stringstream ss;
    ss << u8"DELETE FROM `MAC_EVENT` WHERE `ID` IN (";

    for (std::size_t i = 0; i < records.size(); ++i) {
        if (i != 0)
            ss << u8", ";
        ss << records.at(i).id;
    }
    ss << u8")";

    char *errstr = 0;
    std::string sqlstr = ss.str();
    const char *deletesql = sqlstr.c_str();

    syslog(LOG_DEBUG, "deleting records: `%s'", deletesql);

    int result = sqlite3_exec(_db, deletesql, NULL, NULL, &errstr);

    if (result != SQLITE_OK) {
        if (errstr) {
            syslog(LOG_ERR, "failed to delete uploaded records: %s, %s", sqlite3_errstr(result), errstr);
            sqlite3_free(errstr);
        } else {
            syslog(LOG_ERR, "failed to delete uploaded records: %s", sqlite3_errstr(result));
        }
    }
}

void MacEventUploader::upload()
{
    std::vector<ProbeRequestRecord> records = readMacEventsFromDb();
    if (records.empty())
        return;

    std::string json = macEventsToJson(records);


    curl_easy_setopt(_curlUploadRequest, CURLOPT_URL, _endpoint.c_str());
    curl_easy_setopt(_curlUploadRequest, CURLOPT_POST, 1l);
    curl_easy_setopt(_curlUploadRequest, CURLOPT_POSTFIELDS, json.c_str());
    curl_easy_setopt(_curlUploadRequest, CURLOPT_POSTFIELDSIZE, json.size());
    curl_easy_setopt(_curlUploadRequest, CURLOPT_HTTPHEADER, _headers);
    curl_easy_setopt(_curlUploadRequest, CURLOPT_WRITEFUNCTION,
                     static_cast<size_t (*)(char *, size_t, size_t, void *)>(
                         [](char *, size_t s, size_t n, void *) -> size_t {
        return s * n;
    }));
    // This static_cast is necessary.
    // The compiler miscasted the lambda to a I-don't-know type causing seg fault

    CURLcode res = curl_easy_perform(_curlUploadRequest);
    if (res != CURLE_OK) {
        syslog(LOG_ERR, "failed to upload data: %s", curl_easy_strerror(res));
        return;
    }

    syslog(LOG_INFO, "mac events data uploaded");
    syslog(LOG_DEBUG, "mac events data of %u bytes: %s",
            (unsigned int)json.size(), json.c_str());

    cleanUpDbRecrds(records);
}

std::string MacEventUploader::macEventsToJson(const std::vector<MacEventUploader::ProbeRequestRecord> &records)
{
    // Minimize string
    std::stringstream ss;
    ss << "[";

    for (std::size_t i = 0; i < records.size(); ++i) {
        ProbeRequestRecord r = records.at(i);

        if (i != 0)
            ss << ",";

        ss << "{"
              "\"mac\":" << r.MAC << ","
              "\"iface\":\"" << r.iface << "\","
              "\"timestamp\":" << r.timestamp << ""
              "}";
    }

    ss << "]";

    return ss.str();
}
