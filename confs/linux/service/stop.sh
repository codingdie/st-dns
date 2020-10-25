set -e
scriptDir=$(
  cd $(dirname $0)
  pwd
)
unamestr=$(uname -a | tr 'A-Z' 'a-z')
type=""
if [ "$(echo "$unamestr" | grep ubuntu)" != "" ]; then
  service st-dns stop
  systemctl daemon-reload
  update-rc.d -f st-dns remove
  systemctl daemon-reload
  type="ubuntu"
fi
echo "st-dns service stop success in $type!"
