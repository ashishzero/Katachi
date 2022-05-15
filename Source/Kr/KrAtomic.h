#pragma once
#include <stdint.h>
#include "KrCommon.h"

#if PLATFORM_WINDOWS == 1 && (ARCH_X64 == 1 || ARCH_X86 == 1)
#include <intrin.h>

int32_t AtomicInc(int32_t volatile *addend) { return _InterlockedIncrement((volatile long *)addend); }
int32_t AtomicDec(int32_t volatile *addend) { return _InterlockedDecrement((volatile long *)addend); }
int32_t AtomicAdd(int32_t volatile *addend, int32_t value) { return _interlockedadd((volatile long *)addend, value); }
int32_t AtomicSub(int32_t volatile *sub, int32_t value) { return _interlockedadd((volatile long *)sub, -value); }
int32_t AtomicCmpExg(int32_t volatile *dst, int32_t exchange, int32_t comperand) { return _InterlockedCompareExchange((volatile long *)dst, exchange, comperand); }
void *  AtomicCmpExg(void *volatile *dst, void *exchange, void *comperand) { return _InterlockedCompareExchangePointer(dst, exchange, comperand); }
int32_t AtomicLoad(int32_t volatile *src) { return *src; }
int32_t AtomicExchange(int32_t volatile *src, int32_t value) { return _InterlockedExchange((volatile long *)src, value); }
void    AtomicStore(int32_t volatile *src, int32_t value) { _InterlockedExchange((volatile long *)src, value); }
void *  AtomicLoad(void *volatile *dst) { return *dst; }
void *  AtomicExchange(void *volatile *src, void *value) { return _InterlockedExchangePointer(src, value); }
void    AtomicStore(void *volatile *src, void *value) { _InterlockedExchangePointer(src, value); }

#if ARCH_X64 == 1
int64_t AtomicInc(int64_t volatile *addend) { return _InterlockedIncrement64((volatile long long *)addend); }
int64_t AtomicDec(int64_t volatile *addend) { return _InterlockedDecrement64((volatile long long *)addend); }
int64_t AtomicAdd(int64_t volatile *addend, int64_t value) { return _interlockedadd64((volatile long long *)addend, value); }
int64_t AtomicSub(int64_t volatile *sub, int64_t value) { return _interlockedadd64((volatile long long *)sub, -value); }
int64_t AtomicCmpExg(int64_t volatile *dst, int64_t exchange, int64_t comperand) { return _InterlockedCompareExchange64((volatile long long *)dst, exchange, comperand); }
int64_t AtomicLoad(int64_t volatile *src) { return *src; }
int64_t AtomicExchange(int64_t volatile *src, int64_t value) { return _InterlockedExchange64((volatile long long *)src, value); }
void    AtomicStore(int64_t volatile *src, int64_t value) { _InterlockedExchange64((volatile long long *)src, value); }
#endif

#else

int32_t AtomicInc(int32_t volatile *addend) { return __sync_add_and_fetch(addend, 1); }
int32_t AtomicDec(int32_t volatile *addend) { return __sync_sub_and_fetch(addend, 1); }
int32_t AtomicAdd(int32_t volatile *addend, int32_t value) { return __sync_add_and_fetch(addend, value); }
int32_t AtomicSub(int32_t volatile *sub, int32_t value) { return __sync_sub_and_fetch(sub, value); }
int32_t AtomicCmpExg(int32_t volatile *dst, int32_t exchange, int32_t comperand) { return __sync_val_compare_and_swap(dst, comperand, exchange); }
void *  AtomicCmpExg(void *volatile *dst, void *exchange, void *comperand) { return __sync_val_compare_and_swap(dst, comperand, exchange); }
int32_t AtomicLoad(int32_t volatile *src) { return __atomic_load_n(src, __ATOMIC_SEQ_CST); }
int32_t AtomicExchange(int32_t volatile *src, int32_t value) { return __atomic_exchange_n(src, value, __ATOMIC_SEQ_CST); }
void    AtomicStore(int32_t volatile *src, int32_t value) { __atomic_store_n(src, value, __ATOMIC_SEQ_CST); }
void *  AtomicLoad(void *volatile *dst) { return __atomic_load_n(src, __ATOMIC_SEQ_CST); }
void *  AtomicExchange(void *volatile *src, void *value) { return __atomic_exchange_n(src, value, __ATOMIC_SEQ_CST); }
void    AtomicStore(void *volatile *src, void *value) { __atomic_store_n(src, value, __ATOMIC_SEQ_CST); }

#if ARCH_X64 == 1 || ARCH_ARM64 == 1
int64_t AtomicInc(int64_t volatile *addend) { return __sync_add_and_fetch(addend, 1); }
int64_t AtomicDec(int64_t volatile *addend) { return __sync_add_and_fetch(addend, 1); }
int64_t AtomicAdd(int64_t volatile *addend, int64_t value) { return __sync_add_and_fetch(addend, value); }
int64_t AtomicSub(int64_t volatile *sub, int64_t value) { return __sync_sub_and_fetch(sub, value); }
int64_t AtomicCmpExg(int64_t volatile *dst, int64_t exchange, int64_t comperand) { return __sync_val_compare_and_swap(dst, comperand, exchange); }
int64_t AtomicLoad(int64_t volatile *src) { return __atomic_load_n(src, __ATOMIC_SEQ_CST); }
int64_t AtomicExchange(int64_t volatile *src, int64_t value) { return __atomic_exchange_n(src, value, __ATOMIC_SEQ_CST); }
void    AtomicStore(int64_t volatile *src, int64_t value) { __atomic_store_n(src, value, __ATOMIC_SEQ_CST); }
#endif

#endif

template <typename T>
void *AtomicCmpExg(T *volatile *dst, T *exchange, T *comperand) {
	return (T *)AtomicCmpExg((void *volatile *)dst, (void *)exchange, (void *)comperand);
}

//
//
//

struct Atomic_Guard {
	int32_t volatile value;
};

inline void SpinLock(Atomic_Guard *guard) {
	while (AtomicCmpExg(&guard->value, 1, 0) == 1)
		;
}

inline void SpinUnlock(Atomic_Guard *guard) {
	AtomicStore(&guard->value, 0);
}
