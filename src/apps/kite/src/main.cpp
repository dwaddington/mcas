#include <api/components.h>
#include <api/mcas_itf.h>
#include <assert.h>
#include <common/cycles.h>
#include <common/logging.h>
#include <common/str_utils.h>
#include <stdint.h>
#include <stdio.h>
#include <boost/program_options.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "kite.h"

using namespace std;

struct Options {
  unsigned debug_level;
  std::string server;
  std::string device;
  unsigned port;
  string filename;
} g_options;

Component::IMCAS *init(const std::string &server_hostname, int port);
void do_work(Component::IMCAS *mcas, string &filename);

int main(int argc, char *argv[]) {
  namespace po = boost::program_options;

  try {
    po::options_description desc("Options");

    desc.add_options()("help", "Show help")(
        "server", po::value<std::string>()->default_value("10.0.0.21"),
        "Server hostname")("device",
                           po::value<std::string>()->default_value("mlx5_0"),
                           "Device (e.g. mlnx5_0)")(
        "port", po::value<unsigned>()->default_value(11911), "Server port")(
        "debug", po::value<unsigned>()->default_value(0), "Debug level")(
        "file", po::value<string>(&g_options.filename)->required(),
        "Kmer file path");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help") > 0) {
      std::cout << desc;
      return -1;
    }

    if (vm.count("server") == 0) {
      std::cout << "--server option is required\n";
      return -1;
    }

    if (vm.count("file") == 0) {
      std::cout << "--file option is required\n";
      return -1;
    }

    g_options.server = vm["server"].as<std::string>();
    g_options.device = vm["device"].as<std::string>();
    g_options.port = vm["port"].as<unsigned>();
    g_options.debug_level = vm["debug"].as<unsigned>();
    g_options.filename = vm["file"].as<string>();

    // mcas::Global::debug_level = g_options.debug_level =
    //     vm["debug"].as<unsigned>();
    auto mcasptr =
        init(vm["server"].as<std::string>(), vm["port"].as<unsigned>());
    do_work(mcasptr, g_options.filename);
    mcasptr->release_ref();
  } catch (po::error e) {
    printf("bad command line option\n");
    return -1;
  }

  return 0;
}

Component::IMCAS *init(const std::string &server_hostname, int port) {
  using namespace Component;

  IBase *comp = Component::load_component("libcomponent-mcasclient.so",
                                          mcas_client_factory);

  auto fact = (IMCAS_factory *)comp->query_interface(IMCAS_factory::iid());
  if (!fact) throw Logic_exception("unable to create MCAS factory");

  std::stringstream url;
  url << g_options.server << ":" << g_options.port;

  IMCAS *mcas = fact->mcas_create(g_options.debug_level, "None", url.str(),
                                  g_options.device);

  if (!mcas) throw Logic_exception("unable to create MCAS client instance");

  fact->release_ref();

  return mcas;
}

void do_work(Component::IMCAS *mcas, string &filename) {
  using namespace Component;

  ifstream fs(filename, ios_base::in | ios_base::binary);
  File_binary_header_t hdr;
  fs.read(reinterpret_cast<char *>(&hdr), sizeof(File_binary_header_t));
  assert(hdr.magic == KSB_FILE_MAGIC);

  string gnome_id = filename.substr(filename.find_last_of('/') + 1, 16);

  PLOG(
      "Loading from binary set format "
      "(magic=%x,K=%u,genome_count=%lu,kmer_count=%lu,collecting_origins=%d,"
      "single=%lx)",
      hdr.magic, hdr.K, hdr.genome_count, hdr.kmer_count,
      hdr.collecting_origins, hdr.single_genome_id);
  vector<string> kmers;

  for (uint64_t i = 0; i < hdr.kmer_count; i++) {
    File_binary_kmer_t kmer_record;
    fs.read(reinterpret_cast<char *>(&kmer_record), sizeof(File_binary_kmer_t));

    PLOG("kmer_record (genome_vector_len=%u)", kmer_record.genome_vector_len);
    /* read kmer data */
    assert(hdr.K % 2 == 0);
    size_t kmer_data_len = hdr.K / 2;
    base_4bit_t kmer_data[kmer_data_len];
    fs.read(reinterpret_cast<char *>(kmer_data), kmer_data_len);
    PLOG("single gnome id: %s", gnome_id.c_str());
    PLOG("kmer: %s", str(kmer_data, hdr.K).c_str());
    kmers.push_back(str(kmer_data, hdr.K));
  }
  PLOG("loaded .ksb file K=%u origins=%d record: %d/%d", hdr.K,
       hdr.collecting_origins, kmers.size(), hdr.kmer_count);

  const std::string poolname = "poolname";

  auto pool = mcas->create_pool(poolname, GB(10), 0, /* flags */
                                hdr.kmer_count);     /* obj count */
  if (pool == IKVStore::POOL_ERROR)
    throw General_exception("create_pool (%s) failed", poolname.c_str());

  std::string request, response;

  auto start_time = std::chrono::high_resolution_clock::now();
  for (string kmer : kmers) {
    mcas->invoke_put_ado(pool, kmer, gnome_id, "EMPTYPTR", 0,
                         IMCAS::ADO_FLAG_NO_OVERWRITE, response);
  }
  __sync_synchronize();
  auto end_time = std::chrono::high_resolution_clock::now();
  mcas->close_pool(pool);
  auto secs = std::chrono::duration<double>(end_time - start_time).count();

  double per_sec = (((double)hdr.kmer_count) / secs);
  PINF("Time: %.2f sec", secs);
  PINF("Rate: %.0f /sec", per_sec);

  // virtual status_t invoke_ado(const IKVStore::pool_t pool,
  //                              const std::string& key,
  //                              const std::vector<uint8_t>& request,
  //                              const uint32_t flags,
  //                              std::vector<uint8_t>& out_response,
  //                              const size_t value_size = 0) = 0;
}
