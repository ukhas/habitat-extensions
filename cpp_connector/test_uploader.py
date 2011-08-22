import subprocess
import tempfile
import json
import elementtree

class ProxyException:
    def __init__(self, name, what=None):
        self.name = name
        self.what = what

    def __str__(self):
        return "ProxyException: {0.name}: {0.what!r}".format(self)

class Proxy:
    def __init__(self, callsign, couch_uri=None, couch_db=None,
                 max_merge_attempts=None,
                 command="./cpp_connector", with_valgrind=True):
        if with_valgrind:
            self.xmlfile = tempfile.TemporaryFile("a+b")
            args = ("valgrind", "--quiet", "--xml=yes",
                    "--xml-fd=2", command)
            self.p = subprocess.Popen(args, stdin=subprocess.PIPE,
                                      stdout=subprocess.PIPE,
                                      stderr=self.xmlfile)
        else:
            self.xmlfile = None
            self.p = subprocess.Popen(command, stdin=subprocess.PIPE,
                                      stdout=subprocess.PIPE)


        init_args = ["init", callsign]

        for a in [couch_uri, couch_db, max_merge_attempts]:
            if a is None:
                break
            init_args.append(a)

        self._proxy(init_args, False)

    def _proxy(self, command, get_response=True):
        self.p.stdin.write(json.dumps(command))
        self.p.stdin.write("\n")

        if get_response:
            line = self.p.stdout.readline()
            assert line and line.endswith("\n")
            obj = json.loads(line)

            if obj[0] == "return":
                return obj[1]
            elif obj[0] == "error":
                if len(obj) == 3:
                    raise ProxyException(obj[1], obj[2])
                else:
                    raise ProxyException(obj[1])
            else:
                raise AssertionError("len(obj)")

    def _check_valgrind(self):
        if self.xmlfile:
            self.xmlfile.seek(0)
            tree = elementtree.ElementTree.parse(self.xmlfile)
            assert tree.find("errorcounts").getchildren() == 0

    def payload_telemetry(self, data, *args):
        return self._proxy(["payload_telemetry", data] + list(args))

    def listener_telemetry(self, data, *args):
        return self._proxy(["listener_telemetry", data] + list(args))

    def listener_info(self, data, *args):
        return self._proxy(["listener_info", data] + list(args))

if __name__ == "__main__":
    p = Proxy("test uploader proxy test", "http://localhost:5984")
    print repr(p.payload_telemetry("testing proxy"))
