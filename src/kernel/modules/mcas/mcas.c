/**
 * @file    mcas.c
 * @author  Daniel Waddington (daniel.waddington@acm.org)
 * @date    24 June 2020
 * @version 0.2
 * @brief   MCAS support module.
 */

#include <linux/version.h>
#include <linux/init.h>             // Macros used to mark up functions e.g., __init __exit
#include <linux/module.h>           // Core header for loading LKMs into the kernel
#include <linux/kernel.h>           // Contains types, macros, functions for the kernel
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/io.h>

#define PAGE_ROUND_UP(x) ( roundup(x, PAGE_SIZE) )
#define PAGE_ROUND_DOWN(x) ( rounddown(x, PAGE_SIZE) )

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,13,0)
#include <asm/cacheflush.h>
#else
#include <asm/set_memory.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0)
#define check_access(X,Y) access_ok(VERIFY_READ, X, Y)
#else
#define check_access(X,Y) access_ok(X, Y)
#endif


#include <asm/pgtable.h>
#include <asm/uaccess.h>

//#define DEBUG


#include "mcas.h"

struct Exposure {
  struct list_head list;
  u64 token;
  u64 paddr;  
  size_t size;
  void * kaddr;
  pid_t owner;
};

LIST_HEAD(g_exposure_list);
DEFINE_MUTEX(g_exposure_list_lock);


extern int walk_page_table(u64 vaddr, u64* paddr);

static int mcas_mmap(struct file *file, struct vm_area_struct *vma);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,18,0)
static int mcas_fault_mmap(struct vm_fault * fault);
#else
static vm_fault_t mcas_fault_mmap(struct vm_fault * fault);
#endif

inline static bool check_aligned(void* p, unsigned alignment)
{
  return (!(((unsigned long)p) & (alignment - 1UL)));
}

inline int current_has_parent(pid_t parent)
{
  struct task_struct * task = get_current();
  return task->real_parent->pid == parent;
}

static int add_exposure(pid_t pid, u64 token, u64 paddr, size_t size, void * kaddr)
{
  struct Exposure *exp, *ptr;
  
  mutex_lock(&g_exposure_list_lock);

  /* check for existing entry */
  list_for_each_entry(ptr, &g_exposure_list, list) {
    if (ptr->token == token) {
      pr_err("mcas: token(%llx) already exists\n", token);
      mutex_unlock(&g_exposure_list_lock);      
      return -1;
    }
  }

  exp = kmalloc(sizeof(struct Exposure), GFP_KERNEL);

  if(exp) {
    exp->token = token;
    exp->paddr = paddr;
    exp->size = size;
    exp->owner = pid;
    exp->kaddr = kaddr;
    list_add(&exp->list, &g_exposure_list);
  }
  else {
    pr_err("mcas: add_exposure failed\n");
  }

#ifdef DEBUG
  pr_info("mcas: exposure added (pid=%x, token=%llx, paddr=%llx, size=%lu, kaddr=%p)\n",
          pid, exp->token, exp->paddr, exp->size, kaddr);
#endif
  
  mutex_unlock(&g_exposure_list_lock);
  return 0;
}

static int remove_exposure(pid_t pid, u64 token)
{
  struct Exposure * ptr = NULL;

#ifdef DEBUG
  pr_info("mcas: remove_exposure (token=%llx)\n", token);
#endif
  mutex_lock(&g_exposure_list_lock);
  
  list_for_each_entry(ptr, &g_exposure_list, list) {
    if (ptr->token == token) {
      list_del(&ptr->list);
      pr_info("mcas: removed exposure %llx\n", token);
      mutex_unlock(&g_exposure_list_lock);      
      return 0;
    }
  }

  pr_err("mcas: token (%llx) not found for remove_exposure\n", token);
  mutex_unlock(&g_exposure_list_lock);
  return -1;
}

static int fetch_exposure(u64 token,
                          u64 * out_paddr,
                          size_t * out_size,
			  void ** out_kaddr)
{
  struct Exposure * ptr = NULL;
  if(!out_paddr || !out_size) {
    pr_err("fetch_exposure passed bad params\n");
    return -1;
  }
  mutex_lock(&g_exposure_list_lock);
  list_for_each_entry(ptr, &g_exposure_list, list) {
    if (ptr->token == token) {
      *out_paddr = ptr->paddr;
      *out_size = ptr->size;
      *out_kaddr = ptr->kaddr;

      mutex_unlock(&g_exposure_list_lock);
#ifdef DEBUG      
      pr_info("mcas: got exposure (phys:0x%llx %lu)", *out_paddr, *out_size);
#endif
      return 0;
    }
  }
  mutex_unlock(&g_exposure_list_lock);
  return -1;
}

