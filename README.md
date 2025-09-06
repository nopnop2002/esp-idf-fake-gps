# esp-idf-fake-gps
Fake GPS using ESP-IDF.

![Image](https://github.com/user-attachments/assets/59925f3a-3425-40c8-8a0f-778154664f84)

Windows application for NMEA Generator Utility is published [here](http://4river.a.la9.jp/gps/#004).   
This tool generates NMEA sentences for GPS receivers and outputs them to serial or TCP ports.   
This works great.   
When using TCP, this app acts as a TCP server.   
With this tool, you can go wherever you want in no time.   

Use ESP32S2/S3 to transfer NMEA sentence received by TCP to USB.   

```
+----------------+     +------------+                        +---------------+
|    REAL GPS    |     |  UART-USB  |                        | Windows/Linux |
|                |UART |  Converter |USB                     |               |
|                |---->|RX          |----------------------->|               |
|                |     |            |                        |               |
+----------------+     +------------+                        +---------------+

+----------------+     +------------+                        +---------------+
| NMEA Generator |     | ESP32S2/S3 |                        | Windows/Linux |
| as FAKE GPS    |TCP  |            |USB                     |               |
|                |====>|            |----------------------->|               |
|                |     |            |                        |               |
+----------------+     +------------+                        +---------------+
```

In addition, it can provide NMEA sentences to two PCs.   
```
+----------------+     +------------+                        +---------------+
| NMEA Generator |     | ESP32S2/S3 |                        | Windows/Linux |
| as FAKE GPS    |TCP  |            |USB                     |               |
|                |====>|            |----------------------->|               |
|                |     |            |                        |               |
+----------------+     |            |                        +---------------+
                       |            |
                       |            |     +------------+     +---------------+
                       |            |     |  UART-USB  |     | Windows/Linux |
                       |            |UART |  Converter |USB  |               |
                       |          TX|---->|RX          |---->|               |
                       |            |     |            |     |               |
                       +------------+     +------------+     +---------------+
```

ESPs that don't support USB use UART.   
```
+----------------+     +------------+                        +---------------+
|    REAL GPS    |     |  UART-USB  |                        | Windows/Linux |
|                |UART |  Converter |USB                     |               |
|                |---->|RX          |----------------------->|               |
|                |     |            |                        |               |
+----------------+     +------------+                        +---------------+

+----------------+     +------------+     +------------+     +---------------+
| NMEA Generator |     |   ESP32    |     |  UART-USB  |     | Windows/Linux |
| as FAKE GPS    |TCP  |            |UART |  Converter |USB  |               |
|                |====>|          TX|---->|RX          |---->|               |
|                |     |            |     |            |     |               |
+----------------+     +------------+     +------------+     +---------------+
```


# Software requirements   
ESP-IDF V5.0 or later.   
ESP-IDF V4.4 release branch reached EOL in July 2024.   

# Hardware requirements   

1.Windows PC   
Windows applications can be downloaded from [here](http://4river.a.la9.jp/gps/#004).   
Just extract the ZIP file and run it.   
This applications was made by Japan, but NMEAGEN217.zip contains English documents, so there is no problem.   
Any questions about the NMEA Generator Utility should be directed to the author, not me.   

2.USB Connector is required when using USB.   
I used this USB Mini femail:   
![usb-connector](https://user-images.githubusercontent.com/6020549/124848149-3714ba00-dfd7-11eb-8344-8b120790c5c5.JPG)

```
ESP32-S2/S3 BOARD          USB CONNECTOR
                           +--+
                           | || VCC
    [GPIO 19]    --------> | || D-
    [GPIO 20]    --------> | || D+
    [  GND  ]    --------> | || GND
                           +--+
```

# Installation

```Shell
git clone https://github.com/nopnop2002/esp-idf-fake-gps
cd esp-idf-fake-gps
idf.py set-target {esp32/esp32s2/esp32s3/esp32c2/esp32c3}
idf.py menuconfig
idf.py flash
```


# Configuration
You need to know the IP address of your Windows machine beforehand.   
Windows 10 is set not to respond to ping by default.   
You cannot use the ping command to find out.   
Use the ipconfig command from the command prompt.   

![config-top](https://user-images.githubusercontent.com/6020549/223572219-507cad77-ffa8-46eb-bbe3-28c57a873935.jpg)


### Configuration for USB communication(ESP32S2/S3 only)

![config-usb](https://user-images.githubusercontent.com/6020549/223572229-d55a438f-f8a5-4d34-822a-30112f0fcd8a.jpg)


### Configuration for UART communication(All ESP32)

![config-uart](https://user-images.githubusercontent.com/6020549/223572224-08f50cfb-4b83-4fff-a07d-dc8c0ce2e8f4.jpg)


# How to use
- Connect ESP32 to WindowsPC using USB or UART.   

- Build this firmware and start ESP32.   

- Open NMEA Generator Utility.   

- Select TCP for the output port.   
![Image](https://github.com/user-attachments/assets/ddb61fad-7535-4e29-bcaf-bb95e6ea32cb)

- Check Rep.   
 This will repeat the NMEA transmission forever.  
![Image](https://github.com/user-attachments/assets/20417a3b-8382-443e-87b9-19bce5574706)

- Open port.   
 NMEA Generator acts as a TCP server.   
![Image](https://github.com/user-attachments/assets/302b6878-42d5-4efe-972b-14d482a9e84c)

- NMEA transmission start.    
 NMEA Generator shows your current location.   
![Image](https://github.com/user-attachments/assets/48090c91-bf61-440f-afb4-9564cab240fd)
![Image](https://github.com/user-attachments/assets/983afb21-42d6-4d42-8aae-97c2bbc46957)

- Open serial terminal on your host.   
 This will add GPS to your host.   
![screen-linux](https://user-images.githubusercontent.com/6020549/223572696-0ab3cff0-61ae-4683-8146-9d57d8377819.jpg)
![screen-windows](https://user-images.githubusercontent.com/6020549/223572698-b32cc723-6d7d-43c3-83b9-84c818b105b9.jpg)


# Read NMEA with gpsd   
```
$ sudo adduser $USER dialout

$ sudo apt-get install gpsd gpsd-clients

$ sudo vi /etc/default/gpsd
START_DAEMON="true"
USBAUTO="true"
DEVICES="/dev/ttyUSB0"
GPSD_OPTIONS="-n"


$ sudo service gpsd stop
$ sudo service gpsd start
$ sudo systemctl -l --no-pager status gpsd
Åú gpsd.service - GPS (Global Positioning System) Daemon
   Loaded: loaded (/lib/systemd/system/gpsd.service; indirect; vendor preset: enabled)
   Active: active (running) since Tue 2023-03-07 16:18:40 JST; 2s ago
  Process: 5234 ExecStart=/usr/sbin/gpsd $GPSD_OPTIONS $DEVICES (code=exited, status=0/SUCCESS)
 Main PID: 5235 (gpsd)
    Tasks: 1 (limit: 4411)
   CGroup: /system.slice/gpsd.service
           mq5235 /usr/sbin/gpsd -n /dev/ttyACM0
```

# View with gpsmon
```
$ gpsmon
Enter q to quit.   
```

![gpsmon_2023-03-07_16-28-05](https://user-images.githubusercontent.com/6020549/223572752-6a29087e-0eb1-4cc9-9503-902cee070607.png)

# View with cgps

```
$ cgps
Enter q to quit.   
```

![cgps_2023-03-07_16-32-00](https://user-images.githubusercontent.com/6020549/223572794-fae5f644-c873-4726-ac39-259950fc5ee7.png)

# View with FoxtrotGPS
```
$ sudo apt install foxtrotgps

$ sudo vi /etc/default/gpsd
#GPSD_OPTIONS="-n"
GPSD_OPTIONS="-n -G"

$ sudo service gpsd stop
$ sudo service gpsd start
$ sudo systemctl -l --no-pager status gpsd
Åú gpsd.service - GPS (Global Positioning System) Daemon
   Loaded: loaded (/lib/systemd/system/gpsd.service; indirect; vendor preset: enabled)
   Active: active (running) since Tue 2023-03-07 16:18:40 JST; 2s ago
  Process: 5234 ExecStart=/usr/sbin/gpsd $GPSD_OPTIONS $DEVICES (code=exited, status=0/SUCCESS)
 Main PID: 5235 (gpsd)
    Tasks: 1 (limit: 4411)
   CGroup: /system.slice/gpsd.service
           mq5235 /usr/sbin/gpsd -n /dev/ttyACM0

$ foxtrotgps
```
It flew over Manhattan's Central Park at 100km/h.   

![foxtrotgps_2023-03-07_16-49-09](https://user-images.githubusercontent.com/6020549/223572849-58d6b6a5-c986-4c40-bd1b-258775729472.png)
![foxtrotgps_2023-03-07_17-04-07](https://user-images.githubusercontent.com/6020549/223572857-12ff77fc-3158-46f6-b25a-18a3667068a8.png)

# Using FoxtrotGPS from remote
```
$ sudo vi /lib/systemd/system/gpsd.socket
#ListenStream=127.0.0.1:2947
ListenStream=0.0.0.0:2947

$ sudo service gpsd stop
$ sudo service gpsd start
$ sudo systemctl -l --no-pager status gpsd
Åú gpsd.service - GPS (Global Positioning System) Daemon
   Loaded: loaded (/lib/systemd/system/gpsd.service; indirect; vendor preset: enabled)
   Active: active (running) since Tue 2023-03-07 16:18:40 JST; 2s ago
  Process: 5234 ExecStart=/usr/sbin/gpsd $GPSD_OPTIONS $DEVICES (code=exited, status=0/SUCCESS)
 Main PID: 5235 (gpsd)
    Tasks: 1 (limit: 4411)
   CGroup: /system.slice/gpsd.service
           mq5235 /usr/sbin/gpsd -n /dev/ttyACM0

# From remote
$ foxtrotgps <IP address of the server where gpsd is running>:2947
```

# View with ROS   
You can use nmea_serial_driver and rviz_satellite.   
The nmea serial driver APT package has a bug and does not work properly.   
Must be installed from source.   
I live in Nagoya, Japan, but my ROS robot is currently moving through Manhattan's Central Park.   

![rviz_satellite_newyork_2023-03-07_14-12-39](https://user-images.githubusercontent.com/6020549/223572894-728b9b82-8473-4ba4-9805-c69039573497.png)

