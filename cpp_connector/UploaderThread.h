/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE. */

#ifndef DLFLDIGI_HABITAT_UPLOADERTHREAD_H
#define DLFLDIGI_HABITAT_UPLOADERTHREAD_H

#include "EZ.h"
#include "Uploader.h"
#include <deque>
#include <memory>
#include <json/json.h>

using namespace std;

/* Add some more EZ stuff. */
namespace EZ {

class ConditionVariable : public Mutex
{
    pthread_cond_t condvar;

public:
    ConditionVariable();
    ~ConditionVariable();

    /* You *need* to have the mutex to do this!
     * Create an EZ::MutexLock on the ConditionVariable */
    void wait();
    void timedwait(const struct timespec *abstime);
    void signal();
    void broadcast();
};

template <typename item>
class Queue
{
    ConditionVariable condvar;
    deque<item> item_deque;

public:
    void put(item &x);
    item get();
};

template <typename item> 
void Queue<item>::put(item &x)
{
    MutexLock lock(condvar);
    item_deque.push_back(x);
    condvar.signal();
}

template <typename item>
item Queue<item>::get()
{
    MutexLock lock(condvar);

    while (!item_deque.size())
        condvar.wait();

    item x = item_deque.front();
    item_deque.pop_front();
    return x;
}

class ThreadAttr
{
    pthread_attr_t attr;
    friend class SimpleThread;

public:
    ThreadAttr();
    ~ThreadAttr();
};

class SimpleThread
{
    Mutex mutex;
    pthread_t thread;
    bool started;
    bool joined;
    void *exit_arg;

public:
    SimpleThread();
    virtual ~SimpleThread();

    virtual void *run();
    void start();
    void *join();
};

} /* namespace EZ */

namespace dlfldigi_habitat {

class UploaderThread;

class UploaderAction
{
protected:
    string result;
    void check(habitat::Uploader *u);

private:
    virtual void apply(UploaderThread &uthr);
    virtual string describe();

    friend class UploaderThread;

public:
    virtual ~UploaderAction() {};
};

class UploaderSettings : public UploaderAction
{
    const string callsign, couch_uri, couch_db;
    const int max_merge_attempts;

    UploaderSettings(const string &ca, const string &co_u,
                     const string &co_db, int mx)
        : callsign(ca), couch_uri(co_u), couch_db(co_db),
          max_merge_attempts(mx)
        {};
    ~UploaderSettings() {};

    void apply(UploaderThread &uthr);
    string describe();

    friend class UploaderThread;
};

class UploaderPayloadTelemetry : public UploaderAction
{
    const string data;
    const Json::Value metadata;
    const int time_created;

    UploaderPayloadTelemetry(const string &da, const Json::Value &mda,
                             const int tc)
        : data(da), metadata(mda), time_created(tc) {};
    ~UploaderPayloadTelemetry() {};

    void apply(UploaderThread &uthr);
    string describe();

    friend class UploaderThread;
};

class UploaderListenerTelemetry : public UploaderAction
{
    const Json::Value data;
    const int time_created;

    UploaderListenerTelemetry(const Json::Value &da, int tc)
        : data(da), time_created(tc) {};
    ~UploaderListenerTelemetry() {};

    void apply(UploaderThread &uthr);
    string describe();

    friend class UploaderThread;
};

class UploaderListenerInfo : public UploaderAction
{
    const Json::Value &data;
    const int time_created;

    UploaderListenerInfo(const Json::Value &da, int tc)
        : data(da), time_created(tc) {};
    ~UploaderListenerInfo() {};

    void apply(UploaderThread &uthr);
    string describe();

    friend class UploaderThread;
};

class UploaderFlights : public UploaderAction
{
    void apply(UploaderThread &uthr);
    string describe();
    friend class UploaderThread;
};

class UploaderShutdown : public UploaderAction
{
    void apply(UploaderThread &uthr);
    string describe();
    friend class UploaderThread;
};

class UploaderThread : public EZ::SimpleThread
{
    EZ::Queue<UploaderAction *> queue;
    auto_ptr<habitat::Uploader> uploader;

    bool queued_shutdown;

    void queue_action(UploaderAction *ac);

    friend class UploaderAction;
    friend class UploaderSettings;
    friend class UploaderPayloadTelemetry;
    friend class UploaderListenerTelemetry;
    friend class UploaderListenerInfo;
    friend class UploaderFlights;

public:
    UploaderThread();
    virtual ~UploaderThread();

    void settings(const string &callsign,
                  const string &couch_uri="http://habhub.org",
                  const string &couch_db="habitat",
                  int max_merge_attempts=20);
    void payload_telemetry(const string &data,
                           const Json::Value &metadata=Json::Value::null,
                           int time_created=-1);
    void listener_telemetry(const Json::Value &data, int time_created=-1);
    void listener_info(const Json::Value &data, int time_created=-1);
    void flights();
    void shutdown();

    void *run();
    void detach();

    virtual void log(const string &message);
    virtual void got_flights(const vector<Json::Value> &flights);
};

} /* namespace habitat */

#endif
