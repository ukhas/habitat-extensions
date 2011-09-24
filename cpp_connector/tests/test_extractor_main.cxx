/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE. */

#include <iostream>
#include <stdexcept>
#include <json/json.h>

#include "Extractor.h"
#include "UKHASExtractor.h"

using namespace std;

class JsonIOExtractorManager : public habitat::ExtractorManager
{
    void write(const string &name, const Json::Value &arg)
    {
        Json::Value root(Json::arrayValue);
        root.append(name);
        root.append(arg);

        Json::FastWriter writer;
        cout << writer.write(root);
    };

public:
    JsonIOExtractorManager(habitat::UploaderThread &u)
        : habitat::ExtractorManager(u) {};
    void status(const string &msg) { write("status", msg); };
    void data(const Json::Value &d) { write("data", d); };
};

/* This differs from an auto_ptr: it deletes whatever is pointed to,
 * even if that pointer changes */
template<typename T>
class DeleteLater
{
    T **the_pointer;
public:
    DeleteLater(T **p) : the_pointer(p) {};
    ~DeleteLater() { delete *the_pointer; };
};

void handle_command(const Json::Value &command,
                    JsonIOExtractorManager &manager,
                    habitat::UKHASExtractor &extractor);

int main(int argc, char **argv)
{
    habitat::UploaderThread thread;
    JsonIOExtractorManager manager(thread);
    habitat::UKHASExtractor extractor;
    DeleteLater<Json::Value> destroyer(&(manager.current_payload));

    for (;;)
    {
        char line[1024];
        cin.getline(line, 1024);

        if (line[0] == '\0')
            break;

        Json::Reader reader;
        Json::Value command;

        if (!reader.parse(line, command, false))
            throw runtime_error("JSON parsing failed");

        if (!command.isArray() || !command[0u].isString())
            throw runtime_error("Invalid JSON input");

        handle_command(command, manager, extractor);
    }
}

void handle_command(const Json::Value &command,
                    JsonIOExtractorManager &manager,
                    habitat::UKHASExtractor &extractor)
{
    string command_name = command[0u].asString();
    const Json::Value &arg = command[1u];

    if (command_name == "add")
    {
        if (arg != "UKHASExtractor")
            throw runtime_error("Only UKHASExtractor implemented");

        manager.add(extractor);
    }
    else if (command_name == "skipped")
    {
        if (!arg.isInt())
            throw runtime_error("Invalid JSON input");

        manager.skipped(arg.asInt());
    }
    else if (command_name == "push")
    {
        const Json::Value &arg2 = command[2u];

        if (!arg.isString() || arg.asString().length() != 1)
            throw runtime_error("Invalid JSON input");

        if (arg2.isInt())
            manager.push(arg.asString()[0],
                         static_cast<enum habitat::push_flags>(arg2.asInt()));
        else if (arg2.isNull())
            manager.push(arg.asString()[0]);
        else
            throw runtime_error("Invalid JSON input");
    }
    else if (command_name == "set_current_payload")
    {
        delete manager.current_payload;
        manager.current_payload = new Json::Value(arg);
    }
    else
    {
        throw runtime_error("Invalid JSON input");
    }
}
