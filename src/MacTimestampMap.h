/* Copyright (C) - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 *
 * Proprietary and confidential
 *
 * Written by Afa Cheng <afa@afa.moe>, 2016
 */

#ifndef MACTIMESTAMPMAP_H
#define MACTIMESTAMPMAP_H

#include <ctime>
#include <unordered_map>

class MacTimestampMap : public std::unordered_map<std::uint64_t, std::time_t>
{
public:
    MacTimestampMap(uint32_t retiringTime = 0);

    uint32_t retiringTime() const
    {
        return _retiringTime;
    }
    void setRetiringTime(const uint32_t &retiringTime)
    {
        _retiringTime = retiringTime;
    }

    void cleanup();

private:
    uint32_t _retiringTime;
};

#endif // MACTIMESTAMPMAP_H
