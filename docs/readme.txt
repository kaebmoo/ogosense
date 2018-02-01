Prerequisites 

Download the Arduino IDE : https://www.arduino.cc/en/Main/Software
Get started in Arduino : https://wiki.wemos.cc/tutorials:get_started:get_started_in_arduino

ThingSpeak (example : https://www.mathworks.com/help/thingspeak/read-and-post-temperature-data.html)
WiFiManager https://github.com/tzapu/WiFiManager 
Arduino library for the WEMOS SHT30 Shiled : https://github.com/wemos/WEMOS_SHT3x_Arduino_Library
Arduino library for TM1637 (LED Driver) : https://github.com/avishorp/TM1637
Arduino Timer library : https://github.com/JChristensen/Timer
NETPIE client library for ESP8266 : https://github.com/netpieio/microgear-esp8266-arduino
Wemos D1 mini Pro : https://wiki.wemos.cc/products:d1:d1_mini_pro 

เครื่องวัดอุณหภูมิและความชื้น
ความสามารถเบื้องต้น

บันทึกได้ละเอียด 1 วินาที ถึง 24 ชั่วโมง
ส่งข้อมูลผ่านอินเทอร์เน็ตได้
ส่งข้อมูลขึ้น thingspeak หรือ netpie ได้ เพื่อดูข้อมูล โดยสามารถดูได้ทุกเวลา และสามารถดูผลแบบกราฟได้
บันทึกข้อมูลลง SDCARD ได้สูงสุด 32GB (FAT32) (เป็นทางเลือก)
เลือกหน่วยวัดอุณหภูมิ เป็น องศาเซลเซียส หรือ องศาฟาเรนไฮซ์ หรือทั้งคู่ได้
เชื่อมต่อผ่าน wifi ได้
เครื่องวัดและบันทึกค่าอุณหภูมิและความชื้นสัมพัทธ์ ผ่าน Wifi โดยจะเก็บข้อมูลในระบบ Cloud ทำให้สามารถดูข้อมูลได้ทุกที่ ทุกเวลาในโลก
ไม่ต้องเดินสายสัญญาณ ทำให้สามารถติดตั้งเครื่องไว้ตรงไหนก็ได้ ที่สัญญาณ wifi ไปถึง 
ใช้ Sensor ของ Sensirion SHT30 ให้ค่าผิดพลาด +- 3 สำหรับ % RH และ +- 0.3 สำหรับอุณหภูมิองศาเซลเซียส (ในช่วง 0-65 องศาเซลเซียส)
ช่วงการทำงาน -40 - 125 องศาเซลเซียส (https://www.sensirion.com/en/environmental-sensors/humidity-sensors/digital-humidity-sensors-for-various-applications/)
แสดงผลผ่าน LED 7-segment ได้ (เป็นทางเลือก)
มี LED แสดงสถานะการทำงาน
ตั้งค่า wifi ด้วย browser ผ่านทางโทรศัพท์ iPhone, Android ได้
ส่งข้อมูลผ่าน MQTT ได้

เครื่องวัดอุณหภูมิและความชื้น พร้อมระบบควบคุม
นอกจากความสามารถเบื้องต้นแล้ว ยังเพิ่มการควบคุมผ่าน Relay เพื่อใช้ควบคุมอุปกรณ์ไฟฟ้า 
ความสามารถการทำงานของ Relay (NO, NC) 220VAC 10A 
สามารถตั้งค่าการทำงานของ Relay ด้วยการกำหนดค่า (set point) อุณหภูมิ หรือ ความชื้น หรือทั้งคู่ เพื่อที่จะให้ Relay ทำงาน เช่น กำหนดให้ Relay ทำงานที่อุณหภูมิ 25 องศาเซลเซียส
กำหนดช่วงของการหยุดทำงานของ Relay ได้ ทั้งด้านสูงและด้านต่ำจากจุดกำหนดค่า (set point) เช่น ช่วงการทำงาน = 10 ถ้าจุดกำหนดค่าอยู่ที่ 25 จะหมายถึงจุดหยุดทำงานจะอยู่ที่ 25+10 = 35 (จุดหยุดทำงาน)  หรือ 25-10 = 15 (จุดหยุดทำงาน) เป็นต้น
แสดงสถานะการทำงานของ Relay ผ่านทาง Internet ได้