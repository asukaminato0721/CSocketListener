#!/usr/bin/env bash
set -eux -o pipefail
TEMPDIR=/tmp/wolfram-engine-install
BASEURL=https://archive.raspberrypi.org/debian/pool/main/w/wolfram-engine
WOLFRAMENGINEDEB=wolfram-engine_14.1.0+202408191410_arm64.deb

SUDO=''
if [ ${EUID} != 0 ]; then
    SUDO='sudo'
fi

pushd "${PWD}" > /dev/null
rm -fr ${TEMPDIR}
mkdir -p ${TEMPDIR}
cd ${TEMPDIR}

echo -n "Downloading Wolfram Engine... "
wget -q ${BASEURL}/${WOLFRAMENGINEDEB} -O ${WOLFRAMENGINEDEB}
echo "done."

echo -n "Installing Wolfram Engine... "
${SUDO} apt install ${TEMPDIR}/${WOLFRAMENGINEDEB}
echo "done."

popd > /dev/null
echo -n "Cleaning up... "
rm -fr ${TEMPDIR}
echo "done."

# from official sh
