/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/snap_utils.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_factory.h>

#include "backends/libvirt/libvirt_virtual_machine_factory.h"
#include "backends/qemu/qemu_virtual_machine_factory.h"
#include "backends/virtualbox/virtualbox_virtual_machine_factory.h"
#include "logger/journald_logger.h"
#include "shared/linux/process_factory.h"
#include "shared/sshfs_server_process_spec.h"
#include <disabled_update_prompt.h>

#include <QStandardPaths>

#include <signal.h>
#include <sys/prctl.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mu = multipass::utils;

namespace
{
constexpr auto autostart_filename = "multipass.gui.autostart.desktop";

QString find_desktop_target()
{
    const auto target_subpath = QDir{mp::client_name}.filePath(autostart_filename);
    const auto target_path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, target_subpath);

    if (target_path.isEmpty())
    {
        QString detail{};
        for (const auto& path : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
            detail += QStringLiteral("\n  ") + path + "/" + target_subpath;

        throw mp::AutostartSetupException{
            fmt::format("could not locate the autostart .desktop file '{}'", autostart_filename), detail.toStdString()};
    }

    return target_path;
}

} // namespace

QString mp::platform::autostart_test_data()
{
    return autostart_filename;
}

void mp::platform::setup_gui_autostart_prerequisites()
{
    const auto config_dir = QDir{QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)};
    const auto autostart_dir = QDir{config_dir.absoluteFilePath("autostart")};
    const auto link_path = autostart_dir.absoluteFilePath(autostart_filename);
    const auto target_path = find_desktop_target();

    if (!QFile(link_path).exists())
    {
        autostart_dir.mkpath(".");
        if (!QFile{target_path}.link(link_path))
            throw std::runtime_error(fmt::format("failed to link file '{}' to '{}': {}({})", link_path, target_path,
                                                 strerror(errno), errno));
    }
}

std::string mp::platform::default_server_address()
{
    std::string base_dir;

    if (mu::is_snap())
    {
        // if Snap, client and daemon can both access $SNAP_COMMON so can put socket there
        base_dir = mu::snap_common_dir().toStdString();
    }
    else
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
    return backend == "qemu" || backend == "libvirt" || backend == "virtualbox";
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
    else
        throw std::runtime_error(fmt::format("Unsupported virtualization driver: {}", driver));
}

std::unique_ptr<mp::Process> mp::platform::make_sshfs_server_process(const mp::SSHFSServerConfig& config)
{
    return mp::ProcessFactory::instance().create_process(std::make_unique<mp::SSHFSServerProcessSpec>(config));
}

mp::UpdatePrompt::UPtr mp::platform::make_update_prompt()
{
    return std::make_unique<DisabledUpdatePrompt>();
}

mp::logging::Logger::UPtr mp::platform::make_logger(mp::logging::Level level)
{
    return std::make_unique<logging::JournaldLogger>(level);
}

bool mp::platform::link(const char* target, const char* link)
{
    return ::link(target, link) == 0;
}

bool mp::platform::is_alias_supported(const std::string& alias, const std::string& remote)
{
    return true;
}

bool mp::platform::is_remote_supported(const std::string& remote)
{
    return true;
}

bool mp::platform::is_image_url_supported()
{
    return true;
}

void mp::platform::emit_signal_when_parent_dies(int sig)
{
    prctl(PR_SET_PDEATHSIG, sig); // ensures if parent dies, this process it sent this signal
}
