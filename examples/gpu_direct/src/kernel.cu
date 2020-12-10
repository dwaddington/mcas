/** 
 * Must have OFED compiled with CUDA and nv_peer_memory module loaded.
 * 
 */
#include <chrono>
#include <stdio.h>
#include <common/logging.h>
#include <common/dump_utils.h>
#include <common/utils.h>
#include <api/mcas_itf.h>
#include <cuda.h>

#define ASSERT(x)           \
	do {											\
	if (!(x)) {										\
		fprintf(stdout, "Assertion \"%s\" failed at %s:%d\n", #x, __FILE__, __LINE__);	\
	}											\
} while (0)

#define CUDA_CHECK(x)  if(x != cudaSuccess) \
    PERR("error: cuda err=%s", cudaGetErrorString (cudaGetLastError()));

#define CUCHECK(stmt)                           \
	do {					\
	CUresult result = (stmt);		\
	ASSERT(CUDA_SUCCESS == result);		\
} while (0)

static CUdevice cuDevice;
static CUcontext cuContext;

__global__ void verify_memory(void * ptr)
{
  char * p = (char *) ptr;
  printf("Viewed from GPU: 0x%02x 0x%02x 0x%02x ...\n", p[0] & 0xFF, p[1] & 0xFF, p[2] & 0xFF);
}

extern "C" void run_cuda(component::IMCAS * mcas)
{ 
  PMAJOR("run_test (cuda app lib)");

  CUresult error = cuInit(0);
	if (error != CUDA_SUCCESS)
    throw General_exception("cuInit(0) returned %d", error);

	int deviceCount = 0;
	error = cuDeviceGetCount(&deviceCount);

	if (error != CUDA_SUCCESS)
    throw General_exception("cuDeviceGetCount() returned %d", error);
  
	/* This function call returns 0 if there are no CUDA capable devices. */
	if (deviceCount == 0)
		throw General_exception("there are no available device(s) that support CUDA");
  else if (deviceCount == 1)
		PMAJOR("There is 1 device supporting CUDA");
	else
		PMAJOR("There are %d devices supporting CUDA, picking first...", deviceCount);

  int devID = 0;
	/* pick up device with zero ordinal (default, or devID) */
	CUCHECK(cuDeviceGet(&cuDevice, devID));

  char name[128];
	CUCHECK(cuDeviceGetName(name, sizeof(name), devID));
	PMAJOR("[pid = %d, dev = %d] device name = [%s]", getpid(), cuDevice, name);
	PMAJOR("creating CUDA Ctx");

	/* Create context */
	error = cuCtxCreate(&cuContext, CU_CTX_MAP_HOST, cuDevice);
	if (error != CUDA_SUCCESS)
		throw General_exception("cuCtxCreate() error=%d", error);

	PMAJOR("making it the current CUDA Ctx");
	error = cuCtxSetCurrent(cuContext);
	if (error != CUDA_SUCCESS)
		throw General_exception("cuCtxSetCurrent() error=%d", error);
  
  const size_t buffer_size = MB(128); /* there is some sort of inherent limit on this size */
  CUdeviceptr d_A;

  /* allocate GPU side memory and map with gdr into CPU side */
	error = cuMemAlloc(&d_A, buffer_size);
	if (error != CUDA_SUCCESS)
		throw General_exception("cuMemAlloc error=%d", error);
  
	PMAJOR("allocated GPU buffer address at %016llx pointer=%p",
         d_A, (void *) d_A);

    /* register memory with RDMA engine */
  auto handle = mcas->register_direct_memory((void*)d_A, buffer_size);
  assert(handle);
  PMAJOR("registered memory with MCAS for direct transfers.");

  PMAJOR("set buffer to 0xBB");
  cuMemsetD8(d_A, 0xBB, buffer_size);
  verify_memory<<<1,1>>>((char*)d_A); /* executes on GPU */
  cudaDeviceSynchronize();
  
  /* create pool */
  auto pool = mcas->create_pool("gpuPool", GiB(8));
  
  /* put into MCAS */
  status_t rc;
  auto start = std::chrono::high_resolution_clock::now();

  constexpr unsigned ITERATIONS = 10;
  
  for(unsigned i=0;i<ITERATIONS;i++) {
    rc = mcas->put_direct(pool, "key0", (void*)d_A, buffer_size, handle);
    assert(rc == S_OK);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto secs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0;
  PMAJOR("GPU --> MCAS Throughput: %f MB/s", (128.0f * ITERATIONS) / secs);

  /* zero memory on GPU */
  cuMemsetD8(d_A, 0x0, buffer_size);
  cudaDeviceSynchronize();

  PMAJOR("Zero'ed GPU memory.");
  verify_memory<<<1,1>>>((char*)d_A);
  cudaDeviceSynchronize();

  PMAJOR("About to read back from MCAS...");
  /* reload memory from mcas */
  size_t rsize = buffer_size;

  start = std::chrono::high_resolution_clock::now();

  for(unsigned i=0;i<ITERATIONS;i++) {
    rc = mcas->get_direct(pool, "key0", (void*)d_A, rsize, handle);
    assert(rc == S_OK);
    assert(rsize == buffer_size);
  }
  
  end = std::chrono::high_resolution_clock::now();
  secs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0;
  PMAJOR("GPU <-- MCAS Throughput: %f MB/s", (128.0f * ITERATIONS) / secs);

  /* re-verify from GPU side and voila!! */
  cudaDeviceSynchronize();

  PMAJOR("Verifying memory, should be 0xBB-array again...");
  verify_memory<<<1,1>>>((char*)d_A);

  cudaDeviceSynchronize();
    
  /* clean up */
  mcas->unregister_direct_memory(handle);
  mcas->close_pool(pool);

  PMAJOR("Voila!");
}



