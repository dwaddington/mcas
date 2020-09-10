#include <iostream>
#include <string>
#include <assert.h>
#include <api/components.h>
#include <api/cluster_itf.h>
#include <common/utils.h>
#include <common/task.h>
#include <common/logging.h>
#include <common/str_utils.h>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

po::variables_map vm;

class Task : public common::Tasklet {
public:

  Task(const std::string& end_point) : _node_name("undefined"), _end_point(end_point), _zyre(nullptr) {
  }
  
  virtual void initialize(unsigned core) override {

    using namespace component;
    
    std::stringstream ss;
    ss << vm["base-name"].as<std::string>() << "-" << core;
    _node_name = ss.str().c_str();
    
    /* create object instance through factory */
    component::IBase * comp = component::load_component("libcomponent-zyre.so",
                                                        component::cluster_zyre_factory);
    
    assert(comp);
    ICluster_factory * fact = static_cast<ICluster_factory *>(comp->query_interface(ICluster_factory::iid()));
    _zyre = make_itf_ref(fact->create(vm["debug"].as<unsigned>(), // debug_level
                                      _node_name,
                                      vm["nic"].as<std::string>(),//net_interface,
                                      vm["port"].as<unsigned int>() // port
                                      ));
    
    assert(_zyre.get());
  
    fact->release_ref();
    
    _zyre->start_node();
  
  }

#define SEND_COUNT 20
  
  virtual bool do_work(unsigned core) override
  {
    auto group = vm["group"].as<std::string>();
    _zyre->group_join(group);

    if(core == 0) {
      sleep(1);
      for(int i=0;i<SEND_COUNT;i++) {
      	std::stringstream ss;
        ss << "HELLO-FROM-" << core << "-" << i;
        _zyre->shout(group, "X-MSG", ss.str());
        sleep(1);

      }

    }
    else {

      unsigned int received = 0;

      while(received < SEND_COUNT) {


        std::string sender_uuid;
        std::string type;
        std::string message;
        std::vector<std::string> values;
        while(!_zyre->poll_recv(sender_uuid, type, message, values)) usleep(1000);

        if(values.size() > 0) {
	
          PLOG("(%u) received [%s,%s,%s,%s]",
               core,
               type.c_str(),
               message.c_str(),
               values[0].c_str() ? values[0].c_str() : "none",
               values[1].c_str() ? values[1].c_str() : "none");
        }
        else {
          PLOG("(%u) received [%s,%s]",
               core,
               type.c_str(),
               message.c_str());
        }
      
        if(type == "SHOUT")
          received++;

      }

      PMAJOR("(%u) received %u", core, received);
    }
    
    _zyre->stop_node();
    _zyre->destroy_node();
    return false;
  }

private:
  std::string _node_name;
  std::string _end_point;
  component::Itf_ref<component::ICluster> _zyre;
};


int main(int argc, char * argv[])
{
  
  try {
    po::options_description desc("Options");
    // clang-format off
    desc.add_options()
      ("help", "Show help")
      ("threads", po::value<unsigned>()->default_value(1), "Number of client threads")
      ("end-point", po::value<std::string>()->default_value("undefined"), "End point")
      ("group", po::value<std::string>()->default_value("Possy!"), "Cluster group")
      ("base-name", po::value<std::string>()->default_value(common::random_string(8)), "Base node name")
      ("base-core", po::value<unsigned>()->default_value(0), "Base core")
      ("port", po::value<unsigned>()->default_value(0), "Port")
      ("debug", po::value<unsigned>()->default_value(0), "Debug level")
      ("nic", po::value<std::string>()->default_value("eth0"), "Network interface")
      ;
    // clang-format on    


    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help") > 0) {
      std::cout << desc;
      return -1;
    }
  }
  catch (const po::error &) {
    printf("bad command line option\n");
    return -1;
  }

  auto base_core = vm["base-core"].as<unsigned>();
  cpu_mask_t mask;
  for (unsigned c = 0; c < vm["threads"].as<unsigned>(); c++) {
    mask.add_core(c + base_core);
  }

  common::Per_core_tasking<Task, const std::string> t(mask,
                                                      vm["end-point"].as<std::string>());
  t.wait_for_all();


  return 0;
}

  
