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

#ifndef MULTIPASS_MOCK_SETTINGS_H
#define MULTIPASS_MOCK_SETTINGS_H

#include <multipass/settings.h>

#include <gmock/gmock.h>

namespace multipass
{
namespace test
{
class MockSettings : public Settings // This will automatically verify expectations set upon it at the end of each test
{
public:
    using Settings::get_default; // promote visibility
    using Settings::Settings;    // ctor

    static void mockit();
    static MockSettings& mock_instance();

    MOCK_CONST_METHOD1(get, QString(const QString&));
    MOCK_METHOD2(set, void(const QString&, const QString&));

private:
    class Accountant : public ::testing::EmptyTestEventListener
    {
    public:
        void OnTestEnd(const ::testing::TestInfo&) override;
    };

    class TestEnv : public ::testing::Environment
    {
    public:
        void SetUp() override;
        void TearDown() override;

    private:
        void register_accountant();
        void release_accountant();

        Accountant* accountant = nullptr; // non-owning ptr
    };
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_SETTINGS_H
