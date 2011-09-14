#include "UploaderThread.h"
#include <stdexcept>
#include <sstream>
#include <pthread.h>

namespace EZ {

ConditionVariable::ConditionVariable() : Mutex()
{
    int result = pthread_cond_init(&condvar, NULL);

    if (result != 0)
        throw runtime_error("Failed to create condvar");
}

ConditionVariable::~ConditionVariable()
{
    pthread_cond_destroy(&condvar);
}

void ConditionVariable::wait()
{
    pthread_cond_wait(&condvar, &mutex);
}

void ConditionVariable::timedwait(const struct timespec *abstime)
{
    pthread_cond_timedwait(&condvar, &mutex, abstime);
}

void ConditionVariable::signal()
{
    pthread_cond_signal(&condvar);
}

void ConditionVariable::broadcast()
{
    pthread_cond_broadcast(&condvar);
}

/* Queue's methods are in UploadThread.h since it's templated */

ThreadAttr::ThreadAttr()
{
    int result = pthread_attr_init(&attr);

    if (result != 0)
        throw runtime_error("Failed to create attr");
}

ThreadAttr::~ThreadAttr()
{
    pthread_attr_destroy(&attr);
}

SimpleThread::SimpleThread()
    : started(false), joined(false), exit_arg(NULL)
    {}

SimpleThread::~SimpleThread()
{
    MutexLock lock(mutex);

    if (started && !joined)
        join();
}

void *thread_starter(void *arg)
{
    SimpleThread *t = static_cast<SimpleThread *>(arg);
    return t->run();
}

void SimpleThread::start()
{
    MutexLock lock(mutex);

    if (started)
        return;

    ThreadAttr attr;
    int result;
    
    result = pthread_attr_setdetachstate(&attr.attr, PTHREAD_CREATE_JOINABLE);

    if (result != 0)
        throw runtime_error("Failed to set detach state");

    result = pthread_create(&thread, &attr.attr, thread_starter, this);

    if (result != 0)
        throw runtime_error("Failed to create a thread");

    started = true;
}

void *SimpleThread::join()
{
    MutexLock lock(mutex);

    if (!started)
        throw runtime_error("Cannot join a thread that hasn't started");

    if (joined)
        return NULL;

    int result = pthread_join(thread, &exit_arg);
    if (result != 0)
        throw runtime_error("Failed to join thread");

    joined = true;
    return exit_arg;
}

void *SimpleThread::run()
{
    throw runtime_error("No thread run() provided");
}

} /* namespace EZ */

namespace dlfldigi_habitat {

void UploaderAction::check(habitat::Uploader *u)
{
    if (u == NULL)
        throw runtime_error("Uploader settings were not initialised");
}

void UploaderAction::apply(UploaderThread &uthr)
{
    throw runtime_error("UploaderAction::apply was not overridden");
}

string UploaderAction::describe()
{
    throw runtime_error("UploaderAction::describe was not overridden");
}

void UploaderSettings::apply(UploaderThread &uthr)
{
    uthr.uploader.reset(new habitat::Uploader(
        callsign, couch_uri, couch_db, max_merge_attempts));
    result = "Success";
}

string UploaderSettings::describe()
{
    stringstream ss(stringstream::out);
    ss << "Uploader('" << callsign << "', '" << couch_uri << "', '"
       << couch_db << "', '" << max_merge_attempts << ")";
    return ss.str();
}

void UploaderPayloadTelemetry::apply(UploaderThread &uthr)
{
    check(uthr.uploader.get());
    result = uthr.uploader->payload_telemetry(data, metadata, time_created);
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
    result = uthr.uploader->listener_telemetry(data, time_created);
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
    result = uthr.uploader->listener_info(data, time_created);
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
    result = "Success";
}

string UploaderFlights::describe()
{
    return "Uploader.flights()";
}

void UploaderShutdown::apply(UploaderThread &uthr)
{
    result = "thrown";
    throw this;
}

string UploaderShutdown::describe()
{
    return "Shutdown";
}

UploaderThread::UploaderThread() : queued_shutdown(false)
{
    start();
}

UploaderThread::~UploaderThread()
{
    if (!queued_shutdown)
        shutdown();
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
    if (!queued_shutdown)
    {
        queue_action(new UploaderShutdown());
        queued_shutdown = true;
    }

    join();
}

void *UploaderThread::run()
{
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
            log("Shutting down");
            break;
        }
        catch (runtime_error e)
        {
            log(string("Caught exception ") + e.what());
            continue;
        }

        log("Finished: " + action->result);
    }

    return NULL;
}

void UploaderThread::log(const string &message)
{
    /* Bin silently */
}

void UploaderThread::got_flights(const vector<Json::Value> &flights)
{
    /* Bin silently */
}

} /* namespace dlfldigi_habitat */
