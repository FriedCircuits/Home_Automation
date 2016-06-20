#!/usr/bin/env python
#JeeNode to MQTT gateway
#Author: William Garrido
#	 Mobilewill.us
#	 FriedCircuits.us
#Assumes JeeNodes are running the room node sketch
#License: CC-BY-SA
#Example for decoding from http://jeelabs.net/boards/7/topics/2603

import serial, sys
import paho.mqtt.client as paho

#Print out debugging to console
debug = False

#Serial port JeeNode receiver is on
ser= serial.Serial('/dev/ttyUSB0', 57600)

#Function to print connection results
def on_connect(client, userdata, flags, rc):
	if debug == True:
		print("CONNACK received with code %d." % (rc))

#Function to print out publish message id 
def on_publish(client, userdata, mid):
	if debug == True:
		print("mid: "+str(mid))

#Setup MQTT client object and connect
client = paho.Client(client_id="jeenode")
client.on_connect = on_connect
client.on_publish = on_publish
client.connect("localhost", 1883)
#Start background thread to manage MQTT processes including reconnecting
client.loop_start()

#Continuously check serial port for data
#Once recieved split the data check if Ack 
#and if not shift the data into seperate variables and publish based on node ID. 
while 1 :
	msg = ser.readline()
	if len(msg) > 0:
		if debug == True:
			sys.stdout.write(msg)
        	data = msg.split(" ")
        	if data[2].isdigit == True:
			a = int(data[2]) 
		      	b = int(data[3])
        		c = int(data[4])
        		d = int(data[5])       
	       		node_id = str(int(data[1]) & 0x1f)
    			msg_seq = str(int(data[3]))
        		light       = a 
	        	motion      = b & 1
        		humidity    = b >> 1
        		temperature = str(((256 * (d&3) + c) ^ 512) - 512)
	        	battery     = (d >> 2) & 1
	        	temperature = temperature[0:2] + '.' + temperature[-1]
			if debug == True:
				print data[1]
				print light
				print motion
				print humidity
				print temperature 
				print battery
			mqtt = "jeenode/" + str(data[1]) + "/"
			data = mqtt + "light"
			(rc, mid) = client.publish(data, str(light))
			data = mqtt + "temp"
			(rc, mid) = client.publish(data, str(temperature))
			data = mqtt + "motion"
			(rc, mid) = client.publish(data, str(motion))
			data = mqtt + "humidity"
			(rc, mid) = client.publish(data, str(humidity))
			data = mqtt + "battery"
			(rc, mid) = client.publish(data, str(battery))

	
		
