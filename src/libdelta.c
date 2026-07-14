#include "libdelta.h"

#include <continuityvm/ffi.h>
#include <libentropy/entropy.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

request_table_t *init_request_table(uint64_t worker_id) {
	request_table_t *table = malloc(sizeof(request_table_t));
	table->worker_id = worker_id;
	table->ct = 0;

	__CONTINUITYVM_FFI_Watch(worker_id);

	return table;
}

void drop_request_table(request_table_t *table) {
	for (uint64_t i = 0; i < table->ct; ++i) {
		// スタックから取り出す
		__CONTINUITYVM_FFI_Unlink(table->requests[i].data, table->worker_id);
		__CONTINUITYVM_FFI_Rehash();

		free(table->requests[i].data);
	}

	free(table);
}

entropy_request_t *make_entropy_request(uint64_t worker_id, uint64_t n, uint64_t *thread_id_components, double accuracy) {
	__CONTINUITYVM_FFI_ContinuityThreadID *builder = __CONTINUITYVM_FFI_MakeContinuityThreadIDBuilder();
	__CONTINUITYVM_FFI_SetThreadIDAccuracy(builder, accuracy, __CONTINUITYVM_FFI_LAGRANGE2);

	for (uint64_t i = 0; i < n; ++i) {
		__CONTINUITYVM_FFI_ContinuityThreadIDBuilderPush(builder, thread_id_components[i]);
	}

	__CONTINUITYVM_FFI_ContinuityThreadID *thread_id = __CONTINUITYVM_FFI_ContinuityThreadIDBuilderFinish(builder);

	uint64_t *hash = thread_send_request(worker_id, thread_id, NULL, NULL, 0, 0x100); // 256ビットパケット転送
	if (hash == NULL) {
		fprintf(stderr, "Failed to send entropy data request through worker %lu", worker_id);
		__CONTINUITYVM_FFI_Flush();
		return NULL;
	}

	uint64_t id = thread_get_request_id(hash); // VMスタックはIDを含む
	if (id == 0) {
		fprintf(stderr, "Failed to get request id from continuity hash");
		__CONTINUITYVM_FFI_Flush();
		return NULL;
	}

	long long status = thread_get_status(hash);
	uint8_t *data = thread_get_request_data(hash);
	uint64_t size = data != NULL ? __CONTINUITYVM_FFI_Sizeof(worker_id, hash, data) : 0;
	deltaf_kind_t kind = get_kind_from_hash_value(hash);

	entropy_request_t *request = malloc(sizeof(entropy_request_t));
	request->id = id;
	request->status = status;
	request->data = data;
	request->size = size;
	request->kind = kind;

	__CONTINUITYVM_FFI_Flush();

	return request;
}

void push_request(request_table_t *table, entropy_request_t *request) {
	if (table->ct == ENTROPY_REQUEST_TABLE_SIZE - 1) {
		fprintf(stderr, "Entropy request table is full please flush");
		return;
	}

	table->requests[table->ct++] = *request;
	__CONTINUITYVM_FFI_Rehash();
}

entropy_request_t *pop_request(request_table_t *table) {
	if (table->ct == 0) {
		fprintf(stderr, "Entropy request table is empty");
		return NULL;
	}

	entropy_request_t *request = &table->requests[--table->ct];

	__CONTINUITYVM_FFI_Decorator__MarkThrowable();
	verify_entropy_request_integrity(table->worker_id, &request);
	__CONTINUITYVM_FFI_Decorator__UnmarkThrowable();

	if (request == NULL) {
		// ヤバい
		fprintf(stderr, "Entropy request has been destroyed");
		return NULL;
	}

	table->requests[table->ct].data = NULL;
	__CONTINUITYVM_FFI_Rehash();

	return request;
}

double get_deltaf_stability(request_table_t *table, uint64_t n, uint64_t *thread_id_components) {
	// TODO
	return 1.0;
}

deltaf_kind_t get_kind_from_hash_value(entropy_hash_value_t value) {
	if (value == __CONTINUITYVM_FFI_NoneType) {
		return DELTAF_KIND_NONE;
	}

	if (__CONTINUITYVM_FFI_Gte(__CONTINUITYVM_FFI_Math__Abs(compute_entropy_hash_offset(value, NULL, 0) & __CONTINUITYVM_FFI_NC), LIBENTROPY_MAX_DIFFERENCE)) {
		return DELTAF_KIND_OFFSET;
	}

	if (value == __CONTINUITYVM_FFI_UnitType) {
		return DELTAF_KIND_CONTINUOUS;
	}

	if (__CONTINUITYVM_FFI_IsInfinity(__CONTINUITYVM_FFI_Math__Divergence(value, __CONTINUITYVM_FFI_NC))) {
		return DELTAF_KIND_INFINITE;
	}

	// ヤバい
	return DELTAF_KIND_ERRONEOUS;
}
