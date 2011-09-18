#ifndef HABITAT_TEST_EXTRACTOR_MOCK_UPLOADERTHREAD_H
#define HABITAT_TEST_EXTRACTOR_MOCK_UPLOADERTHREAD_H

/* Prevent inclusion of real UploaderThread */
#define HABITAT_UPLOADERTHREAD_H

#include <iostream>
#include <string>
#include <json/json.h>

namespace habitat {

class UploaderThread
{
public:
    void payload_telemetry(const std::string &data,
                           const Json::Value &metadata=Json::Value::null,
                           int time_created=-1)
    {
        Json::Value root(Json::arrayValue);
        root.append("upload");
        root.append(data);
        root.append(metadata);
        root.append(time_created);

        Json::FastWriter writer;
        std::cout << writer.write(root);
    };
};

} /* namespace habitat */

#endif /* HABITAT_TEST_EXTRACTOR_MOCK_UPLOADERTHREAD_H */
