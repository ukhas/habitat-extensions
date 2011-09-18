/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see COPYING. */

#include "Extractor.h"
#include <vector>

namespace habitat {

void ExtractorManager::add(Extractor &e)
{
    extractors.push_back(&e);
    e.mgr = this;
}

void ExtractorManager::skipped(int n)
{
    vector<Extractor *>::iterator it;

    for (it = extractors.begin(); it != extractors.end(); it++)
        (*it)->skipped(n);
}

void ExtractorManager::push(char b, enum push_flags flags)
{
    vector<Extractor *>::iterator it;

    for (it = extractors.begin(); it != extractors.end(); it++)
        (*it)->push(b, flags);
}

} /* namespace habitat */
