#!/usr/bin/env python
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
Read in an XML payload configuration doc and upload it to CouchDB as a sandbox
flight document, used to aid transition from the old dl system to habitat.
"""

import sys
if len(sys.argv) != 4:
    n = sys.argv[0]
    print "Usage: {0} <payload xml file> <couch URI> <couch db>".format(n)
    sys.exit(1)

import couchdbkit
import os.path
import time
import pprint
import elementtree.ElementTree as ET
from xml.parsers.expat import ExpatError

try:
    tree = ET.parse(sys.argv[1])
except (IOError, ExpatError, AttributeError) as e:
    print "Could not parse XML: {0}".format(e)
    sys.exit(1)

callsign = tree.findtext("sentence/callsign")
if not callsign:
    print "Could not find a callsign in the document."
    sys.exit(1)

doc = {
    "type": "flight",
    "name": os.path.basename(sys.argv[1]).split(".")[0],
    "start": int(time.time()),
    "end": "sandbox",
    "metadata": {
        "imported_from_xml": True
    },
    "payloads": {
        callsign : {
            "radio": {
                "frequency": tree.findtext("frequency"),
                "mode": tree.findtext("mode")
            },
            "telemetry": {
                "modulation": "rtty",
                "shift": tree.findtext("txtype/rtty/shift"),
                "encoding": tree.findtext("txtype/rtty/coding"),
                "baud": tree.findtext("txtype/rtty/baud")
            },
            "sentence": {
                "protocol": "UKHAS",
                "checksum": "crc16-ccitt",
                "payload": callsign,
                "fields": []
            }
        }
    }
}

type_map = {
    "fixed": "base.ascii_float",
    "char": "base.ascii_string",
    "integer": "base.ascii_int",
    "time": "stdtelem.time",
    "decimal": "base.ascii_float",
    "custom": "base.ascii_string",
    "custom_data": "base.ascii_string"
}

server = couchdbkit.Server(sys.argv[2])
db = server[sys.argv[3]]

for field in tree.getiterator("field"):
    if field.findtext("dbfield") == "callsign":
        continue

    new_field = {
        "name": field.findtext("dbfield"),
        "type": type_map.get(field.findtext("datatype"), "base.ascii_string")
    }
    
    if field.findtext("dbfield") in ("latitude", "longitude"):
        if field.findtext("format"):
            new_field["format"] = field.findtext("format")
        else:
            new_field["format"] = "dd.dddd"

    doc["payloads"][callsign]["sentence"]["fields"].append(new_field)

print "Saving document:"
pprint.pprint(doc)
db.save_doc(doc)
