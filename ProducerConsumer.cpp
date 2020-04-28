#include <iostream>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <omp.h>

// Logging without corruption.
// Supposed to be used in a multi-threading environment.
// Puts endline automatically.
class ThreadSafeLogger
{
public:
	ThreadSafeLogger(std::ostream& outputStream = std::cout)
		: os(outputStream)
	{
		ss << "[TRACE] ";
	}

	~ThreadSafeLogger()
	{
		std::lock_guard<std::mutex> lock(mut);
		os << ss.str() << "\n";
		os.flush();
	}

	template<class T>
	ThreadSafeLogger& operator<<(T&& data)
	{
		ss << data;
		return *this;
	}

private:
	std::stringstream ss;
	std::ostream& os;
	static std::mutex mut;
};

std::mutex ThreadSafeLogger::mut;

#define TS_LOG() ThreadSafeLogger{}


template<typename T>
class ProducerConsumer
{
public:
	using ProducerFunc = std::function<T()>;
	using ConsumerFunc = std::function<void(const T&)>;

	ProducerConsumer(ProducerFunc produce, ConsumerFunc consume)
		: produce(produce),
		consume(consume)
	{}

	void run(int itemCount)
	{
	#pragma omp parallel shared(itemCount)
		{
			int num = itemCount;
			while(itemCount>0)
			{
				T item;
				if (omp_get_thread_num() == 0 && num >0 )
				{
					item = produce();
					buffer.push(item);
					num--;
				}
						if (omp_get_thread_num()!=0) {
							while (itemCount>0)
							{
								if (buffer.empty())
									continue;
								else
								{
									item = buffer.pop();
									consume(item);
									itemCount--;
									break;
								}
							}
						}
			}
		}
	}

private:
	template <typename Elem> 
	class QueueSynchronized
	{
	public:
		void push(const Elem& val)
		{
			std::lock_guard<std::mutex> lock(mut);
			q.push(val);
			cond.notify_one();
		}

		Elem pop()
		{
			std::unique_lock<std::mutex> lock(mut);
			while (q.empty())
				cond.wait(lock);
			
			Elem val = q.front();
			q.pop();
			return val;
		}

		bool empty() const
		{
			std::lock_guard<std::mutex> lock(mut);
			return q.empty();
		}

	private:
		std::queue<Elem> q;
		mutable std::mutex mut;
		std::condition_variable cond;
	};

private:
	ProducerFunc produce; // function to produce an item
	ConsumerFunc consume; // function to consume an item
	QueueSynchronized<T> buffer;
};


struct Dummy { int num; static int glob; };
int Dummy::glob = 0;

int main()
{
	auto produce = []() { 
		Dummy item{ Dummy::glob++ };
		TS_LOG() << "generating Dummy " << item.num;
		return item;
	};

	auto consume = [](const Dummy& item) {
		TS_LOG() << "calculating dummy " << item.num;
	};

	constexpr int MAX_ITEM_TO_PRODUCE = 5;
	ProducerConsumer<Dummy> problem(produce, consume);
	problem.run(MAX_ITEM_TO_PRODUCE);

	system("pause");
	return 0;
}
