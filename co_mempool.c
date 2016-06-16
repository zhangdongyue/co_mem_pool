//=============================================
//Copyright (c) DONGYUE.ZDY All Rights Reserved.
//=============================================

/*
 * @file   co_mempool.c
 * @author dongyue.zippy (zhangdy1986@gmail.com)
 * @date   2016-06-15
 * @brief  memory pool
 * */

#include "co_mempool.h"

static void *co_palloc_block(co_mempool_t *pool, size_t size);
static void *co_palloc_large(co_mempool_t *pool, size_t size);

void * co_mem_alloc(size_t size) {
	void *p;
	p = malloc(size);
	if (p == NULL) {
		log_err("malloc size %d failed",size);
	}

	log_debug("malloc %p:%uz", p, size);

	return p;
}

void * co_mem_calloc(size_t size) {
	void *p;
	p = co_mem_alloc(size);
	if (p) {
		memset(p, 0, size);
	}
	return p;
}

#if (HAVE_POSIX_MEMALIGN)
void * co_memalign(size_t alignment, size_t size)
{
	void *p;
	int err;
	err = posix_memalign(&p, alignment, size);
	if (err) {
		log_err("posix_memalign(%uz, %uz) failed", alignment, size);
		p = NULL;
    }

	log_debug("posix_memalign :%p:%uz @%uz", p, size, alignment);	
    return p;
}
#elif (HAVE_MEMALIGN)
void * co_memalign(size_t alignment, size_t size)
{
	void *p;
	p = memalign(alignment, size);

	if (p == NULL) {
		log_err("memalign(%uz, %uz) failed", alignment, size);
	}

	log_debug("memalign :%p:%uz @%uz", p, size, alignment);	
	return p;
}
#endif

co_mempool_t * co_create_mempool(size_t size)
{
	co_mempool_t	*p; 
	co_pagesize  =	getpagesize();

	/* Or memalign(alignment, size) which return p 
	 * if have no memory align function , 
	 * please use co_alloc(size), 
	 * here we use posix_xx on linux
	 */
	p = co_memalign(CO_MEMPOOL_ALIGNMENT, size);  
	if (p == NULL) {
		log_err("co_memalign(%uz, %uz) failed", 
			CO_MEMPOOL_ALIGNMENT, size);
		return NULL;
	}

	p->d.last   = (unsigned char *)p + sizeof(co_mempool_t);
	p->d.end    = (unsigned char *)p + size;
	p->d.next   = NULL;
	p->d.failed = 0;

	size = size - sizeof(co_mempool_t); //deduct the header size.
	p->max = (size < CO_MAX_ALLOC_FROM_MEMPOOL) ? size : CO_MAX_ALLOC_FROM_MEMPOOL;

	p->current = p;
	//p->chain   = NULL;
	p->large   = NULL;
	p->cleanup = NULL;

	return p;

} /* co_create_mempool */

void co_destroy_mempool(co_mempool_t *pool)
{
	co_mempool_t            *p, *n;
	co_mempool_large_t      *l;
	co_mempool_cleanup_t    *c;

	for (c = pool->cleanup; c; c = c->next) {
		if (c->handler) {
			//TODO p log
			c->handler(c->data);
		}
	}

	for (l = pool->large; l; l = l->next) {
		//TODO p log
		if (l->alloc) {
			free(l->alloc);	
		}
	}

#if (CO_DEBUG)
	for (p = pool, n = pool->d.next; /* void */;p = n, n = n->d.next) {

		log_debug("free:%p,unused:%d", p, (p->d.end - p->d.last));
		
		if (n==NULL) {
			break;
		}
	}
#endif

	for (p = pool, n = pool->d.next; /* void */;p = n, n = n->d.next) {

		free(p);	

		if (n==NULL) {
			break;
		}
	}

} /* co_destroy_mempool */

void co_reset_mempool(co_mempool_t * pool)
{
	co_mempool_t       *p;
	co_mempool_large_t *l;

	for (l = pool->large; l; l = l->next) {
		if (l->alloc) {
			free(l->alloc);
		}
	}

	for (p = pool; p; p = p->d.next) {
		p->d.last = (unsigned char *)p + sizeof(co_mempool_t);
		p->d.failed = 0;
	}

	p->current = pool;
	//p->chain = NULL;
	p->large = NULL;

} /* co_reset_mempool */

void * co_palloc(co_mempool_t * pool, size_t size)
{
	unsigned char *m;
	co_mempool_t *p;

	if (size <= pool->max) {
		p = pool->current;

		do {
			m = co_align_ptr(p->d.last, CO_ALIGNMENT); 
			if ((size_t)(p->d.end - m) >= size) {
				p->d.last = m + size;
				return m;
			}
		} while (p);

		return co_palloc_block(pool, size);
	}	

	return co_palloc_large(pool, size);

} /* co_palloc */

void * co_pnalloc(co_mempool_t * pool, size_t size)
{
	unsigned char *m;
	co_mempool_t *p;

	if (size <= pool->max) {
		p = pool->current;

		do {
			m = p->d.last;
			if ((size_t)(p->d.end - m) >= size) {
				p->d.last = m + size;
				return m;
			}

			p = p->d.next;
		} while (p);

		return co_palloc_block(pool, size);
	}

	return co_palloc_large(pool, size);

} /* co_pnalloc */

