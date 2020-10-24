set -e
scriptDir=$(
  cd $(dirname $0)
  pwd
)
cp -f ${scriptDir}/st-dns /etc/init.d/
chmod +x /etc/init.d/st-dns
unamestr=$(uname -a | tr 'A-Z' 'a-z')
if [[ "$unamestr" =~ "ubuntu" ]]; then
  update-rc.d -f st-dns defaults
  service st-dns start
fi
echo "st-dns service start success!"
