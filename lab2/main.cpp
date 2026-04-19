#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <climits>
#include <mutex>
#include <thread>
#include <atomic>


using std::chrono::nanoseconds;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;

std::vector<int> generateArray(std::size_t n)
{
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(-1000, 1000);
    std::vector<int> arr(n);
    for (auto& x : arr) x = dist(rng);
    return arr;
}

struct Result
{
    long long difference;
    int minOdd;
};

Result sequentialOdd(const std::vector<int>& arr)
{
    long long diff = 0;
    int minOdd = INT_MAX;
    bool first = true;

    for (int x : arr)
        {
        if (x % 2 != 0)
            {
            if (first)
                {
                diff = x;
                first = false;
            }
            else
                {
                diff -= x;
            }
            if (x < minOdd) minOdd = x;
        }
    }
    return { diff, minOdd };
}

Result mutexOdd(const std::vector<int>& arr, std::size_t numThreads) {
    long long totalSum = 0;
    int firstOdd = INT_MAX;
    int minOdd = INT_MAX;
    std::mutex mtx;
    std::vector<std::thread> threads;
    std::size_t chunkSize = arr.size() / numThreads;

    for (std::size_t i = 0; i < numThreads; i++) {
        std::size_t start = i * chunkSize;
        std::size_t end = (i == numThreads - 1) ? arr.size() : start + chunkSize;

        threads.emplace_back([&, start, end]() {
            long long localSum = 0;
            int localMin = INT_MAX;

            for (std::size_t j = start; j < end; j++) {
                int x = arr[j];
                if (x % 2 != 0) {
                    localSum += x;
                    if (x < localMin) localMin = x;
                }
            }

            std::lock_guard<std::mutex> lock(mtx);
            totalSum += localSum;
            if (localMin < minOdd) minOdd = localMin;
        });
    }

    for (auto& t : threads) t.join();


    int firstOddVal = INT_MAX;
    for (int x : arr) {
        if (x % 2 != 0) { firstOddVal = x; break; }
    }

    long long diff = (firstOddVal != INT_MAX) ? (2 * firstOddVal - totalSum) : 0;
    return { diff, minOdd };
}

Result atomicCAS(const std::vector<int>& arr, std::size_t numThreads) {
    std::atomic<long long> totalSum(0);
    std::atomic<int> minOdd(INT_MAX);

    std::vector<std::thread> threads;
    std::size_t chunkSize = arr.size() / numThreads;

    for (std::size_t i = 0; i < numThreads; i++) {
        std::size_t start = i * chunkSize;
        std::size_t end = (i == numThreads - 1) ? arr.size() : start + chunkSize;

        threads.emplace_back([&, start, end]() {
            long long localSum = 0;
            int localMin = INT_MAX;

            for (std::size_t j = start; j < end; j++) {
                int x = arr[j];
                if (x % 2 != 0) {
                    localSum += x;
                    if (x < localMin) localMin = x;
                }
            }

            long long expected = totalSum.load();
            while (!totalSum.compare_exchange_weak(expected, expected + localSum)) {
            }

            int expectedMin = minOdd.load();
            while (localMin < expectedMin) {
                if (minOdd.compare_exchange_weak(expectedMin, localMin)) {
                    break;
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    int firstOddVal = INT_MAX;
    for (int x : arr) {
        if (x % 2 != 0) { firstOddVal = x; break; }
    }

    long long diff = (firstOddVal != INT_MAX) ? (2LL * firstOddVal - totalSum.load()) : 0;
    return { diff, minOdd.load() };
}

void runTest(std::size_t size)
{
    std::vector<int> arr = generateArray(size);

    volatile long long sink = sequentialOdd(arr).difference;

    auto begin = high_resolution_clock::now();
    Result result = sequentialOdd(arr);
    auto end = high_resolution_clock::now();

    sink = result.difference;

    auto elapsed = duration_cast<nanoseconds>(end - begin);

    std::cout << "Size: " << size
              << " | Difference: " << result.difference
              << " | Min odd: " << result.minOdd
              << " | Time: " << elapsed.count() * 1e-9 << " s\n";
}

int main()
{

    std::vector<int> test = {3, 2, 5, 8, 7, 4, 1};
    Result r = sequentialOdd(test);
    std::cout << "=== Verification ===\n";
    std::cout << "Array: 3 2 5 8 7 4 1\n";
    std::cout << "Odd elements: 3 5 7 1\n";
    std::cout << "Difference: 3-5-7-1 = " << r.difference << "\n";
    std::cout << "Min odd: " << r.minOdd << "\n\n";

    std::cout << "=== Performance ===\n";
    std::size_t sizes[] =
        {
        100'000,
        1'000'000,
        10'000'000,
        50'000'000,
        100'000'000
    };

    for (auto size : sizes)
        {
        runTest(size);
    }

    std::cout << "\n=== Mutex version (20 threads) ===\n";
    std::size_t numThreads = 20;

    for (auto size : sizes) {
        std::vector<int> arr = generateArray(size);

        volatile long long sink = mutexOdd(arr, numThreads).difference;

        auto begin = high_resolution_clock::now();
        Result result = mutexOdd(arr, numThreads);
        auto end = high_resolution_clock::now();

        sink = result.difference;

        auto elapsed = duration_cast<nanoseconds>(end - begin);
        std::cout << "Size: " << size
                  << " | Difference: " << result.difference
                  << " | Min odd: " << result.minOdd
                  << " | Time: " << elapsed.count() * 1e-9 << " s\n";
    }

    std::cout << "\n=== Atomic CAS version (20 threads) ===\n";

    for (auto size : sizes) {
        std::vector<int> arr = generateArray(size);

        volatile long long sink = atomicCAS(arr, numThreads).difference;

        auto begin = high_resolution_clock::now();
        Result result = atomicCAS(arr, numThreads);
        auto end = high_resolution_clock::now();

        sink = result.difference;

        auto elapsed = duration_cast<nanoseconds>(end - begin);
        std::cout << "Size: " << size
                  << " | Difference: " << result.difference
                  << " | Min odd: " << result.minOdd
                  << " | Time: " << elapsed.count() * 1e-9 << " s\n";
    }

    return 0;
}