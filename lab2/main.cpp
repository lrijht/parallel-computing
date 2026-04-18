#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <climits>

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

    return 0;
}