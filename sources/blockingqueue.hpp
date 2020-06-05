//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <queue>
#include <mutex>

template<typename ITEM>
class BlockingQueue {
	typedef std::mutex Mutex;

public:
	BlockingQueue(int num_push=0)
	{
		for (int i = 0; i < num_push; i++)
			push(ITEM());
	}

	void push(const ITEM& value) { // push
		std::lock_guard<Mutex> lock(mutex);
		queue.push(std::move(value));
		condition.notify_one();
	}

	bool try_pop(ITEM& value) { // non-blocking pop
		std::lock_guard<Mutex> lock(mutex);
		if (queue.empty()) return false;
		value = std::move(queue.front());
		queue.pop();
		return true;
	}

	ITEM wait_pop() { // blocking pop
		std::unique_lock<Mutex> lock(mutex);
		condition.wait(lock, [this] {return !queue.empty(); });
		ITEM const value = std::move(queue.front());
		queue.pop();
		return value;
	}

	bool empty() //const 
	{ // queue is empty?
		std::lock_guard<Mutex> lock(mutex);
		return queue.empty();
	}

	void clear() { // remove all items
		ITEM item;
		while (try_pop(item));
	}

private:
	Mutex mutex;
	std::queue<ITEM> queue;
	std::condition_variable condition;
};