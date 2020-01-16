#include <EASTL/string.h>
#include <EASTL/map.h>

#include <boost/io/ios_state.hpp>
#include <cstdlib>
#include <string>
#include <iostream>

namespace
{
	class map_tag
	{
		std::string _tag;
	public:
		map_tag(const std::string &tag_)
			: _tag(tag_)
		{}
		const std::string &tag() const { return _tag; }
	};

	/* A "tracker" class, for our purposes, write a single-line report before and after
	 * every tracked modfication
	 */
	template <typename D>
		class tracker
		{
			void report(const char *what_, const void *p_, std::size_t s_, char t_) const
			{
				boost::io::ios_iostate_saver st(std::cout);
				std::cout << static_cast<const D *>(this)->id() << ": " << what_ << " " << t_ << " " << p_ << "." << std::hex << s_ << "\n";
			}

		public:
			/* Function which must be provided by the tracker. Called before a write
			 * to an area with pre-existing data.
			 *  - p locates the area
			 *  - s is the size of the area, in bytes
			 *  - t is an optional value for debugging a class which supports tracking.
			 */
			void track_pre(const void *p_, std::size_t s_, char t_ = '0') const noexcept
			{
				report("pre-write", p_, s_, t_);
			}

			void track_post(const void *p_, std::size_t s_, char t_ = '1') const noexcept
			{
				report("post-write", p_, s_, t_);
			}
		};

	class map_tracker
		: public tracker<map_tracker>
	{
		/* To make things intersting, class map_tracker has state. Not all EASTL
		 * types which support tracking support a stateful tracker. basic_string
		 * and deque do not, and others which might (list, vector) have not been
		 * tested. But map does support one.
		 */
		map_tag *_id;

	public:
		map_tracker(map_tag *id_)
			: _id(id_)
		{}

		std::string id() const { return "map_tracker " + _id->tag(); }
	};

	class string_tracker
		: public tracker<string_tracker>
	{
		/* Class string_tracker has no state. We wish it could have state, but the EASTL
		 * string implementation complains when the size of Allocator is non-zero.
		 */
	public:
		std::string id() const { return "string_tracker"; }
	};

	/* An allocator without tracking, for mix-in */
	class base_allocator
	{
	protected:
		~base_allocator() = default;
	public:
		void* allocate(size_t n, int = 0) { return ::malloc(n); }
		void* allocate(size_t n, size_t alignment, size_t offset, int = 0)
		{
			return offset == 0
				? ::aligned_alloc(alignment, n)
				: nullptr;
				;
		}
		void  deallocate(void* p, size_t) { ::free(p); };
	};

	/* A tracking string allocator, combining the non-tracking allocator and the string_tracker */
	class string_allocator
		: public base_allocator
		, public string_tracker
	{
	public:
		using tracker_type = string_tracker;
		/* EASTL passes a "default name" to the string allocator */
		string_allocator(const char *) {}
	};

	bool operator==(const string_allocator &, const string_allocator &) { return true; }

	/* A tracking map allocator, combining the non-tracking allocator and the map_tracker */
	class map_allocator
		: public base_allocator
		, public map_tracker
	{
	public:
		using tracker_type = map_tracker;

		map_allocator(
			map_tag * id_
		)
			: base_allocator()
			, map_tracker( id_ )
		{}
	};

	void annotate(const char *str)
	{
		std::cout << str << "\n";
	}
}

int main(int, char *[])
{
	/* The map_tracker has a sample state - a name tag. Here is that tag. */
	map_tag m_tag("White");

	/* We could make a map of any two types, but eastl::basic_string has
	 * the advantage that it supports tracking.
	 * A user-define type would have to provide its own tracking (typically
	 * by including a tracker, and calling that tracker track_pre and track_post
	 * member functions when the type was modified).
	 */
	using string = eastl::basic_string<char, string_allocator>;

	using map = eastl::map<string, string, std::less<string>, map_allocator>;

	annotate("Enter block (and construct map)");
	{
		map m{map_allocator(&m_tag)};

		annotate("Construct key1");
		string key1("Charlotte");

		annotate("Insert key1 and a string into map");
		m[key1] = string("an ordinary gray spider");

		annotate("Construct key2");
		string key2("Wilbur");

		annotate("Construct value2");
		string value2("Some Pig");

		annotate("Assign key2 to map[key2]");
		m[key2] = value2;

		annotate("Modify map[key2]");
		m[key2].append(", he is Terrific");

		annotate("Delete map[key1]");
		m.erase(key1);

		annotate("Exit block (and destruct map)");
	}
	annotate("Block exited");

}
