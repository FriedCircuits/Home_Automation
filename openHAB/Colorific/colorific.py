#OpenHAB Colorific RGB Blub Control
# Created 10/24/2015 Author William Garrido
# FriedCircuits - https://github.com/friedcirucits
# CC-BY-SA
#Usage
# Arg 1 = Blub Address
# Arg 2 = Red
# Arg 3 = Green
# Arg 4 = Blue
# Ref: https://github.com/adafruit/BLE_Colorific/blob/master/colorific.py

#import colorsys
#import math
import sys
import pexpect

# Get bulb address from command parameters.
if len(sys.argv) != 5:
        print 'Error must specify bulb address as parameter!'
        print 'Usage: sudo python colorific.py <bulb address>'
        print 'Example: sudo python colorific.py XX:XX:XX:XX:XX:XX'
        sys.exit(1)

bulb = sys.argv[1]
#hue = float(sys.argv[2])
#sat = float(sys.argv[3])
#bright = float(sys.argv[4])
r = int(sys.argv[2])
g = int(sys.argv[3])
b = int(sys.argv[4])

try:
        # Compute 24-bit RGB color based on HSV values.
        #r, g, b = map(lambda x: int(x*255.0), colorsys.hsv_to_rgb(hue, sat, bright))
        cmd = 'gatttool -i hci0 -b {0} --char-write-req -a 0x0028 -n 58010301ff00{1:02X}{2:02X}{3:02X}'.format(bulb,r,g,b)
        gatt = pexpect.spawn(cmd, timeout=10)
        gatt.expect('Characteristic value was written successfully')
except Exception:
        pass
