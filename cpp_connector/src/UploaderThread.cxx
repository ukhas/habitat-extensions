/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE. */

#include "UploaderThread.h"
#include <stdexcept>
#include <sstream>

namespace habitat {

void UploaderAction::check(habitat::Uploader *u)
{
    if (u == NULL)
        throw runtime_error("Uploader settings were not initialised");
}

void UploaderSettings::apply(UploaderThread &uthr)
{
    uthr.uploader.reset(new habitat::Uploader(
        callsign, couch_uri, couch_db, max_merge_attempts));
    uthr.initialised();
}

string UploaderSettings::describe()
{
    stringstream ss(stringstream::out);
    ss << "Uploader('" << callsign << "', '" << couch_uri << "', '"
       << couch_db << "', " << max_merge_attempts << ")";
    return ss.str();
}

void UploaderReset::apply(UploaderThread &uthr)
{
    uthr.uploader.reset();
    uthr.reset_done();
}

string UploaderReset::describe()
{
    return "~Uploader()";
}

void UploaderPayloadTelemetry::apply(UploaderThread &uthr)
{
    check(uthr.uploader.get());

    string result;
    result = uthr.uploader->payload_telemetry(data, metadata, time_created);

    uthr.saved_id("payload_telemetry", result);
}

string UploaderPayloadTelemetry::describe()
{
    stringstream ss(stringstream::out);
    Json::FastWriter writer;
    string metadata_json = writer.write(metadata);
    metadata_json.erase(metadata_json.length() - 1, 1);
    ss << "Uploader.payload_telemetry('" << data << "', "
       << metadata_json << ", " << time_created << ")";
    return ss.str();
}

void UploaderListenerTelemetry::apply(UploaderThread &uthr)
{
    check(uthr.uploader.get());
    string result = uthr.uploader->listener_telemetry(data, time_created);
    uthr.saved_id("listener_telemetry", result);
}

string UploaderListenerTelemetry::describe()
{
    stringstream ss(stringstream::out);
    Json::FastWriter writer;
    string data_json = writer.write(data);
    data_json.erase(data_json.length() - 1, 1);
    ss << "Uploader.listener_telemetry(" << data_json << ", " 
       << time_created << ")";
    return ss.str();
}

void UploaderListenerInfo::apply(UploaderThread &uthr)
{
    check(uthr.uploader.get());
    string result = uthr.uploader->listener_info(data, time_created);
    uthr.saved_id("listener_info", result);
}

string UploaderListenerInfo::describe()
{
    stringstream ss(stringstream::out);
    Json::FastWriter writer;
    string data_json = writer.write(data);
    data_json.erase(data_json.length() - 1, 1);
    ss << "Uploader.listener_info(" << data_json << ", " 
       << time_created << ")";
    return ss.str();
}

void UploaderFlights::apply(UploaderThread &uthr)
{
    check(uthr.uploader.get());
    auto_ptr< vector<Json::Value> > flights;
    flights.reset(uthr.uploader->flights());
    uthr.got_flights(*flights);
}

string UploaderFlights::describe()
{
    return "Uploader.flights()";
}

void UploaderShutdown::apply(UploaderThread &uthr)
{
    throw this;
}

string UploaderShutdown::describe()
{
    return "Shutdown";
}

UploaderThread::UploaderThread() : queued_shutdown(false) {}

UploaderThread::~UploaderThread()
{
    EZ::MutexLock lock(mutex);

    if (!queued_shutdown)
        shutdown();

    join();
}

void UploaderThread::queue_action(UploaderAction *action)
{
    auto_ptr<UploaderAction> destroyer(action);

    log("Queuing " + action->describe());
    queue.put(action);
    destroyer.release();
}

void UploaderThread::settings(const string &callsign, const string &couch_uri,
                              const string &couch_db, int max_merge_attempts)
{
    queue_action(
        new UploaderSettings(callsign, couch_uri, couch_db, max_merge_attempts)
    );
}

void UploaderThread::reset()
{
    queue_action(new UploaderReset());
}

void UploaderThread::payload_telemetry(const string &data,
                                       const Json::Value &metadata,
                                       int time_created)
{
    queue_action(new UploaderPayloadTelemetry(data, metadata, time_created));
}

void UploaderThread::listener_telemetry(const Json::Value &data,
                                        int time_created)
{
    queue_action(new UploaderListenerTelemetry(data, time_created));
}

void UploaderThread::listener_info(const Json::Value &data,
                                   int time_created)
{
    queue_action(new UploaderListenerInfo(data, time_created));
}

void UploaderThread::flights()
{
    queue_action(new UploaderFlights());
}

void UploaderThread::shutdown()
{
    /* Borrow the SimpleThread mutex to make queued_shutdown access safe */
    EZ::MutexLock lock(mutex);

    if (!queued_shutdown)
    {
        queue_action(new UploaderShutdown());
        queued_shutdown = true;
    }
}

void *UploaderThread::run()
{
    log("Started");

    for (;;)
    {
        auto_ptr<UploaderAction> action(queue.get());

        log("Running " + action->describe());

        try
        {
            action->apply(*this);
        }
        catch (UploaderShutdown *s)
        {
            break;
        }
        catch (runtime_error e)
        {
            caught_exception(e);
            continue;
        }
        catch (invalid_argument e)
        {
            caught_exception(e);
            continue;
        }
    }

    log("Shutting down");

    return NULL;
}

void UploaderThread::warning(const string &message)
{
    log("Warning: " + message);
}

void UploaderThread::saved_id(const string &type, const string &id)
{
    log("Saved " + type + " doc: " + id);
}

void UploaderThread::initialised()
{
    log("Initialised Uploader");
}

void UploaderThread::reset_done()
{
    log("Settings reset");
}

void UploaderThread::caught_exception(const runtime_error &error)
{
    const string what(error.what());
    warning("Caught runtime_error: " + what);
}

void UploaderThread::caught_exception(const invalid_argument &error)
{
    const string what(error.what());
    warning("Caught invalid_argument: " + what);
}

void UploaderThread::got_flights(const vector<Json::Value> &flights)
{
    log("Default action: got_flights; discarding.");
}

} /* namespace habitat */
