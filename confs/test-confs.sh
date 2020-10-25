if [ "$1" != "" ]; then
  rm -rf service
  ln -s $1/service service
fi