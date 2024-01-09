#include <fstream>
#include <iostream>
#include <sstream>
#include <random>

#include "add.h"

int main() {
    std::ifstream file;
    file.open("data.txt");
    if (!file.is_open()) {
        std::cout << "Could not open file." << std::endl;
    }

    int num_passed = 0;
    int num_failed = 0;

    uint32_t i;
    uint32_t a, b, c;
    std::string line;
    while (getline(file, line)) {
        std::istringstream    iss(line);
        std::cout << line << std::endl;
        iss >> std::dec >> i;
        iss >> std::hex >> a;
        iss >> std::hex >> b;
        iss >> std::hex >> c;
        iss.clear();
        if (fp32_add(a, b)==c) {
            num_passed++;
        }
        else {
            num_failed++;
        }
    }
    std::cout << "Hex test -- compared to file:  Total " << num_passed << " " << "PASSED " << num_failed << " FAILED." << std::endl;
    file.close();
}
