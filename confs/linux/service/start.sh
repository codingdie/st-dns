set -e
scriptDir=$(
  cd $(dirname $0)
  pwd
)
cp -f ${scriptDir}/st-dns /etc/init.d/
chmod +x /etc/init.d/st-dns
unamestr=$(uname -a | tr 'A-Z' 'a-z')
type="unkown"
if [ "$(echo "$unamestr" | grep ubuntu)" != "" ]; then
  update-rc.d -f st-dns defaults
  service st-dns start
  systemctl daemon-reload
  type="ubuntu"
fi
echo "st-dns service start success in $type!"
