/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#include <multipass/utils.h>

#include <sstream>

#include <QDir>
#include <QFileInfo>
#include <QProcess>

namespace mp = multipass;

namespace
{
auto quote_for(const std::string& arg)
{
    return arg.find('\'') == std::string::npos ? '\'' : '"';
}
}

QDir mp::utils::base_dir(const QString& path)
{
    QFileInfo info{path};
    return info.absoluteDir();
}

bool mp::utils::valid_memory_value(const QString& mem_string)
{
    QRegExp matcher("\\d+((K|M|G)(B){0,1}){0,1}$");

    return matcher.exactMatch(mem_string);
}

bool mp::utils::valid_hostname(const QString& name_string)
{
    QRegExp matcher("^([a-zA-Z]|[a-zA-Z][a-zA-Z0-9\\-]*[a-zA-Z0-9])");

    return matcher.exactMatch(name_string);
}

bool mp::utils::invalid_target_path(const QString& target_path)
{
    QString sanitized_path{QDir::cleanPath(target_path)};
    QRegExp matcher("/+|/+(dev|proc|sys)(/.*)*|/+home/ubuntu/*");

    return matcher.exactMatch(sanitized_path);
}

std::string mp::utils::to_cmd(const std::vector<std::string>& args, QuoteType quote_type)
{
    std::stringstream cmd;
    for (auto const& arg : args)
    {
        if (quote_type == QuoteType::no_quotes)
        {
            cmd << arg << " ";
        }
        else
        {
            const auto quote = quote_for(arg);
            cmd << quote << arg << quote << " ";
        }
    }
    return cmd.str();
}

bool mp::utils::run_cmd(QString cmd, QStringList args)
{
    QProcess proc;
    proc.setProgram(cmd);
    proc.setArguments(args);

    proc.start();
    proc.waitForFinished();

    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}
