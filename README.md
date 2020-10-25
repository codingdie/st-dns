## What is st-dns?  
st-dns is a smart local dns server which support to config multi UDP/TCP/DOT DNS server chain. every dns server support limit to resolve a specific area IP, so as you config proper, it can smart resolve the IP you want.  

### Dependencies
- [CMake](https://cmake.org/) >= 3.7.2
- [Boost](http://www.boost.org/) >= 1.66.0 (system,filesystem,thread)
- [OpenSSL](https://www.openssl.org/) >= 1.1.0

### How to install?  
1. download the source code
2. execute ```install.sh```
3. the st-dns will install to /usr/local/bin, the default configs will install to /usr/local/etc/st/dns

### How to run?  
#### 1. Run Direct  
*  `st-dns`search config in /etc/st/dns or /usr/local/etc/st/dns
*  `st-dns  -c /xxx/xxx`  specific the config folder
#### 2. Run As Service(Recommend)
*  `sudo st-dns -d start`  
*  `sudo st-dns -d stop`  

### How to config?  
```
{
  "ip": "127.0.0.1", #local IP to bind
  "port": "53",      #local port to bind
  "servers": [       #dns server chain
    {
      "type": "UDP",        #dns server type supprot UDP/TCP/TCP_SSL
      "ip": "192.168.31.1", #dns server IP
      "port": 53,           #dns server port
      "area": "LAN",        #dns server area
      "only_area_ip": true  #force the server only resolve the eare ip, 
                            #the area defined in configFolder/../area-ips, 
                            #the filename is area name, you can custom it;
    },
    {
      "type": "TCP_SSL",
      "ip": "223.5.5.5",
      "port": 853,
      "area": "CN",
      "only_area_ip": true
    },
    {
      "type": "TCP_SSL",
      "ip": "8.8.8.8",
      "port": 853,
      "area": "!CN",
      "only_area_ip": true
    }
  ],
  "dns_cache_expire": 3600, #the dns record cache time
}
```

     

