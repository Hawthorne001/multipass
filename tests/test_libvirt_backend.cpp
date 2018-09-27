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

#include <src/platform/backends/libvirt/libvirt_virtual_machine_factory.h>

#include "mock_libvirt.h"
#include "mock_status_monitor.h"
#include "stub_ssh_key_provider.h"
#include "stub_status_monitor.h"
#include "temp_dir.h"
#include "temp_file.h"

#include <multipass/platform.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>

#include <cstdlib>

#include <gmock/gmock.h>

#include <fmt/format.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
template<typename T>
auto fake_handle()
{
    return reinterpret_cast<T>(0xDEADBEEF);
}
}

struct LibVirtBackend : public Test
{
    LibVirtBackend()
    {
        connect_close.returnValue(0);
        domain_free.returnValue(0);
        network_free.returnValue(0);
        leases.returnValue(0);
    }
    mpt::TempFile dummy_image;
    mpt::TempFile dummy_cloud_init_iso;
    mpt::StubSSHKeyProvider key_provider;
    mp::VirtualMachineDescription default_description{2,
                                                      "3M",
                                                      "",
                                                      "pied-piper-valley",
                                                      "",
                                                      "",
                                                      {dummy_image.name(), "", "", "", "", "", "", {}},
                                                      dummy_cloud_init_iso.name(),
                                                      key_provider};
    mpt::TempDir data_dir;

    decltype(MOCK(virConnectClose)) connect_close{MOCK(virConnectClose)};
    decltype(MOCK(virDomainFree)) domain_free{MOCK(virDomainFree)};
    decltype(MOCK(virNetworkFree)) network_free{MOCK(virNetworkFree)};
    decltype(MOCK(virNetworkGetDHCPLeases)) leases{MOCK(virNetworkGetDHCPLeases)};
};

TEST_F(LibVirtBackend, failed_connection_throws)
{
    REPLACE(virConnectOpen, [](auto...) { return nullptr; });
    EXPECT_THROW(mp::LibVirtVirtualMachineFactory backend{data_dir.path()}, std::runtime_error);
}

TEST_F(LibVirtBackend, creates_in_off_state)
{
    REPLACE(virConnectOpen, [](auto...) { return fake_handle<virConnectPtr>(); });
    REPLACE(virNetworkLookupByName, [](auto...) { return fake_handle<virNetworkPtr>(); });
    REPLACE(virNetworkGetBridgeName, [](auto...) {
        std::string bridge_name{"mpvirt0"};
        return strdup(bridge_name.c_str());
    });
    REPLACE(virNetworkIsActive, [](auto...) { return 1; });
    REPLACE(virDomainLookupByName, [](auto...) { return fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto...) { return VIR_DOMAIN_NOSTATE; });
    REPLACE(virDomainGetXMLDesc, [](auto...) {
        std::string domain_desc{"mac"};
        return strdup(domain_desc.c_str());
    });

    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};
    mpt::StubVMStatusMonitor stub_monitor;
    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(LibVirtBackend, machine_sends_monitoring_events)
{
    REPLACE(virConnectOpen, [](auto...) { return fake_handle<virConnectPtr>(); });
    REPLACE(virNetworkLookupByName, [](auto...) { return fake_handle<virNetworkPtr>(); });
    REPLACE(virNetworkGetBridgeName, [](auto...) {
        std::string bridge_name{"mpvirt0"};
        return strdup(bridge_name.c_str());
    });
    REPLACE(virNetworkIsActive, [](auto...) { return 1; });
    REPLACE(virDomainLookupByName, [](auto...) { return fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto...) { return VIR_DOMAIN_NOSTATE; });
    REPLACE(virDomainGetXMLDesc, [](auto...) {
        std::string domain_desc{"mac"};
        return strdup(domain_desc.c_str());
    });
    REPLACE(virDomainCreate, [](auto...) { return 0; });
    REPLACE(virDomainShutdown, [](auto...) { return 0; });

    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};
    mpt::MockVMStatusMonitor mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_CALL(mock_monitor, persist_state_for(_));
    EXPECT_CALL(mock_monitor, on_resume());
    machine->start();

    EXPECT_CALL(mock_monitor, persist_state_for(_)).Times(AtLeast(1));
    EXPECT_CALL(mock_monitor, on_shutdown());
    machine->shutdown();
}
