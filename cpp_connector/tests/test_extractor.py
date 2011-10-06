import subprocess
import json
import fcntl
import os
import errno

# Mox-esque class that is 'equal' to another string if the value it is
# initialised is contained in that string; used to avoid writing out the
# whole of check_status()
class EqualIfIn:
    def __init__(self, test):
        self.test = test
    def __eq__(self, rhs):
        return isinstance(rhs, basestring) and self.test.lower() in rhs.lower()
    def __repr__(self):
        return "<EqIn " + repr(self.test) + ">"

class Proxy:
    def __init__(self, command):
        self.closed = False
        self.p = subprocess.Popen(command, stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE)

    def _write(self, command):
        print ">>", repr(command)
        self.p.stdin.write(json.dumps(command))
        self.p.stdin.write("\n")

    def _read(self):
        line = self.p.stdout.readline()
        assert line and line.endswith("\n")
        obj = json.loads(line)

        print "<<", repr(obj)
        return obj

    def check_quiet(self):
        fd = self.p.stdout.fileno()
        fl = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)

        try:
            line = self.p.stdout.readline()
            print ">>", line
        except IOError as e:
            if e.errno != errno.EAGAIN:
                raise
        else:
            # line is '' when EOF; '\n' is an empty line
            if line != '':
                raise AssertionError("expected IOError(EAGAIN), not " +
                                     repr(line))

        fcntl.fcntl(fd, fcntl.F_SETFL, fl)

    def add(self, name):
        self._write(["add", name])

    def skipped(self, num):
        self._write(["skipped", num])

    def push(self, data):
        for char in data:
            self._write(["push", char])

    def set_current_payload(self, value):
        self._write(["set_current_payload", value])

    def check(self, match):
        obj = self._read()
        assert len(obj) >= len(match)
        assert obj[:len(match)] == match

    def _check_type(self, name, arg):
        if arg:
            self.check([name, arg])
        else:
            self.check([name])

    def check_status(self, message=None):
        if message:
            message = EqualIfIn(message)
        self._check_type("status", message)

    def check_data(self, data=None):
        self._check_type("data", data)

    def check_upload(self, data=None):
        self._check_type("upload", data)

    def __del__(self):
        if not self.closed:
            self.close(check=False)

    def close(self, check=True):
        self.closed = True
        self.p.stdin.close()
        ret = self.p.wait()

        if check:
            self.check_quiet()
            assert ret == 0

class TestExtractorManager:
    def setup(self):
        self.extr = Proxy("tests/extractor")

    def teardown(self):
        self.extr.close()

    def test_management(self):
        self.extr.push("$$this,is,a,string\n")
        self.extr.check_quiet()

        self.extr.add("UKHASExtractor")
        self.extr.push("$$this,is,a,string\n")
        self.extr.check_status("start delim")
        self.extr.check_upload()
        self.extr.check_status("extracted")
        self.extr.check_status("parse failed")
        self.extr.check_data()

