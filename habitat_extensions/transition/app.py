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
This module provides a super lightweight flask application that provides an
easy interface to an Uploader for other processes
"""

import flask
import base64
import json
import time
import couchdbkit
from xml.sax.saxutils import escape as htmlescape
from habitat import uploader
from . import config

# Monkey patch float precision
json.encoder.FLOAT_REPR = lambda o: format(o, '.5f')

app = flask.Flask("habitat_extensions.app")

@app.route("/")
def hello():
    return """
    <html>
    <body>

    <h3>receivers list</h3>
    <p><a href="receivers">JSON</a></p>

    <form action="payload_telemetry" method="POST">
    <h3>payload_telemetry</h3>
    <p>Callsign: <input type="text" name="callsign"></p>
    <p>String (b64): <input type="text" name="string"></p>
    <p>Metadata (json): <input type="text" name="metadata" value="{}"></p>
    <p>Time created (int, POSIX): <input type="text" name="time_created"></p>
    <p><input type="submit" value="GO">
    </form>

    <form action="listener_info" method="POST">
    <h3>listener_info</h3>
    <p>Callsign: <input type="text" name="callsign"></p>
    <p>Data (json): <input type="text" name="data" value="{}"></p>
    <p>Time created (int, POSIX): <input type="text" name="time_created"></p>
    <p><input type="submit" value="GO">
    </form>

    <form action="listener_telemetry" method="POST">
    <h3>listener_telemetry</h3>
    <p>Callsign: <input type="text" name="callsign"></p>
    <p>Data (json): <input type="text" name="data" value="{}"></p>
    <p>Time created (int, POSIX): <input type="text" name="time_created"></p>
    <p><input type="submit" value="GO">
    </form>

    </body>
    </html>
    """

def get_time_created():
    if "time_created" not in flask.request.form:
        return None

    time_created = flask.request.form["time_created"]
    if not time_created:
        return None

    return int(time_created)

@app.route("/payload_telemetry", methods=["POST"])
def payload_telemetry():
    callsign = flask.request.form["callsign"]
    string = base64.b64decode(flask.request.form["string"])
    metadata = json.loads(flask.request.form["metadata"])
    time_created = get_time_created()

    assert callsign and string
    assert isinstance(metadata, dict)

    u = uploader.Uploader(callsign=callsign, **config.COUCH_SETTINGS)
    u.payload_telemetry(string, metadata, time_created)

    return "OK"

@app.route("/listener_info", methods=["POST"])
def listener_info():
    callsign = flask.request.form["callsign"]
    data = json.loads(flask.request.form["data"])
    time_created = get_time_created()

    assert callsign and data
    assert isinstance(data, dict)

    u = uploader.Uploader(callsign=callsign, **config.COUCH_SETTINGS)
    u.listener_info(data, time_created)

    return "OK"

@app.route("/listener_telemetry", methods=["POST"])
def listener_telemetry():
    callsign = flask.request.form["callsign"]
    data = json.loads(flask.request.form["data"])
    time_created = get_time_created()

    assert callsign and data
    assert isinstance(data, dict)

    u = uploader.Uploader(callsign=callsign, **config.COUCH_SETTINGS)
    u.listener_telemetry(data, time_created)

    return "OK"

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

@app.route("/receivers")
def receivers():
    couch_settings = config.COUCH_SETTINGS
    couch_server = couchdbkit.Server(couch_settings["couch_uri"])
    couch_db = couch_server[couch_settings["couch_db"]]

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
