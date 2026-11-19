#include "session_cases.hpp"
#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) { std::cerr << "Supply a session case name\n"; return 2; }
    for (const auto& test : session_tests::cases) {
        if (test.name == argv[1]) {
            try { test.run(); std::cout << test.name << " passed\n"; return 0; }
            catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
        }
    }
    return 2;
}
