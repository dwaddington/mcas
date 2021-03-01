/*
  Copyright [2021] [IBM Corporation]
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
  http://www.apache.org/licenses/LICENSE-2.0
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

#include <mpi.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <boost/program_options.hpp>

#include <common/exceptions.h>
#include <common/str_utils.h> /* random_string */
#include <common/utils.h> /* MiB */
#include <common/task.h>

#include <api/components.h>
#include <api/mcas_itf.h>

#include "tasks.h"

int main(int argc, char** argv)
{
  // Initialize the MPI environment
  MPI_Init(&argc, &argv);

  // Get the number of processes
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  // Get the rank of the process
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

  // // Get the name of the processor
  // char processor_name[MPI_MAX_PROCESSOR_NAME];
  // int name_len;
  // MPI_Get_processor_name(processor_name, &name_len);

  using namespace component;

  try {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("help", "Show help")
      ("debug", po::value<unsigned>()->default_value(0), "Debug level 0-3")
      ("patience", po::value<unsigned>()->default_value(30), "Patience with server (seconds)")
      ("server", po::value<std::string>()->default_value("10.0.0.101"), "Server network IP address")
      ("port", po::value<unsigned>()->default_value(11911), "Server network port")
      ("device", po::value<std::string>()->default_value("mlx5_0"), "Network device (e.g., mlx5_0)")
      ("basecore", po::value<unsigned>()->default_value(0), "Starting base worker core")
      ("cores", po::value<unsigned>()->default_value(1), "Core/thread count")
      ("key", po::value<unsigned>()->default_value(8), "Size of key in bytes")
      ("value", po::value<unsigned>()->default_value(16), "Size of value in bytes")
      ("pairs", po::value<unsigned>()->default_value(100000), "Number of key-value pairs")
      ("poolsize", po::value<unsigned>()->default_value(2), "Size of pool in GiB")
      ("log", po::value<std::string>()->default_value("/tmp/cpp-bench-log.txt"), "File to log results")
      ("test", po::value<std::string>()->default_value("read"), "Test 'read','write','rw50'")
      ("repeats", po::value<unsigned>()->default_value(1), "Number of experiment repeats")
      ("cps", po::value<unsigned>()->default_value(5), "Number of clients per shard (port)")
      ("direct","Use put_direct and get_direct APIs")
      ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help") > 0) {
      std::cout << desc;
      return -1;
    }

    Options.addr        = vm["server"].as<std::string>();
    Options.debug_level = vm["debug"].as<unsigned>();
    Options.patience    = vm["patience"].as<unsigned>();
    Options.base_core   = vm["basecore"].as<unsigned>();
    Options.cores       = vm["cores"].as<unsigned>();
    Options.key_size    = vm["key"].as<unsigned>();
    Options.value_size  = vm["value"].as<unsigned>();
    Options.pairs       = vm["pairs"].as<unsigned>();
    Options.device      = vm["device"].as<std::string>();
    Options.pool_size   = vm["poolsize"].as<unsigned>();
    Options.log         = vm["log"].as<std::string>();
    Options.test        = vm["test"].as<std::string>();
    Options.repeats     = vm["repeats"].as<unsigned>();
    Options.port        = vm["port"].as<unsigned>();
    Options.cps         = vm["cps"].as<unsigned>();
    Options.direct      = vm.count("direct");
  }
  catch (...) {
    std::cerr << "bad command line option configuration\n";
    return -1;
  }


  /* load component and create factory */
  IBase *comp = load_component("libcomponent-mcasclient.so", mcas_client_factory);
  factory = static_cast<IMCAS_factory *>(comp->query_interface(IMCAS_factory::iid()));
  assert(factory);

  /* derive port from clients-per-shard option */
  
  unsigned port = Options.port + (world_rank / Options.cps);
  
  Options.addr = Options.addr + ":" + std::to_string(port);

  PINF("* Rank %d using endpoint (%s)", world_rank, Options.addr.c_str());
  
  unsigned long iops = 0;
    
  {
    if(Options.test == "read") {
      Read_IOPS_task task(world_rank);
      MPI_Barrier(MPI_COMM_WORLD); 
      while(task.do_work(world_rank));
      iops = task.cleanup(world_rank);
    }
    else if(Options.test == "write") {
      Write_IOPS_task task(world_rank);
      MPI_Barrier(MPI_COMM_WORLD); 
      while(task.do_work(world_rank));
      iops = task.cleanup(world_rank);
    }
    else if(Options.test == "rw50") {
      Mixed_IOPS_task task(world_rank);
      MPI_Barrier(MPI_COMM_WORLD); 
      while(task.do_work(world_rank));
      iops = task.cleanup(world_rank);        
    }

  }

  MPI_Barrier(MPI_COMM_WORLD);

  /* collect results */
  if(world_rank == 0) {
    unsigned long total_iops = iops;
    
    for(int r=1; r < world_size; r++) {
      unsigned long rank_iops = 0;
      MPI_Status status;
      MPI_Recv(&rank_iops, 1, MPI_UNSIGNED_LONG, r, 0, MPI_COMM_WORLD, &status);
      PLOG("Got result (%lu) from rank: %d", rank_iops, r);
      total_iops += rank_iops;
    }

    PINF("Total IOPS: %lu", total_iops);
  }
  else {    
    MPI_Send(&iops, 1, MPI_UNSIGNED_LONG, 0, 0, MPI_COMM_WORLD);
  }


  factory->release_ref();

  // Finalize the MPI environment.
  MPI_Finalize();
}

#pragma GCC diagnostic pop
