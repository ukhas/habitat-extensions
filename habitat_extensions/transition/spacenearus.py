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
from . import config

__all__ = ["SpaceNearUsSink"]
logger = logging.getLogger("habitat_extensions.transition.spacenearus")

class SpaceNearUs:
    """
    The SpaceNearUs daemon forwards on parsed telemetry to the spacenear.us
    tracker (or a copy of it) to use as an alternative frontend.
    """
    def __init__(self, couch_settings, tracker):
        self.tracker = tracker
        server = couchdbkit.Server(couch_settings["couch_uri"])
        self.db = server[couch_settings["couch_db"]]

    def run(self):
        """
        Start a continuous connection to CouchDB's _changes feed, watching for
        new unparsed telemetry.
        """
        update_seq = self.db.info()["update_seq"]

        consumer = couchdbkit.Consumer(self.db)
        consumer.wait(self.couch_callback, filter="habitat/spacenear",
                      since=update_seq, heartbeat=1000, include_docs=True)

    def couch_callback(self, result):
        """
        Take a payload_telemetry doc and submit it to spacenear.us
        """

        doc_id = result["id"]
        doc = result["doc"]

        logger.debug("Considering doc " + doc_id)

        if doc["type"] == "payload_telemetry":
            self.payload_telemetry(doc)
        elif doc["type"] == "listener_telemetry":
            self.listener_telemetry(doc)

    def payload_telemetry(self, doc):
        fields = {
            "vehicle": "payload",
            "lat": "latitude",
            "lon": "longitude",
            "alt": "altitude",
            "heading": "heading",
            "speed": "speed"
        }

        if "data" not in doc:
            logger.warning("ignoring doc due to no data")
            return

        data = doc["data"]

        if not isinstance(data, dict):
            logger.warning("ignoring doc where data is not a dict")
            return

        # XXX: Crude hack!
        receivers = doc["receivers"].items()
        receivers.sort(key=lambda x: x[1]["time_uploaded"])
        last_callsign = receivers[-1][0]

        params = {}

        timestr = "{hour:02d}{minute:02d}{second:02d}"
        params["time"] = timestr.format(**data["time"])

        self._copy_fields(fields, data, params)
        params["pass"] = "aurora"
        self._post_to_track(params)

    def listener_telemetry(self, doc):
        fields = {
            "vehicle": "callsign",
            "lat": "latitude",
            "lon": "longitude"
        }

        if "data" not in doc or "callsign" not in doc:
            return

        data = doc["data"]
        callsign = data["callsign"]

        if "chase" not in callsign:
            return

        if not isinstance(data, dict):
            logger.warning("ignoring doc where data is not a dict")
            return

        params = {}

        timestr = "{hour:02d}:{minute:02d}:{second:02d}"
        params["time"] = timestr.format(**data["time"])

        self._copy_fields(fields, data, params)
        params["pass"] = "aurora"
        self._post_to_track(params)

    def _copy_fields(self, fields, data, params):
        for (tgt, src) in fields.items():
            try:
                params[tgt] = data[src]
            except KeyError:
                continue

    def _post_to_track(self, params):
        qs = urlencode(params, True)
        logger.debug("encoded data: " + qs)
        u = urlopen(self.tracker.format(qs))

def main():
    logging.basicConfig(level=logging.DEBUG)
    # See habitat.main
    logging.getLogger("restkit").setLevel(logging.WARNING)
    logger.debug("Starting up")
    s = SpaceNearUs(config.COUCH_SETTINGS, config.TRACKER)
    s.run()
