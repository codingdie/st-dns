if [ -f "/usr/bin/st-dns" ];then
    sudo /usr/bin/st-dns service stop
fi;
if [ -f "/usr/local/bin/st-dns" ];then
    sudo /usr/local/bin/st-dns service stop
fi;
sudo rm -rf /usr/local/bin/st-dns
sudo rm -rf /usr/bin/st-dns
sudo rm -rf /usr/local/etc/st/dns
sudo rm -rf /etc/st/dns