static long mcas_dev_ioctl(struct file *filp,
                          unsigned int ioctl,
                          unsigned long arg)
{
  long r = -EINVAL;
  unsigned long size;
  int rc;
#ifdef DEBUG
  pr_notice("mcas: ioctl (%d) (arg=%lx)\n", ioctl, arg);
#endif

  switch(ioctl) {
  case IOCTL_CMD_EXPOSE:
    {
      IOCTL_EXPOSE_msg msg;

      if(!check_access((void*) arg, sizeof(IOCTL_EXPOSE_msg))) { 
        pr_err("mcas: dev_ioctl passed invalid in_data param\n"); 
        return r; 
      } 

      rc = copy_from_user(&msg,
                          ((IOCTL_EXPOSE_msg *) arg),
                          sizeof(IOCTL_EXPOSE_msg));

      if(rc > 0) { 
        pr_err("mcas: copy_from_user failed\n"); 
        return r; 
      }

      size = PAGE_ROUND_UP(msg.vaddr_size);
      pr_info("mcas: token=%llu vaddr=0x%llx vaddr_size=%lu\n",
              msg.token, (unsigned long long) msg.vaddr, size);

      {
        u64 paddr = 0;
	void * kaddr = 0;
	unsigned long pfn;
	struct page * page;
	
        walk_page_table((u64) msg.vaddr, &paddr);

        if(paddr == 0) {
          pr_err("mcas: page table walk failed\n");
          return -EINVAL;
        }

	pfn = paddr >> PAGE_SHIFT;

	if (!pfn_valid(pfn)) {
	  pr_err("mcas: in expose, PFN not valid, no struct page!");
	  return -EINVAL;
	}  

	kaddr = memremap(paddr, size, MEMREMAP_WB);	

#ifdef DEBUG
	pr_info("mcas: memremap %llx (phys) -> %p (virt) [0x%x,0x%x,..]",
		paddr, kaddr, ((char*)kaddr)[0],((char*)kaddr)[0]);
#endif

	/* need to add for each page? */
	page = pfn_to_page(pfn);
	get_page(page);

        if(add_exposure(task_pid_nr(current),
                        msg.token,
                        paddr,
                        size,
			kaddr) != 0) {
          return -EINVAL;
        }
      }
      break;
    }
  case IOCTL_CMD_REMOVE:
    {
      IOCTL_REMOVE_msg msg;

      if(!check_access((void*) arg, sizeof(IOCTL_REMOVE_msg))) { 
        pr_err("mcas: dev_ioctl passed invalid in_data param\n"); 
        return r; 
      } 

      rc = copy_from_user(&msg,
                          ((IOCTL_REMOVE_msg *) arg),
                          sizeof(IOCTL_REMOVE_msg));

      if(rc > 0) { 
        pr_err("mcas: copy_from_user failed\n"); 
        return r; 
      }
      pr_info("mcas: calling remove_exposure\n");
      return remove_exposure(task_pid_nr(current), msg.token);
    }
  case IOCTL_CMD_QUERY:
    {
      IOCTL_QUERY_msg msg;
      u64 paddr = 0;
      size_t size = 0;
      void * kaddr;
      
      if(!check_access((void*) arg, sizeof(IOCTL_QUERY_msg))) { 
        pr_err("mcas: dev_ioctl passed invalid in_data param\n"); 
        return r; 
      } 

      rc = copy_from_user(&msg,
                          ((IOCTL_QUERY_msg *) arg),
                          sizeof(IOCTL_QUERY_msg));

      if(rc > 0) { 
        pr_err("mcas: copy_from_user failed\n"); 
        return r; 
      }
#ifdef DEBUG
      pr_info("mcas: calling query_exposure (token=%llx)\n", msg.token);
#endif
      
      fetch_exposure(msg.token, &paddr, &size, &kaddr);
      
      msg.size = size;
      rc = copy_to_user(((IOCTL_QUERY_msg *) arg),
                        &msg,
                        sizeof(IOCTL_QUERY_msg));
      if(rc > 0) {
        pr_err("mcas: copy_to_user failed\n"); 
        return r; 
      }

      return 0;
    }
  default:
    pr_info("mcas: unknown ioctl (%u)\n", ioctl);
    return -EINVAL;
  }
  
  return 0;
}

static int mcas_dev_release(struct inode *inode, struct file *file)
{
  return 0;
}



static const struct file_operations mcas_chardev_ops = {
  .owner          = THIS_MODULE,
  .unlocked_ioctl = mcas_dev_ioctl,
#ifdef CONFIG_COMPAT
  .compat_ioctl   = mcas_dev_ioctl,
#endif
  .llseek         = noop_llseek,
  .release        = mcas_dev_release,
  .mmap           = mcas_mmap,
};

static struct miscdevice mcas_dev = {
  MISC_DYNAMIC_MINOR,
  "mcas",
  &mcas_chardev_ops,
};

