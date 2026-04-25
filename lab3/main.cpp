#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <random>
#include <atomic>
#include <iomanip>

using namespace std::chrono;

// Метрики пулу
struct Metrics {
    std::atomic<int> tasksAccepted{0};
    std::atomic<int> tasksRejected{0};
    std::atomic<int> tasksCompleted{0};
    std::atomic<long long> totalWaitTimeMs{0};
    std::atomic<long long> totalTaskTimeMs{0};

    // Метрики по кожному worker
    std::atomic<int> workerTaskCount[8];
    std::atomic<long long> workerTotalTimeMs[8];

    Metrics() {
        for (int i = 0; i < 8; i++) {
            workerTaskCount[i] = 0;
            workerTotalTimeMs[i] = 0;
        }
    }

    void print(double testDurationSec) const {
        int completed = tasksCompleted.load();
        double avgWait = completed > 0 ? (totalWaitTimeMs.load() / 1000.0) / completed : 0;
        double avgTask = completed > 0 ? (totalTaskTimeMs.load() / 1000.0) / completed : 0;

        std::cout << "\nMETRICS \n";
        std::cout << "Test duration:       " << testDurationSec << " s\n";
        std::cout << "Tasks accepted:      " << tasksAccepted.load() << "\n";
        std::cout << "Tasks rejected:      " << tasksRejected.load() << "\n";
        std::cout << "Tasks completed:     " << completed << "\n";
        std::cout << "Avg wait time:       " << avgWait << " s\n";
        std::cout << "Avg task exec time:  " << avgTask << " s\n";

        std::cout << "\nPer Worker Stats\n";
        std::cout << std::left
                  << std::setw(10) << "Worker"
                  << std::setw(15) << "Tasks done"
                  << std::setw(20) << "Total exec time(s)"
                  << std::setw(20) << "Avg exec time(s)\n";
        std::cout << std::string(65, '-') << "\n";

        for (int i = 0; i < 8; i++) {
            int count = workerTaskCount[i].load();
            double total = workerTotalTimeMs[i].load() / 1000.0;
            double avg = count > 0 ? total / count : 0;
            std::cout << std::left
                      << std::setw(10) << i
                      << std::setw(15) << count
                      << std::setw(20) << total
                      << std::setw(20) << avg << "\n";
        }
    }
};

// Один робочий потік

struct Worker {
    std::thread thread;
    std::function<void()> task;
    int taskDuration = 0;
    int taskId = -1;
    bool busy = false;
    bool terminate = false;
    std::mutex mtx;
    std::condition_variable cv;
};

// Пул потоків
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

        int id = taskCounter++;

        for (int i = 0; i < WORKER_COUNT; i++) {
            std::unique_lock<std::mutex> lock(workers[i]->mtx);
            if (!workers[i]->busy && !workers[i]->terminate) {
                workers[i]->task = std::move(task);
                workers[i]->taskDuration = durationSec;
                workers[i]->taskId = id;
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
    std::atomic<int> taskCounter{0};
    Metrics& m_metrics;

    void workerRoutine(int id) {
        while (true) {
            std::function<void()> task;
            int taskId = -1;
            auto waitStart = high_resolution_clock::now();

            {
                std::unique_lock<std::mutex> lock(workers[id]->mtx);
                workers[id]->cv.wait(lock, [&] {
                    return workers[id]->busy || workers[id]->terminate;
                });
                if (workers[id]->terminate) return;
                task = std::move(workers[id]->task);
                taskId = workers[id]->taskId;
            }

            auto waitEnd = high_resolution_clock::now();
            long long waitMs = duration_cast<milliseconds>(waitEnd - waitStart).count();
            m_metrics.totalWaitTimeMs += waitMs;

            // Timestamp початку
            auto now = system_clock::now();
            auto timeT = system_clock::to_time_t(now);
            std::tm tm;
            localtime_s(&tm, &timeT);
            char buf[16];
            strftime(buf, sizeof(buf), "%H:%M:%S", &tm);

            std::cout << "[Worker " << id << "] Task #" << taskId
                      << " started at " << buf
                      << " (waited " << waitMs / 1000.0 << "s)\n";

            auto taskStart = high_resolution_clock::now();
            if (task) task();
            auto taskEnd = high_resolution_clock::now();

            long long taskMs = duration_cast<milliseconds>(taskEnd - taskStart).count();
            m_metrics.totalTaskTimeMs += taskMs;
            m_metrics.tasksCompleted++;
            m_metrics.workerTaskCount[id]++;
            m_metrics.workerTotalTimeMs[id] += taskMs;

            // Timestamp завершення
            auto now2 = system_clock::now();
            auto timeT2 = system_clock::to_time_t(now2);
            std::tm tm2;
            localtime_s(&tm2, &timeT2);
            strftime(buf, sizeof(buf), "%H:%M:%S", &tm2);

            std::cout << "[Worker " << id << "] Task #" << taskId
                      << " done at " << buf
                      << " (exec " << taskMs / 1000.0 << "s)\n";

            {
                std::unique_lock<std::mutex> lock(workers[id]->mtx);
                workers[id]->busy = false;
            }
        }
    }
};

// Генератор задач (з кількох потоків)
void taskGenerator(ThreadPool& pool, int generatorId, std::atomic<bool>& running) {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> duration(10, 14);
    std::uniform_int_distribution<int> interval(1, 3);

    while (running) {
        int dur = duration(rng);
        bool accepted = pool.addTask([dur]() {
            std::this_thread::sleep_for(seconds(dur));
        }, dur);

        std::cout << "[Gen " << generatorId << "] Task " << dur << "s "
                  << (accepted ? "accepted" : "REJECTED") << "\n";

        std::this_thread::sleep_for(seconds(interval(rng)));
    }
}

int main() {
    std::cout << "\n TEST 1: 30s, 2 generators\n";
    {
        Metrics metrics;
        ThreadPool pool(metrics);
        std::atomic<bool> running{true};

        // Задачі додаються з ДВОХ окремих потоків
        std::thread gen1(taskGenerator, std::ref(pool), 1, std::ref(running));
        std::thread gen2(taskGenerator, std::ref(pool), 2, std::ref(running));

        std::this_thread::sleep_for(seconds(30));
        running = false;
        gen1.join();
        gen2.join();

        metrics.print(30);
    }

    std::cout << "\n TEST 2: 60s, 3 generators\n";
    {
        Metrics metrics;
        ThreadPool pool(metrics);
        std::atomic<bool> running{true};

        std::thread gen1(taskGenerator, std::ref(pool), 1, std::ref(running));
        std::thread gen2(taskGenerator, std::ref(pool), 2, std::ref(running));
        std::thread gen3(taskGenerator, std::ref(pool), 3, std::ref(running));

        std::this_thread::sleep_for(seconds(60));
        running = false;
        gen1.join();
        gen2.join();
        gen3.join();

        metrics.print(60);
    }

    std::cout << "\n TEST 3: pause/resume, 2 generators \n";
    {
        Metrics metrics;
        ThreadPool pool(metrics);
        std::atomic<bool> running{true};

        std::thread gen1(taskGenerator, std::ref(pool), 1, std::ref(running));
        std::thread gen2(taskGenerator, std::ref(pool), 2, std::ref(running));

        std::this_thread::sleep_for(seconds(15));
        pool.pause();
        std::cout << "[Main] Pool paused for 10s\n";
        std::this_thread::sleep_for(seconds(10));
        pool.resume();
        std::cout << "[Main] Pool resumed\n";
        std::this_thread::sleep_for(seconds(15));

        running = false;
        gen1.join();
        gen2.join();

        metrics.print(40);
    }

    return 0;
}