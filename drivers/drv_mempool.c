#include "drv_mempool.h"
#include "rthw.h"



#ifdef MEMPOOL_128
static struct rt_mempool	m_mempool_128_ctrl;
static rt_uint8_t			m_mempool_128_pool[MEMPOOL_128_POOL_SIZE];
#endif

#ifdef MEMPOOL_256
static struct rt_mempool	m_mempool_256_ctrl;
static rt_uint8_t			m_mempool_256_pool[MEMPOOL_256_POOL_SIZE];
#endif

#ifdef MEMPOOL_512
static struct rt_mempool	m_mempool_512_ctrl;
static rt_uint8_t			m_mempool_512_pool[MEMPOOL_512_POOL_SIZE];
#endif

#ifdef MEMPOOL_1024
static struct rt_mempool	m_mempool_1024_ctrl;
static rt_uint8_t			m_mempool_1024_pool[MEMPOOL_1024_POOL_SIZE];
#endif

#ifdef MEMPOOL_2048
static struct rt_mempool	m_mempool_2048_ctrl;
static rt_uint8_t			m_mempool_2048_pool[MEMPOOL_2048_POOL_SIZE];
#endif



//初始化
static int _mempool_init(void)
{
	rt_err_t err;
	
#ifdef MEMPOOL_128
	err = rt_mp_init(&m_mempool_128_ctrl, "mem", (void *)m_mempool_128_pool, MEMPOOL_128_POOL_SIZE, MEMPOOL_128_BLOCK_SIZE);
	while(RT_EOK != err);
#endif
	
#ifdef MEMPOOL_256
	err = rt_mp_init(&m_mempool_256_ctrl, "mem", (void *)m_mempool_256_pool, MEMPOOL_256_POOL_SIZE, MEMPOOL_256_BLOCK_SIZE);
	while(RT_EOK != err);
#endif
	
#ifdef MEMPOOL_512
	err = rt_mp_init(&m_mempool_512_ctrl, "mem", (void *)m_mempool_512_pool, MEMPOOL_512_POOL_SIZE, MEMPOOL_512_BLOCK_SIZE);
	while(RT_EOK != err);
#endif
	
#ifdef MEMPOOL_1024
	err = rt_mp_init(&m_mempool_1024_ctrl, "mem", (void *)m_mempool_1024_pool, MEMPOOL_1024_POOL_SIZE, MEMPOOL_1024_BLOCK_SIZE);
	while(RT_EOK != err);
#endif
	
#ifdef MEMPOOL_2048
	err = rt_mp_init(&m_mempool_2048_ctrl, "mem", (void *)m_mempool_2048_pool, MEMPOOL_2048_POOL_SIZE, MEMPOOL_2048_BLOCK_SIZE);
	while(RT_EOK != err);
#endif

	return 0;
}
INIT_PREV_EXPORT(_mempool_init);



//申请内存
void *mempool_req(rt_uint16_t bytes_req, rt_int32_t ticks)
{
	void *pmem;
	
	if(0 == bytes_req)
	{
		return (void *)0;
	}
	
#ifdef MEMPOOL_128
	if(bytes_req <= MEMPOOL_128_BLOCK_SIZE)
	{
		pmem = rt_mp_alloc(&m_mempool_128_ctrl, ticks);
		if((void *)0 != pmem)
		{
			return pmem;
		}
		else if(RT_WAITING_NO != ticks)
		{
			return pmem;
		}
	}
#endif
	
#ifdef MEMPOOL_256
	if(bytes_req <= MEMPOOL_256_BLOCK_SIZE)
	{
		pmem = rt_mp_alloc(&m_mempool_256_ctrl, ticks);
		if((void *)0 != pmem)
		{
			return pmem;
		}
		else if(RT_WAITING_NO != ticks)
		{
			return pmem;
		}
	}
#endif
	
#ifdef MEMPOOL_512
	if(bytes_req <= MEMPOOL_512_BLOCK_SIZE)
	{
		pmem = rt_mp_alloc(&m_mempool_512_ctrl, ticks);
		if((void *)0 != pmem)
		{
			return pmem;
		}
		else if(RT_WAITING_NO != ticks)
		{
			return pmem;
		}
	}
#endif
	
#ifdef MEMPOOL_1024
	if(bytes_req <= MEMPOOL_1024_BLOCK_SIZE)
	{
		pmem = rt_mp_alloc(&m_mempool_1024_ctrl, ticks);
		if((void *)0 != pmem)
		{
			return pmem;
		}
		else if(RT_WAITING_NO != ticks)
		{
			return pmem;
		}
	}
#endif
	
#ifdef MEMPOOL_2048
	if(bytes_req <= MEMPOOL_2048_BLOCK_SIZE)
	{
		pmem = rt_mp_alloc(&m_mempool_2048_ctrl, ticks);
		if((void *)0 != pmem)
		{
			return pmem;
		}
		else if(RT_WAITING_NO != ticks)
		{
			return pmem;
		}
	}
#endif

	return (void *)0;
}

//内存池信息
rt_uint16_t mempool_info(rt_uint8_t pool)
{
	rt_uint16_t	info = 0;
	rt_base_t	level;

	switch(pool)
	{
	case 0:
#ifdef MEMPOOL_128
		level = rt_hw_interrupt_disable();
		info = m_mempool_128_ctrl.block_total_count;
		info <<= 8;
		info += m_mempool_128_ctrl.block_free_count;
		rt_hw_interrupt_enable(level);
#endif
		break;
	case 1:
#ifdef MEMPOOL_256
		level = rt_hw_interrupt_disable();
		info = m_mempool_256_ctrl.block_total_count;
		info <<= 8;
		info += m_mempool_256_ctrl.block_free_count;
		rt_hw_interrupt_enable(level);
#endif
		break;
	case 2:
#ifdef MEMPOOL_512
		level = rt_hw_interrupt_disable();
		info = m_mempool_512_ctrl.block_total_count;
		info <<= 8;
		info += m_mempool_512_ctrl.block_free_count;
		rt_hw_interrupt_enable(level);
#endif
		break;
	case 3:
#ifdef MEMPOOL_1024
		level = rt_hw_interrupt_disable();
		info = m_mempool_1024_ctrl.block_total_count;
		info <<= 8;
		info += m_mempool_1024_ctrl.block_free_count;
		rt_hw_interrupt_enable(level);
#endif
		break;
	case 4:
#ifdef MEMPOOL_2048
		level = rt_hw_interrupt_disable();
		info = m_mempool_2048_ctrl.block_total_count;
		info <<= 8;
		info += m_mempool_2048_ctrl.block_free_count;
		rt_hw_interrupt_enable(level);
#endif
		break;
	}

	return info;
}

