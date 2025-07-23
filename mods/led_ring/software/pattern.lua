-- LED patern (list of LED positions to use)
--
-- Format: 
-- { {angle_offset=0, r=0, g=0, b=254}, {angle_offset=180, r=254, g=0, b=0} }
--
-- Values of single item:
--   angle_offset (0..359): 0 = current position
--   r, g, b (0..255): color value for R, G and B
PATTERN = {
    {angle_offset=0, r=0, g=0, b=255},
    {angle_offset=120, r=255, g=0, b=0},
    {angle_offset=240, r=255, g=255, b=255},
}
