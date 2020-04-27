#include <iostream>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include "Spreadsheet/Spreadsheet.h"
#include <time.h>


class MyTimer
{
public:
	MyTimer() {}
	~MyTimer() {}
	void startTimer()
	{
		start = std::chrono::high_resolution_clock::now();
	}
	double stopTimer()
	{
		end = std::chrono::high_resolution_clock::now();
		duration = end - start;
		double result = duration.count();
		return result;
	}
private:
	std::chrono::time_point<std::chrono::steady_clock> start, end;
	std::chrono::duration<float> duration;
};


// Logging without corruption.
// Supposed to be used in a multi-threading environment.
// Puts endline automatically.
class ThreadSafeLogger
{
public:
	ThreadSafeLogger(std::ostream& outputStream = std::cout) : os(outputStream)
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

	ProducerConsumer(ProducerFunc produce, ConsumerFunc consume) : produce(produce), consume(consume) {}

	void run(int itemCount)
	{
#pragma omp parallel
		{
#pragma omp for
			for (int i = 0; i < itemCount; i++)
			{
				T item = produce();
				buffer.push(item);
				
				if (!buffer.empty())
					item = buffer.pop();
				consume(item);
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


struct SpreadsheetContainer { Spreadsheet spreadsheet; int num; static int globalCounter; };
int SpreadsheetContainer::globalCounter = 0;


int main()
{
	srand((unsigned)time(NULL));

	auto produce = []() {
		SpreadsheetContainer item;
		item.num = SpreadsheetContainer::globalCounter++;
		item.spreadsheet.resize(rand() % 6 + 3, rand() % 6 + 3);

		item.spreadsheet = SpreadsheetGenerator::generate(item.spreadsheet.getRows(), item.spreadsheet.getColumns());
#pragma omp critical 
		{
			TS_LOG() << "generating spreadsheet #" << item.num;
			item.spreadsheet.print();
		}
		return item;
	};

	auto consume = [](const SpreadsheetContainer& item) {
		SpreadsheetContainer calculatedItem;
		calculatedItem.spreadsheet.resize(item.spreadsheet.getRows(), item.spreadsheet.getColumns());

		calculatedItem.spreadsheet = SpreadsheetCalculator::calculateSpreadsheet(item.spreadsheet);
#pragma omp critical
		{
			TS_LOG() << "calculating spreadsheet #" << item.num;
			calculatedItem.spreadsheet.print();
		}
	};


	MyTimer timer; timer.startTimer();
	constexpr int MAX_ITEM_TO_PRODUCE = 100;
	ProducerConsumer<SpreadsheetContainer> problem(produce, consume);
	problem.run(MAX_ITEM_TO_PRODUCE);
	std::cout << "\n\n\nTIME: " << timer.stopTimer() << std::endl;

	system("pause");
	return 0;
}

//ТЗ
//замість заглушок (Dummy) необхідно використати Spreadsheet, тобто налаштувати клас ProducerConsumer для роботи зі Spreadsheet об'єктами. 
//використовуючи засоби OpenMP, перепроектувати на багатопоточність(ProducerConsumer::run() метод), так щоб багато потоків паралельно постачали(produce) визначену кількість 
//таблиць і багато потоків паралельно споживали(consume) їх. В якості функції produce у вас має бути генерація рандомної таблиці, а функції consume - обчислення таблиці.
//Не забудьте невеличку синхронізацію між потоками - має бути спродюсовано тільки визначену кількість itemCount таблиць; не має виникати жодних deadlock-ів; виконання не повинне зациклюватись.
