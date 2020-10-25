if [ -f "/usr/bin/st-dns" ]; then
  sudo /usr/bin/st-dns -d stop
fi
if [ -f "/usr/local/bin/st-dns" ]; then
  sudo /usr/local/bin/st-dns -d stop
fi
rm -rf /usr/local/bin/st-dns
rm -rf /usr/bin/st-dns
rm -rf /usr/local/etc/st/dns
rm -rf /etc/st/dns
