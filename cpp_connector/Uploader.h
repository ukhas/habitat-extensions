/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE. */

#ifndef HABITATCPP_UPLOADER_H
#define HABITATCPP_UPLOADER_H

#include <iostream>
#include <string>

using namespace std;

namespace habitat {

class Uploader
{
    string callsign;
    string couch_uri;
    string couch_db;
    int max_merge_attempts;

public:
    Uploader(string callsign, string couch_uri="http://habhub.org",
             string couch_db="habitat", int max_merge_attempts=20);
    ~Uploader();
    void payload_telemetry(string string, double frequency=-1,
                           int time_created=-1);
    string listener_telemetry(time_t time, double latitude,
                              double longitude, int altitude,
                              int time_created=-1);
    string listener_info(string name, string location, string radio,
                         string antenna, int time_created=-1);
};

} /* namespace habitat */

#endif
