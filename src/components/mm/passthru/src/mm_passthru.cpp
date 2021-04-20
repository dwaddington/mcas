#include "mm_passthru.h"

/**
 * Factory entry point
 *
 */
extern "C" void* factory_createInstance(component::uuid_t component_id)
{
  if (component_id == Passthru_memory_manager_factory::component_id())
    return static_cast<void*>(new Passthru_memory_manager_factory());
  else
    return NULL;
}
