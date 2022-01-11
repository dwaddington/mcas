/*
 * Description  : PM sorting ADO test
 * Authors      : Omri Arad, Yoav Ben Shimon, Ron Zadicario
 * Authors email: omriarad3@gmail.com, yoavbenshimon@gmail.com, ronzadi@gmail.com
 * License      : Apache License, Version 2.0
 * Done as part of the Workshop on Search Data Structures for High-Performance Databases
 * at Tel-Aviv University.
 */

#include <unistd.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <boost/program_options.hpp>
#include <common/str_utils.h>
#include <common/utils.h> /* MiB */
#include "cpp_pm_sort_client.h"
#include "cpp_pm_sort_plugin.h"

struct Options
{
	unsigned debug_level;
	unsigned patience;
	std::string server;
	std::string device;
	unsigned port;
	unsigned type;
	bool reinit;
	bool verify;
} g_options;


int main(int argc, char *argv[])
{
	namespace po = boost::program_options;

	try {
		po::options_description desc("Options");

		desc.add_options()("help", "Show help")
			("server", po::value<std::string>()->default_value("10.0.0.101"), "Server hostname")
			("device", po::value<std::string>()->default_value("mlx5_0"), "Device (e.g. mlnx5_0)")
			("port", po::value<unsigned>()->default_value(11911), "Server port")
			("type", po::value<unsigned>()->default_value(3), "Sort type")
			("reinit", po::value<bool>()->default_value(false), "reinit")
			("verify", po::value<bool>()->default_value(true), "verify");

		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if (vm.count("help") > 0) {
			std::cout << desc;
			return -1;
		}

		g_options.server = vm["server"].as<std::string>();
		g_options.device = vm["device"].as<std::string>();
		g_options.port = vm["port"].as<unsigned>();
		g_options.type = vm["type"].as<unsigned>();
		g_options.reinit = vm["reinit"].as<bool>();
		g_options.verify = vm["verify"].as<bool>();
	}
	catch (po::error &) {
		printf("bad command line option\n");
		return -1;
	}

	std::stringstream url;
	url << g_options.server << ":" << g_options.port;

	/* main line */
	using namespace cpp_pm_sort;

	Client m(0, 1800, url.str(), g_options.device);

	if (g_options.reinit) {
		m.delete_pool("pool");
	}

	auto pool = m.open_pool("pool", false /* not read_only */);
	if (!pool) {
		printf("creating pool\n");
		pool = m.create_pool("pool", POOL_SIZE);
	}

	printf("starting init\n");
	m.init(pool);
	printf("finished init, starting sort\n");
	m.sort(pool, g_options.type);
	printf("finished sort\n");
	if (g_options.verify) {
		m.verify(pool);
		printf("finished verify\n");
	}

	m.close_pool(pool);

	return 0;
}
