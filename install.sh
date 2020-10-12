a=`uname  -a`

b="Darwin"
c="centos"
d="ubuntu"

if [[ $a =~ $b ]];then
    echo "mac"
else
    echo "linux"
fi