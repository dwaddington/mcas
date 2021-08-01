#ifndef __PERSONALITY_CPP_SYMTAB_CLIENT_H__
#define __PERSONALITY_CPP_SYMTAB_CLIENT_H__

#include <string>
#include <common/logging.h>
#include <common/exceptions.h>
#include <api/components.h>
#include <api/mcas_itf.h>
#include <ccpm/immutable_list.h>
#include <cpp_rangeindex_proto_generated.h>
#include <nop/utility/stream_writer.h>
#include <flatbuffers/flatbuffers.h>
#include "cpp_rangeindex_types.h"

namespace cpp_rangeindex_personality
{
using namespace flatbuffers;
using namespace component;
using namespace rangeindex_ADO_protocol;

class Symbol_table
{
public:
  Symbol_table(component::IMCAS * mcas,
               const component::IMCAS::pool_t pool,
               std::string table_name)
    : _mcas(mcas), _pool(pool),  _table_name(table_name)
  {
    if(mcas == nullptr) throw std::invalid_argument("bad parameter");

    /* initialize */
    std::vector<component::IMCAS::ADO_response> response;
    
    status_t rc = _mcas->invoke_ado(_pool,
                                    _table_name,
                                    nullptr,
                                    0,
                                    IMCAS::ADO_FLAG_CREATE_ONLY,
                                    response,
                                    sizeof(cpp_rangeindex_personality::Value_root));
    if(rc != S_OK) std::logic_error("init invoke_ado failed");
  }

  void add_word(const std::string& word) {
    assert(word.empty() == false);
    FlatBufferBuilder fbb;
    auto flat_word = fbb.CreateString(word);
    auto cmd = CreatePutRequest(fbb, flat_word);
    auto msg = CreateMessage(fbb, Command_PutRequest, cmd.Union());
    fbb.Finish(msg);
    
    std::vector<component::IMCAS::ADO_response> response;
    
    status_t rc = _mcas->invoke_ado(_pool,
                                    _table_name,
                                    fbb.GetBufferPointer(), /* request */
                                    fbb.GetSize(),
                                    0,
                                    response);
    if(rc != S_OK)
      throw General_exception("add_word failed");
  }

  void build_index() {
    FlatBufferBuilder fbb;
    auto cmd = CreateBuildIndex(fbb);
    auto msg = CreateMessage(fbb, Command_BuildIndex, cmd.Union());
    fbb.Finish(msg);
    
    std::vector<component::IMCAS::ADO_response> response;
    
    status_t rc = _mcas->invoke_ado(_pool,
                                    _table_name,
                                    fbb.GetBufferPointer(), /* request */
                                    fbb.GetSize(),
                                    0,
                                    response);
    if(rc != S_OK)
      throw General_exception("build_index failed");

  }

  std::string get_string(uint64_t bol) {
    FlatBufferBuilder fbb;
    //    auto flat_bol = fbb.CreateString(word);
    auto cmd = CreateGetString(fbb, bol);
    auto msg = CreateMessage(fbb, Command_GetString, cmd.Union());
    fbb.Finish(msg);
    
    std::vector<component::IMCAS::ADO_response> response;
    
    status_t rc = _mcas->invoke_ado(_pool,
                                    _table_name,
                                    fbb.GetBufferPointer(), /* request */
                                    fbb.GetSize(),
                                    0,
                                    response);
    if(rc != S_OK)
      throw General_exception("get_bol failed");

    assert(response.size() == 1);
    return std::string(response[0].data(), response[0].data_len());
  }


  uint64_t get_symbol(const std::string& word) {
    FlatBufferBuilder fbb;
    auto flat_word = fbb.CreateString(word);
    auto cmd = CreateGetSymbol(fbb, flat_word);
    auto msg = CreateMessage(fbb, Command_GetSymbol, cmd.Union());
    fbb.Finish(msg);
    
    std::vector<component::IMCAS::ADO_response> response;
    
    status_t rc = _mcas->invoke_ado(_pool,
                                    _table_name,
                                    fbb.GetBufferPointer(), /* request */
                                    fbb.GetSize(),
                                    0,
                                    response);
    if(rc != S_OK)
      throw General_exception("get_bol failed");

    assert(response.size() == 1);
    assert(response[0].data_len() == 8);
    
    return *(reinterpret_cast<const uint64_t*>(response[0].data()));
  }

private:
  component::IMCAS *             _mcas;
  const component::IMCAS::pool_t _pool;
  const std::string              _table_name;
};
  


}

#endif // __PERSONALITY_CPP_SYMTAB_CLIENT_H__
