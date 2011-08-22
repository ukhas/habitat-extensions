/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE. */

#include <iostream>
#include <memory>
#include <stdexcept>
#include <ctime>

#include "EZ.h"
#include "Uploader.h"

using namespace std;

static habitat::Uploader *proxy_constructor(Json::Value command);
static string proxy_listener_info(habitat::Uploader *u, Json::Value command);
static string proxy_listener_telemetry(habitat::Uploader *u,
                                       Json::Value command);
static string proxy_payload_telemetry(habitat::Uploader *u,
                                      Json::Value command);
static void report_result(const string &arg1, const string &arg2="",
                          const string &arg3="");

int main(int argc, char **argv)
{
    EZ::cURLGlobal cgl;

    auto_ptr<habitat::Uploader> u;

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

        string command_name = command[0u].asString();
        if (!u.get() && command_name != "init")
            throw runtime_error("You must initialise it first");

        try
        {
            string return_value;

            if (command_name == "init")
                u.reset(proxy_constructor(command));
            else if (command_name == "listener_info")
                return_value = proxy_listener_info(u.get(), command);
            else if (command_name == "listener_telemetry")
                return_value = proxy_listener_telemetry(u.get(), command);
            else if (command_name == "payload_telemetry")
                return_value = proxy_payload_telemetry(u.get(), command);
            else
                throw runtime_error("invalid command name");

            if (command_name != "init")
                report_result("return", return_value);
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
    }

    return 0;
}

static habitat::Uploader *proxy_constructor(Json::Value command)
{
    const Json::Value &callsign = command[1u];
    const Json::Value &couch_uri = command[2u];
    const Json::Value &couch_db = command[3u];
    const Json::Value &max_merge_attempts = command[4u];

    /* all other args will be verified by the constructor */
    if (!max_merge_attempts.isNull() &&
        !max_merge_attempts.isInt())
    {
        throw invalid_argument("max_merge_attempts");
    }

    if (max_merge_attempts.isNull() && couch_db.isNull() &&
        couch_uri.isNull())
    {
        return new habitat::Uploader(callsign.asString());
    }
    else if (max_merge_attempts.isNull() && couch_db.isNull())
    {
        return new habitat::Uploader(callsign.asString(),
                                     couch_uri.asString());
    }
    else if (max_merge_attempts.isNull())
    {
        return new habitat::Uploader(callsign.asString(),
                                     couch_uri.asString(),
                                     couch_db.asString());
    }
    else
    {
        return new habitat::Uploader(callsign.asString(),
                                     couch_uri.asString(),
                                     couch_db.asString(),
                                     max_merge_attempts.asInt());
    }
}

static string proxy_listener_info(habitat::Uploader *u, Json::Value command)
{
    const Json::Value &data = command[1u];
    const Json::Value &tc = command[2u];

    if (tc.isNull())
        return u->listener_info(data);
    else
        return u->listener_info(data, tc.asInt());
}

static string proxy_listener_telemetry(habitat::Uploader *u,
                                       Json::Value command)
{
    const Json::Value &data = command[1u];
    const Json::Value &tc = command[2u];

    if (tc.isNull())
        return u->listener_telemetry(data);
    else
        return u->listener_telemetry(data, tc.asInt());
}

static string proxy_payload_telemetry(habitat::Uploader *u,
                                      Json::Value command)
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

static void report_result(const string &arg1, const string &arg2,
                          const string &arg3)
{
    Json::Value report(Json::arrayValue);

    report.append(arg1);

    if (arg2.length())
    {
        report.append(arg2);

        if (arg3.length())
        {
            report.append(arg3);
        }
    }

    Json::FastWriter writer;
    cout << writer.write(report);
}
