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

#include "github_update_prompt.h"

#include <multipass/optional.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/version.h>

#include "new_release_monitor.h"

namespace mp = multipass;

namespace
{
constexpr auto new_release_check_frequency = std::chrono::hours(24);
constexpr auto notify_user_frequency = std::chrono::hours(6);
} // namespace

mp::GithubUpdatePrompt::GithubUpdatePrompt()
    : monitor{std::make_unique<NewReleaseMonitor>(mp::version_string, ::new_release_check_frequency)},
      last_shown{std::chrono::system_clock::now() - notify_user_frequency} // so we show update message soon after start
{
}

mp::GithubUpdatePrompt::~GithubUpdatePrompt() = default;

bool mp::GithubUpdatePrompt::is_time_to_show()
{
    return monitor->get_new_release() && last_shown + ::notify_user_frequency < std::chrono::system_clock::now();
}

void mp::GithubUpdatePrompt::populate(mp::UpdateInfo* update_info)
{
    auto new_release = monitor->get_new_release();
    if (new_release)
    {
        update_info->set_version(new_release->version.toStdString());
        update_info->set_url(new_release->url.toEncoded());
        last_shown = std::chrono::system_clock::now();
    }
}

void mp::GithubUpdatePrompt::populate_if_time_to_show(mp::UpdateInfo* update_info)
{
    if (is_time_to_show())
        populate(update_info);
}
