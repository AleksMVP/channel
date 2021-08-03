#include <iostream>
#include <thread>
#include "../include/channel.h"

int main() {
    Go::Channel<int> chan(0);

    auto t = std::thread([&]{
        for (int i = 0; i < 500; i++) {
            chan.pushWait(i);
        }
    });

    auto lol = std::thread([&]{
        for (const auto& item : chan) {
            std::cout << item << std::endl;
        }
    });

    t.join();

    chan.close();
    lol.join();
}