#ifndef HABITAT_TEST_EXTRACTOR_MOCKS_H
#define HABITAT_TEST_EXTRACTOR_MOCKS_H

/* Prevent EZ.h inclusion */
#define HABITAT_EZ_H

#include <assert.h>

namespace EZ {

/* The tests are single threaded, so this works */

class Mutex
{
protected:
    int lock_count;
    friend class MutexLock;

public:
    Mutex() : lock_count(0) {};
    ~Mutex() {};
};

class MutexLock
{
    Mutex &m;

public:
    MutexLock(Mutex &_m) : m(_m) { m.lock_count++; };
    ~MutexLock() { m.lock_count--; assert(m.lock_count >= 0); };
};

}

/* Prevent inclusion of real UploaderThread */
#define HABITAT_UPLOADERTHREAD_H

#include <iostream>
#include <string>
#include <algorithm>
#include <json/json.h>

namespace habitat {

static void inplace_nonulls(char &c)
{
    /* JsonCPP doesn't support '\0' in strings :'-( */
    if (c == '\0')
        c = '\1';
}

class UploaderThread
{
public:
    void payload_telemetry(const std::string &data,
                           const Json::Value &metadata=Json::Value::null,
                           int time_created=-1)
    {
        std::string data_copy(data);
        std::for_each(data_copy.begin(), data_copy.end(), inplace_nonulls);

        Json::Value root(Json::arrayValue);
        root.append("upload");
        root.append(data_copy);
        root.append(metadata);
        root.append(time_created);

        Json::FastWriter writer;
        std::cout << writer.write(root);
    };
};

} /* namespace habitat */

#endif /* HABITAT_TEST_EXTRACTOR_MOCK_UPLOADERTHREAD_H */
