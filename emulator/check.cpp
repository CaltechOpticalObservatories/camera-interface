#include <iostream>
#include <filesystem>

int main() {
    #ifdef __cpp_lib_filesystem
        std::cout << "std::filesystem is supported\n";
    #else
        std::cout << "std::filesystem is not supported\n";
    #endif

    return 0;
}

