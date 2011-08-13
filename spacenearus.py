# Copyright 2011 (C) Adam Greig, Daniel Richman
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
A daemon that uploads parsed telemetry data to the spacenear.us tracker
"""

from urllib import urlencode
from urllib2 import urlopen
import logging
import couchdbkit

__all__ = ["SpaceNearUsSink"]
logger = logging.getLogger("habitat.spacenearus")

class SpaceNearUs:
    """
    The SpaceNearUs daemon forwards on parsed telemetry to the spacenear.us
    tracker (or a copy of it) to use as an alternative frontend.
    """
    def __init__(self, config):
        self.config = config
        self.couch_server = couchdbkit.Server(self.config["couch_uri"])
        self.db = self.couch_server[self.config["couch_db"]]

    def run(self):
        """
        Start a continuous connection to CouchDB's _changes feed, watching for
        new unparsed telemetry.
        """
        update_seq = self.db.info()["update_seq"]

        consumer = couchdbkit.Consumer(self.db)
        consumer.wait(self.couch_callback, filter="habitat/parsed",
                      since=update_seq, heartbeat=1000, include_docs=True)

    def couch_callback(self, result):
        """
        Take a payload_telemetry doc and submit it to spacenear.us
        """

        doc = result["doc"]

        fields = {
            "vehicle": "payload",
            "lat": "latitude",
            "lon": "longitude",
            "alt": "altitude",
            "heading": "heading",
            "speed": "speed"
        }

        data = doc["data"]

        # XXX: Crude hack!
        last_callsign = doc["receivers"].keys()[-1]
        params = {"callsign": last_callsign, "pass": "aurora"}

        timestr = "{hour:02d}{minute:02d}{second:02d}"
        params["time"] = timestr.format(**data["time"])

        for (tgt, src) in fields.items():
            try:
                params[tgt] = data[src]
            except KeyError:
                continue

        qs = urlencode(data, True)
        logger.debug("encoded data: " + qs)
        u = urlopen("{tracker}?{qs}".format(tracker=self.config["tracker"],
                                            qs=qs))

if __name__ == "__main__":
    # TODO: once the parser's main and stuff is refactored, use that
    config = {
        "couch_uri": "http://localhost:5984/",
        "couch_db": "habitat",
        "tracker": "http://habhub.org/tracker/track.php"
    }

    logging.basicConfig(level=logging.DEBUG)
    # See habitat.main
    logging.getLogger("restkit").setLevel(logging.WARNING)
    logger.debug("Starting up")
    s = SpaceNearUs(config)
    s.run()
