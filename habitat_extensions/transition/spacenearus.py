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
import traceback
import threading
import Queue
import copy
import json
from . import config

__all__ = ["SpaceNearUs"]
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

        self.recent_doc_ids = []
        self.recent_doc_receivers = {}

        self.upload_queue = Queue.Queue()
        self.recent_lock = threading.RLock()

    def run(self):
        """
        Start a continuous connection to CouchDB's _changes feed, watching for
        new unparsed telemetry.
        """

        for i in xrange(5):
            t = threading.Thread(target=self.uploader_thread)
            t.daemon = True
            t.start()

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
            "speed": "speed",
            "temp_inside": "temp_int",
            "seq": "count"
        }

        if "data" not in doc:
            logger.warning("ignoring doc due to no data")
            return

        data = doc["data"]

        if not isinstance(data, dict):
            logger.warning("ignoring doc where data is not a dict")
            return

        with self.recent_lock:
            doc_id = doc["_id"]
            if doc_id in self.recent_doc_receivers:
                new_receivers = set(doc["receivers"].keys()) - \
                                set(self.recent_doc_receivers[doc_id])
                self.recent_doc_receivers[doc_id] = doc["receivers"].keys()
                if len(new_receivers) == 0:
                    logger.warning("ignoring doc due to no new receivers")
                    return
            else:
                # WARNING: the uploader will re-upload every single callsign
                # if it encounters a doc it had forgotten.
                if len(self.recent_doc_ids) > 30:
                    remove_doc_ids = self.recent_doc_ids[:-30]
                    self.recent_doc_ids = self.recent_doc_ids[-30:]
                    for i in remove_doc_ids:
                        del self.recent_doc_receivers[i]
                self.recent_doc_ids.append(doc_id)
                self.recent_doc_receivers[doc_id] = doc["receivers"].keys()
                new_receivers = doc["receivers"].keys()

        params = {}

        self._copy_fields(fields, data, params)

        try:
            timestr = "{hour:02d}{minute:02d}{second:02d}"
            params["time"] = timestr.format(**data["time"])
        except KeyError:
            pass

        unused_data = {}
        used_keys = set(fields.values() + ["time"])
        unused_keys = set(data.keys()) - used_keys

        for key in unused_keys:
            if not key.startswith("_"):
                unused_data[key] = data[key]

        params["data"] = json.dumps(unused_data)

        params["pass"] = "aurora"

        for callsign in new_receivers:
            p = copy.deepcopy(params)
            p["callsign"] = callsign
            self.upload_queue.put(p)

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

        if "chase" not in callsign.lower():
            return

        if not isinstance(data, dict):
            logger.warning("ignoring doc where data is not a dict")
            return

        params = {}

        self._copy_fields(fields, data, params)

        try:
            timestr = "{hour:02d}:{minute:02d}:{second:02d}"
            params["time"] = timestr.format(**data["time"])
        except KeyError:
            pass

        params["pass"] = "aurora"
        self.upload_queue.put(params)

    def uploader_thread(self):
        while True:
            params = self.upload_queue.get()
            self._post_to_track(params)
            self.upload_queue.task_done()

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
