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

#ifndef MULTIPASS_STUB_CONSOLE_H
#define MULTIPASS_STUB_CONSOLE_H

#include <multipass/console.h>

namespace multipass
{
namespace test
{
struct StubConsole final : public multipass::Console
{
    void setup_console() override{};

    int read_console(std::array<char, 512>& buffer) override
    {
        return 0;
    };

    void write_console(const char* buffer, int bytes) override{};

    void signal_console() override{};

    bool is_window_size_changed() override
    {
        return false;
    };

    bool is_interactive() override
    {
        return true;
    };

    WindowGeometry get_window_geometry() override
    {
        return WindowGeometry{-1, -1};
    };

    void restore_console() override{};
};
}
}
#endif // MULTIPASS_STUB_CONSOLE_H
