# OpenHAB light switch

This repository encapsulates Cadsoft's Eagle files and firmware for OpenHAB light switch using ESP8266-01 module. There is single variant under switch with relay and single/double variants with relays/triacs for round wiring boxes.

For powering module can be used any 5V/ >=800mA power supply such as USB charger for phone.

Schematics is very simple. For single variant GPIO0 of ESP8266 is used for driving relay/triac and GPIO2 for observation of original switch. Light can be turned on / off using original switch or OpenHAB admin screen. For double variant is used I/O expander PCA9536 which two pins used for driving relays/triacs and other two for switches observation.

For uploading code to ESP8266 is needed Arduino. In source code is needed to specify WIFI AP parameters, network parameters, OpenHAB item name as well as address where OpenHAB server listens. After module startup is on his IP address available HTTP server where some parameters can be redefined. It's useful in case for example changing WIFI password.

For handle lightswitch in OpenHAB you need to modify you sitemap, items and rules file. In items file (/configurations/items/&lt;yourname&gt;.items) is needed to define new item:
```
Switch bathroomMirror "Bathroom (mirror light)"
```
Now you can define rule in rules file (/configurations/rules/&lt;yourname&gt;.rules), which sends request to IP address of module whenever the status changes. Instead of 192.168.128.200 is need to specify IP address of your module.
```
rule bathroomMirror
when
  Item bathroomMirror changed
then 
  if (bathroomMirror.state == ON) {
    sendHttpGetRequest("http://192.168.128.200:80/ON")
  } else {
    sendHttpGetRequest("http://192.168.128.200:80/OFF")
  }
end 
```
Finally can be switch presented in sitemap file (/configurations/sitemaps/&lt;yourname&gt;.sitemap):
```
Switch item=bathroomMirror
```
It's easy and it looks pretty good:

![alt](/images/mobile.png?raw=true)


##Schematics:
### Single relay
![alt](/eagle/lightswitchx1_sch.png?raw=true)
### Double relay
![alt](/eagle/lightswitchx2_sch.png?raw=true)
### Single triac
![alt](/eagle/lightswitchx1_triac_sch.png?raw=true)
### Double triac
![alt](/eagle/lightswitchx2_triac_sch.png?raw=true)

##PCBs:
PCB is realized as single-sided with some wire jumpers. ESP8266 is mounted as always in socket to enable firmware update.
![alt](/eagle/lightswitchx1_brd.png?raw=true)![alt](/eagle/lightswitchx2_brd.png?raw=true)
![alt](/eagle/lightswitchx1_triac_brd.png?raw=true)![alt](/eagle/lightswitchx2_triac_brd.png?raw=true)

###Some project images:

Module with power supply

![alt](/images/2017-01-02%2021.51.43.jpg?raw=true)

Already mounted on its place

![alt](/images/2017-01-07%2013.30.43.jpg?raw=true)
![alt](/images/2017-01-07%2014.20.36.jpg?raw=true)
