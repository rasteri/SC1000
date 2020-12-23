# Firmware for the ESP8266

Using LUA based NodeMCU firmware: https://nodemcu.readthedocs.io/en/release/


## Download firmware

Use the provided `./nodemcu-1.5.4.1-final-14-modules-2019-12-31-14-52-16-float__adc_file_gpio_http_i2c_net_node_rtctime_tmr_u8g_uart_wifi_ws2812_tls.bin` firmware.

Or build and/or download your custom firmware:
* Docs: https://nodemcu.readthedocs.io/en/release/build/
* Cloud build: https://nodemcu-build.com/


This project requires the following modules: `file gpio net node tmr uart ws2812`


## Flashing firmware

Easiest way is to use the python-based [esptool](https://github.com/espressif/esptool).

`pip install esptool`

`esptool.py --port /dev/ttyUSB0 write_flash 0x00000 ./nodemcu-1.5.4.1-final-14-modules-2019-12-31-14-52-16-float__adc_file_gpio_http_i2c_net_node_rtctime_tmr_u8g_uart_wifi_ws2812_tls.bin`
(Adjust the USB port to your actual one)


During next boot after successful flashing, the ESP should display a message similar to:
```
NodeMCU custom build by frightanic.com
	branch: 1.5.4.1-final
	commit: b9436bdfa452c098d5cb42a352ca124c80b91b25
	SSL: true
	modules: adc,file,gpio,http,i2c,net,node,rtctime,tmr,u8g,uart,wifi,ws2812,tls
 build created on 2019-12-31 14:51
 powered by Lua 5.1.4 on SDK 1.5.4.1(39cb9a32)
```