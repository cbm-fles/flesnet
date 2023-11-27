#!/bin/bash

set -e
set -u

PDA_VERSION="11.6.7"
PDA_KERNEL_VERSION="0.9.0"
USER_NAME=`id -u -n`

install_kernel()
{
  cd /tmp
  wget https://github.com/cbm-fles/pda/releases/download/$1/pda-kernel-dkms_$2-1_amd64.deb
  apt-get install ./pda-kernel-dkms_$2-1_amd64.deb
}

install_lib()
{
  cd /tmp
  wget https://github.com/cbm-fles/pda/releases/download/$1/libpda4_$1-1_amd64.deb
  apt-get install ./libpda4_$1-1_amd64.deb
}

# Default install is done via Debian packages. Functions blow are kept for reference.

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

    install_kernel $PDA_VERSION $PDA_KERNEL_VERSION
    install_lib $PDA_VERSION
else
    echo "Running as user!"
    sudo $0
fi
