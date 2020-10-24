set -e
scriptDir=$(
  cd $(dirname $0)
  pwd
)
unameStr=$(uname -a | tr 'A-Z' 'a-z')
if [[ "$unameStr" =~ "ubuntu" ]]; then
  service st-dns stop
  update-rc.d -f st-dns remove
fi

echo "st-dns service stop success!"
