#pragma once

template <typename T> class AutoLock
{
public:
	AutoLock(T& lock) : _lock(lock) {
		lock.Lock();
	}

	~AutoLock() {
		_lock.Unlock();
	}

private:
	T& _lock;
};
