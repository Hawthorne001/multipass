/*
 * Copyright (C) 2018-2021 Canonical, Ltd.
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

#include <multipass/client_cert_store.h>
#include <multipass/constants.h>
#include <multipass/file_ops.h>
#include <multipass/utils.h>

#include <QDir>
#include <QFile>
#include <QSaveFile>

#include <stdexcept>

namespace mp = multipass;

namespace
{
constexpr auto chain_name = "multipass_client_certs.pem";

auto load_certs_from_file(const multipass::Path& cert_dir)
{
    QList<QSslCertificate> certs;
    auto path = QDir(cert_dir).filePath(chain_name);

    QFile cert_file{path};

    if (cert_file.exists())
    {
        cert_file.open(QFile::ReadOnly);
        certs = QSslCertificate::fromDevice(&cert_file);
    }

    return certs;
}
} // namespace

mp::ClientCertStore::ClientCertStore(const multipass::Path& data_dir)
    : cert_dir{QDir(data_dir).filePath(mp::registered_certs_dir)},
      authenticated_client_certs{load_certs_from_file(cert_dir)}
{
}

void mp::ClientCertStore::add_cert(const std::string& pem_cert)
{
    QSslCertificate cert(QByteArray::fromStdString(pem_cert));

    if (cert.isNull())
        throw std::runtime_error("invalid certificate data");

    if (verify_cert(cert))
        return;

    QDir dir{mp::utils::make_dir(cert_dir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)};
    QSaveFile file{dir.filePath(chain_name)};
    if (!MP_FILEOPS.open(file, QIODevice::WriteOnly))
        throw std::runtime_error("failed to create file to store certificate");

    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);

    // QIODevice::Append is not supported in QSaveFile, so must write out all of the
    // existing clients certs each time.
    for (const auto& saved_cert : authenticated_client_certs)
    {
        MP_FILEOPS.write(file, saved_cert.toPem());
    }

    MP_FILEOPS.write(file, cert.toPem());

    if (!MP_FILEOPS.commit(file))
        throw std::runtime_error("failed to write certificate");

    authenticated_client_certs.push_back(cert);
}

std::string mp::ClientCertStore::PEM_cert_chain() const
{
    QDir dir{cert_dir};
    auto path = dir.filePath(chain_name);
    if (QFile::exists(path))
        return mp::utils::contents_of(path);
    return {};
}

bool mp::ClientCertStore::verify_cert(const std::string& pem_cert)
{
    return verify_cert(QSslCertificate(QByteArray::fromStdString(pem_cert)));
}

bool mp::ClientCertStore::verify_cert(const QSslCertificate& cert)
{
    return authenticated_client_certs.contains(cert);
}

bool mp::ClientCertStore::empty()
{
    return authenticated_client_certs.empty();
}
