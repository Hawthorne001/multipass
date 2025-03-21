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

#include "common.h"

#include <multipass/logging/level.h>
#include <multipass/logging/standard_logger.h>
#include <sstream>

namespace mpl = multipass::logging;

using uut_t = mpl::StandardLogger;

TEST(standard_logger_tests, call_log)
{
    std::ostringstream mock_stderr;
    uut_t logger{mpl::Level::debug, mock_stderr};
    logger.log(mpl::Level::debug, "cat", "msg");
    ASSERT_THAT(mock_stderr.str(), testing::HasSubstr("[debug] [cat] msg"));
}

TEST(standard_logger_tests, call_log_filtered)
{
    std::ostringstream mock_stderr;
    uut_t logger{mpl::Level::debug, mock_stderr};
    logger.log(mpl::Level::trace, "cat", "msg");
    ASSERT_TRUE(mock_stderr.str().empty());
}
