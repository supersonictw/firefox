#!/usr/bin/env bash

set -v -e -x

# Parse for the -t option.
m=x64
for i in "$@"; do
    case "$i" in
        -t|--target) m= ;;
        --target=*) m="${i#*=}" ;;
        *) [[ -z "$m" ]] && m="$i" ;;
    esac
done
[[ "$m" == "ia32" ]] && m=x86
source "$(dirname "$0")/setup.sh"

# Install GYP.
pushd gyp
python -m virtualenv test-env
test-env/Scripts/python setup.py install
test-env/Scripts/python -m pip install --upgrade pip
test-env/Scripts/pip install --upgrade setuptools
# Fool GYP.
touch "${VSPATH}/VC/vcvarsall.bat"
export GYP_MSVS_OVERRIDE_PATH="${VSPATH}"
export GYP_MSVS_VERSION=2015
popd

export PATH="${PATH}:${PWD}/ninja/bin:${PWD}/gyp/test-env/Scripts"

# Clone NSPR.
hg_clone https://hg.mozilla.org/projects/nspr nspr default

# Build with gyp.
./nss/build.sh -g -v --enable-libpkix "$@"

# Package.
7z a public/build/dist.7z dist
