# Copyright (C) 2011 APEX Team, Daniel Richman
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""
Sensor functions for APEX II, Launch II.

Format:
  $$APEX,<TICKS>,<TIME>,<LAT DDMM.MM>,<LON DDMM.MM>,
  <ALT MMMMM>,<SPEED DDD>,<BEARING DDD>,<SATS DD>,
  <INT_TEMP>,<EXT_TEMP>,<PRESSURE HHH>,<BATT_VOLTS HHH>,
  <IRD_1 HHHH>,<IRD_2 HHHH>,<LIGHT HHHHHHHHH>,
  <RSSI HH>*<CHECKSUM HHHH>

Example string:
  $$APEX,671,19:44:08,5119.4909,-00012.2620,146,000,000,
  04,24.63,23.13,CFD,972,0000,0000,000000005,20*E396

"""

import math

__all__ = ["pressure", "batteryvoltage", "ird", "light", "rssi"]

def _check_length(n, data):
    if len(data) != n:
        return ValueError("Invalid data length")

def pressure(config, data):
    # Pressure: hhh fixed length hexadecimal
    # 12 bit ADC referenced to 5 Volts
    # data = 10 * (((data / 4096) * 5) + 5 * 0.095) / (5 * 0.009)
    # data = (625/2304) * data + (950/9)

    _check_length(3, data)
    data = int(data, 16)
    return int(math.round((float(625 * data) / 2304.0) + (950.0/9.0)))

def batteryvoltage(config, data):
    # Battery: hhh fixed length hexadecimal
    # 12 bit ADC referenced to 5 Volts

    _check_length(3, data)
    data = int(data, 16)
    return float(data * 5) / 4096

def ird(config, data):
    # IRDs: hhhh fixed length hexadecimal
    # Counts in the last 30 seconds

    _check_length(4, data)
    return int(data, 16)

def light(config, data):
    # Light: hhhhhhhhh fixed length
    # Concatenated red (2), green (2), blue (2) and white (2) values, 
    # followed by a multiplier (1).

    _check_length(9, data)
    values = [("red", 2), ("green", 2), ("blue", 2), ("white", 2),
              ("mult", 1)]
    new_data = {}
    for (name, size) in values:
        size = v[1]
        value = data[:size]
        data = data[size:]
        value = int(value, 16)
        new_data[name] = value

    for m in ["red", "green", "blue"]:
        new_data[m] *= new_data["mult"]
    del new_data["mult"]

    return new_data

def rssi(config, data):
    _check_length(2, data)
    data = int(value, 16)
    return int(math.round(float(100 * data) / 256.0))
