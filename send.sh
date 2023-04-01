#!/bin/bash

MAC="50:02:91:E4:16:25"
Broadcast="255.255.255.255"
PortNumber="9"
echo -e $(echo $(printf 'f%.0s' {1..12}; printf "$(echo $MAC | sed 's/://g')%.0s" {1..16}) | sed -e 's/../\\x&/g') | nc -w1 -u -b $Broadcast $PortNumber
