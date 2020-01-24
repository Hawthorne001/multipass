/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#include "qemu_base_process_spec.h"

#include <multipass/snap_utils.h>
#include <shared/linux/backend_utils.h>

namespace mp = multipass;
namespace mu = multipass::utils;

QString mp::QemuBaseProcessSpec::program() const
{
    return "qemu-system-" + mp::backend::cpu_arch();
}

QString mp::QemuBaseProcessSpec::working_directory() const
{
    if (mu::is_snap())
        return mu::snap_dir().append("/qemu");
    return QString();
}

QString mp::QemuBaseProcessSpec::apparmor_profile() const
{
    return QString();
}
