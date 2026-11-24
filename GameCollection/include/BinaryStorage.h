#pragma once
#include "Collection.h"
#include <string>

using namespace std;

class BinaryStorage
{
public:
    static bool save(const string& fileName, const Collection& collection);
    static bool load(const string& fileName, Collection& collection);
};
