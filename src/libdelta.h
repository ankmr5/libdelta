#ifndef __LIBDELTA_H__
#define __LIBDELTA_H__

#include <stdlib.h>

#define ENTROPY_REQUEST_TABLE_SIZE 0x1000

typedef enum deltaf_kind {
	DELTAF_KIND_NONE,
	DELTAF_KIND_OFFSET,
	DELTAF_KIND_CONTINUOUS,
	DELTAF_KIND_INFINITE,
	DELTAF_KIND_ERRONEOUS,
} deltaf_kind_t;

typedef enum request_status_code {
	STATUS_ERROR = 0b0001,
	STATUS_OK = 0b0010,
	STATUS_FORBIDDEN = 0b0100,
} request_status_code_t;

typedef struct entropy_request {
	uint64_t id;
	uint64_t size;
	deltaf_kind_t kind;
	uint8_t *data;
	request_status_code_t status;
} entropy_request_t;

typedef struct request_table {
	u_int64_t worker_id;
	u_int64_t ct;

	entropy_request_t requests[ENTROPY_REQUEST_TABLE_SIZE];
} request_table_t;

request_table_t *init_request_table(u_int64_t worker_id);
void drop_request_table(request_table_t *table);

entropy_request_t *make_entropy_request(uint64_t worker_id, uint64_t n, uint64_t *thread_id_components, double accuracy);

void push_request(request_table_t *table, entropy_request_t *request);
entropy_request_t *pop_request(request_table_t *table);

double get_deltaf_stability(request_table_t *table, uint64_t n, uint64_t *thread_id_components);

#endif /* __LIBDELTA_H__ */
