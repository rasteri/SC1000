led_buffer:fill(0, 0, 0); 
local degree_per_led = 360 / LED_COUNT

function _angle_to_led_pos(angle)  -- returns index of LED (1..LED_COUNT) by angle in degree
  angle = angle + math.ceil( -angle / 360 ) * 360 -- normalize angle to 0..359
  pos = math.ceil((angle+1) / degree_per_led)
  return pos
end

function r(angle) 
  -- Set new rotation position (angle= 0..360°).
  -- Triggered via direct RPC from serial port.
  led_buffer:fill(0, 0, 0);
  for i = 1, #PATTERN do
    led_pos = _angle_to_led_pos(angle + PATTERN[i].angle_offset)
    led_buffer:set(led_pos, PATTERN[i].g, PATTERN[i].r, PATTERN[i].b)
  end
  ws2812.write(led_buffer)
end


if DEBUG == 1 then
  print "[DEBUG] Demo mode with fake rotation signal."
  local debug_tmr = tmr.create(); local debug_angle = 0; local debug_angle_step = 5
  debug_tmr:register(20, tmr.ALARM_AUTO, function (t) debug_angle = debug_angle + debug_angle_step; debug_angle = debug_angle + math.ceil( -debug_angle / 360 ) * 360;  r(debug_angle) end)
  debug_tmr:start()
end
