#include "Common.h"

#include <string.h>

thread_local Thread_Context ThreadContext;
static Thread_Context_Params ThreadContextDefaultParams;

uint8_t *AlignPointer(uint8_t *location, size_t alignment) {
	return (uint8_t *)((size_t)(location + (alignment - 1)) & ~(alignment - 1));
}

size_t AlignSize(size_t location, size_t alignment) {
	return ((location + (alignment - 1)) & ~(alignment - 1));
}

Memory_Arena MemoryArenaCreate(size_t max_size) {
	Memory_Arena arena;
	arena.reserved = max_size;
	arena.memory = (uint8_t *)VirtualMemoryAllocate(0, arena.reserved);
	arena.commit_pos = 0;
	arena.current_pos = 0;
	return arena;
}

void MemoryArenaDestroy(Memory_Arena *arena) {
	VirtualMemoryFree(arena->memory, arena->reserved);
}

void MemoryArenaReset(Memory_Arena *arena) {
	arena->current_pos = 0;
}

size_t MemoryArenaSizeLeft(Memory_Arena *arena) {
	return arena->reserved - arena->current_pos;
}

void *PushSize(Memory_Arena *arena, size_t size) {
	void *ptr = 0;
	if (arena->current_pos + size <= arena->reserved) {
		ptr = arena->memory + arena->current_pos;
		arena->current_pos += size;
		if (arena->current_pos > arena->commit_pos) {
			size_t commit_pos = AlignPower2Up(arena->current_pos, MEMORY_ALLOCATOR_COMMIT_SIZE - 1);
			commit_pos = Minimum(commit_pos, arena->reserved);
			VirtualMemoryCommit(arena->memory + arena->commit_pos, commit_pos - arena->commit_pos);
			arena->commit_pos = commit_pos;
		}
	}
	return ptr;
}

void *PushSizeAligned(Memory_Arena *arena, size_t size, uint32_t alignment) {
	size_t aligned_current_pos = AlignSize(arena->current_pos, alignment);
	size_t alloc_size = aligned_current_pos - arena->current_pos + size;
	if (PushSize(arena, alloc_size))
		return arena->memory + aligned_current_pos;
	return 0;
}

void SetAllocationPosition(Memory_Arena *arena, size_t pos) {
	if (pos < arena->current_pos) {
		arena->current_pos = pos;
		size_t commit_pos = AlignPower2Up(pos, MEMORY_ALLOCATOR_COMMIT_SIZE - 1);
		commit_pos = Minimum(commit_pos, arena->reserved);

		if (commit_pos < arena->commit_pos) {
			VirtualMemoryDecommit(arena->memory + commit_pos, arena->commit_pos - commit_pos);
			arena->commit_pos = commit_pos;
		}
	}
}

Temporary_Memory BeginTemporaryMemory(Memory_Arena *arena) {
	Temporary_Memory mem;
	mem.arena = arena;
	mem.position = arena->current_pos;
	return mem;
}

void EndTemporaryMemory(Temporary_Memory *temp) {
	temp->arena->current_pos = temp->position;
}

void FreeTemporaryMemory(Temporary_Memory *temp) {
	SetAllocationPosition(temp->arena, temp->position);
}

Memory_Arena *ThreadScratchpad() {
	return &ThreadContext.scratchpad.arena[0];
}

Memory_Arena *ThreadScratchpadI(uint32_t i) {
	Assert(i < ArrayCount(ThreadContext.scratchpad.arena));
	return &ThreadContext.scratchpad.arena[i];
}

Memory_Arena *ThreadUnusedScratchpad(Memory_Arena *arenas, uint32_t count) {
	for (auto &thread_arena : ThreadContext.scratchpad.arena) {
		bool conflict = false;
		for (uint32_t index = 0; index < count; ++index) {
			if (&thread_arena == &arenas[index]) {
				conflict = true;
				break;
			}
		}
		if (!conflict)
			return &thread_arena;
	}

	return nullptr;
}

void ResetThreadScratchpad() {
	for (auto &thread_arena : ThreadContext.scratchpad.arena) {
		MemoryArenaReset(&thread_arena);
	}
}

static void *MemoryArenaAllocatorAllocate(size_t size, void *context) {
	Memory_Arena *arena = (Memory_Arena *)context;
	return PushSizeAligned(arena, size, sizeof(size_t));
}

static void *MemoryArenaAllocatorReallocate(void *ptr, size_t previous_size, size_t new_size, void *context) {
	Memory_Arena *arena = (Memory_Arena *)context;

	if (previous_size > new_size)
		return ptr;

	if (arena->memory + arena->current_pos == ((uint8_t *)ptr + previous_size)) {
		PushSize(arena, new_size - previous_size);
		return ptr;
	}

	void *new_ptr = PushSizeAligned(arena, new_size, sizeof(size_t));
	memmove(ptr, new_ptr, previous_size);
	return new_ptr;
}

static void MemoryArenaAllocatorFree(void *ptr, void *context) {}

Memory_Allocator MemoryArenaAllocator(Memory_Arena *arena) {
	Memory_Allocator allocator;
	allocator.allocate = MemoryArenaAllocatorAllocate;
	allocator.reallocate = MemoryArenaAllocatorReallocate;
	allocator.free = MemoryArenaAllocatorFree;
	allocator.context = arena;
	return allocator;
}

