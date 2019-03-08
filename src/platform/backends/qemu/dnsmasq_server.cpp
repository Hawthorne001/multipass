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

#include "dnsmasq_server.h"

#include "dnsmasq_process_spec.h"
#include <multipass/logging/log.h>
#include <multipass/utils.h>
#include <shared/linux/process.h>
#include <shared/linux/process_factory.h>

#include <fmt/format.h>

#include <fstream>
namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
auto make_dnsmasq_process(const mp::ProcessFactory* process_factory, const mp::Path& data_dir,
                          const QString& bridge_name, const mp::IPAddress& bridge_addr, const mp::IPAddress& start,
                          const mp::IPAddress& end)
{
    auto process_spec = std::make_unique<mp::DNSMasqProcessSpec>(data_dir, bridge_name, bridge_addr, start, end);
    return process_factory->create_process(std::move(process_spec));
}
} // namespace

mp::DNSMasqServer::DNSMasqServer(const ProcessFactory* process_factory, const Path& data_dir,
                                 const QString& bridge_name, const IPAddress& bridge_addr, const IPAddress& start,
                                 const IPAddress& end)
    : data_dir{data_dir},
      dnsmasq_cmd{make_dnsmasq_process(process_factory, data_dir, bridge_name, bridge_addr, start, end)},
      bridge_name{bridge_name}
{
    dnsmasq_cmd->start();
}

mp::DNSMasqServer::~DNSMasqServer()
{
    dnsmasq_cmd->kill();
    dnsmasq_cmd->waitForFinished();
}

mp::optional<mp::IPAddress> mp::DNSMasqServer::get_ip_for(const std::string& hw_addr)
{
    // DNSMasq leases entries consist of:
    // <lease expiration> <mac addr> <ipv4> <name> * * *
    const auto path = data_dir.filePath("dnsmasq.leases").toStdString();
    const std::string delimiter{" "};
    const int hw_addr_idx{1};
    const int ipv4_idx{2};
    std::ifstream leases_file{path};
    std::string line;
    while (getline(leases_file, line))
    {
        const auto fields = mp::utils::split(line, delimiter);
        if (fields.size() > 2 && fields[hw_addr_idx] == hw_addr)
            return mp::optional<mp::IPAddress>{fields[ipv4_idx]};
    }
    return mp::nullopt;
}

void mp::DNSMasqServer::release_mac(const std::string& hw_addr)
{
    auto ip = get_ip_for(hw_addr);
    if (!ip)
    {
        mpl::log(mpl::Level::warning, "dnsmasq", fmt::format("attempting to release non-existant addr: {}", hw_addr));
        return;
    }

    QProcess dhcp_release;
    QObject::connect(&dhcp_release, &QProcess::errorOccurred, [&ip, &hw_addr](QProcess::ProcessError error) {
        mpl::log(mpl::Level::warning, "dnsmasq",
                 fmt::format("failed to release ip addr {} with mac {}", ip.value().as_string(), hw_addr));
    });

    auto log_exit_status = [&ip, &hw_addr](int exit_code, QProcess::ExitStatus exit_status) {
        if (exit_code == 0 && exit_status == QProcess::NormalExit)
            return;

        auto msg = fmt::format("failed to release ip addr {} with mac {}, exit_code: {}", ip.value().as_string(),
                               hw_addr, exit_code);
        mpl::log(mpl::Level::warning, "dnsmasq", msg);
    };
    QObject::connect(&dhcp_release, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                     log_exit_status);

    dhcp_release.start("dhcp_release", QStringList() << bridge_name << QString::fromStdString(ip.value().as_string())
                                                     << QString::fromStdString(hw_addr));

    dhcp_release.waitForFinished();
}
