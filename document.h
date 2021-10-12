#pragma once

#include <iostream>
#include <string>


struct Document {
    Document(int id = 0, double relevance = 0.0, int rating = 0);

    int id;
    double relevance;
    int rating;
};


enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};