set -e
scriptDir=$(cd $(dirname $0); pwd)
update-rc.d -f st-dns remove
echo "st-dns service stop success!"
