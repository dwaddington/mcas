#include <unistd.h>
#include <chrono>
#include <common/str_utils.h>
#include "example_fb_client.h"

using namespace example_fb;

int main()
{
  Client m(0,"10.0.0.101:11911","mlx5_0");

  auto pool = m.create_pool("myPets", MB(64));

  m.put(pool, "dog", "Violet");
  m.put(pool, "dog", "MadameFluffFace");
  m.put(pool, "cat", "Jasmine");
  m.put(pool, "kitten", "Jenny");
  m.put(pool, "chicken", "Nugget");
  m.put(pool, "chicken", "Bob");
  m.put(pool, "chicken", "Rosemary");
  m.put(pool, "chicken", "Ferdie");
  m.put(pool, "chicken", "Zucchini");


  std::string chicken_name;
  m.get(pool, "chicken", 0, chicken_name);
  PLOG("chicken 0: %s", chicken_name.c_str());

  m.get(pool, "chicken", -1, chicken_name);
  PLOG("chicken -1: %s", chicken_name.c_str());

  m.get(pool, "chicken", -2, chicken_name);
  PLOG("chicken -2: %s", chicken_name.c_str());

  PMAJOR("Running throughput Put test...");

  constexpr unsigned count = 10;

  std::vector<std::string> v_values;
  for(unsigned i=0;i<count;i++) {
    v_values.push_back(Common::random_string(16));    
  }

  auto start_time = std::chrono::high_resolution_clock::now();
  for(unsigned i=0;i<count;i++) {
    m.put(pool, "hotkey", v_values[i]);
  }
  auto end_time = std::chrono::high_resolution_clock::now();

  auto secs = std::chrono::duration<double>(end_time - start_time).count();
  auto iops = double(count) / secs;
  PLOG("%f iops", iops);
  
  m.close_pool(pool);
  m.delete_pool("myPets");
  
  return 0;
}
