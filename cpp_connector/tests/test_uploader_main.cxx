/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE. */

#include <iostream>
#include <memory>
#include <stdexcept>
#include <ctime>

#include "EZ.h"
#include "Uploader.h"

#ifdef THREADED
#include "UploaderThread.h"
#endif

using namespace std;

template <typename T>
class SafeValue
{
    EZ::Mutex mutex;
    T value;

public:
    SafeValue(T init) : value(init) {};
    ~SafeValue() {};
    bool get() { EZ::MutexLock lock(mutex); return value; }
    void set(T v) { EZ::MutexLock lock(mutex); value = v; }
};

static Json::Value proxy_callback(const string &name, const Json::Value &args);
static Json::Value repackage_flights(const vector<Json::Value> &flights);
static void report_result(const Json::Value &arg1,
                          const Json::Value &arg2=Json::Value::null,
                          const Json::Value &arg3=Json::Value::null);

#ifndef THREADED
typedef habitat::Uploader TestSubject;
typedef string r_string;
typedef Json::Value r_json;
static TestSubject *proxy_constructor(Json::Value command);
#else
class TestUploaderThread : public habitat::UploaderThread
{
    void log(const string &message) { report_result("log", message); };

    void saved_id(const string &type, const string &id)
        { report_result("return", id); };

    void initialised() { report_result("return"); };

    void reset_done() { report_result("return"); };

    void caught_exception(const runtime_error &error)
        { report_result("error", "runtime_error", error.what()); }

    void caught_exception(const invalid_argument &error)
        { report_result("error", "invalid_argument", error.what()); }

    void got_flights(const vector<Json::Value> &flights)
        { report_result("return", repackage_flights(flights)); }
};

typedef TestUploaderThread TestSubject;
typedef void r_string;
typedef void r_json;
static void proxy_constructor(TestSubject *u, Json::Value command);
static void proxy_reset(TestSubject *u);
#endif

static r_string proxy_listener_info(TestSubject *u, Json::Value command);
static r_string proxy_listener_telemetry(TestSubject *u, Json::Value command);
static r_string proxy_payload_telemetry(TestSubject *u, Json::Value command);
static r_json proxy_flights(TestSubject *u);

static EZ::cURLGlobal cgl;
static EZ::Mutex cout_lock;
static SafeValue<bool> enable_callbacks(false);
static SafeValue<int> last_time(10000);

#ifdef THREADED
static EZ::Queue<Json::Value> callback_responses;
#endif

int main(int argc, char **argv)
{
#ifndef THREADED
    auto_ptr<habitat::Uploader> u;
#else
    enable_callbacks.set(true);
    TestSubject thread;
    thread.start();
#endif

    for (;;)
    {
        char line[1024];
        cin.getline(line, 1024);

        if (line[0] == '\0')
        {
#ifdef THREADED
            enable_callbacks.set(false);
#endif
            break;
        }

        Json::Reader reader;
        Json::Value command;

        if (!reader.parse(line, command, false))
            throw runtime_error("JSON parsing failed");

        if (!command.isArray() || !command[0u].isString())
            throw runtime_error("Invalid JSON input");

        string command_name = command[0u].asString();

#ifndef THREADED
        if (!u.get() && command_name != "init")
            throw runtime_error("You must initialise it first");

        enable_callbacks.set(true);

        try
        {
            Json::Value return_value;

            if (command_name == "init")
                u.reset(proxy_constructor(command));
            else if (command_name == "listener_info")
                return_value = proxy_listener_info(u.get(), command);
            else if (command_name == "listener_telemetry")
                return_value = proxy_listener_telemetry(u.get(), command);
            else if (command_name == "payload_telemetry")
                return_value = proxy_payload_telemetry(u.get(), command);
            else if (command_name == "flights")
                return_value = proxy_flights(u.get());
            else
                throw runtime_error("invalid command name");

            if (command_name != "init")
                report_result("return", return_value);
            else
                report_result("return");
        }
        catch (runtime_error e)
        {
            if (e.what() == string("invalid command name"))
                throw;

            report_result("error", "runtime_error", e.what());
        }
        catch (invalid_argument e)
        {
            report_result("error", "invalid_argument", e.what());
        }
        catch (...)
        {
            report_result("error", "unknown_error");
        }

        enable_callbacks.set(false);
#else
        if (command_name == "init")
            proxy_constructor(&thread, command);
        else if (command_name == "reset")
            proxy_reset(&thread);
        else if (command_name == "listener_info")
            proxy_listener_info(&thread, command);
        else if (command_name == "listener_telemetry")
            proxy_listener_telemetry(&thread, command);
        else if (command_name == "payload_telemetry")
            proxy_payload_telemetry(&thread, command);
        else if (command_name == "flights")
            proxy_flights(&thread);
        else if (command_name == "return")
            callback_responses.put(command);
#endif
    }

#ifdef THREADED
    thread.shutdown();
#endif

    return 0;
}

