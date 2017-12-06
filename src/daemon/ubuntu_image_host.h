/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#ifndef MULTIPASS_UBUNTU_IMAGE_HOST_H
#define MULTIPASS_UBUNTU_IMAGE_HOST_H

#include <multipass/simple_streams_manifest.h>
#include <multipass/vm_image_host.h>

#include <QString>

#include <chrono>
#include <string>
#include <unordered_map>

namespace multipass
{
constexpr auto release_remote = "release";
constexpr auto daily_remote = "daily";

class URLDownloader;
class UbuntuVMImageHost final : public VMImageHost
{
public:
    UbuntuVMImageHost(std::unordered_map<std::string, std::string> remotes, URLDownloader* downloader,
                      std::chrono::seconds manifest_time_to_live);
    VMImageInfo info_for(const Query& query) override;
    std::vector<VMImageInfo> all_info_for(const Query& query) override;
    VMImageInfo info_for_full_hash(const std::string& full_hash) override;
    void for_each_entry_do(const Action& action) override;
    std::string get_default_remote() override;

private:
    void update_manifest();
    std::unique_ptr<SimpleStreamsManifest>& manifest_from(const std::string& remote);
    void match_alias(const QString& key, const VMImageInfo** info, const SimpleStreamsManifest& manifest);
    std::chrono::seconds manifest_time_to_live;
    std::chrono::steady_clock::time_point last_update;
    std::unordered_map<std::string, std::unique_ptr<SimpleStreamsManifest>> manifests;
    URLDownloader* const url_downloader;
    std::unordered_map<std::string, std::string> remotes;
    QString index_path;
};
}
#endif // MULTIPASS_UBUNTU_IMAGE_HOST_H
