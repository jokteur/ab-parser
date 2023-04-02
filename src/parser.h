#pragma once
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>

#include "definitions.h"
#include "helpers.h"


// Implementation is inspired from http://github.com/mity/md4c
namespace AB {
    bool parse(const std::string* text, OFFSET start, OFFSET end, const Parser* parser);
};