#include <ATen/native/DispatchStub.h>

#include <c10/util/Exception.h>

#include <cpuinfo.h>
#include <cstdlib>
#include <cstring>

namespace at { namespace native {

static CPUCapability compute_cpu_capability() {
  auto envar = std::getenv("ATEN_CPU_CAPABILITY");
  if (envar) {
#ifdef HAVE_VSX_CPU_DEFINITION
    if (strcmp(envar, "vsx") == 0) {
      return CPUCapability::VSX;
    }
#else
    if (strcmp(envar, "avx2") == 0) {
      return CPUCapability::AVX2;
    }
    if (strcmp(envar, "avx") == 0) {
      return CPUCapability::AVX;
    }
#endif
    if (strcmp(envar, "default") == 0) {
      return CPUCapability::DEFAULT;
    }
    TORCH_WARN("ignoring invalid value for ATEN_CPU_CAPABILITY: ", envar);
  }

#if !defined(__powerpc__) && !defined(__s390x__)
  if (cpuinfo_initialize()) {
    if (cpuinfo_has_x86_avx2() && cpuinfo_has_x86_fma3()) {
      return CPUCapability::AVX2;
    }
    if (cpuinfo_has_x86_avx()) {
      return CPUCapability::AVX;
    }
  }
#endif
#ifdef HAVE_VSX_CPU_DEFINITION
  return CPUCapability::VSX;
#else
  return CPUCapability::DEFAULT;
#endif
}

CPUCapability get_cpu_capability() {
  static CPUCapability capability = compute_cpu_capability();
  return capability;
}

void* DispatchStubImpl::get_call_ptr_impl(DeviceType device_type) {
  switch (device_type) {
    case DeviceType::CPU: {
      // Use memory_order_relaxed here since even if two threads race,
      // they will still compute the same value for cpu_dispatch_ptr.
      auto fptr = cpu_dispatch_ptr.load(std::memory_order_relaxed);
      if (!fptr) {
        fptr = choose_cpu_fn();
        cpu_dispatch_ptr.store(fptr, std::memory_order_relaxed);
      }
      return fptr;
    }

    case DeviceType::CUDA:
      TORCH_INTERNAL_ASSERT(cuda_dispatch_ptr, "DispatchStub: missing CUDA kernel");
      return cuda_dispatch_ptr;

    case DeviceType::HIP:
      TORCH_INTERNAL_ASSERT(hip_dispatch_ptr, "DispatchStub: missing HIP kernel");
      return hip_dispatch_ptr;

    default:
      AT_ERROR("DispatchStub: unsupported device type", device_type);
  }
}

}}  // namespace at::native
