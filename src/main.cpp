#include <iostream>
#include "velox/core/version.hpp"

int main() {
    std::cout << "velox-match " << velox::kVersion << " — engine offline\n";
    return 0;
}
