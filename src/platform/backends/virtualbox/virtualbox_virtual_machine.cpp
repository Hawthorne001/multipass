/*
 * Copyright (C) 2019-2020 Canonical, Ltd.
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

#include "virtualbox_virtual_machine.h"

#include <multipass/exceptions/start_exception.h>
#include <multipass/logging/log.h>
#include <multipass/network_interface.h>
#include <multipass/optional.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>

#include <shared/shared_backend_utils.h>

#include <fmt/format.h>

#include <QProcess>
#include <QRegularExpression>
#include <QtNetwork/QTcpServer>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace
{

auto instance_state_for(const QString& name)
{
    QRegularExpression vmstate_re("VMState=\"([a-z]+)\"");

    QProcess vminfo;
    vminfo.start("VBoxManage", {"showvminfo", name, "--machinereadable"});
    auto success = vminfo.waitForFinished();
    if (!success || vminfo.exitStatus() != QProcess::NormalExit) {
        throw std::runtime_error(fmt::format("Failed to run VBoxManage: {}", vminfo.errorString().toStdString()));
    }
    auto vminfo_output = QString::fromUtf8(vminfo.readAllStandardOutput());
    auto vmstate_match = vmstate_re.match(vminfo_output);

    if (vmstate_match.hasMatch())
    {
        auto state = vmstate_match.captured(1);

        mpl::log(mpl::Level::debug, name.toStdString(), fmt::format("Got VMState: {}", state.toStdString()));

        if (state == "starting" || state == "restoring")
        {
            return mp::VirtualMachine::State::starting;
        }
        else if (state == "running" || state == "paused" || state == "onlinesnapshotting" || state == "stopping")
        {
            return mp::VirtualMachine::State::running;
        }
        else if (state == "saving")
        {
            return mp::VirtualMachine::State::suspending;
        }
        else if (state == "saved")
        {
            return mp::VirtualMachine::State::suspended;
        }
        else if (state == "poweroff" || state == "aborted")
        {
            return mp::VirtualMachine::State::stopped;
        }

        mpl::log(mpl::Level::error, name.toStdString(),
                 fmt::format("Failed to parse instance state: {}", vmstate_match.captured().toStdString()));
    }
    else if (vminfo.exitCode() == 0)
    {
        mpl::log(mpl::Level::error, name.toStdString(), fmt::format("Failed to parse info output: {}", vminfo_output.toStdString()));
    }

    return mp::VirtualMachine::State::unknown;
}

QStringList networking_arguments(const mp::VirtualMachineDescription& desc)
{
    // Start with the default interface, which is also the first interface in the input vector.
    QStringList arguments{"--nic1", "nat", "--macaddress1",
                          QString::fromStdString(desc.default_interface.mac_address).remove(':')};

    for (size_t i = 0; i < desc.extra_interfaces.size(); ++i)
    {
        QString iface_index_str = QString::number(i + 2, 10);
        arguments.push_back("--nic" + iface_index_str);
        arguments.push_back("bridged");
        if (!desc.extra_interfaces[i].id.empty())
        {
            arguments.push_back("--bridgeadapter" + iface_index_str);
            arguments.push_back(QString::fromStdString(desc.extra_interfaces[i].id));
        }
        arguments.push_back("--macaddress" + iface_index_str);
        arguments.push_back(QString::fromStdString(desc.extra_interfaces[i].mac_address).remove(':'));
    }

    return arguments;
}
} // namespace

mp::VirtualBoxVirtualMachine::VirtualBoxVirtualMachine(const VirtualMachineDescription& desc, VMStatusMonitor& monitor)
    : VirtualMachine{desc.vm_name},
      name{QString::fromStdString(desc.vm_name)},
      username{desc.ssh_username},
      monitor{&monitor}
{

    if (!mpu::process_log_on_error("VBoxManage", {"showvminfo", name, "--machinereadable"},
                                   "Could not get instance info: {}", name))
    {
        mpu::process_throw_on_error(
            "VBoxManage", {"createvm", "--name", name, "--groups", "/Multipass", "--ostype", "ubuntu_64", "--register"},
            "Could not create VM: {}", name);

        QStringList modify_arguments{
            "modifyvm",
            name,
            "--cpus",
            QString::number(desc.num_cores),
            "--memory",
            QString::number(desc.mem_size.in_megabytes()),
            "--boot1",
            "disk",
            "--boot2",
            "none",
            "--boot3",
            "none",
            "--boot4",
            "none",
            "--acpi",
            "on",
            "--firmware",
            "bios",
            "--rtcuseutc",
            "on",
            "--audio",
            "none",
            "--uart1",
            "0x3f8",
            "4",
            "--uartmode1",
            "file",
            QString("%1/%2.log").arg(MP_STDPATHS.writableLocation(StandardPaths::TempLocation)).arg(name)};
        modify_arguments += networking_arguments(desc);

        mpu::process_throw_on_error("VBoxManage", modify_arguments, "Could not modify VM: {}", name);

        mpu::process_throw_on_error("VBoxManage",
                                    {"storagectl", name, "--add", "sata", "--name", "SATA_0", "--portcount", "2"},
                                    "Could not modify VM: {}", name);

        mpu::process_throw_on_error("VBoxManage",
                                    {"storageattach", name, "--storagectl", "SATA_0", "--port", "0", "--device", "0",
                                     "--type", "hdd", "--medium", desc.image.image_path},
                                    "Could not storageattach HDD: {}", name);

        mpu::process_throw_on_error("VBoxManage",
                                    {"storageattach", name, "--storagectl", "SATA_0", "--port", "1", "--device", "0",
                                     "--type", "dvddrive", "--medium", desc.cloud_init_iso},
                                    "Could not storageattach DVD: {}", name);

        state = State::off;
    }
    else
    {
        state = instance_state_for(name);
    }
}

mp::VirtualBoxVirtualMachine::~VirtualBoxVirtualMachine()
{
    update_suspend_status = false;

    if (current_state() == State::running)
        suspend();
}

void mp::VirtualBoxVirtualMachine::start()
{
    auto present_state = current_state();

    if (present_state == State::running)
        return;

    state = State::starting;
    update_state();

    mpu::process_throw_on_error("VBoxManage", {"startvm", name, "--type", "headless"}, "Could not start VM: {}", name);
}

void mp::VirtualBoxVirtualMachine::stop()
{
    std::unique_lock<decltype(state_mutex)> lock{state_mutex};
    auto present_state = current_state();

    if (present_state == State::running || present_state == State::delayed_shutdown)
    {
        mpu::process_throw_on_error("VBoxManage", {"controlvm", name, "acpipowerbutton"}, "Could not stop VM: {}",
                                    name);
        state = State::stopped;
        port = mp::nullopt;
    }
    else if (present_state == State::starting)
    {
        mpu::process_throw_on_error("VBoxManage", {"controlvm", name, "poweroff"}, "Could not power VM off: {}", name);
        state = State::stopped;
        state_wait.wait(lock, [this] { return shutdown_while_starting; });
        port = mp::nullopt;
    }
    else if (present_state == State::suspended)
    {
        mpl::log(mpl::Level::info, vm_name, fmt::format("Ignoring shutdown issued while suspended"));
    }

    update_state();
    lock.unlock();
}

void mp::VirtualBoxVirtualMachine::shutdown()
{
    stop();
}

void mp::VirtualBoxVirtualMachine::suspend()
{
    auto present_state = instance_state_for(name);

    if (present_state == State::running || present_state == State::delayed_shutdown)
    {
        mpu::process_throw_on_error("VBoxManage", {"controlvm", name, "savestate"}, "Could not suspend VM: {}", name);

        if (update_suspend_status)
        {
            state = State::suspended;
            update_state();
        }
    }
    else if (present_state == State::stopped)
    {
        mpl::log(mpl::Level::info, vm_name, fmt::format("Ignoring suspend issued while stopped"));
    }

    monitor->on_suspend();
}

mp::VirtualMachine::State mp::VirtualBoxVirtualMachine::current_state()
{
    auto present_state = instance_state_for(name);

    if ((state == State::delayed_shutdown && present_state == State::running) || state == State::starting)
        return state;

    state = present_state;
    return state;
}

int mp::VirtualBoxVirtualMachine::ssh_port()
{
    if (!port)
    {
        QTcpServer socket;
        if (!socket.listen(QHostAddress("127.0.0.1")))
        {
            throw std::runtime_error(
                fmt::format("Could not find a port available to listen on: {}", socket.serverError()));
        }

        port.emplace(socket.serverPort());
        socket.close();

        mpu::process_log_on_error("VBoxManage", {"controlvm", name, "natpf1", "delete", "ssh"},
                                  "Could not delete SSH port forwarding: {}", name);

        mpu::process_throw_on_error(
            "VBoxManage", {"controlvm", name, "natpf1", QString::fromStdString(fmt::format("ssh,tcp,,{},,22", *port))},
            "Could not add SSH port forwarding: {}", name);
    }
    return *port;
}

void mp::VirtualBoxVirtualMachine::ensure_vm_is_running()
{
    auto is_vm_running = [this] { return state != State::stopped; };

    mp::backend::ensure_vm_is_running_for(this, is_vm_running, "Instance shutdown during start");
}

void mp::VirtualBoxVirtualMachine::update_state()
{
    monitor->persist_state_for(vm_name, state);
}

std::string mp::VirtualBoxVirtualMachine::ssh_hostname(std::chrono::milliseconds /*timeout*/)
{
    return "127.0.0.1";
}

std::string mp::VirtualBoxVirtualMachine::ssh_username()
{
    return username;
}

std::string mp::VirtualBoxVirtualMachine::ipv4()
{
    return "N/A";
}

std::string mp::VirtualBoxVirtualMachine::ipv6()
{
    return {};
}

void mp::VirtualBoxVirtualMachine::wait_until_ssh_up(std::chrono::milliseconds timeout)
{
    mpu::wait_until_ssh_up(this, timeout, std::bind(&VirtualBoxVirtualMachine::ensure_vm_is_running, this));
}
