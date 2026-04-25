#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <vector>
#include <chrono>
#include <random>
#include <atomic>

using namespace std::chrono;


struct Worker {
    std::thread thread;
    std::function<void()> task;
    bool busy = false;
    bool terminate = false;
    std::mutex mtx;
    std::condition_variable cv;
};


class ThreadPool {
public:
    static constexpr int WORKER_COUNT = 8;

    ThreadPool() {
        for (int i = 0; i < WORKER_COUNT; i++) {
            workers[i] = std::make_unique<Worker>();
            workers[i]->thread = std::thread(&ThreadPool::workerRoutine, this, i);
        }
        std::cout << "[Pool] Initialized with " << WORKER_COUNT << " workers\n";
    }

    ~ThreadPool() {
        shutdown();
    }

    bool addTask(std::function<void()> task) {
        for (int i = 0; i < WORKER_COUNT; i++) {
            std::unique_lock<std::mutex> lock(workers[i]->mtx);
            if (!workers[i]->busy && !workers[i]->terminate) {
                workers[i]->task = std::move(task);
                workers[i]->busy = true;
                workers[i]->cv.notify_one();
                return true;
            }
        }
        return false;
    }

    void shutdown() {
        for (int i = 0; i < WORKER_COUNT; i++) {
            std::unique_lock<std::mutex> lock(workers[i]->mtx);
            workers[i]->terminate = true;
            workers[i]->cv.notify_one();
        }
        for (int i = 0; i < WORKER_COUNT; i++) {
            if (workers[i]->thread.joinable()) {
                workers[i]->thread.join();
            }
        }
        std::cout << "[Pool] Shutdown complete\n";
    }

    void shutdownNow() {
        for (int i = 0; i < WORKER_COUNT; i++) {
            std::unique_lock<std::mutex> lock(workers[i]->mtx);
            workers[i]->terminate = true;
            workers[i]->task = nullptr;
            workers[i]->cv.notify_one();
        }
        for (int i = 0; i < WORKER_COUNT; i++) {
            if (workers[i]->thread.joinable()) {
                workers[i]->thread.join();
            }
        }
        std::cout << "[Pool] Force shutdown complete\n";
    }

    void pause() {
        paused = true;
        std::cout << "[Pool] Paused\n";
    }

    void resume() {
        paused = false;
        std::cout << "[Pool] Resumed\n";
    }

    bool isPaused() const { return paused; }

private:
    std::unique_ptr<Worker> workers[WORKER_COUNT];
    std::atomic<bool> paused{false};

    void workerRoutine(int id) {
        while (true) {
            std::function<void()> task;
            auto waitStart = high_resolution_clock::now();

            {
                std::unique_lock<std::mutex> lock(workers[id]->mtx);
                workers[id]->cv.wait(lock, [&] {
                    return workers[id]->busy || workers[id]->terminate;
                });

                if (workers[id]->terminate) {
                    std::cout << "[Worker " << id << "] Terminating\n";
                    return;
                }

                task = std::move(workers[id]->task);
            }

            auto waitEnd = high_resolution_clock::now();
            double waitTime = duration_cast<milliseconds>(waitEnd - waitStart).count() / 1000.0;
            std::cout << "[Worker " << id << "] waited " << waitTime << "s, starting task\n";

            if (task) task();

            {
                std::unique_lock<std::mutex> lock(workers[id]->mtx);
                workers[id]->busy = false;
            }

            std::cout << "[Worker " << id << "] Task done\n";
        }
    }
};

int main() {
    ThreadPool pool;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> taskDuration(10, 14);

    std::cout << "\n Test 1: Add 8 tasks \n";
    for (int i = 0; i < 8; i++) {
        int duration = taskDuration(rng);
        bool accepted = pool.addTask([i, duration]() {
            std::cout << "[Task " << i << "] Running for " << duration << "s\n";
            std::this_thread::sleep_for(std::chrono::seconds(duration));
        });
        std::cout << "[Main] Task " << i << (accepted ? " accepted" : " REJECTED") << "\n";
    }

    std::cout << "\n Test 2: Add tasks while all busy (should reject) \n";
    for (int i = 8; i < 12; i++) {
        bool accepted = pool.addTask([i]() {
            std::this_thread::sleep_for(std::chrono::seconds(10));
        });
        std::cout << "[Main] Task " << i << (accepted ? " accepted" : " REJECTED") << "\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(15));

    return 0;
}