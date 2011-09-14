#include "UploaderThread.h"
#include "EZ.h"
#include <iostream>

class StderrUploaderThread : public dlfldigi_habitat::UploaderThread
{
    void log(const string &message)
    {
        std::cerr << message << endl;
    };

    void got_flights(const vector<Json::Value> &flights)
    {
        std::cerr << "Got flights!" << endl;

        vector<Json::Value>::const_iterator it;
        for (it = flights.begin(); it != flights.end(); it++)
            std::cerr << (*it);
    };
};

int main(int argc, char **argv)
{
    EZ::cURLGlobal cgl;
    //StderrUploaderThread thread;
    dlfldigi_habitat::UploaderThread thread;

    thread.settings("DANIEL_ASYNC", "http://localhost:5984", "habitat");
    thread.payload_telemetry("Hello from C++ UploaderThread");
    thread.shutdown();

    cerr << "main() out" << endl;
}
