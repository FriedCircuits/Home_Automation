#!/bin/bash
#Created by William Garrido
#Nov 5th 2015
#http://mobilewill.us
#CC-BY-SA
#Wifi/Bluetooth proximity detection
#Orginal version was for DomotiGA and now for openHAB
#openHab version  also based on work from
#https://code.google.com/p/openhab-samples/wiki/Tricks#Use_cheap_bluetooth_dongles_on_remote_PCs_to_detect_your_phone/w

#Phones to look for over Wifi and Bluetooth
#Device Index Array
DEVICENAME[0]="DeviceName1"
DEVICEIP[0]="192.168.X.X"
DEVICEMAC[0]="XX:XX:XX:XX:XX:XX"
DEVICEITEM[0]="device_Name1"

DEVICENAME[1]="DeviceName2"
DEVICEIP[1]="192.168.X.X"
DEVICEMAC[1]="XX:XX:XX:XX:XX:XX"
DEVICEITEM[1]="device_Name2"

occupiedItem="occupiedState"
occupiedFlag=false

openHABIP="192.168.X.X:8080"

#echo ${DEVICENAME[0]}
#echo ${DEVICENAME[1]}

while true
do
  for ((i=0;i<${#DEVICENAME[@]};i++))
  do
	#echo ${DEVICENAME[$i]}
  	DEVICES=`hcitool name ${DEVICEMAC[$i]}`

	  if [[ $DEVICES = *${DEVICENAME[$i]}* ]]
	  then
	    #echo "if true"
	    curl --max-time 2 --connect-timeout 2 --header "Content-Type: text/plain" --request PUT --data "ON" http://$openHABIP/rest/items/${DEVICEITEM[$i]}/state
	    occupiedFlag=true
	  else
	    #echo "if else"
	    curl --max-time 2 --connect-timeout 2 --header "Content-Type: text/plain" --request PUT --data "OFF" http://$openHABIP/rest/items/${DEVICEITEM[$i]}/state
	  fi
  done

  if [[ $occupiedFlag == true ]]
  then
    #echo "true"
    curl --max-time 2 --connect-timeout 2 --header "Content-Type: text/plain" --request PUT --data "ON" http://$openHABIP/rest/items/$occupiedItem/state
  else
    #echo "false"
    curl --max-time 2 --connect-timeout 2 --header "Content-Type: text/plain" --request PUT --data "OFF" http://$openHABIP/rest/items/$occupiedItem/state
  fi

  occupiedFlag=false
  sleep 30
done
