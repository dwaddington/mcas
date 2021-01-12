#ifndef __KVSTORE_PROGRAM_OPTIONS_H__
#define __KVSTORE_PROGRAM_OPTIONS_H__

#include <boost/optional.hpp>
#include <boost/program_options.hpp>

#include <chrono>
#include <string>
#include <vector>

class ProgramOptions {
 private:
  bool component_is(const std::string &c) const { return component == c; }

 public:
  /* finalized in constructor */
  std::string test;
  std::string component;
  std::string cores;
  std::size_t elements;
  unsigned    key_length;
  unsigned    value_length;
  bool        do_json_reporting;
  bool        pin;
  bool        continuous;
  bool        verbose;
  bool        summary;
  unsigned    get_attr_pct;
  unsigned    read_pct;
  unsigned    insert_erase_pct;
  /* finalized later */
  std::string                                            devices;
  unsigned                                               time_secs;
  boost::optional<std::string>                           path;
  std::string                                            pool_name;
  unsigned long long                                     size;
  std::uint32_t                                          flags;
  std::string                                            report_file_name;
  unsigned                                               bin_count;
  double                                                 bin_threshold_min;
  double                                                 bin_threshold_max;
  unsigned                                               debug_level;
  boost::optional<std::chrono::system_clock::time_point> start_time;
  boost::optional<unsigned>                              duration;
  unsigned                                               report_interval;
  std::string                                            owner;
  std::string                                            server_address;
  boost::optional<std::string>                           provider;
  unsigned                                               port;
  boost::optional<unsigned>                              port_increment;
  boost::optional<std::string>                           device_name;
  boost::optional<std::string>                           src_addr;
  boost::optional<std::string>                           pci_addr;
  boost::optional<std::string>                           log_file;
  bool                                                   random;

  ProgramOptions(const boost::program_options::variables_map &);

  static std::string infiniband_device_text();

  static void add_program_options(boost::program_options::options_description &desc,
                                  const std::vector<std::string> &             test_vector);
};

#endif
