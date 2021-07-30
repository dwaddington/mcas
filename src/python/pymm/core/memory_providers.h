#ifndef __MEMORY_PROVIDERS__
#define __MEMORY_PROVIDERS__

class Mmap_memory_provider;

/** 
 * Interface class for memory providers
 * 
 */
class Transient_memory_provider
{
public:
  virtual void * malloc(size_t n) = 0;
  virtual void * calloc(size_t nelem, size_t elsize) = 0;
  virtual void * realloc(void * p, size_t n) = 0;
  virtual void   free(void * p) = 0;
};

inline bool ends_with(std::string const & value, std::string const & ending)
{
  if (ending.size() > value.size()) return false;
  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

/** 
 * Provider for tiering between persistent memory and mmap
 * 
 */
class Tiered_memory_provider : public Transient_memory_provider
{
public:
  void * malloc(size_t n) {
    return nullptr;
  }
  
  void * calloc(size_t nelem, size_t elsize) {
    return nullptr;
  }
  
  void * realloc(void * p, size_t n) {
    return nullptr;
  }
  
  void free(void * p) {
  }
};

/** 
 * Provider for persistent memory
 * 
 */
class Pmem_memory_provider : public Transient_memory_provider
{
public:
  void * malloc(size_t n) {
    return nullptr;
  }
  
  void * calloc(size_t nelem, size_t elsize) {
    return nullptr;
  }
  
  void * realloc(void * p, size_t n) {
    return nullptr;
  }
  
  void free(void * p) {
  }
};

/** 
 * Use mmap'ed file for each allocation
 * 
 * @param file_directory Directory for temporary files
 * 
 */
class Mmap_memory_provider : public Transient_memory_provider
{
private:
  static constexpr size_t BASE_ADDR = 0x0FAB00000000ULL;

  const std::string _dir;
  const uint64_t _base = BASE_ADDR;
  uint64_t       _addr = BASE_ADDR;
  
  std::map<void *, std::string> _filemap;
public:
  Mmap_memory_provider(const std::string& file_directory = "/tmp") : _dir(file_directory) {

    PLOG("mmap memory provider (%s)", file_directory.c_str());
    struct stat st;
    if((stat(file_directory.c_str(),&st) != 0) ||
       (st.st_mode & (S_IFDIR == 0))) {
      throw API_exception("invalid directory (%s)", file_directory.c_str());
    }

    { /* clean up prior .mem files */
      DIR * folder = opendir(file_directory.c_str());
      struct dirent *next_file;
      char filepath[256];

      while((next_file = readdir(folder)) != NULL )  {
        // build the path for each file in the folder
        snprintf(filepath, 255, "%s/%s", file_directory.c_str(), next_file->d_name);
        if(ends_with(std::string(filepath),".mem")) {
          remove(filepath);
        }
      }
      closedir(folder);
    }
  }

  bool belongs(void * addr)  {
    return (reinterpret_cast<uint64_t>(addr) >= _base) &&
      (reinterpret_cast<uint64_t>(addr) < _addr);
  }

  void * malloc(size_t n) {

    const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    size_t rounded_n = round_up_page(n);
    std::stringstream ss;
    void * p;
    
    ss << _dir << "/mmap_transient_memory_" << _addr << ".mem";
    std::string filename = ss.str();
    int fdout;
    if((fdout = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, mode)) >= 0) {
      /* create space in file */
      if(ftruncate(fdout, rounded_n) == 0) {
        p = mmap(reinterpret_cast<void*>(_addr), /* help debugging */
                 rounded_n,
                 PROT_READ | PROT_WRITE,
                 MAP_SHARED,
                 fdout, /* file */
                 0 /* offset */);
        _addr += rounded_n;

        close(fdout);
        _filemap[p] = filename;
        PLOG("[PyMM]: allocated %p", p);
        return p;
      }
    }
    throw General_exception("transient_memory: malloc failed");
    return nullptr;
  }

  void * calloc(size_t nelem, size_t elsize) {
    size_t size = nelem * elsize;
    void * p = this->malloc(size);
    PLOG("[PyMM]: callocated %p", p);
    memset(p, 0, size);
    return p;
  }
  
  void * realloc(void * p, size_t n) {

    if(belongs(p)) {
      PLOG("[PyMM]: reallocate attempt (%p,%lu)", p, n);
      return nullptr;
    }

    return ::realloc(p,n);
  }
  
  void free(void * p) {

    if(p==nullptr || belongs(p)==false) {
      ::free(p);
      return;
    }

    if(_filemap.find(p) == _filemap.end())
      throw General_exception("trying to free bad address");

    const char * fname = _filemap[p].c_str();
    struct stat stat_buf;
    stat(fname, &stat_buf);
    assert(rc == 0);
    ::munmap(p, stat_buf.st_size);
    PLOG("[PyMM]: freeing (%p,%lu)", p, stat_buf.st_size);
    remove(fname);
    _filemap.erase(p);
  }
};


#endif // __MEMORY_PROVIDERS__

