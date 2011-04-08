# Copyright 2011 (C) Adam Greig
#
# This file is part of habitat.
#
# habitat is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# habitat is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with habitat.  If not, see <http://www.gnu.org/licenses/>.

"""
A sink to upload parsed telemetry data to the spacenear.us tracker
"""

from urllib import urlencode
from urllib2 import urlopen
import logging
from habitat.message_server import SimpleSink, Message

__all__ = ["SpaceNearUsSink"]
logger = logging.getLogger("habitat.spacenearus")

class SpaceNearUsSink(SimpleSink):
    """
    The SpaceNearUsSink forwards on parsed telemetry to the spacenear.us
    tracker (or a copy of it) to use as an alternative frontend.
    """
    def setup(self):
        """We only care for some message types"""
        self.add_type(Message.TELEM)

    def message(self, message):
        """
        Take an incoming message and submit it to spacenear.us
        """
        fields = {
            "vehicle": "payload",
            "lat": "latitude",
            "lon": "longitude",
            "alt": "altitude",
            "heading": "heading",
            "speed": "speed"
        }
        data = {"callsign": message.source.callsign, "pass": "aurora"}
        timestr = "{hour:02d}{minute:02d}{second:02d}"
        data["time"] = timestr.format(**message.data["time"])
        for field in fields:
            try:
                data[field] = message.data[fields[field]]
            except KeyError:
                continue
        qs = urlencode(data, True)
        logger.debug("encoded data: " + qs)
        u = urlopen("http://habhub.org/tracker/track.php?" + qs)