/* Add new data block to append the memory pool, 
 * which size is equel to total size of pool before,
 * that is,increase double every time*/
static void * co_palloc_block(co_mempool_t *pool, size_t size)
{
	unsigned char *m;
	size_t psize;
	co_mempool_t *p, *new;

	psize = (size_t)(pool->d.end - (unsigned char *)pool);

	m = co_memalign(CO_MEMPOOL_ALIGNMENT, psize);
	if (m == NULL) {
		log_err("co_memalign(%uz, %uz) failed", 
			CO_MEMPOOL_ALIGNMENT, psize);
		return NULL;
	}

	new = (co_mempool_t *)m;

	new->d.end = m + psize;
	new->d.next = NULL;
	new->d.failed = 0;

	/* Here should be sizeof(co_mempool_t) ,
	 * the append pool blocks only use the
	 * co_mempool_data_t ,so we cut it*/
	m += sizeof(co_mempool_data_t);
	m = co_align_ptr(m, CO_ALIGNMENT);
	new->d.last = m + size;

	for (p = pool->current; p->d.next; p = p->d.next) {
		/* if four times alloc failed on a data block,
		 * never use it and change the current pool point to next */
		if (p->d.failed++ > 4) {
			pool->current = p->d.next;
		}
	}

	p->d.next = new;

	return m;

} /* co_palloc_block */

static void * co_palloc_large(co_mempool_t *pool, size_t size)
{
	void * p;
	unsigned int n;
	co_mempool_large_t *large;

	p = malloc(size);
	if (p == NULL) {
		//TODO log
		return NULL;
	}

	n = 0;

	for (large = pool->large; large; large = large->next) {
		if (large->alloc == NULL) {
			large->alloc = p;
			return p;
		}

		if (n++ > 3) {
			break;
		}
	}

	large = co_palloc(pool, sizeof(co_mempool_large_t));
	if (large == NULL) {
		free(p);
		return NULL;
	}

	/* Insert large node to head */
	large->alloc = p;
	large->next = pool->large;
	pool->large = large;	

	return p;

} /* co_palloc_large */

void * co_pmemalign(co_mempool_t *pool, size_t size, size_t alignment)
{
	void *p;
	co_mempool_large_t * large;

	p = co_memalign(alignment, size);
	if (p == NULL) {
		return NULL;
	}

	large = co_palloc(pool, sizeof(co_mempool_large_t));
	if (large == NULL) {
		free(p);
		return NULL;
	}

	large->alloc = p;
	large->next = pool->large;
	pool->large = large;

	return p;

} /* co_pmemalign */

int co_pfree(co_mempool_t * pool, void *p)
{
	co_mempool_large_t *l;

	for (l = pool->large; l; l = l->next) {
		if (p == l->alloc) {
			//TODO: log print
			free(l->alloc);
			l->alloc = NULL;

			return CO_OK; 
		}
	}

	return CO_ERR;

} /* co_pfree */

void * co_pcalloc(co_mempool_t *pool, size_t size)
{
	void *p;

	p = co_palloc(pool, size);
	if (p) {
		memset(p, 0, size);
	}

	return p;

} /* co_pcalloc */

co_mempool_cleanup_t * co_mempool_cleanup_add(co_mempool_t * p,size_t size)
{
	co_mempool_cleanup_t *c;

	c = co_palloc(p, sizeof(co_mempool_cleanup_t));
	if (c == NULL) {
		return NULL;
	}

	if (size) {
		c->data = co_palloc(p, size);
		if (c->data == NULL) {
			return NULL;
		}
	} else {
		c->data = NULL;
	}

	c->handler = NULL; //define by users
	c->next = p->cleanup;

	p->cleanup = c;

	return c;

} /* co_mempool_cleanup_add  */

void co_mempool_run_cleanup_file(co_mempool_t *p, int fd)
{
	co_mempool_cleanup_t *c;
	co_mempool_cleanup_file_t *cf;

	for (c = p->cleanup; c; c = c->next) {
		if (c->handler == co_mempool_cleanup_file) {
			cf = c->data;

			if (cf->fd == fd) {
				c->handler(cf);
				c->handler = NULL;
				return;
			}
		}
	}

} /* co_mempool_run_cleanup_file */

void co_mempool_cleanup_file(void *data) 
{
	co_mempool_cleanup_file_t *c = data;

	if (close(c->fd) == -1) {
		log_err("failed close file");
	}

} /* co_mempool_cleanup_file */

void co_mempool_delete_file(void *data) 
{
	co_mempool_cleanup_file_t *c = data;

	int err;

	if (unlink(c->name) == -1) {
		err = errno;

		if (err != ENOENT) {
			log_err("delete file %s failed", c->name);
		}
	}

	if (close(c->fd) == -1) {
		log_err("close file:%s failed", c->name);
	}

} /* co_mempool_delete_file */

