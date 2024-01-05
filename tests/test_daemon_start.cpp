/*
 * Copyright (C) Canonical, Ltd.
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

// The daemon test fixture contains premock code so it must be included first.
#include "daemon_test_fixture.h"

#include "common.h"
#include "mock_image_host.h"
#include "mock_mount_handler.h"
#include "mock_platform.h"
#include "mock_server_reader_writer.h"
#include "mock_settings.h"
#include "mock_virtual_machine.h"
#include "mock_vm_image_vault.h"

#include <src/daemon/daemon.h>

#include <multipass/constants.h>
#include <multipass/format.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct TestDaemonStart : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillRepeatedly(Return("true"));
    }

    const std::string mock_instance_name{"real-zebraphant"};
    const std::string mac_addr{"52:54:00:73:76:28"};
    std::vector<mp::NetworkInterface> extra_interfaces;

    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = attr.first;

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;
};

TEST_F(TestDaemonStart, successfulStartOkStatus)
{
    auto mock_factory = use_a_mock_vm_factory();
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });
    EXPECT_CALL(*instance_ptr, wait_until_ssh_up).WillRepeatedly(Return());
    EXPECT_CALL(*instance_ptr, current_state()).WillRepeatedly(Return(mp::VirtualMachine::State::off));
    EXPECT_CALL(*instance_ptr, start()).Times(1);

    config_builder.data_directory = temp_dir->path();
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    mp::Daemon daemon{config_builder.build()};

    mp::StartRequest request;
    request.mutable_instance_names()->add_instance_name(mock_instance_name);

    auto status = call_daemon_slot(daemon, &mp::Daemon::start, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::StartReply, mp::StartRequest>>{});

    EXPECT_TRUE(status.ok());
}

TEST_F(TestDaemonStart, messageOnSSHError)
{
    REPLACE(ssh_new, []() { return nullptr; }); // This makes creating the SSH session throw.

    std::vector<mp::NetworkInterface> unconfigured{{"eth7", "", true}};

    auto mock_factory = use_a_mock_vm_factory();
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, unconfigured));

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });

    EXPECT_CALL(*instance_ptr, wait_until_ssh_up).WillRepeatedly(Return());
    EXPECT_CALL(*instance_ptr, current_state()).WillRepeatedly(Return(mp::VirtualMachine::State::off));
    EXPECT_CALL(*instance_ptr, start()).Times(1);
    EXPECT_CALL(*instance_ptr, add_network_interface(_, _)).Times(1);

    config_builder.data_directory = temp_dir->path();
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    mp::Daemon daemon{config_builder.build()};

    mp::StartRequest request;
    request.mutable_instance_names()->add_instance_name(mock_instance_name);

    StrictMock<mpt::MockServerReaderWriter<mp::StartReply, mp::StartRequest>> server;

    EXPECT_CALL(server, Write(Property(&mp::StartReply::log_line, HasSubstr("Cannot create a SSH shell")), _)).Times(1);

    auto status = call_daemon_slot(daemon, &mp::Daemon::start, request, std::move(server));

    EXPECT_THAT(status.error_message(), StrEq(""));
    EXPECT_TRUE(status.ok());
}

struct WithSSH : public TestDaemonStart, WithParamInterface<int>
{
};

TEST_P(WithSSH, startConfiguresInterfaces)
{
    ssh_channel_callbacks callbacks{nullptr};
    auto add_channel_cbs = [&callbacks](ssh_channel, ssh_channel_callbacks cb) {
        callbacks = cb;
        return SSH_OK;
    };
    REPLACE(ssh_add_channel_callbacks, add_channel_cbs);

    int expected_status{GetParam()}; // The exit status given by SSH polling.
    auto event_dopoll = [&callbacks, &expected_status](auto...) {
        if (!callbacks)
            return SSH_ERROR;
        callbacks->channel_exit_status_function(nullptr, nullptr, expected_status, callbacks->userdata);
        return SSH_OK;
    };
    REPLACE(ssh_event_dopoll, event_dopoll);

    std::string fake_output{"some output"};
    auto remaining = fake_output.size();
    auto channel_read = [&fake_output, &remaining](ssh_channel, void* dest, uint32_t count, int is_stderr, int) {
        const auto num_to_copy = std::min(count, static_cast<uint32_t>(remaining));
        const auto begin = fake_output.begin() + fake_output.size() - remaining;
        std::copy_n(begin, num_to_copy, reinterpret_cast<char*>(dest));
        remaining -= num_to_copy;
        return num_to_copy;
    };
    REPLACE(ssh_channel_read_timeout, channel_read);

    std::vector<mp::NetworkInterface> unconfigured{{"eth7", "", true}};

    auto mock_factory = use_a_mock_vm_factory();
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, unconfigured));

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });

    EXPECT_CALL(*instance_ptr, wait_until_ssh_up).WillRepeatedly(Return());
    EXPECT_CALL(*instance_ptr, current_state()).WillRepeatedly(Return(mp::VirtualMachine::State::off));
    EXPECT_CALL(*instance_ptr, start()).Times(1);
    EXPECT_CALL(*instance_ptr, add_network_interface(_, _)).Times(1);

    config_builder.data_directory = temp_dir->path();
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    mp::Daemon daemon{config_builder.build()};

    mp::StartRequest request;
    request.mutable_instance_names()->add_instance_name(mock_instance_name);

    StrictMock<mpt::MockServerReaderWriter<mp::StartReply, mp::StartRequest>> server;

    if (expected_status)
    {
        EXPECT_CALL(server,
                    Write(Property(&mp::StartReply::log_line, HasSubstr("Failure configuring network interfaces")), _))
            .Times(1);
    }
    else
    {
        EXPECT_CALL(server, Write(_, _)).Times(0);
    }

    auto status = call_daemon_slot(daemon, &mp::Daemon::start, request, std::move(server));

    EXPECT_THAT(status.error_message(), StrEq(""));
    EXPECT_TRUE(status.ok());
}

INSTANTIATE_TEST_SUITE_P(TestDaemonStart, WithSSH, Values(0, 1, -1));

TEST_F(TestDaemonStart, unknownStateDoesNotStart)
{
    auto mock_factory = use_a_mock_vm_factory();
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });

    EXPECT_CALL(*instance_ptr, current_state()).WillRepeatedly(Return(mp::VirtualMachine::State::unknown));
    EXPECT_CALL(*instance_ptr, start()).Times(0);

    config_builder.data_directory = temp_dir->path();
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    mp::Daemon daemon{config_builder.build()};

    mp::StartRequest request;
    request.mutable_instance_names()->add_instance_name(mock_instance_name);

    auto status = call_daemon_slot(daemon, &mp::Daemon::start, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::StartReply, mp::StartRequest>>{});

    EXPECT_FALSE(status.ok());
}

TEST_F(TestDaemonStart, suspendingStateDoesNotStartHasError)
{
    auto mock_factory = use_a_mock_vm_factory();
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });

    EXPECT_CALL(*instance_ptr, current_state()).WillRepeatedly(Return(mp::VirtualMachine::State::suspending));
    EXPECT_CALL(*instance_ptr, start()).Times(0);

    config_builder.data_directory = temp_dir->path();
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    mp::Daemon daemon{config_builder.build()};

    mp::StartRequest request;
    request.mutable_instance_names()->add_instance_name(mock_instance_name);

    auto status = call_daemon_slot(daemon, &mp::Daemon::start, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::StartReply, mp::StartRequest>>{});

    EXPECT_FALSE(status.ok());

    EXPECT_THAT(status.error_message(),
                HasSubstr(fmt::format("Cannot start the instance \'{}\' while suspending", mock_instance_name)));
}

TEST_F(TestDaemonStart, definedMountsInitializedDuringStart)
{
    const std::string fake_target_path{"/home/luke/skywalker"}, fake_source_path{"/home/han/solo"};
    const mp::id_mappings uid_mappings{{1000, 1001}}, gid_mappings{{1002, 1003}};
    const mp::VMMount mount{fake_source_path, gid_mappings, uid_mappings, mp::VMMount::MountType::Native};
    std::unordered_map<std::string, mp::VMMount> mounts{{fake_target_path, mount}};

    auto mock_factory = use_a_mock_vm_factory();
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces, mounts));

    auto mock_mount_handler = std::make_unique<mpt::MockMountHandler>();
    EXPECT_CALL(*mock_mount_handler, activate_impl).Times(1);

    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_vm, wait_until_ssh_up).WillRepeatedly(Return());
    EXPECT_CALL(*mock_vm, current_state).WillRepeatedly(Return(mp::VirtualMachine::State::off));
    EXPECT_CALL(*mock_vm, start).Times(1);
    EXPECT_CALL(*mock_vm, make_native_mount_handler).WillOnce(Return(std::move(mock_mount_handler)));

    EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce(Return(std::move(mock_vm)));

    config_builder.data_directory = temp_dir->path();
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    mp::Daemon daemon{config_builder.build()};

    mp::StartRequest request;
    request.mutable_instance_names()->add_instance_name(mock_instance_name);

    auto status = call_daemon_slot(daemon, &mp::Daemon::start, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::StartReply, mp::StartRequest>>{});

    EXPECT_TRUE(status.ok());
}

TEST_F(TestDaemonStart, removingMountOnFailedStart)
{
    const std::string fake_target_path{"/home/luke/skywalker"}, fake_source_path{"/home/han/solo"};
    const mp::id_mappings uid_mappings{{1000, 1001}}, gid_mappings{{1002, 1003}};
    const mp::VMMount mount{fake_source_path, gid_mappings, uid_mappings, mp::VMMount::MountType::Native};
    std::unordered_map<std::string, mp::VMMount> mounts{{fake_target_path, mount}};

    auto mock_factory = use_a_mock_vm_factory();
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces, mounts));

    auto error = "failed to start mount";
    auto mock_mount_handler = std::make_unique<mpt::MockMountHandler>();
    EXPECT_CALL(*mock_mount_handler, activate_impl).WillOnce(Throw(std::runtime_error{error}));

    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_vm, wait_until_ssh_up).WillRepeatedly(Return());
    EXPECT_CALL(*mock_vm, current_state).WillRepeatedly(Return(mp::VirtualMachine::State::off));
    EXPECT_CALL(*mock_vm, start).Times(1);
    EXPECT_CALL(*mock_vm, make_native_mount_handler).WillOnce(Return(std::move(mock_mount_handler)));

    EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce(Return(std::move(mock_vm)));

    auto log = fmt::format("Removing mount \"{}\" from '{}': {}\n", fake_target_path, mock_instance_name, error);
    StrictMock<mpt::MockServerReaderWriter<mp::StartReply, mp::StartRequest>> server;
    EXPECT_CALL(server, Write(Property(&mp::StartReply::log_line, Eq(log)), _));

    config_builder.data_directory = temp_dir->path();
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    mp::Daemon daemon{config_builder.build()};

    mp::StartRequest request;
    request.mutable_instance_names()->add_instance_name(mock_instance_name);

    auto status = call_daemon_slot(daemon, &mp::Daemon::start, request, std::move(server));
    EXPECT_TRUE(status.ok());
}
