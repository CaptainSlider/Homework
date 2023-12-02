#pragma once

template <typename T> class AutoLock {
public:
	AutoLock(T& lock) : _lock(lock) {
		_lock.Lock();
	}

	~AutoLock() {
		_lock.UnLock();
	}

private:
	T& _lock;
};