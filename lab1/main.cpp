#include <iostream>
#include <string>
#include <chrono>
#include <random>
#include <thread>
#include <vector>


using std::chrono::nanoseconds;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;

std::string generateString(std::size_t n)
{
    const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::mt19937 rng(42);
    std::uniform_int_distribution<> dist(0, chars.size() - 1);
    std::string result(n, ' ');
    for (auto& c : result) c = chars[dist(rng)];
    return result;
}

std::string reverseSequential(const std::string& s)
{
    std::string result(s.rbegin(), s.rend());
    return result;
}

void reverseChunk(const std::string& input, std::string& output,
                  std::size_t inputStart, std::size_t inputEnd,
                  std::size_t outputStart)
{
    for (std::size_t i = inputStart; i < inputEnd; i++) {
        output[outputStart + (inputEnd - 1 - i)] = input[i];
    }
}

std::string reverseParallel(const std::string& s, std::size_t numThreads)
{
    std::string result(s.size(), ' ');
    std::vector<std::thread> threads;
    std::size_t chunkSize = s.size() / numThreads;

    for (std::size_t i = 0; i < numThreads; i++)
        {
        std::size_t inputStart = i * chunkSize;
        std::size_t inputEnd = (i == numThreads - 1) ? s.size() : inputStart + chunkSize;
        // Позиція у вихідному рядку
        std::size_t outputStart = s.size() - inputEnd;

        threads.emplace_back(reverseChunk, std::ref(s), std::ref(result),
                             inputStart, inputEnd, outputStart);
    }

    for (auto& t : threads) t.join();

    return result;
}


void runSequential(std::size_t size)
{
    std::string str = generateString(size);
    volatile char sink = reverseSequential(str)[0];

    auto begin = high_resolution_clock::now();
    std::string reversed = reverseSequential(str);
    auto end = high_resolution_clock::now();
    sink = reversed[0];

    auto elapsed = duration_cast<nanoseconds>(end - begin);
    std::cout << "[Sequential] Size: " << size
              << " | Time: " << elapsed.count() * 1e-9 << " s\n";
}

void runParallel(std::size_t size, std::size_t numThreads)
{
    std::string str = generateString(size);
    volatile char sink = reverseParallel(str, numThreads)[0];

    auto begin = high_resolution_clock::now();
    std::string reversed = reverseParallel(str, numThreads);
    auto end = high_resolution_clock::now();
    sink = reversed[0];

    auto elapsed = duration_cast<nanoseconds>(end - begin);
    std::cout << "[Parallel " << numThreads << " threads] Size: " << size
              << " | Time: " << elapsed.count() * 1e-9 << " s\n";
}

int main()
{
    std::string test = "Hello World 12345";
    std::string seqResult = reverseSequential(test);
    std::string parResult = reverseParallel(test, 4);
    std::cout << "=== Verification ===\n";
    std::cout << "Original:   " << test << "\n";
    std::cout << "Sequential: " << seqResult << "\n";
    std::cout << "Parallel:   " << parResult << "\n";
    std::cout << (seqResult == parResult ? "OK: results match!\n" : "ERROR: mismatch!\n");
    std::cout << "\n=== Performance ===\n";
    std::size_t sizes[] =
        {
        100'000,
        1'000'000,
        5'000'000,
        10'000'000,
        50'000'000,
        100'000'000
    };
    std::size_t threadCounts[] = {7, 14, 20, 40, 80, 160, 320};

    for (auto size : sizes)
        {
        std::cout << "\n--- Size: " << size << " ---\n";
        runSequential(size);
        for (auto threads : threadCounts) {
            runParallel(size, threads);
        }
    }

    return 0;
}
