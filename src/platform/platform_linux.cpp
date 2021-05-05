/*
 * Copyright (C) 2017-2021 Canonical, Ltd.
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

#include <multipass/constants.h>
#include <multipass/exceptions/autostart_setup_exception.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/exceptions/snap_environment_exception.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/process/qemuimg_process_spec.h>
#include <multipass/snap_utils.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_factory.h>

#include "backends/libvirt/libvirt_virtual_machine_factory.h"
#include "backends/lxd/lxd_virtual_machine_factory.h"
#include "backends/qemu/qemu_virtual_machine_factory.h"
#include "backends/virtualbox/virtualbox_virtual_machine_factory.h"
#include "logger/journald_logger.h"
#include "platform_linux_detail.h"
#include "platform_shared.h"
#include "shared/linux/process_factory.h"
#include "shared/sshfs_server_process_spec.h"
#include <disabled_update_prompt.h>

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QTextStream>

#include <linux/if_arp.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mu = multipass::utils;

namespace
{
constexpr auto autostart_filename = "multipass.gui.autostart.desktop";
constexpr auto category = "Linux platform";

// Fetch the ARP protocol HARDWARE identifier.
int get_net_type(const QDir& net_dir) // types defined in if_arp.h
{
    static constexpr auto default_ret = -1;
    static const auto type_filename = QStringLiteral("type");

    QFile type_file{net_dir.filePath(type_filename)};
    if (type_file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        bool ok;
        auto got = QTextStream{&type_file}.read(6).toInt(&ok); // 6 chars enough for up to 0xFFFF; 0 returned on failure
        return ok ? got : default_ret;
    }

    auto snap_hint = mu::in_multipass_snap() ? " Is the 'network-observe' snap interface connected?" : "";
    mpl::log(mpl::Level::warning, category, fmt::format("Could not read {}.{}", type_file.fileName(), snap_hint));

    return default_ret;
}

// device types found in Linux source (in drivers/net/): PHY, bareudp, bond, geneve, gtp, macsec, ppp, vxlan, wlan, wwan
// should be empty for ethernet
QString get_net_devtype(const QDir& net_dir)
{
    static constexpr auto max_read = 5000;
    static const auto uevent_filename = QStringLiteral("uevent");
    static const auto devtype_regex =
        QRegularExpression{QStringLiteral("^DEVTYPE=(.*)$"), QRegularExpression::MultilineOption};

    QFile uevent_file{net_dir.filePath(uevent_filename)};
    if (uevent_file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        auto contents = QTextStream{&uevent_file}.read(max_read);
        return devtype_regex.match(contents).captured(1);
    }

    mpl::log(mpl::Level::warning, category, fmt::format("Could not read {}", uevent_file.fileName()));
    return {};
}

bool is_virtual_net(const QDir& net_dir)
{
    static const auto virtual_dir = QStringLiteral("virtual");

    return net_dir.canonicalPath().contains(virtual_dir, Qt::CaseInsensitive);
}

bool is_ethernet(const QDir& net_dir)
{
    static const auto wireless = QStringLiteral("wireless");

    return !is_virtual_net(net_dir) && !net_dir.exists(wireless) && get_net_type(net_dir) == ARPHRD_ETHER &&
           get_net_devtype(net_dir).isEmpty();
}

mp::NetworkInterfaceInfo get_network(const QDir& net_dir)
{
    static const auto bridge_fname = QStringLiteral("brif");

    std::string type, description;
    if (auto bridge = "bridge"; net_dir.exists(bridge))
    {
        type = bridge;
        QStringList bridge_members = QDir{net_dir.filePath(bridge_fname)}.entryList(QDir::NoDotAndDotDot | QDir::Dirs);
        description = bridge_members.isEmpty() ? "Empty network bridge"
                                               : fmt::format("Network bridge with {}", bridge_members.join(", "));
    }
    else if (is_ethernet(net_dir))
    {
        type = "ethernet";
        description = "Ethernet device";
    }

    return mp::NetworkInterfaceInfo{net_dir.dirName().toStdString(), std::move(type), std::move(description)};
}
} // namespace

std::map<std::string, mp::NetworkInterfaceInfo> mp::platform::Platform::get_network_interfaces_info() const
{
    static const auto sysfs = QDir{QStringLiteral("/sys/class/net")};
    return detail::get_network_interfaces_from(sysfs);
}

QString mp::platform::Platform::get_workflows_url_override()
{
    return QString::fromUtf8(qgetenv("MULTIPASS_WORKFLOWS_URL"));
}

bool mp::platform::Platform::is_alias_supported(const std::string& alias, const std::string& remote)
{
    return true;
}

bool mp::platform::Platform::is_remote_supported(const std::string& remote)
{
    // snapcraft:core{18,20} images don't work on LXD yet, so whack it altogether.
    return remote != "snapcraft" || utils::get_driver_str() != "lxd";
}

bool mp::platform::Platform::link(const char* target, const char* link)
{
    return ::link(target, link) == 0;
}

auto mp::platform::detail::get_network_interfaces_from(const QDir& sys_dir)
    -> std::map<std::string, NetworkInterfaceInfo>
{
    auto ifaces_info = std::map<std::string, mp::NetworkInterfaceInfo>();
    for (const auto& entry : sys_dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs))
    {
        auto iface = get_network(QDir{sys_dir.filePath(entry)});
        auto name = iface.id; // (can't rely on param evaluation order)
        ifaces_info.emplace(std::move(name), std::move(iface));
    }

    return ifaces_info;
}

std::map<QString, QString> mp::platform::extra_settings_defaults()
{
    return {};
}

QString mp::platform::interpret_setting(const QString& key, const QString& val)
{
    if (key == hotkey_key)
        return mp::platform::interpret_hotkey(val);

    // this should not happen (settings should have found it to be an invalid key)
    throw InvalidSettingsException(key, val, "Setting unavailable on Linux");
}

void mp::platform::sync_winterm_profiles()
{
    // NOOP on Linux
}

QString mp::platform::autostart_test_data()
{
    return autostart_filename;
}

void mp::platform::setup_gui_autostart_prerequisites()
{
    const auto config_dir = QDir{MP_STDPATHS.writableLocation(StandardPaths::GenericConfigLocation)};
    const auto link_dir = QDir{config_dir.absoluteFilePath("autostart")};
    mu::link_autostart_file(link_dir, mp::client_name, autostart_filename);
}

std::string mp::platform::default_server_address()
{
    std::string base_dir;

    try
    {
        // if Snap, client and daemon can both access $SNAP_COMMON so can put socket there
        base_dir = mu::snap_common_dir().toStdString();
    }
    catch (const mp::SnapEnvironmentException&)
    {
        base_dir = "/run";
    }
    return "unix:" + base_dir + "/multipass_socket";
}

QString mp::platform::default_driver()
{
    return QStringLiteral("qemu");
}

QString mp::platform::daemon_config_home() // temporary
{
    auto ret = QString{qgetenv("DAEMON_CONFIG_HOME")};
    if (ret.isEmpty())
        ret = QStringLiteral("/root/.config");

    ret = QDir{ret}.absoluteFilePath(mp::daemon_name);
    return ret;
}

bool mp::platform::is_backend_supported(const QString& backend)
{
    return backend == "qemu" || backend == "libvirt" || backend == "lxd" || backend == "virtualbox";
}

mp::VirtualMachineFactory::UPtr mp::platform::vm_backend(const mp::Path& data_dir)
{
    const auto& driver = utils::get_driver_str();
    if (driver == QStringLiteral("qemu"))
        return std::make_unique<QemuVirtualMachineFactory>(data_dir);
    else if (driver == QStringLiteral("libvirt"))
        return std::make_unique<LibVirtVirtualMachineFactory>(data_dir);
    else if (driver == QStringLiteral("virtualbox"))
        return std::make_unique<VirtualBoxVirtualMachineFactory>();
    else if (driver == QStringLiteral("lxd"))
        return std::make_unique<LXDVirtualMachineFactory>(data_dir);
    else
        throw std::runtime_error(fmt::format("Unsupported virtualization driver: {}", driver));
}

std::unique_ptr<mp::Process> mp::platform::make_sshfs_server_process(const mp::SSHFSServerConfig& config)
{
    return MP_PROCFACTORY.create_process(std::make_unique<mp::SSHFSServerProcessSpec>(config));
}

std::unique_ptr<mp::Process> mp::platform::make_process(std::unique_ptr<mp::ProcessSpec>&& process_spec)
{
    return MP_PROCFACTORY.create_process(std::move(process_spec));
}

mp::UpdatePrompt::UPtr mp::platform::make_update_prompt()
{
    return std::make_unique<DisabledUpdatePrompt>();
}

mp::logging::Logger::UPtr mp::platform::make_logger(mp::logging::Level level)
{
    return std::make_unique<logging::JournaldLogger>(level);
}

bool mp::platform::is_image_url_supported()
{
    return true;
}

std::string mp::platform::reinterpret_interface_id(const std::string& ux_id)
{
    return ux_id;
}
