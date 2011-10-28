/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see COPYING. */

#include "UKHASExtractor.h"
#include <json/json.h>
#include <stdexcept>
#include <string>
#include <sstream>
#include <algorithm>
#include <stdio.h>
#include <stdint.h>

using namespace std;

namespace habitat {

void UKHASExtractor::reset_buffer()
{
    buffer.resize(0);
    buffer.clear();
    buffer.reserve(256);
}

void UKHASExtractor::skipped(int n)
{
    if (n > 20)
        n = 20;

    for (int i = 0; i < n; i++)
        push('\0', PUSH_NONE);
}

void UKHASExtractor::push(char b, enum push_flags flags)
{
    if (last == '$' && b == '$')
    {
        /* Start delimiter: "$$" */
        reset_buffer();
        buffer.push_back(last);
        buffer.push_back(b);

        garbage_count = 0;
        extracting = true;

        mgr->status("UKHAS Extractor: found start delimiter");
    }
    else if (extracting && b == '\n')
    {
        /* End delimiter: "\n" */
        buffer.push_back(b);
        mgr->uthr.payload_telemetry(buffer);

        mgr->status("UKHAS Extractor: extracted string");

        try
        {
            mgr->data(crude_parse());
        }
        catch (runtime_error e)
        {
            mgr->status("UKHAS Extractor: crude parse failed: " +
                        string(e.what()));

            Json::Value bare(Json::objectValue);
            bare["_sentence"] = buffer;
            mgr->data(bare);
        }

        reset_buffer();
        extracting = false;
    }
    else if (extracting)
    {
        /* baudot doesn't support '*', so we use '#'. */
        if ((flags & PUSH_BAUDOT_HACK) && b == '#')
            b = '*';

        buffer.push_back(b);

        if (b < 0x20 || b > 0x7E)
            garbage_count++;

        /* Sane limits to avoid uploading tonnes of garbage */
        if (buffer.length() > 1000 || garbage_count > 16)
        {
            mgr->status("UKHAS Extractor: giving up");

            reset_buffer();
            extracting = false;
        }
    }

    last = b;
}

static void inplace_toupper(char &c)
{
    if (c >= 'a' && c <= 'z')
        c -= 32;
}

static string checksum_xor(const string &s)
{
    uint8_t checksum = 0;
    for (string::const_iterator it = s.begin(); it != s.end(); it++)
        checksum ^= (*it);

    char temp[3];
    snprintf(temp, sizeof(temp), "%.02X", checksum);
    return string(temp);
}

static string checksum_crc16_ccitt(const string &s)
{
    /* From avr-libc docs: Modified BSD (GPL, BSD, DFSG compatible) */
    uint16_t crc = 0xFFFF;

    for (string::const_iterator it = s.begin(); it != s.end(); it++)
    {
        crc = crc ^ ((uint16_t (*it)) << 8);

        for (int i = 0; i < 8; i++)
        {
            bool s = crc & 0x8000;
            crc <<= 1;
            crc ^= (s ? 0x1021 : 0);
        }
    }

    char temp[5];
    snprintf(temp, sizeof(temp), "%.04X", crc);
    return string(temp);
}

static vector<string> split(const string &input, const char c)
{
    vector<string> parts;
    size_t pos = 0, lastpos = 0;

    do
    {
        /* pos returns npos? substr will grab to end of string. */
        pos = input.find_first_of(c, lastpos);

        if (pos == string::npos)
            parts.push_back(input.substr(lastpos));
        else
            parts.push_back(input.substr(lastpos, pos - lastpos));

        lastpos = pos + 1;
    }
    while (pos != string::npos);

    return parts;
}

static void split_string(const string &buffer, string *data, string *checksum)
{
    if (buffer.substr(0, 2) != "$$")
        throw runtime_error("String does not begin with $$");

    if (buffer[buffer.length() - 1] != '\n')
        throw runtime_error("String does not end with '\\n'");

    size_t pos = buffer.find_last_of('*');
    if (pos == string::npos)
        throw runtime_error("No checksum");

    size_t check_start = pos + 1;
    size_t check_end = buffer.length() - 1;
    size_t check_length = check_end - check_start;

    if (check_length != 2 && check_length != 4)
        throw runtime_error("Invalid checksum length");

    size_t data_start = 2;
    size_t data_length = pos - data_start;

    *data = buffer.substr(data_start, data_length);
    *checksum = buffer.substr(check_start, check_length);
}

static string examine_checksum(const string &data, const string &checksum_o)
{
    string checksum = checksum_o;
    for_each(checksum.begin(), checksum.end(), inplace_toupper);

    string expect, name;

    if (checksum.length() == 2)
    {
        expect = checksum_xor(data);
        name = "xor";
    }
    else if (checksum.length() == 4)
    {
        expect = checksum_crc16_ccitt(data);
        name = "crc16-ccitt";
    }
    else
    {
        throw runtime_error("Invalid checksum length");
    }

    if (expect != checksum)
        throw runtime_error("Invalid checksum: expected " + expect);

    return name;
}

static bool is_ddmmmm_field(const Json::Value &field)
{
    if (field["sensor"] != "stdtelem.coordinate")
        return false;

    if (!field["format"].isString())
        return false;

    string format = field["format"].asString();

    /* does it match d+m+\.m+ ? */

    size_t pos;

    pos = format.find_first_not_of('d');
    if (pos == string::npos || format[pos] != 'm')
        return false;

    pos = format.find_first_not_of('m', pos);
    if (pos == string::npos || format[pos] != '.')
        return false;

    pos++;

    pos = format.find_first_not_of('m', pos);
    if (pos != string::npos)
        return false;

    return true;
}

static string convert_ddmmmm(const string &value)
{
    size_t split = value.find('.');
    if (split == string::npos || split <= 2)
        throw runtime_error("invalid '.' pos when converting ddmm");

    string left = value.substr(0, split - 2);
    string right = value.substr(split - 2);

    istringstream lis(left), ris(right);
    double left_val, right_val;
    lis >> left_val;
    ris >> right_val;

    if (lis.fail() || ris.fail() || lis.peek() != EOF || ris.peek() != EOF)
        throw runtime_error("couldn't parse left or right parts (ddmm)");

    if (right_val >= 60 || right_val < 0)
        throw runtime_error("invalid right part (ddmm)");

    double dd = left_val + (right_val / 60);
    
    ostringstream os;
    os.precision(value.length() - value.find_first_not_of("0+-") - 2);
    os << dd;
    return os.str();
}

static void extract_fields(Json::Value &data, const Json::Value &fields,
                           const vector<string> &parts)
{
    vector<string>::const_iterator part = parts.begin() + 1;
    Json::Value::const_iterator field = fields.begin();

    while (field != fields.end() && part != parts.end())
    {
        const string key = (*field)["name"].asString();
        const string value = (*part);

        if (!key.length())
            throw runtime_error("Invalid configuration (empty field name)");

        if (value.length())
        {
            if (is_ddmmmm_field(*field))
                data[key] = convert_ddmmmm(value);
            else
                data[key] = value;
        }

        field++;
        part++;
    }
}

static void check_callsign(const Json::Value &settings,
                           const vector<string> &parts)
{
    if (!parts.size() || !parts[0].size())
        throw runtime_error("Empty callsign");

    if (!settings.isNull())
    {
        const string callsign = settings["payload"].asString();
        if (parts[0] != callsign)
            throw runtime_error("Incorrect callsign");
    }
}

static void cook_basic(Json::Value &basic, const string &buffer,
                       const string &callsign)
{
    basic["_sentence"] = buffer;
    basic["_protocol"] = "UKHAS";
    basic["_parsed"] = true;
    basic["payload"] = callsign;
}

static void attempt_settings(Json::Value &data, const Json::Value &sentence,
                             const string &checksum_name,
                             const vector<string> &parts)
{
    const Json::Value &fields = sentence["fields"];

    if (sentence["checksum"] != checksum_name)
        throw runtime_error("Wrong checksum type");

    if (fields.size() != (parts.size() - 1))
        throw runtime_error("Incorrect number of fields");

    extract_fields(data, fields, parts);
}

/* crude_parse is based on the parse() method of
 * habitat.parser_modules.ukhas_parser.UKHASParser */
Json::Value UKHASExtractor::crude_parse()
{
    const Json::Value *settings_ptr = mgr->payload();

    if (!settings_ptr)
        settings_ptr = &(Json::Value::null);

    const Json::Value &settings = *settings_ptr;

    string data, checksum;
    split_string(buffer, &data, &checksum);

    /* Warning: cpp_connector only supports xor and crc16-ccitt, which
     * conveninently are different lengths, so this works. */
    string checksum_name = examine_checksum(data, checksum);
    vector<string> parts = split(data, ',');
    check_callsign(settings, parts);

    Json::Value basic(Json::objectValue);
    cook_basic(basic, buffer, parts[0]);
    const Json::Value &sentence = settings["sentence"];

    /* If array: multiple sentence settings to try with.
     * No settings? No problem; we can still test the checksum */
    if (!sentence.isNull() && sentence.isArray())
    {
        /* Silence errors, and only log them if all attempts fail */
        vector<string> errors;

        for (Json::Value::iterator it = sentence.begin();
             it != sentence.end(); it++)
        {
            try
            {
                Json::Value data(basic);
                attempt_settings(data, (*it), checksum_name, parts);
                return data;
            }
            catch (runtime_error e)
            {
                errors.push_back(e.what());
            }
        }

        /* Couldn't parse using any of the settings... */
        mgr->status("UKHAS Extractor: full parse failed:");
        for (vector<string>::iterator it = errors.begin();
             it != errors.end(); it++)
        {
            mgr->status("UKHAS Extractor: " + (*it));
        }
    }
    else if (!sentence.isNull() && sentence.isObject())
    {
        try
        {
            Json::Value data(basic);
            attempt_settings(data, sentence, checksum_name, parts);
            return data;
        }
        catch (runtime_error e)
        {
            mgr->status("UKHAS Extractor: full parse failed: " +
                        string(e.what()));
        }
    }

    basic["_basic"] = true;
    return basic;
}

} /* namespace habitat */