time_t time(time_t *t) throw()
{
    time_t value;

    if (!enable_callbacks.get())
    {
        value = last_time.get();
    }
    else
    {
        Json::Value result = proxy_callback("time", Json::Value::null);

        if (!result.isInt())
            throw runtime_error("invalid callback response");

        value = result.asInt();
    }

    last_time.set(value);

    if (t)
        *t = value;

    return value;
}

static Json::Value proxy_callback(const string &name, const Json::Value &args)
{
    report_result("callback", name, args);

#ifndef THREADED
    char line[1024];
    cin.getline(line, 1024);

    Json::Reader reader;
    Json::Value response;

    if (!reader.parse(line, response, false))
        throw runtime_error("JSON parsing failed");

    if (!response.isArray() || !response[0u].isString())
        throw runtime_error("Invalid callback response");

    if (response[0u].asString() != "return")
        throw runtime_error("Callback failed");
#else
    Json::Value response = callback_responses.get();
#endif

    return response[1u];
}

#ifndef THREADED
static habitat::Uploader *proxy_constructor(Json::Value command)
#else
static void proxy_constructor(TestSubject *u, Json::Value command)
#endif
{
    const Json::Value &callsign = command[1u];
    const Json::Value &couch_uri = command[2u];
    const Json::Value &couch_db = command[3u];
    const Json::Value &max_merge_attempts = command[4u];

    /* .isString is checked when .asString is used. */
    if (!max_merge_attempts.isNull() && !max_merge_attempts.isInt())
        throw invalid_argument("max_merge_attempts");

#ifndef THREADED
#define construct_it(...) do { return new TestSubject(__VA_ARGS__); } while (0)
#else
#define construct_it(...) do { u->settings(__VA_ARGS__); } while (0)
#endif

    if (max_merge_attempts.isNull() && couch_db.isNull() &&
        couch_uri.isNull())
    {
        construct_it(callsign.asString());
    }
    else if (max_merge_attempts.isNull() && couch_db.isNull())
    {
        construct_it(callsign.asString(), couch_uri.asString());
    }
    else if (max_merge_attempts.isNull())
    {
        construct_it(callsign.asString(), couch_uri.asString(),
                     couch_db.asString());
    }
    else
    {
        construct_it(callsign.asString(), couch_uri.asString(),
                     couch_db.asString(), max_merge_attempts.asInt());
    }

#undef construct_it
}

#ifdef THREADED
static void proxy_reset(TestSubject *u)
{
    u->reset();
}
#endif

static r_string proxy_listener_info(TestSubject *u, Json::Value command)
{
    const Json::Value &data = command[1u];
    const Json::Value &tc = command[2u];

    if (tc.isNull())
        return u->listener_info(data);
    else
        return u->listener_info(data, tc.asInt());
}

static r_string proxy_listener_telemetry(TestSubject *u, Json::Value command)
{
    const Json::Value &data = command[1u];
    const Json::Value &tc = command[2u];

    if (tc.isNull())
        return u->listener_telemetry(data);
    else
        return u->listener_telemetry(data, tc.asInt());
}

static r_string proxy_payload_telemetry(TestSubject *u, Json::Value command)
{
    const Json::Value &data = command[1u];
    const Json::Value &metadata = command[2u];
    const Json::Value &tc = command[3u];

    if (tc.isNull() && metadata.isNull())
        return u->payload_telemetry(data.asString());
    else if (tc.isNull())
        return u->payload_telemetry(data.asString(), metadata);
    else
        return u->payload_telemetry(data.asString(), metadata, tc.asInt());
}

static r_json proxy_flights(TestSubject *u)
{
#ifndef THREADED
    vector<Json::Value> *result = u->flights();
    auto_ptr< vector<Json::Value> > destroyer(result);
    return repackage_flights(*result);
#else
    u->flights();
#endif
}

static Json::Value repackage_flights(const vector<Json::Value> &flights)
{
    Json::Value list(Json::arrayValue);
    vector<Json::Value>::const_iterator it;
    for (it = flights.begin(); it != flights.end(); it++)
        list.append(*it);
    return list;
}

static void report_result(const Json::Value &arg1, const Json::Value &arg2,
                          const Json::Value &arg3)
{
    Json::Value report(Json::arrayValue);

    report.append(arg1);

    if (!arg2.isNull())
    {
        report.append(arg2);

        if (!arg3.isNull())
        {
            report.append(arg3);
        }
    }

    Json::FastWriter writer;

    {
        EZ::MutexLock lock(cout_lock);
        cout << writer.write(report);
        cout.flush();
    }
}
