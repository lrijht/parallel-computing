#include <iostream>
#include <string>
#include <chrono>
#include <random>

using std::chrono::nanoseconds;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;

std::string generateString(std::size_t n) {
    const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::mt19937 rng(42);
    std::uniform_int_distribution<> dist(0, chars.size() - 1);
    std::string result(n, ' ');
    for (auto& c : result) c = chars[dist(rng)];
    return result;
}

std::string reverseSequential(const std::string& s) {
    std::string result(s.rbegin(), s.rend());
    return result;
}

void runTest(std::size_t size) {
    std::string str = generateString(size);

    volatile char sink = reverseSequential(str)[0];

    auto begin = high_resolution_clock::now();
    std::string reversed = reverseSequential(str);
    auto end = high_resolution_clock::now();

    sink = reversed[0];

    auto elapsed = duration_cast<nanoseconds>(end - begin);
    double seconds = elapsed.count() * 1e-9;

    std::cout << "Size: " << size
              << " | Time: " << seconds << " s"
              << " | First 20: " << str.substr(0, 20)
              << " | Last 20 original: " << str.substr(str.size() - 20)
              << " | First 20 reversed: " << reversed.substr(0, 20)
              << "\n";
}

int main() {
    std::size_t sizes[] = {
        100'000,
        1'000'000,
        5'000'000,
        10'000'000,
        50'000'000,
        100'000'000
    };

    for (auto size : sizes) {
        runTest(size);
    }

    return 0;
}