## 无锁队列概述

DPDK中使用了一套无锁的环形缓冲队列机制`rte_ring`,它是一个固定长度的队列，主要包含如下功能.

 - FIFO (First In First Out)
 - Maximum size is fixed; the pointers are stored in a table.
 - Lockless implementation.
 - Multi- or single-consumer dequeue.
 - Multi- or single-producer enqueue.
 - Bulk dequeue.
 - Bulk enqueue.
 - Ability to select different sync modes for producer/consumer.
 - Dequeue start/finish (depending on consumer sync modes).
 - Enqueue start/finish (depending on producer sync mode).

详细文档可以参考：
```mk
http://doc.dpdk.org/guides/prog_guide/ring_lib.html
```

ring的结构如下

![Ring Struct](https://github.com/zartbot/learn_dpdk/blob/main/03_ring/images/1.png?raw=true)

```c
struct rte_ring {
	/*
	 * Note: this field kept the RTE_MEMZONE_NAMESIZE size due to ABI
	 * compatibility requirements, it could be changed to RTE_RING_NAMESIZE
	 * next time the ABI changes
	 */
	char name[RTE_MEMZONE_NAMESIZE] __rte_cache_aligned;
	/**< Name of the ring. */
	int flags;               /**< Flags supplied at creation. */
	const struct rte_memzone *memzone;
			/**< Memzone, if any, containing the rte_ring */
	uint32_t size;           /**< Size of ring. */
	uint32_t mask;           /**< Mask (size-1) of ring. */
	uint32_t capacity;       /**< Usable size of ring */

	char pad0 __rte_cache_aligned; /**< empty cache line */

	/** Ring producer status. */
	RTE_STD_C11
	union {
		struct rte_ring_headtail prod;
		struct rte_ring_hts_headtail hts_prod;
		struct rte_ring_rts_headtail rts_prod;
	}  __rte_cache_aligned;

	char pad1 __rte_cache_aligned; /**< empty cache line */

	/** Ring consumer status. */
	RTE_STD_C11
	union {
		struct rte_ring_headtail cons;
		struct rte_ring_hts_headtail hts_cons;
		struct rte_ring_rts_headtail rts_cons;
	}  __rte_cache_aligned;

	char pad2 __rte_cache_aligned; /**< empty cache line */
};
```
里面有一个union，包含三种结构：
```c
	union {
		struct rte_ring_headtail prod;
		struct rte_ring_hts_headtail hts_prod;
		struct rte_ring_rts_headtail rts_prod;
	} 
```
后两种是多线程使用的Relaxed tail sync(rts)和head/tail sync(hts)暂且不管，我们来看最基本的`rte_ring_headtail`结构
```c
struct rte_ring_headtail {
	volatile uint32_t head;      /**< prod/consumer head. */
	volatile uint32_t tail;      /**< prod/consumer tail. */
	RTE_STD_C11
	union {
		/** sync type of prod/cons */
		enum rte_ring_sync_type sync_type;
		/** deprecated -  True if single prod/cons */
		uint32_t single;
	};
};
```

简单而言，就是这个队列包含四个指针，生产者的头尾指针(prod head/tail)和消费者的头尾指针(consumer head/tail)

由于这个队列可以同时支持 单生产者->单消费者(spsc),单生产者->多消费者(spmc),多生产者->单消费者(mpsc),多生产者->多消费者(mpmc)等几种模式，接下来我们先从最简单的spsc讲起 

## 队列行为

### 单个生产者入队列

```c
static __rte_always_inline unsigned int
__rte_ring_do_enqueue_elem(struct rte_ring *r, const void *obj_table,
		unsigned int esize, unsigned int n,
		enum rte_ring_queue_behavior behavior, unsigned int is_sp,
		unsigned int *free_space)
{
	uint32_t prod_head, prod_next;
	uint32_t free_entries;

	n = __rte_ring_move_prod_head(r, is_sp, n, behavior,
			&prod_head, &prod_next, &free_entries);
	if (n == 0)
		goto end;

	__rte_ring_enqueue_elems(r, prod_head, obj_table, esize, n);

	__rte_ring_update_tail(&r->prod, prod_head, prod_next, is_sp, 1);
end:
	if (free_space != NULL)
		*free_space = free_entries - n;
	return n;
}
```
![Enqueue-1](https://github.com/zartbot/learn_dpdk/blob/main/03_ring/images/2.png?raw=true)

首先是把`prod_head`和`cons_tail`交给临时变量，`prod_next`指针向队列中的下一个对象，并采用检查`cons_tail`来看是否有足够的空间， 如果没有猪狗的空间，则跳转到`end`。


![Enqueue-2](https://github.com/zartbot/learn_dpdk/blob/main/03_ring/images/3.png?raw=true)

然后是`__rte_ring_enqueue_elems`保存对象到当前的`prod_head`位置， 最后是采用`__rte_ring_update_tail`将`prod_head`移动到`prod_next`完成入队操作.

![Enqueue-3](https://github.com/zartbot/learn_dpdk/blob/main/03_ring/images/4.png?raw=true)


### 单消费者出队

```c
static __rte_always_inline unsigned int
__rte_ring_do_dequeue_elem(struct rte_ring *r, void *obj_table,
		unsigned int esize, unsigned int n,
		enum rte_ring_queue_behavior behavior, unsigned int is_sc,
		unsigned int *available)
{
	uint32_t cons_head, cons_next;
	uint32_t entries;

	n = __rte_ring_move_cons_head(r, (int)is_sc, n, behavior,
			&cons_head, &cons_next, &entries);
	if (n == 0)
		goto end;

	__rte_ring_dequeue_elems(r, cons_head, obj_table, esize, n);

	__rte_ring_update_tail(&r->cons, cons_head, cons_next, is_sc, 0);

end:
	if (available != NULL)
		*available = entries - n;
	return n;
}
```
![dequeue-1](https://github.com/zartbot/learn_dpdk/blob/main/03_ring/images/5.png?raw=true)


同样也是将`cons_head`和`prod_tail`交给临时变量，然后`__rte_ring_move_cons_head`前移`cons_head`并指向`cons_next`，通过检查`prod_tail`查看队列是否为空，如果为空就跳转到`end`退出.否则就进行`__rte_ring_dequeue_elems`取出对象.
![dequeue-2](https://github.com/zartbot/learn_dpdk/blob/main/03_ring/images/6.png?raw=true)

取完了，更新`cons_tail`即可.

![dequeue-3](https://github.com/zartbot/learn_dpdk/blob/main/03_ring/images/7.png?raw=true)


### 多生产者入队

当有两个core需要入队时，此时需要一个`CAS`操作

![mp](https://github.com/zartbot/learn_dpdk/blob/main/03_ring/images/8.png?raw=true)


```c
static __rte_always_inline unsigned int
__rte_ring_move_prod_head(struct rte_ring *r, unsigned int is_sp,
		unsigned int n, enum rte_ring_queue_behavior behavior,
		uint32_t *old_head, uint32_t *new_head,
		uint32_t *free_entries)
{
	const uint32_t capacity = r->capacity;
	unsigned int max = n;
	int success;

	do {
		/* Reset n to the initial burst count */
		n = max;

		*old_head = r->prod.head;

		/* add rmb barrier to avoid load/load reorder in weak
		 * memory model. It is noop on x86
		 */
		rte_smp_rmb();

		/*
		 *  The subtraction is done between two unsigned 32bits value
		 * (the result is always modulo 32 bits even if we have
		 * *old_head > cons_tail). So 'free_entries' is always between 0
		 * and capacity (which is < size).
		 */
		*free_entries = (capacity + r->cons.tail - *old_head);

		/* check that we have enough room in ring */
		if (unlikely(n > *free_entries))
			n = (behavior == RTE_RING_QUEUE_FIXED) ?
					0 : *free_entries;

		if (n == 0)
			return 0;

		*new_head = *old_head + n;
		if (is_sp)
			r->prod.head = *new_head, success = 1;
		else
			success = rte_atomic32_cmpset(&r->prod.head,
					*old_head, *new_head);
	} while (unlikely(success == 0));
	return n;
}
```
可以看到在`__rte_ring_move_prod_head`函数中，如果是MP模式，则采用原子操作去确保入队更新
```c
success = rte_atomic32_cmpset(&r->prod.head,
	*old_head, *new_head);
```
同时在update `prod_tail`时，也采用类似的操作去等待
```c
	if (!single)
		while (unlikely(ht->tail != old_val))
			rte_pause();

	ht->tail = new_val;
```

![mp](https://github.com/zartbot/learn_dpdk/blob/main/03_ring/images/9.png?raw=true)


>所以您需要注意，在使用MP、MC时性能会有一些影响,尽量在软件设计上使用SP、SC，当然还有一些保序的问题，留到讲DLB的时候. 顺便说一下，我发现一个很好的`SoC`, intel Atom凌动 P5962B, 有QAT又有内置的以太网控制器，还带新出的DLB，看上去很不错.


## 队列操作实战

示例在github同目录下的`simpleRing`中，首先在main函数里面创建了mempool和ring
```c
zartbot_ring = rte_ring_create(_Z_RING, ring_size, rte_socket_id(), flags);
message_pool = rte_mempool_create(_MSG_POOL, pool_size,
				128, pool_cache, priv_data_sz,
				NULL, NULL, NULL, NULL,
				rte_socket_id(), flags);

if (zartbot_ring == NULL)
	rte_exit(EXIT_FAILURE, "Problem getting receiving ring\n");
if (message_pool == NULL)
	rte_exit(EXIT_FAILURE, "Problem getting message pool\n");
```

mempool和ring都有一个string可以在其它进程中索引获得， 我们在第二个示例中会介绍到,您先留意到这两个常量:

```c
static const char *_MSG_POOL = "MSG_POOL";
static const char *_Z_RING = "ZARTBOT_RING";
```
然后我们分别使用不同的`lcore`发送和接收消息， 发送端主要是采用`rte_ring_enqueue`函数
```c
static int
lcore_send(__rte_unused void *arg)
{
unsigned lcore_id = rte_lcore_id();
struct rte_ring *tx_ring;
	printf("Starting core %u\n", lcore_id);
    void *msg = NULL;
    if (rte_mempool_get(message_pool, &msg) < 0)
		rte_panic("Failed to get message buffer\n");
	
    for (int i=0;i<100;i++) {
        strlcpy((char *)msg, "hello", 128);
	    if (rte_ring_enqueue(zartbot_ring, msg) < 0) {
		    printf("Failed to send message - message discarded\n");
		    rte_mempool_put(message_pool, msg);
	    }
        sleep(1);
    }
}
```
而接收端主要是采用`rte_ring_dequeue`
```c
static int
lcore_recv(__rte_unused void *arg)
{
	unsigned lcore_id = rte_lcore_id();
	printf("Starting core %u\n", lcore_id);
	while (!quit){
		void *msg;
		if (rte_ring_dequeue(zartbot_ring, &msg) < 0){
			usleep(5);
			continue;
		}
		printf("core %u: Received '%s'\n", lcore_id, (char *)msg);
		rte_mempool_put(message_pool, msg);
	}
	return 0;
}
```



