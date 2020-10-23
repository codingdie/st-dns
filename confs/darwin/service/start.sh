set -e
scriptDir=$(cd $(dirname $0); pwd)
cp -f  ${scriptDir}/com.codingdie.st.dns.plist /Library/LaunchDaemons/com.codingdie.st.dns.plist
launchctl unload /Library/LaunchDaemons/com.codingdie.st.dns.plist
launchctl load  /Library/LaunchDaemons/com.codingdie.st.dns.plist
echo "st-dns service start success!"
