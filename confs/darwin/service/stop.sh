set -e
scriptDir=$(cd $(dirname $0); pwd)
if [ -f "/Library/LaunchDaemons/com.codingdie.st.dns.plist" ];then
  launchctl unload /Library/LaunchDaemons/com.codingdie.st.dns.plist
  rm -rf  /Library/LaunchDaemons/com.codingdie.st.dns.plist
fi
if [ -f "${scriptDir}/com.codingdie.st.dns.plist" ];then
  launchctl unload ${scriptDir}/com.codingdie.st.dns.plist
fi
echo "st-dns service stop success!"
