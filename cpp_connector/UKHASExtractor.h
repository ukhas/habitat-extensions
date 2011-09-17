/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see COPYING. */

#ifndef HABITAT_UKHAS_EXTRACTOR_H
#define HABITAT_UKHAS_EXTRACTOR_H

#include "habitat/Extractor.h"

namespace habitat {

class UKHASExtractor : public Extractor
{
    int extracting = false;
    char last = '\0';
    string buffer;
    int garbage_count;

    void reset_buffer();

public:
    void skipped(int n);
    void push(char b, enum push_flags=PUSH_NONE);
};

} /* namespace habitat */

#endif /* HABITAT_UKHAS_EXTRACTOR_H */
