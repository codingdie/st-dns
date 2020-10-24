set -e
scriptDir=$(cd $(dirname $0); pwd)
cp -f ${scriptDir}/st-dns /etc/init.d/
chmod +x /etc/init.d/st-dns
update-rc.d -f st-dns defaults
echo "st-dns service start success!"
