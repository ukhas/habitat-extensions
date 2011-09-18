import subprocess
import json
import fcntl
import os
import errno

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
        except IOError as e:
            if e.errno != errno.EAGAIN:
                raise
        else:
            raise AssertionError("expected IOError(EAGAIN)")

        fcntl.fcntl(fd, fcntl.F_SETFL, fl)

    def add(self, name):
        self._write(["add", name])

    def skipped(self, num):
        self._write(["skipped", num])

    def push(self, data):
        for char in data:
            self._write(["push", char])

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
            assert ret == 0

class TestExtractorManager:
    def setup(self):
        self.extr = Proxy("tests/extractor")

    def teardown(self):
        self.extr.check_quiet()
        self.extr.close()

    def test_management(self):
        self.extr.push("$$this,is,a,string\n")
        self.extr.check_quiet()

        self.extr.add("UKHASExtractor")
        self.extr.push("$$this,is,a,string\n")
        self.extr.check_status()
        self.extr.check_upload()
        self.extr.check_status()

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
        self.extr.check_status("UKHAS Extractor: found start delimiter")

    def test_extracts(self):
        self.extr.push("$$a,simple,test*00\n")
        self.extr.check_status("UKHAS Extractor: found start delimiter")
        self.extr.check_upload("$$a,simple,test*00\n")
        self.extr.check_status("UKHAS Extractor: extracted string")

    def test_can_restart(self):
        self.extr.push("this is some garbage just to mess things up")
        self.extr.check_quiet()
        self.extr.push("$$")
        self.extr.check_status()

        self.extr.push("garbage: after seeing the delimiter, we lose signal.")
        self.extr.push("$$")
        self.extr.check_status()
        self.extr.check_quiet()
        self.extr.push("helloworld")
        self.extr.check_quiet()
        self.extr.push("\n")
        self.extr.check_upload("$$helloworld\n")
        self.extr.check_status()

    def test_gives_up_after_1k(self):
        self.extr.push("$$")
        self.extr.check_status()

        self.extr.push("a" * 1022)
        self.extr.check_quiet()

        # Should have given up, so a \n won't cause an upload:
        self.extr.push("\n")
        self.extr.check_quiet()

    def test_gives_up_after_16garbage(self):
        self.extr.push("$$")
        self.extr.check_status()

        self.extr.push("some,legit,data")
        self.extr.push("\u0004\u0082more printable data\u009e\u00ff" * 4)

        self.extr.push("\n")
        self.extr.check_quiet()
