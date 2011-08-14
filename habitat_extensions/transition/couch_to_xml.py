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
Connect to CouchDB, read in all the payload configs using payload_config view, then output them as XML documents ready for dl-fldigi.
"""

import sys
import couchdbkit
import elementtree.ElementTree as ET
import xml.dom.minidom

def main():
    if len(sys.argv) != 3:
        print "Usage: {0} <couch uri> <couch db>".format(sys.argv[0])
        sys.exit(0)
    try:
        print dump_xml(sys.argv[1], sys.argv[2])
    except Exception as e:
        print "Error getting XML, stopping: {0}: {1}".format(type(e), e)

def dump_xml(couch_uri, couch_db):
    docs = get_flight_docs(couch_uri, couch_db)
    payloads = PayloadsXML()
    for doc in docs:
        payloads.add_doc(doc)
        try:
            payloads.add_doc(doc)
        except Exception as e:
            print "Error occured processing a payload: {0}: {1}".format(
                type(e), e)
            print "Continuing..."
            continue
    return str(payloads)

def get_flight_docs(couch_uri, couch_db):
    server = couchdbkit.Server(couch_uri)
    db = server[couch_db]
    results = db.view("habitat/payload_config", include_docs=True)
    for result in results:
        yield result["doc"]

class PayloadsXML(object):
    def __init__(self):
        self.tree = ET.Element("payloads")

    def add_doc(self, doc):
        payload = PayloadXML(doc)
        self.tree.append(payload.get_xml())

    def __str__(self):
        string = ET.tostring(self.tree, "utf-8")
        reparsed = xml.dom.minidom.parseString(string)
        return reparsed.toprettyxml(indent="  ", encoding="utf-8")

class PayloadXML(object):
    type_map = {
        "stdtelem.time": "time",
        "stdtelem.coordinate": "decimal",
        "base.ascii_int": "integer",
        "base.ascii_float": "decimal",
        "base.ascii_string": "char"
    }
    def __init__(self, doc):
        self.doc = doc
        self.tree = ET.Element("payload")
        self.callsign = self.doc["payloads"].keys()[0]
        self.payload = self.doc["payloads"][self.callsign]

        name = ET.SubElement(self.tree, 'name')
        name.text = str(self.callsign)

        self.transmission = ET.SubElement(self.tree, 'transmission')
        self._add_basic()
        self._add_txtype()
        self._add_sentence()

    def _add_basic(self):
        frequency = ET.SubElement(self.transmission, 'frequency')
        frequency.text = str(self.payload["radio"]["frequency"])

        mode = ET.SubElement(self.transmission, 'mode')
        mode.text = str(self.payload["radio"]["mode"])

        timings = ET.SubElement(self.transmission, 'timings')
        timings.text = "continuous"

    def _add_txtype(self):
        txtype = ET.SubElement(self.transmission, 'txtype')
        rtty = ET.SubElement(txtype, 'rtty')
        shift = ET.SubElement(rtty, 'shift')
        coding = ET.SubElement(rtty, 'coding')
        baud = ET.SubElement(rtty, 'baud')
        parity = ET.SubElement(rtty, 'parity')
        stop = ET.SubElement(rtty, 'stop')
        shift.text = str(self.payload["telemetry"].get("shift", 300))
        coding.text = str(self.payload["telemetry"].get("encoding", "ascii-8"))
        baud.text = str(self.payload["telemetry"].get("baud", 50))
        parity.text = str(self.payload["telemetry"].get("parity", "none"))
        stop.text = str(self.payload["telemetry"].get("stop", 1.0))

    def _add_sentence(self):
        self.sentence = ET.SubElement(self.transmission, 'sentence')
        s_delimiter = ET.SubElement(self.sentence, 'sentence_delimiter')
        s_delimiter.text = "$$"
        f_delimiter = ET.SubElement(self.sentence, 'field_delimiter')
        f_delimiter.text = ","
        callsign = ET.SubElement(self.sentence, 'callsign')
        callsign.text = str(self.callsign)
        fields = ET.SubElement(self.sentence, 'fields')
        fields.text = str(len(self.payload["sentence"]["fields"]) + 1)

        cl = len(self.callsign)
        self._add_field(seq=1, dbfield="callsign", minsize=cl, maxsize=cl,
            datatype="char")

        seq = 2
        for field in self.payload["sentence"]["fields"]:
            if field["type"] == "stdtelem.coordinate":
                data_format = field["format"]
            else:
                data_format = None
            self._add_field(seq=seq, dbfield=field["name"], minsize=0,
                maxsize=999, datatype=self.type_map[field["type"]],
                format=data_format)
            seq += 1

    def _add_field(self, **kwargs):
        field = ET.SubElement(self.sentence, 'field')
        for key, value in kwargs.iteritems():
            if value:
                node = ET.SubElement(field, key)
                node.text = str(value)

    def get_xml(self):
        return self.tree

if __name__ == "__main__":
    main()