/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE. */

#include "EZ.h"
#include <curl/curl.h>
#include <pthread.h>
#include <string>
#include <memory>
#include <stdexcept>
#include <sstream>

using namespace std;

namespace EZ {

Mutex::Mutex()
{
    int result = pthread_mutex_init(&mutex, NULL);

    if (result != 0)
        throw runtime_error("Failed to create mutex");
}

Mutex::~Mutex()
{
    pthread_mutex_destroy(&mutex);
}

MutexLock::MutexLock(Mutex &_m) : m(_m)
{
    pthread_mutex_lock(&(m.mutex));
}

MutexLock::~MutexLock()
{
    pthread_mutex_unlock(&(m.mutex));
}

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

static string http_response_string(long r, string u)
{
    stringstream ss;
    ss << "EZ::HTTPResponse: HTTP " << r << " (" << u << ")";
    return ss.str();
}

HTTPResponse::HTTPResponse(long r, string u)
    : runtime_error(http_response_string(r, u)), response_code(r), url(u) {}

cURL::cURL()
{
    curl = curl_easy_init();

    if (curl == NULL)
    {
        throw runtime_error("Failed to create curl");
    }
}

cURL::~cURL()
{
    curl_easy_cleanup(curl);
}

string cURL::escape(const string &s)
{
    char *result;

    /* cURL wants a handle passed to easy escape for some reason.
     * As far as I can tell it doesn't use it... */
    cURL escaper;
    MutexLock lock(escaper.mutex);

    result = curl_easy_escape(escaper.curl, s.c_str(), s.length());

    if (result == NULL)
        throw runtime_error("curl_easy_escape failed");

    string result_string(result);
    curl_free(result);

    return result_string;
}

string cURL::query_string(const map<string,string> &options,
                          bool add_questionmark)
{
    string result;

    if (add_questionmark)
        result.append("?");

    map<string,string>::const_iterator it;

    for (it = options.begin(); it != options.end(); it++)
    {
        if (it != options.begin())
            result.append("&");

        string key_escaped = escape((*it).first);
        string value_escaped = escape((*it).second);

        result.append(key_escaped);
        result.append("=");
        result.append(value_escaped);
    }

    return result;
}

void cURL::reset()
{
    curl_easy_reset(curl);
}

template<typename T> void cURL::setopt(CURLoption option, T parameter)
{
    CURLcode result = curl_easy_setopt(curl, option, parameter);
    if (result != CURLE_OK)
        throw cURLError(result, "curl_easy_setopt");
}

string cURL::get(const string &url)
{
    MutexLock lock(mutex);

    reset();
    return cURL::perform(url);
}

string cURL::post(const string &url, const string &data)
{
    MutexLock lock(mutex);

    reset();
    setopt(CURLOPT_POSTFIELDS, data.c_str());
    setopt(CURLOPT_POSTFIELDSIZE, data.length());

    return cURL::perform(url);
}

struct read_func_userdata
{
    const string *data;
    size_t written;
};

static size_t read_func(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct read_func_userdata *source =
        static_cast<struct read_func_userdata *>(userdata);
    char *target = static_cast<char *>(ptr);
    size_t max = size * nmemb;
    size_t remaining = source->data->length() - source->written;

    size_t write = remaining;
    if (write > max)
        write = max;

    if (write)
    {
        source->data->copy(target, write, source->written);
        source->written += write;
    }

    return write;
}

string cURL::put(const string &url, const string &data)
{
    MutexLock lock(mutex);

    reset();
    setopt(CURLOPT_UPLOAD, 1);

    struct read_func_userdata userdata;
    userdata.data = &data;
    userdata.written = 0;

    setopt(CURLOPT_READFUNCTION, read_func);
    setopt(CURLOPT_READDATA, &userdata);
    setopt(CURLOPT_INFILESIZE, data.length());

    return cURL::perform(url);
}

static size_t write_func(char *data, size_t size, size_t nmemb, void *userdata)
{
    size_t length = size * nmemb;
    string *target = static_cast<string *>(userdata);

    target->append(data, length);
    return length;
}

string cURL::perform(const string &url)
{
    string response;

    setopt(CURLOPT_URL, url.c_str());
    setopt(CURLOPT_WRITEFUNCTION, write_func);
    setopt(CURLOPT_WRITEDATA, &response);

    CURLcode result;
    result = curl_easy_perform(curl);
    if (result != CURLE_OK)
        throw cURLError(result, "curl_easy_perform");

    long response_code;
    result = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (result != CURLE_OK)
        throw cURLError(result, "curl_easy_getinfo");

    if (response_code < 200 || response_code > 299)
        throw HTTPResponse(response_code, url);

    return response;
}

} /* namespace EZ */
