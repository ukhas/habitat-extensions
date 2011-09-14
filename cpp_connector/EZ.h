/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE. */

#ifndef HABITATCPP_EZ_H
#define HABITATCPP_EZ_H

#include <string>
#include <iostream>
#include <stdexcept>
#include <map>
#include <deque>
#include <curl/curl.h>
#include <pthread.h>

using namespace std;

namespace EZ {

class Mutex
{
protected:
    pthread_mutex_t mutex;

    friend class MutexLock;

public:
    Mutex();
    ~Mutex();
};

class MutexLock
{
    Mutex &m;

public:
    MutexLock(Mutex &_m);
    ~MutexLock();
};

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

    virtual void *run() = 0;
    void start();
    void *join();
};

class cURL
{
    Mutex mutex;
    CURL *curl;

    /* You need to hold the mutex to use these four functions */
    void reset();
    string perform(const string &url);
    template<typename T> void setopt(CURLoption option, T paramater);
    void setopt(CURLoption option, void *paramater);
    void setopt(CURLoption option, long parameter);

public:
    cURL();
    ~cURL();
    static string escape(const string &s);
    static string query_string(const map<string,string> &options,
                               bool add_questionmark=false);
    string get(const string &url);
    string post(const string &url, const string &data);
    string put(const string &url, const string &data);
};

class cURLGlobal
{
public:
    cURLGlobal() { curl_global_init(CURL_GLOBAL_ALL); };
    ~cURLGlobal() { curl_global_cleanup(); };
};

class cURLslist
{
    struct curl_slist *slist;

public:
    cURLslist() { slist = NULL; };
    void append(const char *s) { curl_slist_append(slist, s); };
    ~cURLslist() { curl_slist_free_all(slist); };
    const struct curl_slist *get() { return slist; };
};

class cURLError : public runtime_error
{
    cURLError(const string &what)
        : runtime_error("EZ::cURLError: " + what),
          error(CURLE_OK), function("") {};

    cURLError(CURLcode error, const string &function)
        : runtime_error("EZ::cURLError: " + function + ": " +
                        curl_easy_strerror(error)),
          error(error), function(function) {};

    friend class cURL;

public:
    const CURLcode error;
    const string function;
    ~cURLError() throw() {};
};

class HTTPResponse : public runtime_error
{
    HTTPResponse(long r, string u);
    friend class cURL;

public:
    const long response_code;
    const string url;
    ~HTTPResponse() throw() {};
};

} /* namespace EZ */

#endif /* HABITATCPP_EZ_H */
