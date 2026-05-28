#include "velox/core/version.hpp"
#include <iostream>

int main()
{
    std::cout << "velox-match " << velox::kVersion << " — engine offline\n";
    return 0;
}
