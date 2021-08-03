#include <iostream>
#include <thread>
#include "../include/channel.h"

int main() {
    Channel<int> chan(10);

    auto t = std::thread([&]{
        for (int i = 0; i < 100; i++) {
            chan.pushWait(i);
        }
    });
    auto kek = std::thread([&]{
        for (int i = 0; i < 50; i++) {
            int tmp;
            chan.popWait(tmp);
            std::cout << tmp << std::endl;
        }
    });

    auto lol = std::thread([&]{
        for (int i = 0; i < 50; i++) {
            int tmp;
            chan.popWait(tmp);
            std::cout << tmp << std::endl;
        }
    });

    t.join();
    kek.join();
    lol.join();
}