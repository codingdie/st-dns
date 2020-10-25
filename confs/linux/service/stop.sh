#!/bin/sh
scriptDir=$(
  cd $(dirname $0)
  pwd
)
cp -f ${scriptDir}/st-dns.service /usr/lib/systemd/system/st-dns.service
systemctl daemon-reload  
systemctl stop st-dns  
systemctl disable st-dns  
sleep 3s
systemctl status st-dns 
