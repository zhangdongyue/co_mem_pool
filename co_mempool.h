//=============================================
//Copyright (c) DONGYUE.ZDY All Rights Reserved.
//=============================================

/*
 * @file   co_mempool.h
 * @author dongyue.zippy (zhangdy1986@gmail.com)
 * @date   2016-06-15
 * @brief  memory pool
 * */

#ifndef __CO_MEMPOOL_H__
#define __CO_MEMPOOL_H__

#include<unistd.h>
#include<limits.h>
#include<stdlib.h>
#include<string.h>
#include<stdarg.h>

/* co_pagesize is always 4096 on x86/linux */
static const size_t  co_pagesize = getpagesize();
#define co_align(d, a)	((d) + (a - 1) & ~(a - 1))
#define co_align_ptr(p, a) \
	(unsigned char *) (((unsigned int) (p) + ((unsigned int) a - 1)) & ~((unsigned int) a - 1))

#define CO_ALIGNMENT	sizeof(unsigned long)
#define CO_MAX_ALLOC_FROM_MEMPOOL	(co_pagesize - 1)

#define CO_DEFAULT_MEMPOOL_SIZE (16 * 1024)

#define CO_MEMPOOL_ALIGNMENT	16
#define CO_MIN_MEMPOOL_SIZE 

#define CO_OK 0
#define CO_ERR 1

#define log_err(msg, ...) \
	fprintf(stderr,"[ERROR][%s:%d] "msg" : %s\n", \
			__FILE__, __LINE__, ##__VA_ARGS__, strerror(errno))

#define log_info(msg, ...) \
	fprintf(stderr,"[INFO][%s:%d] "msg" \n", \
			__FILE__, __LINE__, ##__VA_ARGS__)

#if CO_DEBUG
#define log_debug(msg, ...) \
	fprintf(stderr,"[DEBUG][%s:%d] "msg" \n", \
			__FILE__, __LINE__, ##__VA_ARGS__)
#else
#define log_debug(msg, ...) NULL
#endif

typedef void (* co_mempool_cleanup_pt)(void *data);

typedef struct co_mempool_cleanup_s co_mempool_cleanup_t;
struct co_mempool_cleanup_s {
	co_mempool_cleanup_pt	handler;
	void	*data;
	co_mempool_cleanup_t * next;

};

typedef struct co_mempool_large_s	co_mempool_large_t;
struct co_mempool_large_s {
	co_mempool_large_t	*next;
	void	*alloc;
};

typedef struct co_mempool_s	co_mempool_t;
typedef struct {
	unsigned char	*last;
	unsigned char	*end;
	co_mempool_t	*next;
	unsigned int	failed;
} co_mempool_data_t;

struct co_mempool_s {
	co_mempool_data_t	d;
	size_t	max;
	co_mempool_t	*current;
	co_chain_t	*chain;
	co_mempool_large_t	*large;
	co_mempool_cleanup_t	*cleanup;
	//TODO: add log structure
};

typedef struct {
	int	fd;
	unsigned char	*name;
	//TODO: add log structure	
} co_mempool_cleanup_file_t;

/* Replace system calls */
void * co_mem_alloc(size_t size); 
void * co_mem_calloc(size_t size);
#if (HAVE_POSIX_MEMALIGN || HAVE_MEMALIGN) 
void * co_memalign(size_t alignment, size_t size);
#else
#define co_memalign(alignment, size) co_mem_alloc(size)
#endif

#define HAVE_POSIX_MEMALIGN 1

co_mempool_t co_create_mempool(size_t size);
void * co_destroy_mempool(co_mempool_t * pool);
void * co_reset_mempool(co_mempool_t * pool);

/* Alloc on mempool */
void * co_palloc(co_mempool_t *pool, size_t size);
void * co_pnalloc(co_mempool_t *pool, size_t size);
void * co_pcalloc(co_mempool_t *pool, size_t size);
void * co_pmemalign(co_mempool_t * pool, size_t size, size_t alignment);
int co_pfree(co_mempool_t *pool, void *p);

/* Clean up*/
co_mempool_cleanup_t *co_mempool_cleanup_add(co_mempool_t *p, size_t size);
void co_mempool_run_cleanup_file(co_mempool_t *p, int fd);
void co_mempool_cleanup_file(void *data);
void co_mempool_delete_file(void *data);


#endif /* __CO_MEMPOOL_H__ */
