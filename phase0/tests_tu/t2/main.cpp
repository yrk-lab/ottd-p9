// Threads: atomics, mutexes (plain and recursive), a condition variable
// (with a timed wait), exceptions thrown and caught in every thread at
// once, and a function-local static initialized under contention.
// Where threads cannot be made (std::system_error), the work runs
// inline: the output is the same either way.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

static std::atomic<long> atomic_count{0};
static std::mutex mu;
static long locked_count = 0;
static std::recursive_mutex rmu;
static long recursive_count = 0;
static std::atomic<int> constructions{0};
static std::atomic<long> caught{0};

struct Slow {
	int value;
	Slow() : value(42) { constructions++; std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
};

static int Shared()
{
	static Slow s;
	return s.value;
}

static void Bump(int depth)
{
	std::lock_guard<std::recursive_mutex> l(rmu);
	recursive_count++;
	if (depth > 0) Bump(depth - 1);
}

static void Work(int id)
{
	for (int i = 0; i < 20000; i++) {
		atomic_count++;
		std::lock_guard<std::mutex> l(mu);
		locked_count++;
	}
	for (int i = 0; i < 300; i++) {
		try {
			if (i % 3 == id % 3) throw std::runtime_error("thread " + std::to_string(id));
		} catch (const std::runtime_error &e) {
			if (e.what()[0] == 't') caught++;
		}
	}
	Bump(3);
	if (Shared() != 42) std::printf("bad static\n");
}

int main()
{
	std::vector<std::thread> threads;
	std::vector<int> inline_ids;
	for (int id = 0; id < 4; id++) {
		try {
			threads.emplace_back(Work, id);
		} catch (const std::system_error &) {
			inline_ids.push_back(id);
		}
	}
	for (int id : inline_ids) Work(id);
	for (auto &t : threads) t.join();
	std::printf("atomic %ld, locked %ld, recursive %ld, caught %ld, static built %d time(s)\n",
		atomic_count.load(), locked_count, recursive_count, caught.load(), constructions.load());

	/* a handoff through a condition variable, and a wait that times out */
	std::mutex m;
	std::condition_variable cv;
	int stage = 0;
	auto consumer = [&]() {
		std::unique_lock<std::mutex> l(m);
		cv.wait(l, [&] { return stage == 1; });
		stage = 2;
		cv.notify_all();
	};
	std::thread t;
	bool threaded = true;
	try {
		t = std::thread(consumer);
	} catch (const std::system_error &) {
		threaded = false;
	}
	{
		std::unique_lock<std::mutex> l(m);
		stage = 1;
		cv.notify_all();
		if (threaded) cv.wait(l, [&] { return stage == 2; });
		else stage = 2;
	}
	if (threaded) t.join();
	std::unique_lock<std::mutex> l(m);
	bool timedout = !cv.wait_for(l, std::chrono::milliseconds(50), [&] { return stage == 3; });
	std::printf("handoff stage %d, wait timed out %d\n", stage, timedout);
	return 0;
}
