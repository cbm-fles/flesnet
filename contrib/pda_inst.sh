#!/bin/bash

PDA_VERSION="99.99.99"
USER_NAME=`id -u -n`

getdeps()
{
  apt-get install libpci-dev gcc libtool linux-headers-`uname -r`
}

install()
{
  cd /tmp

  rm pda-$1.tar.gz
  wget https://compeng.uni-frankfurt.de/fileadmin/Images/pda/pda-$1.tar.gz

  tar -xf pda-$1.tar.gz
  cd pda-$1
  mkdir /opt/pda/
  ./configure --debug=true --prefix=/opt/pda/$1/
  make install

  cd patches/linux_uio/
  make install
}

patchudev()
{
  cat /etc/pda_sysfs.sh | sed 's|root:pda|root:users|' > /etc/pda_sysfs.tmp
  mv /etc/pda_sysfs.tmp /etc/pda_sysfs.sh
}

patchmodulelist()
{
  ADD_NEEDED=`cat /etc/modules | grep uio_pci_dma`
  if [ -z "$ADD_NEEDED" ]
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

    echo -en '\E[5m'
    echo -e "$CTRL NOW YOU MUST EXTEND (!!) YOUR PATH ENVIRONMENT VARIABLE WITH /opt/pda/$PDA_VERSION/bin/"
    echo -e "$CTRL PLEASE ADD (OR REPLACE IF THE VERSION CHANGED) THE FOLLOWING TO ~/.profile/ :"  
    echo -e "$CTRL export PATH=\"/opt/pda/$PDA_VERSION/bin/:\$PATH\" "
    echo -e ""
    echo -e "$CTRL YOU ALSO MUST RESTART UDEV. A REBOOT WILL DO THE JOB IF IT'S UNCERTAIN WHAT TO DO!"
    echo -en '\E[25m'
else
    echo "Running as user!"
    sudo $0
fi
