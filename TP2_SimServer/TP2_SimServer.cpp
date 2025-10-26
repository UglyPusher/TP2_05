#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::cout << "[sim] BacktestServer stub. Args: ";
    for (int i = 1; i < argc; ++i) std::cout << argv[i] << " ";
    std::cout << "\n";
    std::cout << "[sim] TODO: implement WS/REST endpoints and historical replay.\n";
    return 0;
}
