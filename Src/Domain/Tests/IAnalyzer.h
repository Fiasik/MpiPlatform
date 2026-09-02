#pragma once

#include "Domain/Measurement/Sample.h"

class IAnalyzer
{
public:

    virtual ~IAnalyzer() = default;

    virtual void start() = 0;
    virtual void onSample(const Domain::Measurement::Sample& s) = 0;
    virtual void finish() = 0;
};
