#include <api/components.h>
#include <api/kvstore_itf.h>
#include <common/string_view.h>
#include <common/utils.h> /* MiB */
#include <gsl/pointers> /* not_null */

#include <cstddef> /* size_t */ 
#include <memory> /* unique_ptr */ 
#include <stdexcept> /* runtime_error */
#include <string>

namespace
{
	/* things which differ depending on the type of store used */
	struct custom_store
	{
		virtual ~custom_store() {}
		virtual std::size_t minimum_size(std::size_t s) const { return s; }
		virtual component::uuid_t factory() const = 0;
		virtual std::size_t presumed_allocation() const = 0;
		virtual bool uses_numa_nodes() const { return false; }
	};

	struct custom_mapstore
		: public custom_store
	{
		virtual component::uuid_t factory() const override { return component::mapstore_factory; }
		std::size_t minimum_size(std::size_t s) const override { return std::max(std::size_t(8), s); }
		std::size_t presumed_allocation() const override { return 1ULL << DM_REGION_LOG_GRAIN_SIZE; }
		virtual bool uses_numa_nodes() const { return true; }
	};

	struct custom_hstore
		: public custom_store
	{
		virtual component::uuid_t factory() const override { return component::hstore_factory; }
		std::size_t presumed_allocation() const override { return MiB(32); }
	};

	struct custom_hstore_cc
		: public custom_hstore
	{
		std::size_t presumed_allocation() const override { return MiB(32); }
	};

	custom_mapstore custom_mapstore_i{};
	custom_hstore custom_hstore_i{};
	custom_hstore_cc custom_hstore_cc_i{};

	const std::map<std::string, gsl::not_null<custom_store *>, std::less<>> custom_map =
	{
		{ "mapstore", &custom_mapstore_i },
		{ "hstore", &custom_hstore_i },
		{ "hstore-cc", &custom_hstore_cc_i },
		{ "hstore-mc", &custom_hstore_i },
		{ "hstore-mr", &custom_hstore_i },
		{ "hstore-mm", &custom_hstore_i },
	};
}

gsl::not_null<custom_store *> locate_custom_store(common::string_view store)
{
	const auto c_it = custom_map.find(store);
	if ( c_it == custom_map.end() )
    {
		throw std::runtime_error(common::format("store {} not recognized", store));
    }
	return c_it->second;
}

auto make_kvstore(
	common::string_view store
	, gsl::not_null<custom_store *> c
	, const component::IKVStore_factory::map_create & mc
) -> std::unique_ptr<component::IKVStore>
{
	using IKVStore_factory = component::IKVStore_factory;
	const std::string store_lib = "libcomponent-" + std::string(store) + ".so";
	auto *comp = component::load_component(store_lib.c_str(), c->factory());
	if ( ! comp )
    {
		throw std::runtime_error(common::format("failed to load component {}", store_lib));
    }

	const auto fact = make_itf_ref(static_cast<IKVStore_factory*>(comp->query_interface(IKVStore_factory::iid())));
	const auto kvstore = fact->create(0, mc);

  return std::unique_ptr<component::IKVStore>(kvstore);
}
