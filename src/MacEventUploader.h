/* Copyright (C) - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 *
 * Proprietary and confidential
 *
 * Written by Afa Cheng <afa@afa.moe>, 2016
 */

#ifndef MACEVENTUPLOADER_H
#define MACEVENTUPLOADER_H

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

#include <sqlite3.h>
#include <curl/curl.h>

class MacEventUploader
{
public:
    MacEventUploader(sqlite3 *db, const std::string &baseUrl, const std::string &devid);
    ~MacEventUploader();
    void upload();

    struct ProbeRequestRecord
    {
        std::int64_t id;
        std::uint64_t MAC;
        std::time_t timestamp;
        std::string iface;
    };

private:
    sqlite3 *_db;
    sqlite3_stmt *_selectStmt;
    std::string _baseUrl;
    std::string _devid;
    std::string _endpoint;
    curl_slist  *_headers;

    CURL    *_curlUploadRequest;

    std::string macEventsToJson(const std::vector<ProbeRequestRecord> &records);
    std::vector<ProbeRequestRecord> readMacEventsFromDb();
    void cleanUpDbRecrds(const std::vector<ProbeRequestRecord> &records);
};

#endif // MACEVENTUPLOADER_H