static void vm_open(struct vm_area_struct *vma)
{
#ifdef DEBUG    
  pr_info("mcas: vm_open\n");
#endif
}

static void vm_close(struct vm_area_struct *vma)
{
#ifdef DEBUG  
  pr_info("mcas: vm_close\n");
#endif
}

/* #if LINUX_VERSION_CODE < KERNEL_VERSION(4,13,0) */
/* static int vm_fault(struct vm_area_struct *area,  */
/*                     struct vm_fault *fdata) */
/* #else */
/* static int vm_fault(struct vm_fault *vmf) */
/* #endif */
/* { */
/*   return VM_FAULT_SIGBUS; */
/* } */

static struct vm_operations_struct mmap_fops = {
  .open  = vm_open,
  .close = vm_close,
  .access = generic_access_phys, /* allows GDB access */
  .fault = mcas_fault_mmap,
};

/** 
 * Allows mmap calls to map a virtual region to a specific
 * physical address
 * 
 */
static int mcas_mmap(struct file *file, struct vm_area_struct *vma)
{
  u64 token;
  int rc;
  u64 out_paddr;
  size_t out_size;
  void * kaddr;

#ifdef DEBUG  
  pr_info("mcas: mmap called --------------------\n");
#endif
  
  if(vma->vm_end < vma->vm_start) {
    pr_err("mcas: bad VMA");
    return -EINVAL;
  }

  token = vma->vm_pgoff;
  
#ifdef DEBUG
  pr_info("mcas: mmap flags=%lx token=%llx\n", vma->vm_flags, token);
#endif

  rc = fetch_exposure(token, &out_paddr, &out_size, &kaddr);
  if(rc) {
    pr_err("mcas: bad exposure token (%llu)", token);
    return -EFAULT;
  }

#ifdef DEBUG
  pr_info("mcas: mmap remapping (0x%llx,%lu)\n", out_paddr, out_size);
#endif

  vma->vm_ops = &mmap_fops;
  vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
  vma->vm_private_data = (void*) token;
  vm_open(vma);

  return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,18,0)
static int mcas_fault_mmap(struct vm_fault * vmf)
#else
static vm_fault_t mcas_fault_mmap(struct vm_fault * vmf)
#endif
{
  u64 token = (u64) vmf->vma->vm_private_data; //vmf->pgoff; //>> 12;
  int rc;
  u64 out_paddr;
  size_t out_size;
  struct page * page;
  void * kaddr;
  unsigned long pfn;
  unsigned long page_offset;

#ifdef DEBUG  
  pr_info("mcas: mcas_fault_map (address=%lx, start=%lx, token=%llx)\n",
	  vmf->address, vmf->vma->vm_start, token);
#endif
    
  rc = fetch_exposure(token, &out_paddr, &out_size, &kaddr);

#ifdef DEBUG  
  pr_info("mcas: mcas_fault_map got exposure phys=%llx kaddr=%p\n", out_paddr, kaddr);
  pr_info("mcas: mcas_fault_map kaddr=[0x%x,0x%x,..]", ((char*)kaddr)[0],((char*)kaddr)[0]);
#endif
  
  pfn = out_paddr >> PAGE_SHIFT;
  if (!pfn_valid(pfn)) {
    pr_err("mcas: PFN not valid, no struct page!");
    return VM_FAULT_SIGBUS;
  }

  page_offset = (vmf->address >> PAGE_SHIFT) - (vmf->vma->vm_start >> PAGE_SHIFT);

  if (!pfn_valid(pfn)) { 
    pr_err("mcas: PFN + offset not valid, no struct page!");
    return VM_FAULT_SIGBUS;
  }

#ifdef DEBUG  
  pr_info("mcas: fault page offset = %lu", page_offset);
#endif
  
  page = pfn_to_page(pfn + page_offset);
  get_page(page);
  vmf->page = page;
  
  return 0;
}


static int __init mcas_init(void) {
  int r;

  mcas_dev.mode = S_IRUGO | S_IWUGO; // set permission for /dev/mcas
  
  r = misc_register(&mcas_dev);
  if (r) {
    pr_err("mcas: misc device register failed, error code %d\n", r);
    return r >0 ? -r:r;
  }
  pr_info("mcas: loaded\n");
  
  return 0;
}

static void __exit mcas_exit(void) {
  pr_info("mcas: unloaded\n");
  misc_deregister(&mcas_dev);
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(mcas_init);
module_exit(mcas_exit);

MODULE_LICENSE("GPL");              ///< The license type -- this affects runtime behavior
MODULE_AUTHOR("Daniel Waddington"); ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("MCAS support module.");  ///< The description -- see modinfo
MODULE_VERSION("0.2");              ///< The version of the module
