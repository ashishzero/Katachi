#include "StringBuilder.h"

#include <string.h>
#include <stdio.h>

static String_Builder::Bucket *StringBuilderNewBucket(String_Builder *builder) {
	if (builder->free_list == nullptr) {
		const int bucket_allocation_count = 4;

		const size_t allocation_size = sizeof(String_Builder::Bucket) * bucket_allocation_count;
		auto buckets = (String_Builder::Bucket *)MemoryAllocate(allocation_size, builder->allocator);

		for (int i = 0; i < bucket_allocation_count - 1; ++i) {
			buckets[i].next = &buckets[i + 1];
			buckets[i].flags = 0;
		}

		buckets[0].flags |= STRING_BUILDER_BUCKET_ALLOCATED;
		buckets[bucket_allocation_count - 1].next = nullptr;
		buckets[bucket_allocation_count - 1].flags = 0;
		builder->free_list = buckets;
	}

	auto buk = builder->free_list;
	builder->free_list = buk->next;
	*buk = String_Builder::Bucket{};
	return buk;
}

int WriteBuffer(String_Builder *builder, void *buffer, int64_t size) {
	auto data = (uint8_t *)buffer;
	int written = 0;

	while (size > 0) {
		if (builder->current->written == STRING_BUILDER_BUCKET_SIZE) {
			auto new_bucket = StringBuilderNewBucket(builder);
			builder->current->next = new_bucket;
			builder->current = new_bucket;
		}

		int64_t write_size = Minimum(size, STRING_BUILDER_BUCKET_SIZE - builder->current->written);
		memcpy(builder->current->data + builder->current->written, data, write_size);
		builder->current->written += (int32_t)write_size;

		size -= write_size;
		data += write_size;

		written += (int)write_size;
	}

	builder->written += written;
	return written;
}

int Write(String_Builder *builder, char value) {
	return WriteBuffer(builder, &value, 1);
}

int Write(String_Builder *builder, uint8_t value) {
	return WriteBuffer(builder, &value, 1);
}

int Write(String_Builder *builder, int32_t value) {
	char buffer[128];
	int written = snprintf(buffer, sizeof(buffer), "%d", value);
	return WriteBuffer(builder, &buffer, written);
}

int Write(String_Builder *builder, uint32_t value) {
	char buffer[128];
	int written = snprintf(buffer, sizeof(buffer), "%u", value);
	return WriteBuffer(builder, &buffer, written);
}

int Write(String_Builder *builder, int64_t value) {
	char buffer[128];
	int written = snprintf(buffer, sizeof(buffer), "%zd", value);
	return WriteBuffer(builder, &buffer, written);
}

int Write(String_Builder *builder, uint64_t value) {
	char buffer[128];
	int written = snprintf(buffer, sizeof(buffer), "%zu", value);
	return WriteBuffer(builder, &buffer, written);
}

int Write(String_Builder *builder, float value) {
	char buffer[128];
	int written = snprintf(buffer, sizeof(buffer), "%f", value);
	return WriteBuffer(builder, &buffer, written);
}

int Write(String_Builder *builder, double value) {
	char buffer[128];
	int written = snprintf(buffer, sizeof(buffer), "%f", value);
	return WriteBuffer(builder, &buffer, written);
}

int Write(String_Builder *builder, void *value) {
	char buffer[128];
	int written = snprintf(buffer, sizeof(buffer), "%p", value);
	return WriteBuffer(builder, &buffer, written);
}

int Write(String_Builder *builder, const char *value) {
	return WriteBuffer(builder, (void *)value, strlen(value));
}

int Write(String_Builder *builder, String value) {
	return WriteBuffer(builder, value.data, value.length);
}

int WriteFormatted(String_Builder *builder, const char *format) {
	return WriteBuffer(builder, (void *)format, strlen(format));
}

//
//
//

String BuildString(String_Builder *builder, Memory_Allocator &allocator) {
	String string;
	string.data = (uint8_t *)MemoryAllocate(builder->written + 1, allocator);
	string.length = 0;

	for (auto bucket = &builder->head; bucket; bucket = bucket->next) {
		int64_t copy_size = bucket->written;
		memcpy(string.data + string.length, bucket->data, copy_size);
		string.length += copy_size;
	}

	string.data[string.length] = 0;
	return string;
}

void ResetBuilder(String_Builder *builder) {
	if (builder->current != &builder->head) {
		Assert(builder->current->next == nullptr && builder->head.next);
		builder->current->next = builder->free_list;
		builder->free_list = builder->head.next;
	}

	builder->head = String_Builder::Bucket{};
}

void FreeBuilder(String_Builder *builder) {
	ResetBuilder(builder);

	// Find the first allocated bucket
	auto root = builder->free_list;
	while (root) {
		if (root->flags & STRING_BUILDER_BUCKET_ALLOCATED) {
			break;
		}
		root = root->next;
	}

	if (root) {
		// Remove the buckets that are not allocated from the free-list
		auto parent = root;
		for (auto iter = parent->next; iter; ) {
			if (iter->flags & STRING_BUILDER_BUCKET_ALLOCATED) {
				parent = iter;
				iter = iter->next;
			}
			else {
				parent->next = iter->next;
				iter = iter->next;
			}
		}
	}

	// Finally free the buckets
	while (root) {
		auto ptr = root;
		root = root->next;
		MemoryFree(ptr, builder->allocator);
	}
}
