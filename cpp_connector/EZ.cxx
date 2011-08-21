#include "EZ.h"
#include <curl/curl.h>
#include <string>
#include <memory>

using namespace std;

namespace EZ {

Mutex::Mutex()
{
    int result = pthread_mutex_init(&mutex, NULL);

    if (result != 0)
    {
        throw "Failed to create mutex";
    }
}

Mutex::~Mutex()
{
    pthread_mutex_destroy(&mutex);
}

MutexLock::MutexLock(Mutex *_m) : m(_m)
{
    pthread_mutex_lock(&(m->mutex));
}

MutexLock::~MutexLock()
{
    pthread_mutex_unlock(&(m->mutex));
}

cURL::cURL()
{
    curl = curl_easy_init();

    if (curl == NULL)
    {
        throw "Failed to create curl";
    }
}

cURL::~cURL()
{
    curl_easy_cleanup(curl);
}

cURLError::cURLError(const string &info) 
    : error(CURLE_OK), extra(""), info(info) {}

cURLError::cURLError(CURLcode error, const string &extra) 
    : error(error), extra(extra)
{
    info = extra;
    info.append(": ");
    info.append(curl_easy_strerror(error));
}

ostream &operator<<(ostream &o, cURLError &e)
{
    o << e.get_info();
    return o;
}

string *cURL::escape(const string &s)
{
    char *result;
    string *result_string;

    /* We use the singleton because the other mutex could be locked
     * while doing IO, and we want this to be quick. CURL doesn't
     * actually use the handle in any way... */
    static cURL singleton;
    MutexLock lock(&(singleton.mutex));

    result = curl_easy_escape(singleton.curl, s.c_str(), s.length());

    if (result != NULL)
    {
        result_string = new string(result);
        curl_free(result);
    }
    else
    {
        result_string = new string("");
    }

    return result_string;
}

static size_t write_func(char *data, size_t size, size_t nmemb, void *userdata)
{
    size_t length = size * nmemb;
    string *target = static_cast<string *>(userdata);

    target->append(data, length);
    return length;
}

string *cURL::ez(const string &url, const string &data, int post)
{
    MutexLock lock(&mutex);
    CURLcode result;

    result = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (result != CURLE_OK)
        throw cURLError(result, "curl_easy_setopt");

    if (post)
    {
        result = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        if (result != CURLE_OK)
            throw cURLError(result, "curl_easy_setopt");

        result = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                                  (long) data.length());
    }

    result = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_func);
    if (result != CURLE_OK)
        throw cURLError(result, "curl_easy_setopt");

    auto_ptr<string> response(new string);

    result = curl_easy_setopt(curl, CURLOPT_WRITEDATA, response.get());
    if (result != CURLE_OK)
        throw cURLError(result, "curl_easy_setopt");

    result = curl_easy_perform(curl);

    if (result != CURLE_OK)
        throw cURLError(result, "curl_easy_perform");

    return response.release();
}

} /* namespace EZ */
