Smart Power Plug

Main parts of the code;
- Wifi management 
  Initially the code will search for previously stored Wifi credentials in EEPROM and tries to connect with it. If it cannot, 
  it will go into Wifi-Server mode running on 192.168.4.1. Go there and provide new Wifi SSID and password. Opeon restart, it will connect
  to new Wifi network.
- Calculates Power using EmonLib Library. Get it from https://www.arduinolibraries.info/libraries/emon-lib
  and sends to PubNub channel `channel_chart`. Current transformer used is zmct103c. available from;
  https://artofcircuits.com/product/zmct103c-5a-current-transformer
- Listens for input switch button and sends ON/OFF trigger to PubNub on channel `channel_SPP`
- Listens for uncoming data from PubNub on channel `channel_SPP` and controls the device accordingly.

Note that the PUBNUB Rest API also sends Authorization key as `SPP`. Which means PubNub Access manager should be enabled on you PubNub
account. Provide your publish and subscribe keys too.

See it in action 
https://www.youtube.com/watch?v=9gTDlD1whdw

Circuit diagram:

![](Circuit_diagram.bmp)

