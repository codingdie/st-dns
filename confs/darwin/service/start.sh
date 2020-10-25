set -e
scriptDir=$(
  cd $(dirname $0)
  pwd
)
ulimit -n 10000
cp -f ${scriptDir}/com.codingdie.st.dns.plist /Library/LaunchDaemons/com.codingdie.st.dns.plist
launchctl unload /Library/LaunchDaemons/com.codingdie.st.dns.plist
rm -rf /tmp/st-dns.log
rm -rf /tmp/st-dns.error
launchctl load /Library/LaunchDaemons/com.codingdie.st.dns.plist
echo "st-dns service start success!"
