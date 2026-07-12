#define ASIO_STANDALONE 1
#include <asio.hpp>
#include <iostream>

int main() {
    std::cout << "Creating io_context..." << std::endl;
    asio::io_context io;
    std::cout << "io_context created." << std::endl;

    std::cout << "Creating socket..." << std::endl;
    asio::ip::tcp::socket socket(io);
    std::cout << "socket created." << std::endl;

    return 0;
}
