#include"co_mempool.h"

void test_mem_pool()
{
	/* create memory pool */
	co_mempool_t * pool = co_create_mempool(5120);
	if (pool != NULL) {
		log_info("create pool:%p,size:%u succ...",
				 pool, (size_t)(pool->d.end - pool->d.last));
	} else {
		return;
	}

	/* alloc on pool test */
	char * s1 = co_palloc(pool, 10*sizeof(char));
	if (s1 == NULL) {
		log_info("test s1 = co_palloc(pool:%p, size:%u) failed",
				 pool, 10*sizeof(char));

		return ;
	}
	strcpy(s1, "123456789");
	printf("s1 = %s\n", s1);

	int * n1 = co_palloc(pool, 1000*sizeof(int));
	if (n1 == NULL) {
		log_info("test n1 = co_palloc(pool:%p, size:%u) failed",
				 pool, 1000*sizeof(int));

		return ;
	}

	/* destroy memory pool */
	log_info("destroy pool:%p",pool);
	co_destroy_mempool(pool);
}

int main(void)
{
	test_mem_pool();
	return 0;
}
