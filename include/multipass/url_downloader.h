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
 * Authored by: Chris Townsend <christopher.townsend@canonical.com>
 *              Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#ifndef MULTIPASS_URL_DOWNLOADER_H
#define MULTIPASS_URL_DOWNLOADER_H

#include <multipass/path.h>
#include <multipass/progress_monitor.h>

#include <QByteArray>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>

#include <iostream>

class QUrl;
class QString;
namespace multipass
{
class URLDownloader
{
public:
    URLDownloader() = default;
    URLDownloader(const Path& cache_dir);
    virtual ~URLDownloader() = default;
    virtual void download_to(const QUrl& url, const QString& file_name, int64_t size, const int download_type,
                             const ProgressMonitor& monitor);
    virtual QByteArray download(const QUrl& url);
    virtual QDateTime last_modified(const QUrl& url);

private:
    URLDownloader(const URLDownloader&) = delete;
    URLDownloader& operator=(const URLDownloader&) = delete;

    QNetworkAccessManager manager;
    QNetworkDiskCache network_cache;
};
}
#endif // MULTIPASS_URL_DOWNLOADER_H
