#!/bin/sh
set -e
scriptDir=$(
  cd $(dirname $0)
  pwd
)
cp -f "$scriptDir"/st-dns /etc/init.d/st-dns
/etc/init.d/st-dns  disable
/etc/init.d/st-dns  stop
echo "st-dns service stop success!"