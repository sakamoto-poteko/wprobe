/* Copyright (C) - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 *
 * Proprietary and confidential
 *
 * Written by Afa Cheng <afa@afa.moe>, 2016
 */

#include <vector>

#include "MacTimestampMap.h"

MacTimestampMap::MacTimestampMap(uint32_t retiringTime) :
    _retiringTime(retiringTime)
{
}

void MacTimestampMap::cleanup()
{
    if (!_retiringTime)
        return;

    std::vector<std::uint64_t> toBeRemoved;

    std::time_t now;
    std::time(&now);

    for (auto it = begin(); it != end(); ++it) {
        std::time_t timestamp = it->second;

        double diff = std::difftime(now, timestamp);
        if (diff > _retiringTime)
            toBeRemoved.push_back(it->first);
    }

    for (auto key : toBeRemoved) {
        erase(key);
    }
}




