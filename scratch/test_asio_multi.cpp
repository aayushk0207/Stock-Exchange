#define ASIO_STANDALONE 1
#include <asio.hpp>
#include <iostream>
#include <thread>

void do_accept(asio::ip::tcp::acceptor& acceptor) {
    acceptor.async_accept([](std::error_code ec, asio::ip::tcp::socket socket) {
        std::cout << "Accepted connection!" << std::endl;
    });
}

int main() {
    std::cout << "Creating main io_context..." << std::endl;
    asio::io_context io_main;
    std::cout << "Creating acceptor..." << std::endl;
    asio::ip::tcp::acceptor acceptor(io_main, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 12345));
    std::cout << "Acceptor bound to port 12345." << std::endl;

    do_accept(acceptor);

    std::thread t([&io_main]() {
        std::cout << "io_main.run() starting..." << std::endl;
        io_main.run();
        std::cout << "io_main.run() stopped." << std::endl;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "Creating simulator io_context..." << std::endl;
    asio::io_context io_sim;
    std::cout << "Creating simulator socket..." << std::endl;
    asio::ip::tcp::socket socket(io_sim);
    std::cout << "Simulator socket created." << std::endl;

    std::cout << "Stopping main io_context..." << std::endl;
    io_main.stop();
    t.join();
    std::cout << "Done." << std::endl;

    return 0;
}
