# Copyright 2012 (C) Daniel Richman; GNU GPL 3

import time
import earthmaths
import couchdbkit
import logging
import serial

logger = logging.getLogger("habitat_extensions.autotracker")

class AutoTracker(object):
    def __init__(self, config, daemon_name):
        cfg = config[daemon_name]
        self.limits = cfg["limits"]
        loc = cfg["location"]
        self.listener = (loc["latitude"], loc["longitude"], loc["altitude"])
        self.track_callsign = cfg["track_callsign"]
        self.period = cfg["period"]
        self.old_tgt = None

        self.couch_server = couchdbkit.Server(config["couch_uri"])
        self.db = self.couch_server[config["couch_db"]]

        # Check Connection
        try:
            self.get_position()
        except ValueError:
            pass

        ser = cfg["serial"]
        self.serial_format = ser["format"]
        self.open_serial(ser)

    def open_serial(self, settings):
        self.serial = serial.Serial(settings["file"], settings["baud"])
        logger.info("Opened serial: {file} {baud}".format(**settings))

    def calc_p(self, balloon):
        p = earthmaths.position_info(self.listener, balloon)
        p["bearing"] = self.wrap_bearing(p["bearing"])
        p["elevation"] = self.wrap_bearing(p["elevation"])
        self.check_range(p)
        return p

    def wrap_bearing(self, bearing):
        if bearing < 0:
            bearing += 360
        return bearing

    def wrap_elevation(self, elevation):
        return elevation

    def check_range(self, aim):
        for key in ["bearing", "elevation"]:
            limits = self.limits[key]
            if aim[key] < limits["min"] or aim[key] > limits["max"]:
                return False
        return True

    def get_position(self):
        r = self.db.view("habitat/payload_telemetry",
                         startkey=[self.track_callsign, "end"],
                         descending=True, limit=1, include_docs=True)

        r = list(r)
        if len(r) != 1:
            raise ValueError("Could not get balloon position")
        r = r[0]

        t = r["key"][1]
        if abs(time.time() - t) > 300:
            raise ValueError("Position is more than 5 minutes old")

        d = r["doc"]["data"]
        balloon = (d["latitude"], d["longitude"], d["altitude"])

        logger.debug("Balloon is at " + repr(balloon))

        return balloon

    def aim_at(self, tgt):
        logger.info("Aiming at " + repr(tgt))
        f = self.serial_format

        if f == "simple":
            l = "{0} {1}\n".format(int(tgt[0]), int(tgt[1]))
        else:
            raise NotImplementedError("Unimplemented format " + f)

        logger.debug("Serial write: " + repr(l))
        self.serial.write(l)

    def run(self):
        while True:
            try:
                balloon = self.get_position()
                p = self.calc_p(balloon)
                tgt = (p["bearing"], p["elevation"])
                if tgt != self.old_tgt:
                    self.aim_at(tgt)
                else:
                    logger.debug("Not re-aiming (position unchanged)")
                self.old_tgt = tgt
            except (KeyboardInterrupt, SystemExit) as e:
                raise
            except ValueError as e:
                logger.info("Not aiming: " + str(e))
            except:
                logger.exception("Exception in main loop")

            time.sleep(self.period)
