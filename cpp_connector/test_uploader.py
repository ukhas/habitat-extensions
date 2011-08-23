import subprocess
import tempfile
import json
import elementtree.ElementTree
import BaseHTTPServer
import threading
import collections
import time
import uuid

class ProxyException:
    def __init__(self, name, what=None):
        self.name = name
        self.what = what

    def __str__(self):
        return "ProxyException: {0.name}: {0.what!r}".format(self)

class Callbacks:
    def __init__(self):
        self.lock = threading.RLock()
        self.fake_time = 10000  # set in fake_main.cxx

    def advance_time(self, amount=1):
        with self.lock:
            self.fake_time += amount

    def time(self):
        with self.lock:
            return self.fake_time

    def time_project(self, value):
        """what the time will be value seconds into the future"""
        return 10000 + value

class Proxy:
    def __init__(self, callsign, couch_uri=None, couch_db=None,
                 max_merge_attempts=None,
                 command="./cpp_connector",
                 callbacks=None,
                 with_valgrind=False):

        self.closed = False

        if with_valgrind:
            self.xmlfile = tempfile.NamedTemporaryFile("a+b")
            args = ("valgrind", "--quiet", "--xml=yes",
                    "--xml-file=" + self.xmlfile.name, command)
            self.p = subprocess.Popen(args, stdin=subprocess.PIPE,
                                      stdout=subprocess.PIPE)
        else:
            self.xmlfile = None
            self.p = subprocess.Popen(command, stdin=subprocess.PIPE,
                                      stdout=subprocess.PIPE)


        self.callbacks = callbacks

        init_args = ["init", callsign]

        for a in [couch_uri, couch_db, max_merge_attempts]:
            if a is None:
                break
            init_args.append(a)

        self._proxy(init_args, False)

    def _proxy(self, command, get_response=True):
        self.p.stdin.write(json.dumps(command))
        self.p.stdin.write("\n")

        while True:
            line = self.p.stdout.readline()
            assert line and line.endswith("\n")
            obj = json.loads(line)

            if obj[0] == "return":
                if len(obj) == 1:
                    return None
                else:
                    return obj[1]
            elif obj[0] == "error":
                if len(obj) == 3:
                    raise ProxyException(obj[1], obj[2])
                else:
                    raise ProxyException(obj[1])
            elif obj[0] == "callback":
                (cb, name, args) = obj
                func = getattr(self.callbacks, name)
                if args:
                    result = func(args)
                else:
                    result = func()

                self.p.stdin.write(json.dumps(["return", result]))
                self.p.stdin.write("\n")
            else:
                raise AssertionError("invalid response")

    def __del__(self):
        if not self.closed:
            self.close(check=False)

    def close(self, check=True):
        self.closed = True
        self.p.stdin.close()
        ret = self.p.wait()

        if check:
            assert ret == 0, ret
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
    def __init__(self, server_address=('localhost', 51205), callbacks=None):
        BaseHTTPServer.HTTPServer.__init__(self, server_address,
                                           MockHTTPHandler)
        self.expecting = False
        self.expect_queue = collections.deque()
        self.url = "http://localhost:{0}".format(self.server_port)
        self.timeout = 1
        self.callbacks = callbacks

    def advance_time(self, value):
        self.callbacks.advance_time(value)

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

        expect_100_header = self.headers.getheader('expect')
        expect_100 = expect_100_header and \
                     expect_100_header.lower() == "100-continue"
        support_100 = self.request_version != 'HTTP/0.9'

        if support_100 and expect_100:
            self.wfile.write(self.protocol_version + " 100 Continue\r\n\r\n")

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

        if "advance_time_after" in e:
            self.server.advance_time(e["advance_time_after"])

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

class TestCPPConnector:
    def setup(self):
        self.callbacks = Callbacks()
        self.couchdb = MockHTTP(callbacks=self.callbacks)
        self.uploader = Proxy("PROXYCALL", self.couchdb.url,
                              callbacks=self.callbacks)
        self.uuids = collections.deque()

    def teardown(self):
        self.uploader.close()

    def _gen_fake_uuid(self):
        return str(uuid.uuid1()).replace("-", "")

    def _gen_fake_rev(self, num=1):
        return str(num) + "-" + self._gen_fake_uuid()

    def expect_uuid_request(self):
        new_uuids = [self._gen_fake_uuid() for i in xrange(100)]
        self.uuids.extend(new_uuids)

        self.couchdb.expect_request(
            path="/_uuids?count=100",
            code=200,
            respond_json={"uuids": new_uuids},
        )

    def pop_uuid(self):
        return self.uuids.popleft()

    def test_example(self):
        self.expect_uuid_request()

        should_use_uuids = []

        for i in xrange(10):
            uuid = self.pop_uuid()
            should_use_uuids.append(uuid)

            self.couchdb.expect_request(
                method="PUT",
                path="/habitat/" + uuid,
                body_json={
                    "_id": uuid,
                    "time_created": self.callbacks.time_project(i),
                    "time_uploaded": self.callbacks.time_project(i),
                    "data": {
                        "callsign": "PROXYCALL",
                        "some_data": True
                    },
                    "type": "listener_telemetry"
                },
                code=201,
                respond_json={"id": uuid, "rev": self._gen_fake_rev()},
                advance_time_after=1
            )

        self.couchdb.run()

        for i in xrange(10):
            doc_id = self.uploader.listener_telemetry({"some_data": True})
            assert doc_id == should_use_uuids[i]

        self.couchdb.check()
