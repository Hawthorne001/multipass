/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include <multipass/ssh/scp_client.h>
#include <multipass/ssh/throw_on_error.h>
#include <multipass/utils.h>

#include "ssh_client_key_provider.h"

#include <fmt/format.h>

#include <array>

#include <QFile>
#include <QFileInfo>

namespace mp = multipass;

namespace
{
using SCPUPtr = std::unique_ptr<ssh_scp_struct, void (*)(ssh_scp)>;

SCPUPtr make_scp_session(ssh_session session, int mode, const char* path)
{
    SCPUPtr scp{ssh_scp_new(session, mode, path), ssh_scp_free};

    if (scp == nullptr)
        throw std::runtime_error(fmt::format("could not create new scp session: {}", ssh_get_error(session)));

    return scp;
}

std::string full_destination(const std::string& destination_path, const std::string& filename)
{
    if (destination_path.empty())
    {
        return filename;
    }
    else if (mp::utils::is_dir(destination_path))
    {
        return fmt::format("{}/{}", destination_path, filename);
    }

    return destination_path;
}
} // namespace

mp::SCPClient::SCPClient(const std::string& host, int port, const std::string& username,
                         const std::string& priv_key_blob)
    : SCPClient{std::make_unique<mp::SSHSession>(host, port, username, mp::SSHClientKeyProvider(priv_key_blob))}
{
}

mp::SCPClient::SCPClient(SSHSessionUPtr ssh_session) : ssh_session{std::move(ssh_session)}
{
}

void mp::SCPClient::push_file(const std::string& source_path, const std::string& destination_path)
{
    auto full_destination_path = full_destination(destination_path, mp::utils::filename_for(source_path));
    SCPUPtr scp{make_scp_session(*ssh_session, SSH_SCP_WRITE, full_destination_path.c_str())};
    SSH::throw_on_error(scp, *ssh_session, "[scp push] init failed", ssh_scp_init);

    QFile source(QString::fromStdString(source_path));
    const auto size{source.size()};
    int mode = 0664;
    SSH::throw_on_error(scp, *ssh_session, "[scp push] failed", ssh_scp_push_file, source_path.c_str(), size, mode);

    int total{0};
    std::array<char, 65536u> data;

    if (!source.open(QIODevice::ReadOnly))
        throw std::runtime_error(
            fmt::format("[scp push] error opening file for reading: {}", source.errorString().toStdString()));

    do
    {
        auto r = source.read(data.data(), data.size());

        if (r == -1)
            throw std::runtime_error(
                fmt::format("[scp push] error reading file: {}" + source.errorString().toStdString()));
        if (r == 0)
            break;

        SSH::throw_on_error(scp, *ssh_session, "[scp push] remote write failed", ssh_scp_write, data.data(), r);

        total += r;
    } while (total < size);

    SSH::throw_on_error(scp, *ssh_session, "[scp push] close failed", ssh_scp_close);
}

void mp::SCPClient::pull_file(const std::string& source_path, const std::string& destination_path)
{
    SCPUPtr scp{make_scp_session(*ssh_session, SSH_SCP_READ, source_path.c_str())};
    SSH::throw_on_error(scp, *ssh_session, "[scp pull] init failed", ssh_scp_init);
    int r;

    while ((r = ssh_scp_pull_request(scp.get())) != SSH_SCP_REQUEST_EOF)
    {
        if (r == SSH_ERROR || r == SSH_SCP_REQUEST_WARNING)
            throw std::runtime_error(
                fmt::format("[scp pull] error receiving information for file: {}", ssh_get_error(*ssh_session)));

        auto size = ssh_scp_request_get_size(scp.get());
        std::string filename{ssh_scp_request_get_filename(scp.get())};

        auto total{0u};
        std::array<char, 65536u> data;

        auto full_destination_path = full_destination(destination_path, filename);
        QFile destination(QString::fromStdString(full_destination_path));
        if (!destination.open(QIODevice::WriteOnly))
            throw std::runtime_error(
                fmt::format("[scp pull] error opening file for writing: {}", destination.errorString().toStdString()));

        SSH::throw_on_error(scp, *ssh_session, "[scp pull] accept request failed", ssh_scp_accept_request);

        do
        {
            r = ssh_scp_read(scp.get(), data.data(), data.size());

            if (r == 0)
                break;

            if (destination.write(data.data(), r) == -1)
                throw std::runtime_error(
                    fmt::format("[scp pull] error writing to file: {}", destination.errorString().toStdString()));

            total += r;
        } while (total < size);
    }

    SSH::throw_on_error(scp, *ssh_session, "[scp pull] close failed", ssh_scp_close);
}
