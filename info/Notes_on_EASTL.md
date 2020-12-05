## Crash consistency notes for ADO

ADO data must generally be crash-consistent.
The data will persist across a crash, but will not be useful if the crash leaves the data in an inconsistent state.

Modfications to EASTL container classes use one method of crach consistency: use of a "Tracker" object which provides functions to record persistent value initializations and modifications.

### EASTL Container modifications

In order to support crash consistency in ADOs, modified versions of some EASTL container types are provied.
The types support tracking of initial writes and modifications, which will allow a log mechanism to roll back
partial operations to a previous state.
The tracking generally applies only to the container control elemments, and not to the data objects in the containers.

#### Containers modified to support tracking

- deque
- hash_map, hash_set
- heap
- list
- map
- optional
- set
- slist
- unordered_map, unordered_set
- vector

#### Containers modified to support tracking, including contained elements

- bitset
- string

A "tracker" is derived from an allocator when used with EASTL containers that take an allocator.
This reduces the number of changes needed to embed a tracker in a container.
Note that tracker data is usually common to all collections in an ADO, so the data in an embedded "tracker" is usually at most a pointer to the actual tracking state.
The class log in src/lib/libccpm/include/ccpm/log.h is an example of an actual logging state.
It is used by the unit test at src/lib/libccpm/unit_test/test5.cpp.

### Crash consistency of other data

The consistency method assumed by the EASTL modifications is a log, with a "tracker" type providing the interface between the data structure and the log.

A tracker provides two functions, one called before a modification to record old data values, and the other called after an iniitalization or modification to record a new value.

```
void track_pre(const void \*, std::size_t);
void track_post(const void \*, std::size_t);
```

There is no support for tracking objects other than EASTL containers, but there are incomplete examples at

Tracking a class for which all member initialization and modification is controlled, can be done by mofifying the class member functions to include track_pre and track_post.
Tracking non-class types can sometimes be simplified by "boxing" the types into classes, as Java does. There are incomplete examples at

 - src/lib/EASTL/include/EASTL/internal/tracked.h
 - src/lib/libccpm/include/ccpm/value_tracked.h

