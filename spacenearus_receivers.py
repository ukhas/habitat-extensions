# Copyright 2011 (C) Daniel Richman
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
A flask web app that provides a list of listeners online in the last 24
hours for spacenearus.
"""

import flask
import couchdbkit
import time
import json
from xml.sax.saxutils import escape as htmlescape

# Monkey patch float precision
from json import encoder
encoder.FLOAT_REPR = lambda o: format(o, '.5f')

app = flask.Flask("habitat_extensions.spacenearus_receivers")

couch_settings = {
    "couch_uri": "http://localhost:5984",
    "couch_db": "habitat"
}

couch_server = couchdbkit.Server(couch_settings["couch_uri"])
couch_db = couch_server[couch_settings["couch_db"]]

def listener_filter(item):
    (callsign, data) = item

    if "chase" in callsign:
        return False

    if "telemetry" not in data:
        return False

    if "info" not in data:
        return False

    return True

HTML_DESCRIPTION = """
<font size="-2"><BR>
<B>Radio: </B>{radio_safe}<BR>
<B>Antenna: </B>{antenna_safe}<BR>
<B>Last Contact: </B>{tdiff_hours} hours ago<BR>
</font>
"""

def listener_map(item):
    (callsign, data) = item

    try:
        info = data["info"]["data"]
        telemetry = data["telemetry"]["data"]

        tdiff = int(time.time()) - data["latest"]
        tdiff_hours = tdiff / 3600

        info["radio_safe"] = htmlescape(info["radio"])
        info["antenna_safe"] = htmlescape(info["antenna"])
        info["tdiff_hours"] = tdiff_hours

        return {
            "name": callsign,
            "lat": telemetry["latitude"],
            "lon": telemetry["longitude"],
            "alt": telemetry["altitude"],
            "description": HTML_DESCRIPTION.format(**info)
        }
    except KeyError:
        raise
        return None

@app.route("/")
def receivers():
    listeners = {}

    last_week = int(time.time() - (7 * 24 * 60 * 60))
    startkey = [last_week, None]
    o = {"startkey": startkey, "include_docs": True}

    info = couch_db.view("habitat/listener_info", **o)

    for result in info:
        (time_uploaded, callsign) = result["key"]
        doc = result["doc"]

        l = {"info": doc, "latest": time_uploaded}

        if callsign not in listeners:
            listeners[callsign] = l
        else:
            listeners[callsign].update(l)

    telemetry = couch_db.view("habitat/listener_telemetry", **o)

    for result in telemetry:
        (time_uploaded, callsign) = result["key"]
        doc = result["doc"]

        l = {"telemetry": doc, "latest": time_uploaded}

        if callsign not in listeners:
            listeners[callsign] = l
        else:
            listeners[callsign].update(l)

    # Covert dict to list. Filter, map, then remove any that failed the map.
    listeners = filter(listener_filter, listeners.items())
    listeners = map(listener_map, listeners)
    listeners = filter(None, listeners)

    response = flask.make_response(json.dumps(listeners))
    response.headers["Content-type"] = "application/json"
    return response

if __name__ == "__main__":
    app.run()
