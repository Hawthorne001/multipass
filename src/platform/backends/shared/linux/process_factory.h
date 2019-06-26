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

#ifndef MULTIPASS_PROCESS_FACTORY_H
#define MULTIPASS_PROCESS_FACTORY_H

#include <memory>

#include "process_spec.h"
#include <multipass/singleton.h>

namespace multipass
{
class Process;

class ProcessFactory : public Singleton<ProcessFactory>
{
public:
    ProcessFactory(const Singleton<ProcessFactory>::PrivatePass&);

    virtual std::unique_ptr<Process> create_process(std::unique_ptr<ProcessSpec>&& process_spec) const;
};

} // namespace multipass

#endif // MULTIPASS_PROCESS_FACTORY_H
