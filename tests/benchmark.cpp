#include <filesystem>
#include <chrono>
#include <vector>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include "parser.h"

float round_to_2_decimals(float num) {
    return std::ceil(num * 100.f) / 100.f;
}

std::string pretty_print(float num_bytes) {
    std::stringstream out;
    if (num_bytes / 1000.f < 1) {
        out << std::fixed << std::setprecision(2) << (num_bytes) << "B";
    }
    else if (num_bytes / 1e6 < 1) {
        out << std::fixed << std::setprecision(2) << (num_bytes / 1000.f) << "kB";
    }
    else if (num_bytes / 1e9 < 1) {
        out << std::fixed << std::setprecision(2) << (num_bytes / 1e6) << "MB";
    }
    else {
        out << std::fixed << std::setprecision(2) << (num_bytes / 1e9) << "GB";
    }
    return out.str();
}

int main() {
    AB::Parser parser;

    parser.enter_block = [](AB::BLOCK_TYPE, const std::vector<AB::Boundaries>&, const AB::Attributes&, AB::BlockDetailPtr) -> bool {
        return true;
    };
    parser.leave_block = [](AB::BLOCK_TYPE) -> bool {
        return true;
    };
    parser.enter_span = [](AB::SPAN_TYPE, const std::vector<AB::Boundaries>&, const AB::Attributes&, AB::SpanDetailPtr) {
        return true;
    };
    parser.leave_span = [](AB::SPAN_TYPE) {
        return true;
    };
    parser.text = [](AB::TEXT_TYPE, const std::vector<AB::Boundaries>&) {
        return true;
    };

    std::ifstream ifs("long_doc.ab");
    std::string txt_input((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

    for (int i = 1;i < 1000;) {
        std::string input;
        for (int j = 0;j < i;j++) {
            input += txt_input;
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        AB::parse(&input, 0, (AB::OFFSET)input.length(), &parser);
        auto t2 = std::chrono::high_resolution_clock::now();
        float timing = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() / 1000.f;

        std::cout << "Parsing of " << std::fixed << std::setprecision(2)
            << pretty_print((float)input.length())
            << " of text took " << timing << " ms, or "
            << pretty_print((float)1e3 * (float)input.length() / (float)timing) << "/s"
            << std::endl;
        i += 20;
    }
}