static void *NullMemoryAllocate(size_t size, void *ptr) { return nullptr; }
static void *NullMemoryReallocate(void *ptr, size_t prev_size, size_t size, void *ctx) { return nullptr; }
static void NullMemoryFree(void *ptr, void *context) {}

Memory_Allocator NullMemoryAllocator() {
	Memory_Allocator allocator;
	allocator.allocate = NullMemoryAllocate;
	allocator.reallocate = NullMemoryReallocate;
	allocator.free = NullMemoryFree;
	allocator.context = NULL;
	return allocator;
}

static void *DefaultMemoryAllocateProc(size_t size, void *context);
static void *DefaultMemoryReallocateProc(void *ptr, size_t previous_size, size_t new_size, void *context);
static void DefaultMemoryFreeProc(void *ptr, void *context);

static void InitOSContent();

void InitThreadContext(uint32_t scratchpad_size, Thread_Context_Params *params) {
	InitOSContent();

	if (scratchpad_size) {
		for (auto &thread_arena : ThreadContext.scratchpad.arena) {
			thread_arena = MemoryArenaCreate(scratchpad_size);
		}
	} else {
		memset(&ThreadContext.scratchpad, 0, sizeof(ThreadContext.scratchpad));
	}

	if (!params) {
		if (!ThreadContextDefaultParams.allocator.allocate) {
			ThreadContextDefaultParams.allocator.allocate = DefaultMemoryAllocateProc;
			ThreadContextDefaultParams.allocator.reallocate = DefaultMemoryReallocateProc;
			ThreadContextDefaultParams.allocator.free = DefaultMemoryFreeProc;
		}

		params = &ThreadContextDefaultParams;
	}

	ThreadContext.allocator = params->allocator;
}

//
//
//

void *MemoryAllocate(size_t size, Memory_Allocator &allocator) {
	return allocator.allocate(size, allocator.context);
}

void *MemoryReallocate(size_t old_size, size_t new_size, void *ptr, Memory_Allocator &allocator) {
	return allocator.reallocate(ptr, old_size, new_size, allocator.context);
}

void MemoryFree(void *ptr, Memory_Allocator &allocator) {
	allocator.free(ptr, allocator.context);
}

void *operator new(size_t size, Memory_Allocator &allocator) {
	return allocator.allocate(size, allocator.context);
}

void *operator new[](size_t size, Memory_Allocator &allocator) {
	return allocator.allocate(size, allocator.context);
}

void *operator new(size_t size) {
	return ThreadContext.allocator.allocate(size, ThreadContext.allocator.context);
}

void *operator new[](size_t size) {
	return ThreadContext.allocator.allocate(size, ThreadContext.allocator.context);
}

void operator delete(void *ptr, Memory_Allocator &allocator) {
	allocator.free(ptr, allocator.context);
}

void operator delete[](void *ptr, Memory_Allocator &allocator) {
	allocator.free(ptr, allocator.context);
}

void operator delete(void *ptr) noexcept {
	ThreadContext.allocator.free(ptr, ThreadContext.allocator.context);
}

void operator delete[](void *ptr) noexcept {
	ThreadContext.allocator.free(ptr, ThreadContext.allocator.context);
}

//
//
//

#if PLATFORM_WINDOWS == 1
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

static void InitOSContent() {
	SetConsoleCP(CP_UTF8);
}

static void *DefaultMemoryAllocateProc(size_t size, void *context) {
	HANDLE heap = GetProcessHeap();
	return HeapAlloc(heap, 0, size);
}

static void *DefaultMemoryReallocateProc(void *ptr, size_t previous_size, size_t new_size, void *context) {
	HANDLE heap = GetProcessHeap();
	if (ptr) {
		return HeapReAlloc(heap, 0, ptr, new_size);
	} else {
		return HeapAlloc(heap, 0, new_size);
	}
}

static void DefaultMemoryFreeProc(void *ptr, void *context) {
	HANDLE heap = GetProcessHeap();
	HeapFree(heap, 0, ptr);
}

void *VirtualMemoryAllocate(void *ptr, size_t size) {
	return VirtualAlloc(ptr, size, MEM_RESERVE, PAGE_READWRITE);
}

bool VirtualMemoryCommit(void *ptr, size_t size) {
	return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != NULL;
}

bool VirtualMemoryDecommit(void *ptr, size_t size) {
	return VirtualFree(ptr, size, MEM_DECOMMIT);
}

bool VirtualMemoryFree(void *ptr, size_t size) {
	return VirtualFree(ptr, 0, MEM_RELEASE);
}

#endif

#if PLATFORM_LINUX == 1
#include <sys/mman.h>
#include <stdlib.h>

static void InitOSContent() {}

static void *DefaultMemoryAllocateProc(size_t size, void *context) {
	return malloc(size);
}

static void *DefaultMemoryReallocateProc(void *ptr, size_t previous_size, size_t new_size, void *context) {
	return realloc(ptr, new_size);
}

static void DefaultMemoryFreeProc(void *ptr, void *context) {
	free(ptr);
}

void *VirtualMemoryAllocate(void *ptr, size_t size) {
	void *result = mmap(ptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (result == MAP_FAILED)
		return NULL;
	return result;
}

bool VirtualMemoryCommit(void *ptr, size_t size) {
	return mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0;
}

bool VirtualMemoryDecommit(void *ptr, size_t size) {
	return mprotect(ptr, size, PROT_NONE) == 0;
}

bool VirtualMemoryFree(void *ptr, size_t size) {
	return munmap(ptr, size) == 0;
}

#endif