class TestUKHASExtractor:
    def setup(self):
        self.extr = Proxy("tests/extractor")
        self.extr.add("UKHASExtractor")

    def teardown(self):
        self.extr.close()

    def test_finds_start_delimiter(self):
        self.extr.push("$")
        self.extr.check_quiet()
        self.extr.push("$")
        self.extr.check_status("start delim")

    def test_extracts(self):
        string = "$$a,simple,test*00\n"
        self.extr.check_quiet()
        self.extr.push(string)
        self.extr.check_status("start delim")
        self.extr.check_upload(string)
        self.extr.check_status("extracted")
        self.extr.check_status("parse failed")
        self.extr.check_data({"_sentence": string})

    def test_can_restart(self):
        self.extr.push("this is some garbage just to mess things up")
        self.extr.check_quiet()
        self.extr.push("$$")
        self.extr.check_status("start delim")

        self.extr.push("garbage: after seeing the delimiter, we lose signal.")
        self.extr.push("some extra $s to con$fuse it $")
        self.extr.push("$$")
        self.extr.check_status("start delim")
        self.extr.check_status("start delim")
        self.extr.check_quiet()
        self.extr.push("helloworld")
        self.extr.check_quiet()
        self.extr.push("\n")
        self.extr.check_upload("$$helloworld\n")
        self.extr.check_status("extracted")
        self.extr.check_status("parse failed")
        self.extr.check_data()

    def test_gives_up_after_1k(self):
        self.extr.push("$$")
        self.extr.check_status("start delim")

        self.extr.push("a" * 1022)
        self.extr.check_status("giving up")
        self.extr.check_quiet()

        # Should have given up, so a \n won't cause an upload:
        self.extr.push("\n")
        self.extr.check_quiet()

        self.test_extracts()

    def test_gives_up_after_16skipped(self):
        self.extr.push("$$")
        self.extr.check_status("start delim")
        self.extr.skipped(10000)
        self.extr.check_status("giving up")
        self.extr.check_quiet()
        self.extr.push("\n")
        self.extr.check_quiet()

    def test_gives_up_after_16garbage(self):
        self.extr.push("$$")
        self.extr.check_status("start delim")

        self.extr.push("some,legit,data")
        self.extr.push("\t some printable data" * 17)
        self.extr.check_status("giving up")
        self.extr.check_quiet()

        self.extr.push("\n")
        self.extr.check_quiet()

        self.test_extracts()

    def test_skipped(self):
        self.extr.check_quiet()
        self.extr.push("$$some")
        self.extr.check_status("start delim")
        self.extr.skipped(5)
        self.extr.push("data\n")
        # JsonCPP doesn't support \0 in strings, so the mock UploaderThread
        # replaces it with \1s
        self.extr.check_upload("$$some\1\1\1\1\1data\n")
        self.extr.check_status("extracted")
        self.extr.check_status("parse failed")
        self.extr.check_data()

    def basic_data_dict(self, string, callsign):
        return {"_sentence": string, "_parsed": True, "_basic": True,
                "_protocol": "UKHAS", "payload": callsign}

    def check_noconfig(self, string, callsign):
        self.extr.push(string)
        self.extr.check_status("start delim")
        self.extr.check_upload(string)
        self.extr.check_status("extracted")
        self.extr.check_data(self.basic_data_dict(string, callsign))

    def test_crude_parse_noconfig_xor(self):
        self.check_noconfig("$$mypayload,has,a,valid,checksum*1a\n",
                            "mypayload")

    def test_crude_parse_noconfig_crc16_ccitt(self):
        self.check_noconfig("$$mypayload,has,a,valid,checksum*1018\n",
                            "mypayload")

    crude_parse_flight_doc = {
        "payload": "TESTING",
        "sentence": {
            "checksum": "crc16-ccitt",
            "fields": [
                {"name": "field_a"},
                {"name": "field_b"},
                {"name": "field_c"}
            ],
        }
    }

    def test_crude_parse_config(self):
        self.extr.set_current_payload(self.crude_parse_flight_doc)
        string = "$$TESTING,value_a,value_b,value_c*8C3E\n"
        self.extr.push(string)
        self.extr.check_status("start delim")
        self.extr.check_upload(string)
        self.extr.check_status("extracted")
        self.extr.check_data({"_sentence": string, "_parsed": True,
                              "_protocol": "UKHAS", "payload": "TESTING",
                              "field_a": "value_a", "field_b": "value_b",
                              "field_c": "value_c"})

    def test_crude_checks(self):
        checks = [
            ("$$TESTING,a,b,c*asdfg\n", "invalid checksum len"),
            ("$$TESTING,a,b,c*45\n", "invalid checksum: expected 1A"),
            ("$$TESTING,a,b,c*AAAA\n", "invalid checksum: expected BEBC"),
            ("$$TESTING,val_a,val_b*4EB7\n", "incorrect number of fields"),
            ("$$TESTING,a,b,c*1A\n", "wrong checksum type"),
            ("$$ANOTHER,a,b,c*2355\n", "incorrect callsign"),
        ]

        self.extr.set_current_payload(self.crude_parse_flight_doc)

        for (string, error) in checks:
            self.extr.push(string)
            self.extr.check_status("start delim")
            self.extr.check_upload(string)
            self.extr.check_status("extracted")
            self.extr.check_status(error)
            self.extr.check_data()

    multi_config_flight_doc = {
        "payload": "AWKWARD",
        "sentence": [
            { "checksum": "crc16-ccitt",
              "fields": [ {"name": "fa"}, {"name": "fo"}, {"name": "fc"} ] },
            { "checksum": "crc16-ccitt",
              "fields": [ {"name": "fa"}, {"name": "fb"} ] }
        ]
    }

    def test_multi_config(self):
        self.extr.set_current_payload(self.multi_config_flight_doc)
        string = "$$AWKWARD,hello,world*D4E9\n"
        self.extr.push(string)
        self.extr.check_status("start delim")
        self.extr.check_upload(string)
        self.extr.check_status("extracted")
        self.extr.check_data({"_sentence": string, "_parsed": True,
                              "_protocol": "UKHAS", "payload": "AWKWARD",
                              "fa": "hello", "fb": "world"})

        string = "$$AWKWARD,extended,other,data*F01F\n"
        self.extr.push(string)
        self.extr.check_status("start delim")
        self.extr.check_upload(string)
        self.extr.check_status("extracted")
        self.extr.check_data({"_sentence": string, "_parsed": True,
                              "_protocol": "UKHAS", "payload": "AWKWARD",
                              "fa": "extended", "fo": "other", "fc": "data"})

    ddmmmmmm_flight_doc = {
        "payload": "TESTING",
        "sentence": {
            "checksum": "crc16-ccitt",
            "fields": [
                {"sensor":"stdtelem.coordinate","name":"lat_a",
                 "format":"dd.dddd"},
                {"sensor":"stdtelem.coordinate","name":"lat_b",
                 "format":"ddmm.mm"},
                {"name": "field_b"}
            ],
        }
    }

    def test_ddmmmmmm(self):
        self.extr.set_current_payload(self.ddmmmmmm_flight_doc)
        string = "$$TESTING,0024.124583,5116.5271,whatever*14BA\n"
        self.extr.push(string)
        self.extr.check_status("start delim")
        self.extr.check_upload(string)
        self.extr.check_status("extracted")
        self.extr.check_data({"_sentence": string, "_parsed": True,
                              "_protocol": "UKHAS", "payload": "TESTING",
                              "lat_a": "0024.124583", "lat_b": "51.27545",
                              "field_b": "whatever" })
