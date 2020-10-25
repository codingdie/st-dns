scriptDir=$(
  cd $(dirname $0)
  pwd
)
cp -f ${scriptDir}/st-dns.service /usr/lib/systemd/system/st-dns.service
systemctl daemon-reload 
systemctl enable st-dns 
systemctl reset-failed st-dns.service  
systemctl start st-dns 
sleep 3s
systemctl status st-dns
