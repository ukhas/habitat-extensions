# Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE

import subprocess
import os
import errno
import fcntl
import tempfile
import json
import elementtree.ElementTree
import BaseHTTPServer
import threading
import collections
import time
import uuid
import copy

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
    def __init__(self, command, callsign, couch_uri=None, couch_db=None,
                 max_merge_attempts=None, callbacks=None, with_valgrind=False):

        self.closed = False
        self.blocking = True

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
        self.re_init(callsign, couch_uri, couch_db, max_merge_attempts=None)

    def re_init(self, callsign, couch_uri=None, couch_db=None,
                max_merge_attempts=None):
        init_args = ["init", callsign]

        for a in [couch_uri, couch_db, max_merge_attempts]:
            if a is None:
                break
            init_args.append(a)

        self._proxy(init_args)

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

    def _proxy(self, command):
        self._write(command)
        return self.complete()

    def complete(self):
        while True:
            obj = self._read()

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
                if len(obj) == 3:
                    (cb, name, args) = obj
                else:
                    (cb, name) = obj
                    args = []
                func = getattr(self.callbacks, name)
                result = func(*args)

                self._write(["return", result])
            elif obj[0] == "log":
                pass
            else:
                raise AssertionError("invalid response")

    def unblock(self):
        assert self.blocking
        self.blocking = False

        fd = self.p.stdout.fileno()
        self.fl = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, self.fl | os.O_NONBLOCK)

    def block(self):
        assert not self.blocking
        self.blocking = True

        fd = self.p.stdout.fileno()
        fcntl.fcntl(fd, fcntl.F_SETFL, self.fl)

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

    def flights(self):
        return self._proxy(["flights"])

    def reset(self):
        return self._proxy(["reset"])

temp_port = 51205

def next_temp_port():
    global temp_port
    temp_port += 1
    return temp_port

