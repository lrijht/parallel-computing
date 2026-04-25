#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <random>
#include <atomic>

using namespace std::chrono;

struct Metrics {
    std::atomic<int> tasksAccepted{0};
    std::atomic<int> tasksRejected{0};
    std::atomic<int> tasksCompleted{0};
    std::atomic<long long> totalWaitTimeMs{0};
    std::atomic<long long> totalTaskTimeMs{0};

    void print(double testDurationSec) const {
        int completed = tasksCompleted.load();
        double avgWait = completed > 0 ? (totalWaitTimeMs.load() / 1000.0) / completed : 0;
        double avgTask = completed > 0 ? (totalTaskTimeMs.load() / 1000.0) / completed : 0;

        std::cout << "\n METRICS \n";
        std::cout << "Test duration:       " << testDurationSec << " s\n";
        std::cout << "Tasks accepted:      " << tasksAccepted.load() << "\n";
        std::cout << "Tasks rejected:      " << tasksRejected.load() << "\n";
        std::cout << "Tasks completed:     " << completed << "\n";
        std::cout << "Avg wait time:       " << avgWait << " s\n";
        std::cout << "Avg task exec time:  " << avgTask << " s\n";
    }
};

struct Worker {
    std::thread thread;
    std::function<void()> task;
    int taskDuration = 0;
    bool busy = false;
    bool terminate = false;
    std::mutex mtx;
    std::condition_variable cv;
};

class ThreadPool {
public:
    static constexpr int WORKER_COUNT = 8;

    ThreadPool(Metrics& metrics) : m_metrics(metrics) {
        for (int i = 0; i < WORKER_COUNT; i++) {
            workers[i] = std::make_unique<Worker>();
            workers[i]->thread = std::thread(&ThreadPool::workerRoutine, this, i);
        }
        std::cout << "[Pool] Initialized with " << WORKER_COUNT << " workers\n";
    }

    ~ThreadPool() { shutdown(); }

    bool addTask(std::function<void()> task, int durationSec) {
        if (paused) return false;

        for (int i = 0; i < WORKER_COUNT; i++) {
            std::unique_lock<std::mutex> lock(workers[i]->mtx);
            if (!workers[i]->busy && !workers[i]->terminate) {
                workers[i]->task = std::move(task);
                workers[i]->taskDuration = durationSec;
                workers[i]->busy = true;
                workers[i]->cv.notify_one();
                m_metrics.tasksAccepted++;
                return true;
            }
        }
        m_metrics.tasksRejected++;
        return false;
    }

    void shutdown() {
        for (int i = 0; i < WORKER_COUNT; i++) {
            std::unique_lock<std::mutex> lock(workers[i]->mtx);
            workers[i]->terminate = true;
            workers[i]->cv.notify_one();
        }
        for (int i = 0; i < WORKER_COUNT; i++) {
            if (workers[i]->thread.joinable())
                workers[i]->thread.join();
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
            if (workers[i]->thread.joinable())
                workers[i]->thread.join();
        }
        std::cout << "[Pool] Force shutdown complete\n";
    }

    void pause()  { paused = true;  std::cout << "[Pool] Paused\n"; }
    void resume() { paused = false; std::cout << "[Pool] Resumed\n"; }

private:
    std::unique_ptr<Worker> workers[WORKER_COUNT];
    std::atomic<bool> paused{false};
    Metrics& m_metrics;

    void workerRoutine(int id) {
        while (true) {
            std::function<void()> task;
            auto waitStart = high_resolution_clock::now();

            {
                std::unique_lock<std::mutex> lock(workers[id]->mtx);
                workers[id]->cv.wait(lock, [&] {
                    return workers[id]->busy || workers[id]->terminate;
                });
                if (workers[id]->terminate) return;
                task = std::move(workers[id]->task);
            }

            auto waitEnd = high_resolution_clock::now();
            m_metrics.totalWaitTimeMs += duration_cast<milliseconds>(waitEnd - waitStart).count();

            auto taskStart = high_resolution_clock::now();
            if (task) task();
            auto taskEnd = high_resolution_clock::now();

            m_metrics.totalTaskTimeMs += duration_cast<milliseconds>(taskEnd - taskStart).count();
            m_metrics.tasksCompleted++;

            {
                std::unique_lock<std::mutex> lock(workers[id]->mtx);
                workers[id]->busy = false;
            }
        }
    }
};

void taskGenerator(ThreadPool& pool, std::atomic<bool>& running) {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> duration(10, 14);
    std::uniform_int_distribution<int> interval(1, 3);

    while (running) {
        int dur = duration(rng);
        bool accepted = pool.addTask([dur]() {
            std::this_thread::sleep_for(seconds(dur));
        }, dur);

        std::cout << "[Generator] Task " << dur << "s "
                  << (accepted ? "accepted" : "REJECTED") << "\n";

        std::this_thread::sleep_for(seconds(interval(rng)));
    }
}

int main() {
    std::cout << "\nTEST 1: 30 seconds\n";
    {
        Metrics metrics;
        ThreadPool pool(metrics);
        std::atomic<bool> running{true};
        std::thread gen(taskGenerator, std::ref(pool), std::ref(running));
        std::this_thread::sleep_for(seconds(30));
        running = false;
        gen.join();
        metrics.print(30);
    }

    std::cout << "\n TEST 2: 60 seconds\n";
    {
        Metrics metrics;
        ThreadPool pool(metrics);
        std::atomic<bool> running{true};
        std::thread gen(taskGenerator, std::ref(pool), std::ref(running));
        std::this_thread::sleep_for(seconds(60));
        running = false;
        gen.join();
        metrics.print(60);
    }

    std::cout << "\nTEST 3: pause/resume (40s total)\n";
    {
        Metrics metrics;
        ThreadPool pool(metrics);
        std::atomic<bool> running{true};
        std::thread gen(taskGenerator, std::ref(pool), std::ref(running));
        std::this_thread::sleep_for(seconds(15));
        pool.pause();
        std::this_thread::sleep_for(seconds(10));
        pool.resume();
        std::this_thread::sleep_for(seconds(15));
        running = false;
        gen.join();
        metrics.print(40);
    }

    return 0;
}