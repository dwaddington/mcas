#include <unistd.h>
#include <chrono>
#include <common/str_utils.h>
#include <common/utils.h> /* MiB */
#include "example_fb_client.h"

using namespace example_fb;

int main()
{
  Client m(0,120,"10.0.0.101:11911","mlx5_0");

  auto pool = m.create_pool("myPets", MiB(128));

  m.put(pool, "dog", "Violet");
  m.put(pool, "dog", "MadameFluffFace");
  m.put(pool, "cat", "Jasmine");
  m.put(pool, "kitten", "Jenny");

  PINF("Naming chicken Nugget..");
  m.put(pool, "chicken", "Nugget");

  PINF("Re-naming chicken Bob..");
  m.put(pool, "chicken", "Bob");

  PINF("Re-naming chicken Rosemary..");
  m.put(pool, "chicken", "Rosemary");

  PINF("Re-naming chicken Ferdie..");
  m.put(pool, "chicken", "Ferdie");

  PINF("Re-naming chicken Zucchini..");
  m.put(pool, "chicken", "Zucchini");


  std::string chicken_name;
  m.get(pool, "chicken", 0, chicken_name);
  PLOG("Current chicken 0: %s", chicken_name.c_str());

  m.get(pool, "chicken", -1, chicken_name);
  PLOG("Previous chicken -1: %s", chicken_name.c_str());

  m.get(pool, "chicken", -2, chicken_name);
  PLOG("Previous-previous chicken -2: %s", chicken_name.c_str());

  m.close_pool(pool);
  m.delete_pool("myPets");
  
  return 0;
}
