#!/bin/bash

echo "This script is deprecated. Please install the PDA library using Debian packages."
exit 1

set -e
set -u

PDA_VERSION="11.6.7"
USER_NAME=`id -u -n`

getdeps()
{
  apt-get install dkms libpci-dev gcc libtool libtool-bin libkmod-dev linux-headers-`uname -r`
}

install()
{
  rm -rf /tmp/pda-$1*

  cd /tmp
  wget https://github.com/cbm-fles/pda/archive/$1.tar.gz
  tar -xf $1.tar.gz
  rm -f $1.tar.gz

  cd /tmp/pda-$1
  mkdir -p /opt/pda/
  # ./configure --debug=true --prefix=/opt/pda/$1/
  mkdir build; cd build
  cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/opt/pda/$1/ ..
  make install

  cd /tmp/pda-$1/patches/linux_uio/
  make install

  cd /tmp
  rm -rf /tmp/pda-$1
}

patchmodulelist()
{
  if [ -z "`cat /etc/modules | grep uio_pci_dma`" ]
  then
    echo "# Added by pda install script" >> /etc/modules
    echo "uio_pci_dma" >> /etc/modules
    echo "Module added"
  fi
  modprobe uio_pci_dma
}

add_systemgroup()
{
    if ! grep -q -E "^pda" /etc/group; then
        echo "Creating system group 'pda'"
        addgroup --system pda
    fi
}

clean_old_install()
{
    if [ -f /etc/pda_sysfs.sh ]
    then
        echo "Removing old files"
        rm /etc/pda_sysfs.sh
    fi
}

reload_udev()
{
    udevadm control --reload-rules && udevadm trigger
}

if [ "root" = "$USER_NAME" ]
then
    echo "Now running as $USER_NAME"

    getdeps
    clean_old_install
    add_systemgroup
    install $PDA_VERSION
    patchmodulelist
    reload_udev

    echo -e "Please add all intended flib users to group 'pda', e.g., 'usermod -a -G pda <user>'"
else
    echo "Running as user!"
    sudo $0
fi
