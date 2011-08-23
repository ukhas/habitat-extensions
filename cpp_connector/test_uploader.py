import subprocess
import tempfile
import json
import elementtree.ElementTree
import BaseHTTPServer
import threading
import collections
import time

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
        self.closed = False

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

    def __del__(self):
        if not self.closed:
            self.close(check=False)

    def close(self, check=True):
        self.closed = True
        self.p.stdin.close()
        ret = self.p.wait()

        if check:
            assert ret == 0
            self._check_valgrind()

    def _check_valgrind(self):
        if self.xmlfile:
            self.xmlfile.seek(0)
            tree = elementtree.ElementTree.parse(self.xmlfile)
            assert tree.find("error") == None

    def payload_telemetry(self, data, *args):
        return self._proxy(["payload_telemetry", data] + list(args))

    def listener_telemetry(self, data, *args):
        return self._proxy(["listener_telemetry", data] + list(args))

    def listener_info(self, data, *args):
        return self._proxy(["listener_info", data] + list(args))

class MockHTTP(BaseHTTPServer.HTTPServer):
    def __init__(self, server_address=('localhost', 51205)):
        BaseHTTPServer.HTTPServer.__init__(self, server_address,
                                           MockHTTPHandler)
        self.expecting = False
        self.expect_queue = collections.deque()
        self.url = "http://localhost:{0}".format(self.server_port)
        self.timeout = 1

    expect_defaults = {
        # expect:
        "method": "GET",
        "path": "/",
        "body": None,   # string if you expect something from a POST
        # body_json=object

        # and respond with:
        "code": 404,
        "respond": "If this was a 200, this would be your page"
        # respond_json=object
    }

    def expect_request(self, **kwargs):
        assert not self.expecting

        e = self.expect_defaults.copy()
        e.update(kwargs)

        self.expect_queue.append(e)

    def run(self):
        assert not self.expecting
        self.expecting = True
        self.expect_handled = False
        self.expect_successes = 0
        self.expect_length = len(self.expect_queue)

        self.expect_thread = threading.Thread(target=self._run_expect)
        self.expect_thread.daemon = True
        self.expect_thread.start()

    def check(self):
        assert self.expecting
        self.expect_queue.clear()
        self.expect_thread.join()

        assert self.expect_successes == self.expect_length

        self.expecting = False

    def _run_expect(self):
        self.error = None
        while len(self.expect_queue):
            self.handle_request()

class MockHTTPHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    def compare(self, a, b, what):
        if a != b:
            raise AssertionError("http request expect", what, a, b)

    def check_expect(self):
        assert self.server.expecting
        e = self.server.expect_queue.popleft()

        self.compare(e["method"], self.command, "method")
        self.compare(e["path"], self.path, "path")

        length = self.headers.getheader('content-length')
        if length:
            length = int(length)
            body = self.rfile.read(length)
            assert len(body) == length
        else:
            body = None

        if "body_json" in e:
            self.compare(e["body_json"], json.loads(body), "body_json")
        else:
            self.compare(e["body"], body, "body")

        code = e["code"]
        if "respond_json" in e:
            content = json.dumps(e["respond_json"])
        else:
            content = e["respond"]

        self.send_response(code)
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

        self.server.expect_successes += 1

    def log_request(self, *args, **kwargs):
        pass

    do_POST = check_expect
    do_GET = check_expect
    do_PUT = check_expect

class CloseEnoughTime:
    def __init__(self, now=False, tolerance=1):
        if now is False or now is None:
            self.time = time.time()
            self.later = False
        else:
            self.later = True

        self.tolerance = tolerance

    def __eq__(self, rhs):
        if not isinstance(rhs, int) and not isinstance(rhs, float):
            return False

        if self.later:
            self.time = time.time()

        return abs(self.time - rhs) <= self.tolerance

    def __repr__(self):
        if self.later:
            return "<Tolerant time: now>"
        else:
            return "<Tolerant time: {0}>".format(self.time)

class TestCPPConnector:
    def setup(self):
        self.couchdb = MockHTTP()
        self.uploader = Proxy("PROXYCALL", self.couchdb.url)

    def teardown(self):
        self.uploader.close()

    def test_example(self):
        self.couchdb.expect_request(
            path="/_uuids?count=100",
            code=200,
            respond_json={"uuids": ["meh"]}
        )
        self.couchdb.expect_request(
            method="PUT",
            path="/habitat/meh",
            body_json={
                "_id": "meh",
                "time_created": CloseEnoughTime(),
                "time_uploaded": CloseEnoughTime(),
                "data": {
                    "callsign": "PROXYCALL",
                    "some_data": True
                },
                "type": "listener_telemetry"
            },
            code=201,
            respond_json={"id": "meh", "rev": "blah"}
        )
        self.couchdb.run()

        doc_id = self.uploader.listener_telemetry({"some_data": True})
        assert doc_id == "meh"
        self.couchdb.check()
