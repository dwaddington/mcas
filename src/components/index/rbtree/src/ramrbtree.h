/*
 * (C) Copyright IBM Corporation 2018, 2020. All rights reserved.
 *
 */

#ifndef __RAMRBTREE_COMPONENT_H__
#define __RAMRBTREE_COMPONENT_H__

#include <set>
#include <api/kvindex_itf.h>

class RamRBTree : public component::IKVIndex {
 public:
  RamRBTree(const std::string& owner, const std::string& name);
  RamRBTree();
  virtual ~RamRBTree();

  DECLARE_VERSION(0.1f);
  DECLARE_COMPONENT_UUID(0x8a120985, 0x1253, 0x404d, 0x94d7, 0x77, 0x92, 0x75, 0x21, 0xa1, 0x29); //
  
  void* query_interface(component::uuid_t& itf_uuid) override
  {
    if (itf_uuid == component::IKVIndex::iid()) {
      return static_cast<component::IKVIndex*>(this);
    }
    else
      return NULL;  // we don't support this interface
  }

  void unload() override { delete this; }

 public:
  virtual void        insert(const std::string& key) override;
  virtual void        erase(const std::string& key) override;
  virtual void        clear() override;
  virtual std::string get(offset_t position) const override;
  virtual size_t      count() const override;
  virtual status_t    find(const std::string& key_expression,
                           offset_t           begin_position,
                           find_t             find_type,
                           offset_t&          out_end_position,
                           std::string&       out_matched_key,
                           unsigned           max_comparisons = 0) override;
private:
  std::set<std::string> _index;
};

class RamRBTree_factory : public component::IKVIndex_factory {
 public:
  DECLARE_VERSION(0.1f);
  DECLARE_COMPONENT_UUID(0xfac20985, 0x1253, 0x404d, 0x94d7, 0x77, 0x92, 0x75, 0x21, 0xa1, 0x29); //

  void* query_interface(component::uuid_t& itf_uuid) override
  {
    if (itf_uuid == component::IKVIndex_factory::iid()) {
      return static_cast<component::IKVIndex_factory*>(this);
    }
    else
      return NULL;  // we don't support this interface
  }

  void unload() override { delete this; }

  virtual component::IKVIndex* create(const std::string& owner,
                                      const std::string& name) override
  {
    component::IKVIndex* obj =
        static_cast<component::IKVIndex*>(new RamRBTree(owner, name));
    assert(obj);
    obj->add_ref();
    return obj;
  }
};
#endif
