#!/bin/bash

# Script: fixup-ssl-libs-rpath.sh
# The Brew SSL libraries have a problem: they're trickier to relocate as
# they're not compiled with rpath enabled.
# Ref: https://github.com/Homebrew/homebrew-core/issues/3219#issuecomment-235763791
#
# To make them work in a relocatable package, need to edit the paths to these libs in all
# the binaries to use rpaths.

set -euo pipefail

SSL_PATH="/usr/local/opt/openssl@1.1"
SSL_LIBS="ssl crypto"
BINARIES="bin/multipass bin/multipassd bin/multipass.gui.app/Contents/MacOS/multipass.gui bin/sshfs_server lib/libssh.dylib"

if [ $# -ne 1 ]; then
    echo "Argument required"
    exit 1
fi

CMAKE_BINARY_DIR=$1
TEMP_DIR="${CMAKE_BINARY_DIR}/install-ssh-libs"
mkdir -p "${TEMP_DIR}"

# Determine all the rpaths
RPATH_CHANGES=()
for lib in ${SSL_LIBS}; do
    lib_path="$( greadlink -f ${SSL_PATH}/lib/lib${lib}.dylib )"
    lib_name="$( basename ${lib_path} )"
    RPATH_CHANGES+=("-change")
    RPATH_CHANGES+=("${lib_path}")
    RPATH_CHANGES+=("@rpath/${lib_name}")

    RPATH_CHANGES+=("-change")
    RPATH_CHANGES+=("${SSL_PATH}/lib/${lib_name}")
    RPATH_CHANGES+=("@rpath/${lib_name}")
done

# Install and modify all the libraries
for lib in ${SSL_LIBS}; do
    lib_path="$( greadlink -f ${SSL_PATH}/lib/lib${lib}.dylib )"
    lib_name="$( basename ${lib_path} )"
    target_path="${TEMP_DIR}/${lib_name}"
    install -m 644 "${lib_path}" "${target_path}"
    install_name_tool "${RPATH_CHANGES[@]}" "${target_path}"
    chmod 444 "${target_path}"
done

# Edit the binaries to point to these newly edited libs
for binary in ${BINARIES}; do
    install_name_tool -add_rpath "${TEMP_DIR}" "${CMAKE_BINARY_DIR}/${binary}"
    install_name_tool "${RPATH_CHANGES[@]}" "${CMAKE_BINARY_DIR}/${binary}"
done
