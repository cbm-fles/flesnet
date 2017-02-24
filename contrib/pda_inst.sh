#!/bin/bash

set -e
set -u

PDA_VERSION="11.3.7"
USER_NAME=`id -u -n`

getdeps()
{
  apt-get install libpci-dev gcc libtool libtool-bin libkmod-dev linux-headers-`uname -r`
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
  ./configure --debug=true --prefix=/opt/pda/$1/
  make install

  cd /tmp/pda-$1/patches/linux_uio/
  make install

  cd /tmp
  rm -rf /tmp/pda-$1
}

patchudev()
{
  cat /etc/pda_sysfs.sh | sed 's|UIO_GROUP="pda"|UIO_GROUP="sudo"|' > /etc/pda_sysfs.tmp
  mv /etc/pda_sysfs.tmp /etc/pda_sysfs.sh
  chmod a+x /etc/pda_sysfs.sh
}

patchmodulelist()
{
  if [ -z "`cat /etc/modules | grep uio_pci_dma`" ]
  then
    echo "uio_pci_dma" >> /etc/modules
    echo "Module added"
  fi
  modprobe uio_pci_dma
}



CTRL="\e[1m\e[32m"

if [ "root" = "$USER_NAME" ]
then
    echo "Now running as $USER_NAME"

    getdeps
    install $PDA_VERSION
    patchudev
    patchmodulelist

    echo -en '\e[5m'
    echo -e "$CTRL YOU MUST RESTART UDEV NOW. A REBOOT WILL DO THE JOB IF IT'S UNCERTAIN WHAT TO DO!"
    echo -en '\e[25m'
else
    echo "Running as user!"
    sudo $0
fi
