#ifndef ASYNC_HPP
#define ASYNC_HPP

#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <optional>

namespace myasync {
	template <class T>
	class Async {
		private:
		T *data1 = nullptr;
		T *data2 = nullptr;
		T *dataPointer = nullptr;
		std::mutex dataPointerMutex;
		bool whichData = 0;
		std::function<T*()> function;
		std::thread thread;
		std::atomic<bool> running = false;

		void run() {
			if (whichData == 0) {
				delete data1;
			} else {
				delete data2;
			}

			T *result = function();

			if (whichData == 0) {
				data1 = result;

				dataPointerMutex.lock();
				dataPointer = data1;
				dataPointerMutex.unlock();

				delete data2;
				data2 = nullptr;
			} else {
				data2 = result;

				dataPointerMutex.lock();
				dataPointer = data2;
				dataPointerMutex.unlock();

				delete data1;
				data1 = nullptr;
			}

			whichData = !whichData;
			running = false;
		}

		public:

		void set_function(std::function<T*()> newFunction) {
			function = newFunction;
		}

		// Returns false if it was already running, or if you haven't called set_function()
		bool try_start() {
			if (function == nullptr) {
				return false;
			}

			if (running.load()) {
				return false;
			}

			running = true;
			if (thread.joinable()) {
				thread.join();
			}

			thread = std::thread(&Async::run, this);
			return true;
		}

		void lock() {
			dataPointerMutex.lock();
		}

		void unlock() {
			dataPointerMutex.unlock();
		}

		// Requires manual locking/unlocking
		// Returns nullptr if no data is available.
		T *get_data() {
			return dataPointer;
		}

		// Requires manual locking/unlocking
		void reset_data() {
			dataPointer = nullptr;
		}

		bool is_running() {
			return running.load();
		}

		~Async() {
			if (thread.joinable()) {
				thread.join();
			}
			delete data1;
			delete data2;
		}
	};
}

#endif // ASYNC_HPP
