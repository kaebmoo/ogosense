import paho.mqtt.client as mqtt
import urllib.request
import urllib.parse
import json
import math


broker_address="mqtt.ogonan.com"
googleURL = "https://docs.google.com/forms/d/e/1FAIpQLSfltw8aPeY1ExAUMRH4glmG0aj6EcQPNzW3d9B89HKJgWh7hQ/formResponse?"
roomEntry = 'entry.2004997247='
idEntry = '&entry.1759843708='

def on_message(client, userdata, message):
    print("message received " ,str(message.payload.decode("utf-8")))
    print("message topic=",message.topic)
    print("message qos=",message.qos)
    print("message retain flag=",message.retain)
    result = json.loads(str(message.payload, encoding='utf-8'))
    print(result)
    print(result["Room"])
    print(result["id"])
    print(googleURL)
    print(roomEntry)
    print(idEntry)
    
    # url_req = googleURL + roomEntry + str(result["Room"]) + idEntry + str(result["id"])
    # url_req = googleURL
    # form_data = urllib.parse.urlencode({roomEntry:result["Room"],idEntry:result["id"]})
    # form_data = form_data.encode('utf-8')
    # print(form_data)
    
    url_req = googleURL + roomEntry + str(result["Room"]) + idEntry + str(result["id"])
    print(url_req)

    req = urllib.request.Request(url = url_req)
    response = urllib.request.urlopen(req, timeout=10) # Make the request
    print(response.getcode()) # A 202 indicates that the server has accepted the request

    
    
print("creating new instance")
client = mqtt.Client("Room 1") #create new instance
client.username_pw_set(username="kaebmoo", password="sealwiththekiss")
client.on_message = on_message

print("connecting to broker")
client.connect(broker_address) #connect to broker

print("Subscribing to topic","/rooms/#")
client.subscribe("/rooms/#")

client.loop_forever()() #start the loop


