/*
 * Copyright (C) 2019 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef MULTIPASS_PLATFORM_PROPRIETARY_H
#define MULTIPASS_PLATFORM_PROPRIETARY_H

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace multipass
{
namespace platform
{
constexpr auto unlock_code{"convenient-ichthyosaur"};
// clang-format off
const std::unordered_set<std::string> supported_release_aliases{"core",   "core16", "core18",
                                                                "default", "ubuntu", "lts",
                                                                "16.04", "x", "xenial",
                                                                "18.04", "b", "bionic",
                                                                "20.04", "f", "focal",
                                                                "21.04", "h", "hirsute",
                                                                "21.10", "i", "impish"};
// clang-format on
const std::unordered_set<std::string> supported_core_aliases{"core", "core16", "core18"};
const std::unordered_set<std::string> supported_snapcraft_aliases{"core", "core16", "core18", "core20"};
const std::unordered_map<std::string, std::unordered_set<std::string>> supported_remotes_aliases_map{
    {"release", supported_release_aliases}, {"snapcraft", supported_snapcraft_aliases}, {"appliance", {}}};

inline bool check_unlock_code()
{
    return qgetenv("MULTIPASS_UNLOCK") == unlock_code;
}
} // namespace platform
} // namespace multipass
#endif // MULTIPASS_PLATFORM_PROPRIETARY_H