class MockHTTP(BaseHTTPServer.HTTPServer):
    def __init__(self, server_address=None, callbacks=None):
        if server_address == None:
            server_address = ('localhost', next_temp_port())

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
        print "-- HTTP " + self.command + " " + self.path

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

        if "wait" in e:
            e["wait"].set()

        if "delay" in e:
            e["delay"].wait()

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
    command = "tests/cpp_connector"

    def setup(self):
        self.callbacks = Callbacks()
        self.couchdb = MockHTTP(callbacks=self.callbacks)
        self.uploader = Proxy(self.command, "PROXYCALL", self.couchdb.url,
                              callbacks=self.callbacks, with_valgrind=False)
        self.uuids = collections.deque()

        self.db_path = "/habitat/"

    def teardown(self):
        self.uploader.close()
        self.couchdb.server_close()

    def gen_fake_uuid(self):
        return str(uuid.uuid1()).replace("-", "")

    def gen_fake_rev(self, num=1):
        return str(num) + "-" + self.gen_fake_uuid()

    def expect_uuid_request(self):
        new_uuids = [self.gen_fake_uuid() for i in xrange(100)]
        self.uuids.extend(new_uuids)

        self.couchdb.expect_request(
            path="/_uuids?count=100",
            code=200,
            respond_json={"uuids": new_uuids},
        )

    def pop_uuid(self):
        self.ensure_uuids()
        return self.uuids.popleft()

    def ensure_uuids(self):
        if not len(self.uuids):
            self.expect_uuid_request()

    def expect_save_doc(self, doc, rev=None, **kwargs):
        if not rev:
            rev = 1

        self.couchdb.expect_request(
            method="PUT",
            path=self.db_path + doc["_id"],
            body_json=doc,
            code=201,
            respond_json={"id": doc["_id"], "rev": self.gen_fake_rev(rev)},
            **kwargs
        )

    def test_uses_server_uuids(self):
        should_use_uuids = []

        for i in xrange(200):
            uuid = self.pop_uuid()
            should_use_uuids.append(uuid)

            doc = {
                "_id": uuid,
                "time_created": self.callbacks.time_project(i),
                "time_uploaded": self.callbacks.time_project(i),
                "data": {
                    "callsign": "PROXYCALL",
                    "test": 123.356
                },
                "type": "listener_telemetry"
            }

            self.expect_save_doc(doc, advance_time_after=1)

        self.couchdb.run()

        for i in xrange(200):
            doc_id = self.uploader.listener_telemetry({"test": 123.356})
            assert doc_id == should_use_uuids[i]

        self.couchdb.check()

    def add_sample_listener_docs(self):
        telemetry_data = {"some_data": 123, "_flag": True}
        telemetry_doc = {
            "_id": self.pop_uuid(),
            "data": copy.deepcopy(telemetry_data),
            "type": "listener_telemetry",
            "time_created": self.callbacks.time_project(0),
            "time_uploaded": self.callbacks.time_project(0)
        }
        telemetry_doc["data"]["callsign"] = "PROXYCALL"

        info_data = {"my_radio": "Duga-3", "vehicle": "Tractor"}
        info_doc = {
            "_id": self.pop_uuid(),
            "data": copy.deepcopy(info_data),
            "type": "listener_info",
            "time_created": self.callbacks.time_project(0),
            "time_uploaded": self.callbacks.time_project(0)
        }
        info_doc["data"]["callsign"] = "PROXYCALL"

        self.expect_save_doc(telemetry_doc)
        self.expect_save_doc(info_doc)

        self.couchdb.run()
        self.sample_telemetry_doc_id = \
                self.uploader.listener_telemetry(telemetry_data)
        self.sample_info_doc_id = self.uploader.listener_info(info_data)
        self.couchdb.check()

        assert self.sample_telemetry_doc_id == telemetry_doc["_id"]
        assert self.sample_info_doc_id == info_doc["_id"]

    def test_pushes_listener_docs(self):
        self.add_sample_listener_docs()

        # And now again, but this time, setting time_created.
        telemetry_data = {
            "time": {
                "hour": 12,
                "minute": 40,
                "second": 5
            },
            "latitude": 35.11,
            "longitude": 137.567,
            "altitude": 12
        }
        telemetry_doc = {
            "_id": self.pop_uuid(),
            "data": copy.deepcopy(telemetry_data),
            "type": "listener_telemetry",
            "time_created": 501,
            "time_uploaded": self.callbacks.time_project(0)
        }
        telemetry_doc["data"]["callsign"] = "PROXYCALL"

        info_data = {
            "name": "Daniel Richman",
            "location": "Reading, UK",
            "radio": "Yaesu FT 790R",
            "antenna": "Whip"
        }
        info_doc = {
            "_id": self.pop_uuid(),
            "data": copy.deepcopy(info_data),
            "type": "listener_info",
            "time_created": 409,
            "time_uploaded": self.callbacks.time_project(5)
        }
        info_doc["data"]["callsign"] = "PROXYCALL"

        self.expect_save_doc(telemetry_doc, advance_time_after=5)
        self.expect_save_doc(info_doc)

        self.couchdb.run()
        telemetry_doc_id = \
                self.uploader.listener_telemetry(telemetry_data, 501)
        info_doc_id = self.uploader.listener_info(info_data, 409)
        self.couchdb.check()

        assert telemetry_doc_id == telemetry_doc["_id"]
        assert info_doc_id == info_doc["_id"]

    ptlm_doc_id = "c0be13b259acfd2fe23cd0d1e70555d6" \
                  "8f83926278b23f5b813bdc75f6b9cdd6"
    ptlm_string = "asdf blah \x12 binar\x04\x01 asdfasdfsz"
    ptlm_metadata = {"frequency": 434075000, "misc": "Hi"}
    ptlm_doc = {
        "_id": ptlm_doc_id,
        "data": {
            "_raw": "YXNkZiBibGFoIBIgYmluYXIEASBhc2RmYXNkZnN6"
        },
        "type": "payload_telemetry",
        "receivers": {
            "PROXYCALL": {
                "frequency": 434075000,
                "misc": "Hi"
            }
        }
    }

    def test_payload_telemetry(self):
        # WARNING: JsonCPP does not support strings with \0 in the middle of
        # them, because it does not store the length of the string and instead
        # later figures it out with strlen. This does not harm the uploader
        # because our code converts binary data to base64 before giving it
        # to the json encoder. However, the json stdin proxy call interface
        # isn't going to work with nulls in it.

        receiver_info = {
            "time_created": self.callbacks.time_project(0),
            "time_uploaded": self.callbacks.time_project(0),
        }

        doc = copy.deepcopy(self.ptlm_doc)
        doc["receivers"]["PROXYCALL"].update(receiver_info)

        self.expect_save_doc(doc)
        self.couchdb.run()
        ret_doc_id = self.uploader.payload_telemetry(self.ptlm_string,
                                                     self.ptlm_metadata)
        self.couchdb.check()

        assert ret_doc_id == self.ptlm_doc_id

    def test_adds_latest_listener_doc(self):
        self.add_sample_listener_docs()

        receiver_info = {
            "time_created": self.callbacks.time_project(0),
            "time_uploaded": self.callbacks.time_project(0),
            "latest_listener_telemetry": self.sample_telemetry_doc_id,
            "latest_listener_info": self.sample_info_doc_id
        }

        doc = copy.deepcopy(self.ptlm_doc)
        doc["receivers"]["PROXYCALL"].update(receiver_info)

        self.expect_save_doc(doc)
        self.couchdb.run()
        self.uploader.payload_telemetry(self.ptlm_string, self.ptlm_metadata)
        self.couchdb.check()

    ptlm_doc_existing = {
        "_id": ptlm_doc_id,
        "data": {
            "_raw": "YXNkZiBibGFoIBIgYmluYXIEASBhc2RmYXNkZnN6",
            "some_parsed_data": 12345
        },
        "type": "payload_telemetry",
        "receivers": {
            "SOMEONEELSE": {
                "time_created": 200,
                "time_uploaded": 240,
                "frequency": 434074000,
                "asdf": "World"
            }
        }
    }

    def test_ptlm_merges_payload_conflicts(self):
        receiver_info = {
            "time_created": self.callbacks.time_project(0),
            "time_uploaded": self.callbacks.time_project(0),
        }

        doc = copy.deepcopy(self.ptlm_doc)
        doc["receivers"]["PROXYCALL"].update(receiver_info)

        self.couchdb.expect_request(
            method="PUT",
            path=self.db_path + doc["_id"],
            body_json=doc,
            code=409,
            respond_json={"error": "conflict"},
            advance_time_after=5
        )

        self.couchdb.expect_request(
            path=self.db_path + self.ptlm_doc_id,
            code=200,
            respond_json=self.ptlm_doc_existing,
            advance_time_after=5,
        )

        doc_merged = copy.deepcopy(self.ptlm_doc_existing)
        doc_merged["receivers"]["PROXYCALL"] = \
            copy.deepcopy(self.ptlm_doc["receivers"]["PROXYCALL"])
        receiver_info = copy.deepcopy(receiver_info)
        receiver_info["time_uploaded"] = self.callbacks.time_project(10)
        doc_merged["receivers"]["PROXYCALL"].update(receiver_info)

        self.expect_save_doc(doc_merged)

        self.couchdb.run()
        self.uploader.payload_telemetry(self.ptlm_string, self.ptlm_metadata)
        self.couchdb.check()

    def test_ptlm_refuses_to_merge_collision(self):
        receiver_info = {
            "time_created": self.callbacks.time_project(0),
            "time_uploaded": self.callbacks.time_project(0),
        }

        doc = copy.deepcopy(self.ptlm_doc)
        doc["receivers"]["PROXYCALL"].update(receiver_info)
        del doc["receivers"]["PROXYCALL"]["frequency"]
        del doc["receivers"]["PROXYCALL"]["misc"]

        self.couchdb.expect_request(
            method="PUT",
            path=self.db_path + doc["_id"],
            body_json=doc,
            code=409,
            respond_json={"error": "conflict"},
            advance_time_after=5
        )

        doc_conflict = copy.deepcopy(self.ptlm_doc_existing)
        doc_conflict["data"]["_raw"] = "cGluZWFwcGxlcw=="

        self.couchdb.expect_request(
            path=self.db_path + self.ptlm_doc_id,
            code=200,
            respond_json=doc_conflict
        )

        self.couchdb.run()

        try:
            self.uploader.payload_telemetry(self.ptlm_string)
        except ProxyException, e:
            if e.name == "runtime_error" and \
               e.what == "habitat::CollisionError":
                pass
            else:
                raise
        else:
            raise AssertionError("Did not raise CollisionError")

        self.couchdb.check()

    def add_mock_conflicts(self, n):
        receiver_info = {
            "time_created": self.callbacks.time_project(0),
            "time_uploaded": self.callbacks.time_project(0),
        }

        doc = copy.deepcopy(self.ptlm_doc)
        doc["receivers"]["PROXYCALL"].update(receiver_info)

        self.couchdb.expect_request(
            method="PUT",
            path=self.db_path + self.ptlm_doc_id,
            body_json=doc,
            code=409,
            respond_json={"error": "conflict"}
        )

        doc_existing = copy.deepcopy(self.ptlm_doc_existing)

        doc_merged = copy.deepcopy(self.ptlm_doc_existing)
        doc_merged["receivers"]["PROXYCALL"] = \
            copy.deepcopy(self.ptlm_doc["receivers"]["PROXYCALL"])
        doc_merged["receivers"]["PROXYCALL"].update(receiver_info)

        for i in xrange(n):
            self.couchdb.expect_request(
                path=self.db_path + self.ptlm_doc_id,
                code=200,
                respond_json=doc_existing,
            )

            self.couchdb.expect_request(
                method="PUT",
                path=self.db_path + self.ptlm_doc_id,
                body_json=doc_merged,
                code=409,
                respond_json={"error": "conflict"},
                advance_time_after=1
            )

            doc_existing = copy.deepcopy(doc_existing)
            doc_merged = copy.deepcopy(doc_merged)

            new_call = "listener_{0}".format(i)
            new_info = {"time_created": 600 + i, "time_uploaded": 641 + i}

            doc_existing["receivers"][new_call] = new_info
            doc_merged["receivers"][new_call] = new_info

            doc_merged["receivers"]["PROXYCALL"]["time_uploaded"] = \
                self.callbacks.time_project(i + 1)

        return (doc_existing, doc_merged)

    def test_merges_multiple_conflicts(self):
        (final_doc_existing, final_doc_merged) = self.add_mock_conflicts(15)

        self.couchdb.expect_request(
            path=self.db_path + self.ptlm_doc_id,
            code=200,
            respond_json=final_doc_existing,
        )

        self.expect_save_doc(final_doc_merged)

        self.couchdb.run()
        self.uploader.payload_telemetry(self.ptlm_string, self.ptlm_metadata)
        self.couchdb.check()

    def test_gives_up_after_many_conflicts(self):
        self.add_mock_conflicts(20)
        self.couchdb.run()

        try:
            self.uploader.payload_telemetry(self.ptlm_string,
                                            self.ptlm_metadata)
        except ProxyException, e:
            if e.name == "runtime_error" and \
               e.what == "habitat::UnmergeableError":
                pass
            else:
                raise
        else:
            raise AssertionError("Did not raise UnmergeableError")

    def test_flights(self):
        flights= [{"_id": "flight_{0}".format(i), "a flight": i}
                  for i in xrange(100)]
        rows = [{"id": doc["_id"], "key": None, "value": None, "doc": doc}
                for doc in flights]
        fake_view_response = \
                {"total_rows": len(rows), "offset": 0, "rows": rows}

        # cURL is a little overzealous with its escape(): _ is replaced
        # with %5F. This should be fine

        self.callbacks.advance_time(1925)
        view_time = self.callbacks.time_project(1925)
        options = "include%5Fdocs=true&startkey=" + str(view_time)

        self.couchdb.expect_request(
            path=self.db_path + "_design/uploader%5Fv1/_view/flights?" + options,
            code=200,
            respond_json=copy.deepcopy(fake_view_response)
        )
        self.couchdb.run()

        result = self.uploader.flights()
        assert result == flights

