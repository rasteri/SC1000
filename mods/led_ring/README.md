# LED Ring mod for SC

This is an optional modification to add a LED ring to display the current platter position, like having a sticker on vinyl.

[![Demo Video](https://img.youtube.com/vi/FGX4gD_Zo5w/0.jpg)](https://www.youtube.com/watch?v=FGX4gD_Zo5w)


Required hardware:

* **ESP8266** (~$5)

  Used a dev board for easier development, but plain ESP chip should work too.

* **LED ring** (WS2812 aka Neopixel, ~$10) 

  The more LEDs the ring has, the more precise the angle can be displayed. Tested with 24 LEDs (15°/LED).
  
  Depending on your case: maximum diameter of ~10cm to fit into the default case.

  This mod will also work with an LED strip, it doesn't have to be an actual ring.

  [Supported protocols](https://nodemcu.readthedocs.io/en/release/modules/ws2812/): WS2812, WS2812b, APA104, SK6812 (RGB or RGBW)


## Steps

* Flash the NodeMCU firmware, see [./firmware/README](./firmware/README.md)

* Upload the LUA software, see [./software/README](./software/README.md)

* Connect the hardware

    ```
    ┌──────────┐
    │ ESP8266  │             ┌───────────────┐
    ├──────────┤             │  WS2812 (LED) │
    ¦       .. ¦             ├───────────────┤
    ┤       D4 ├-------------┤ IN            │
    ┤     3.3V ├-------------┤ VCC           │
    ┤      GND ├-------------┤ GND           │
    ¦       .. ¦             └───────────────┘
    ┤     RxD0 ├----┐      ┌───────────────┐
    ┤     TxD0 ├    |      │  J7 (SC1000)  │
    ┤      GND ├----+---┐  ├───────────────┤
    ┤     3.3V ├-┐  │   └--┤GND       3.3V ├-┐
    └──────────┘ |  |      ┤UART3 RX       ├ |
                 |  └------┤UART3 TX       ├ |
                 |         ┤               ├ |
                 |         ┤               ├ |
                 |         └───────────────┘ |
                 └---------------------------┘
    ```


* Enable the mod in `settings.txt`

  ```ini
  # 1 to enable LED ring mod, default 0 (disabled)
  ledringenabled=1
  ```

* [Optional] Configure the LED color and pattern

  See [software/README](./software/README.md)
