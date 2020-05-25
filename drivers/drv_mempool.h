#ifndef __DRV_MEMPOOL__
#define __DRV_MEMPOOL__



#include "rtthread.h"



//内存池
#define MEMPOOL_128
#define MEMPOOL_256
#define MEMPOOL_512
#define MEMPOOL_1024
#define MEMPOOL_2048

//内存块128字节
#ifdef MEMPOOL_128
#define MEMPOOL_128_BLOCK_SIZE			128
#define MEMPOOL_128_BLOCK_NUM			15
#define MEMPOOL_128_POOL_SIZE			((MEMPOOL_128_BLOCK_SIZE + sizeof(rt_uint8_t *)) * MEMPOOL_128_BLOCK_NUM)
#endif

//内存块256字节
#ifdef MEMPOOL_256
#define MEMPOOL_256_BLOCK_SIZE			256
#define MEMPOOL_256_BLOCK_NUM			10
#define MEMPOOL_256_POOL_SIZE			((MEMPOOL_256_BLOCK_SIZE + sizeof(rt_uint8_t *)) * MEMPOOL_256_BLOCK_NUM)
#endif

//内存块512字节
#ifdef MEMPOOL_512
#define MEMPOOL_512_BLOCK_SIZE			512
#define MEMPOOL_512_BLOCK_NUM			10
#define MEMPOOL_512_POOL_SIZE			((MEMPOOL_512_BLOCK_SIZE + sizeof(rt_uint8_t *)) * MEMPOOL_512_BLOCK_NUM)
#endif

//内存块1024字节
#ifdef MEMPOOL_1024
#define MEMPOOL_1024_BLOCK_SIZE			1100
#define MEMPOOL_1024_BLOCK_NUM			10
#define MEMPOOL_1024_POOL_SIZE			((MEMPOOL_1024_BLOCK_SIZE + sizeof(rt_uint8_t *)) * MEMPOOL_1024_BLOCK_NUM)
#endif

//内存块2048字节
#ifdef MEMPOOL_2048
#define MEMPOOL_2048_BLOCK_SIZE			2000
#define MEMPOOL_2048_BLOCK_NUM			5
#define MEMPOOL_2048_POOL_SIZE			((MEMPOOL_2048_BLOCK_SIZE + sizeof(rt_uint8_t *)) * MEMPOOL_2048_BLOCK_NUM)
#endif



//申请内存
void *mempool_req(rt_uint16_t bytes_req, rt_int32_t ticks);

//内存池信息
rt_uint16_t mempool_info(rt_uint8_t pool);



#endif

