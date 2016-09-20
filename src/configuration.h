/* Copyright (C) Lingdong Tech - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 *
 * Proprietary and confidential
 *
 * Written by Afa Cheng <afa@afa.moe>, 2016
 */

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <cstdint>

void parse_config(const char *configpath);
void log_config();
bool write_device_token(std::string token);

std::uint64_t get_sn();

#endif // CONFIGURATION_H
