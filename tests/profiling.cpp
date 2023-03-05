#include <filesystem>
#include <chrono>
#include <vector>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include "parser.h"

#if defined ( __clang__ ) || defined ( __GNUC__ )
#define TracyFunction __PRETTY_FUNCTION__
#elif defined ( _MSC_VER )
# define TracyFunction __FUNCSIG__
#endif
#include <tracy/Tracy.hpp>

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

    // Pause program

    std::ifstream ifs("long_doc.ab");
    std::string txt_input((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
    std::string long_input;

    for (int j = 0;j < 1000;j++) {
        long_input += txt_input;
    }

    for (int i = 0;i < 10;i++) {
        auto t1 = std::chrono::high_resolution_clock::now();
        AB::parse(long_input.c_str(), (AB::SIZE)long_input.length(), &parser);
        auto t2 = std::chrono::high_resolution_clock::now();
        float timing = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() / 1000.f;

        std::cout << "Parsing of " << std::fixed << std::setprecision(2)
            << pretty_print((float)long_input.length())
            << " of text took " << timing << " ms, or "
            << pretty_print((float)1e3 * (float)long_input.length() / (float)timing) << "/s"
            << std::endl;
    }

}