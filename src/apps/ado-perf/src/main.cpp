#include <api/components.h>
#include <api/mcas_itf.h>
#include <common/cycles.h>
#include <common/str_utils.h>
#include <stdio.h>
#include <boost/program_options.hpp>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

struct Options {
  unsigned    debug_level;
  std::string server;
  std::string device;
  unsigned    port;
  bool        async;
} g_options;

Component::IMCAS* init(const std::string& server_hostname, int port);
void              do_work(Component::IMCAS* mcas);

int main(int argc, char* argv[])
{
  namespace po = boost::program_options;

  try {
    po::options_description            desc("Options");
    po::positional_options_description g_pos; /* no positional options */

    desc.add_options()("help", "Show help")(
        "server", po::value<std::string>()->default_value("10.0.0.21"),
        "Server hostname")("device",
                           po::value<std::string>()->default_value("mlx5_0"),
                           "Device (e.g. mlnx5_0)")(
        "port", po::value<unsigned>()->default_value(11911), "Server port")(
        "debug", po::value<unsigned>()->default_value(0), "Debug level")(
        "async", "Use asynchronous invocation");

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv)
                  .options(desc)
                  .positional(g_pos)
                  .run(),
              vm);

    if (vm.count("help") > 0) {
      std::cout << desc;
      return -1;
    }

    if (vm.count("server") == 0) {
      std::cout << "--server option is required\n";
      return -1;
    }

    g_options.server      = vm["server"].as<std::string>();
    g_options.device      = vm["device"].as<std::string>();
    g_options.port        = vm["port"].as<unsigned>();
    g_options.debug_level = vm["debug"].as<unsigned>();
    g_options.async       = vm.count("async");

    // mcas::Global::debug_level = g_options.debug_level =
    //     vm["debug"].as<unsigned>();
    auto mcasptr =
        init(vm["server"].as<std::string>(), vm["port"].as<unsigned>());
    do_work(mcasptr);
    mcasptr->release_ref();
  }
  catch (po::error e) {
    printf("bad command line option\n");
    return -1;
  }

  return 0;
}

Component::IMCAS* init(const std::string& server_hostname, int port)
{
  using namespace Component;

  IBase* comp = Component::load_component("libcomponent-mcasclient.so",
                                          mcas_client_factory);

  auto fact = (IMCAS_factory*) comp->query_interface(IMCAS_factory::iid());
  if (!fact) throw Logic_exception("unable to create MCAS factory");

  std::stringstream url;
  url << g_options.server << ":" << g_options.port;

  IMCAS* mcas = fact->mcas_create(g_options.debug_level, "None", url.str(),
                                  g_options.device);

  if (!mcas) throw Logic_exception("unable to create MCAS client instance");

  fact->release_ref();

  return mcas;
}

void do_work(Component::IMCAS* mcas)
{
  using namespace Component;

  const std::string poolname = "pool0";

  auto pool = mcas->create_pool(poolname, GB(1), 0, /* flags */
                                1000000);           /* obj count */
  if (pool == IKVStore::POOL_ERROR)
    throw General_exception("create_pool (%s) failed", poolname.c_str());

  std::vector<std::string> str_samples;

  unsigned       iterations  = 1000000;
  const unsigned num_strings = iterations;
  for (unsigned i = 0; i < num_strings; i++) {
    auto s = Common::random_string(8);  //(rdtsc() % 32) + 8);
    str_samples.push_back(s);
  }

  mcas->erase(pool, "symbol0");

  std::string request, response;

  auto flags = IMCAS::ADO_FLAG_CREATE_ON_DEMAND;
  if (g_options.async) {
    flags |= IMCAS::ADO_FLAG_ASYNC;
  }

  PLOG("Adding key symbol0..");

  /* synchronous ADO to prime ADO process */
  mcas->invoke_ado(pool, "symbol0", str_samples[0], flags, response, KB(4));

  PLOG("First invocation complete.");

  {
    using clock     = std::chrono::high_resolution_clock;
    auto start_time = clock::now();
    for (unsigned i = 0; i < iterations; i++) {
      mcas->invoke_ado(pool,
                       "symbol0",  // key
                       str_samples[i], flags, response, MB(4));
      if (false) {
        ::usleep(0);
      }
    }
    __sync_synchronize();

    auto secs =
        std::chrono::duration<double>(clock::now() - start_time).count();

    double per_sec = double(iterations) / secs;
    PINF("Synchronous ADO RTT");
    PINF("Time: %.2f sec", secs);
    PINF("Rate: %.0f /sec", per_sec);
  }

  // virtual status_t invoke_ado(const IKVStore::pool_t pool,
  //                              const std::string& key,
  //                              const std::vector<uint8_t>& request,
  //                              const uint32_t flags,
  //                              std::vector<uint8_t>& out_response,
  //                              const size_t value_size = 0) = 0;

  mcas->close_pool(pool);
}
