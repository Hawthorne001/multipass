/*
 * Copyright (C) 2022 Canonical, Ltd.
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

#ifndef MULTIPASS_VM_MOUNT_H
#define MULTIPASS_VM_MOUNT_H

#include <multipass/id_mappings.h>

#include <string>

namespace multipass
{
struct VMMount
{
    enum class MountType : int
    {
        SSHFS = 0,
        Performance = 1
    };

    std::string source_path;
    id_mappings gid_mappings;
    id_mappings uid_mappings;
    MountType mount_type;
};

inline bool operator==(const VMMount& a, const VMMount& b)
{
    return std::tie(a.source_path, a.gid_mappings, a.uid_mappings) ==
           std::tie(b.source_path, b.gid_mappings, b.uid_mappings);
}
} // namespace multipass

#endif // MULTIPASS_VM_MOUNT_H
