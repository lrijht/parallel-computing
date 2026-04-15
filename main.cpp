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

int main() {

    std::string test = "Hello World";
    std::string testReversed = reverseSequential(test);
    std::cout << "Test original: " << test << "\n";
    std::cout << "Test reversed: " << testReversed << "\n\n";

    const std::size_t SIZE = 10'000'000; // 10 мільйонів символів

    std::string str = generateString(SIZE);

    auto begin = high_resolution_clock::now();
    std::string reversed = reverseSequential(str);
    auto end = high_resolution_clock::now();

    auto elapsed = duration_cast<nanoseconds>(end - begin);
    std::cout << "String size: " << SIZE << "\n";
    std::cout << "Sequential time: " << elapsed.count() * 1e-9 << " seconds\n";
    std::cout << "First 20 original: " << str.substr(0, 20) << "\n";
    std::cout << "Last  20 original: " << str.substr(str.size() - 20) << "\n";
    std::cout << "First 20 reversed: " << reversed.substr(0, 20) << "\n";
    std::cout << "Last 20 reversed: " << reversed.substr(str.size() -20) << "\n\n";


    return 0;
}