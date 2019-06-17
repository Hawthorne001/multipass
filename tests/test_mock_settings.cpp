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

#include "mock_settings.h"

#include <multipass/settings.h>

#include <QString>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{

TEST(Settings, can_be_mocked)
{
    const auto test = QStringLiteral("abc"), proof = QStringLiteral("xyz");
    const auto& mock = mpt::MockSettings::mock_instance();

    EXPECT_CALL(mock, get(_)).WillOnce(Return(proof));
    ASSERT_EQ(mp::Settings::instance().get(test), proof);
}

} // namespace
