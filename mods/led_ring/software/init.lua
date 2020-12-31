local boot_delay = 5 -- in sec; allows recovery when script goes crazy

function init_real()
    tmr.stop(0)
    dofile("config.lua")
    dofile("pattern.lua")

    -- Init ws2812 LEDs
    print("[INIT] WS2812 ("..LED_COUNT.." LEDs) ..")
    ws2812.init()
    led_buffer = ws2812.newBuffer(LED_COUNT, BYTES_PER_LED)
    led_buffer:fill(0, 0, 5) -- all LEDs soft blue to show it's initialized
    ws2812.write(led_buffer)
    dofile("led.lua")    

    -- Init serial port to receive RPC calls
    local uart_id = 0 -- only UART 0 is capable of receiving data
    local echo = 1
    uart.setup(uart_id, 115200, 8, uart.PARITY_NONE, uart.STOPBITS_1, echo)
    print ("[UART] Config", uart.getconfig(uart_id))

    wifi.setmode(wifi.NULLMODE)
end

tmr.alarm(0, boot_delay*1000, 0, function() init_real() end)
print("Boot delay ("..boot_delay.."s) ..")
