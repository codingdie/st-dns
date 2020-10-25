#!/bin/sh
set -e
scriptDir=$(
  cd $(dirname $0)
  pwd
)
cp -f "$scriptDir"/st-dns /etc/init.d/st-dns
/etc/init.d/st-dns  enable
/etc/init.d/st-dns  start
echo "st-dns service start success!"