class TestCPPConnectorThreaded(TestCPPConnector):
    command = "tests/cpp_connector_threaded"

    def test_queues_things(self):
        telemetry_data = {"this was queued": True}
        telemetry_doc = {
            "_id": self.pop_uuid(),
            "data": copy.deepcopy(telemetry_data),
            "type": "listener_telemetry",
            "time_created": self.callbacks.time_project(0),
            "time_uploaded": self.callbacks.time_project(0)
        }
        telemetry_doc["data"]["callsign"] = "PROXYCALL"

        info_data = {"5": "this was the second item in the queue"}
        info_doc = {
            "_id": self.pop_uuid(),
            "data": copy.deepcopy(info_data),
            "type": "listener_info",
            "time_created": self.callbacks.time_project(0),
            "time_uploaded": self.callbacks.time_project(0)
        }
        info_doc["data"]["callsign"] = "PROXYCALL"

        delay_one = threading.Event()
        wait_one = threading.Event()
        delay_two = threading.Event()
        wait_two = threading.Event()

        self.expect_save_doc(telemetry_doc, delay=delay_one, wait=wait_one)
        self.expect_save_doc(info_doc, delay=delay_two, wait=wait_two)

        self.couchdb.run()

        self.run_unblocked(self.uploader.listener_telemetry, telemetry_data)
        self.run_unblocked(self.uploader.listener_info, info_data)

        # The complexity of doing this properly justifies this evil hack...
        # right?
        while not wait_one.is_set():
            wait_one.wait(0.1)
            self.run_unblocked(self.uploader.complete)

        delay_one.set()
        assert self.uploader.complete() == telemetry_doc["_id"]

        while not wait_two.is_set():
            wait_two.wait(0.01)
            self.run_unblocked(self.uploader.complete)

        delay_two.set()
        assert self.uploader.complete() == info_doc["_id"]

        self.couchdb.check()

    def run_unblocked(self, func, *args, **kwargs):
        self.uploader.unblock()

        try:
            return func(*args, **kwargs)
        except IOError as e:
            if e.errno != errno.EAGAIN:
                raise
        else:
            raise AssertionError("expected IOError(EAGAIN)")

        self.uploader.block()

    def test_changes_settings(self):
        self.uploader.re_init("NEWCALL", self.couchdb.url)

        receiver_info = {
            "time_created": self.callbacks.time_project(0),
            "time_uploaded": self.callbacks.time_project(0),
        }

        doc = copy.deepcopy(self.ptlm_doc)
        doc["receivers"]["PROXYCALL"].update(receiver_info)
        doc["receivers"]["NEWCALL"] = doc["receivers"]["PROXYCALL"]
        del doc["receivers"]["PROXYCALL"]

        self.expect_save_doc(doc)
        self.couchdb.run()
        ret_doc_id = self.uploader.payload_telemetry(self.ptlm_string,
                                                     self.ptlm_metadata)
        self.couchdb.check()

        assert ret_doc_id == self.ptlm_doc_id

    def test_reset(self):
        self.uploader.re_init("NEWCALL", self.couchdb.url)
        self.uploader.reset()

        self.couchdb.run()  # expect nothing.

        try:
            self.uploader.payload_telemetry("asdf", {})
        except ProxyException as e:
            assert "not initialised" in str(e)
        else:
            raise AssertionError("not initialised was not thrown")

        self.couchdb.check()
