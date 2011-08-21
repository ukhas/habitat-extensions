/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE. */

#include <iostream>
#include <memory>
#include <stdexcept>
#include <ctime>

#include "EZ.h"
#include "CouchDB.h"
#include "Uploader.h"

using namespace std;

void add_info(Json::Value &data)
{
    data["name"] = "Daniel Richman";
    data["location"] = "Testing habitat cpp_uploader";
    data["radio"] = "Test radio";
    data["antenna"] = "Massive yagi";
}

void add_telemetry(Json::Value &data)
{
    struct tm tm;
    time_t now = time(NULL);
    gmtime_r(&now, &tm);

    data["time"] = Json::Value(Json::objectValue);
    data["time"]["hour"] = tm.tm_hour;
    data["time"]["minute"] = tm.tm_min;
    data["time"]["second"] = tm.tm_sec;
    data["latitude"] = -24.456142;
    data["longitude"] = 142.150205;
    data["altitude"] = 123;
}

int main(int argc, char **argv)
{
    EZ::cURLGlobal cgl;

    try
    {
        habitat::Uploader u("M0ZDR", "http://localhost:5984", "habitat");
        Json::Value info, telemetry;

        add_info(info);
        u.listener_info(info);

        add_telemetry(telemetry);
        u.listener_telemetry(telemetry);

        u.payload_telemetry("Hello, completed C++ !");
    }
    catch (runtime_error e)
    {
        cout << "Threw: " << e.what() << endl;
    }

    return 0;
}
