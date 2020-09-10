/* INFO: https://github.com/lorenzo-stoakes/linux-vm-notes/blob/master/sections/page-tables.md */
#include <linux/version.h>
#include <linux/debugfs.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/page-flags.h>
#include <linux/bitmap.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/hugetlb.h>
#include <asm/pgtable.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
//#include <asm/pgtable_type.h>

#ifndef CONFIG_X86_64
#error This code is written for X86-64 only.
#endif

#define DEBUG

/*
 * The dumper groups pagetable entries of the same type into one, and for
 * that it needs to keep some state when walking, and flush this state
 * when a "break" in the continuity is found.
 */
struct pg_state {
	int level;
	pgprot_t current_prot;
	unsigned long start_phy_address;
	unsigned long current_phy_address;
	unsigned long start_address;
	unsigned long current_address;
	const struct addr_marker *marker;
};

struct addr_marker {
	unsigned long start_address;
	const char *name;
};

/* indices for address_markers; keep sync'd w/ address_markers below */
enum address_markers_idx {
	USER_SPACE_NR = 0,
#ifdef CONFIG_X86_64
	KERNEL_SPACE_NR,
	LOW_KERNEL_NR,
	VMALLOC_START_NR,
	VMEMMAP_START_NR,
	HIGH_KERNEL_NR,
	MODULES_VADDR_NR,
	MODULES_END_NR,
#else
	KERNEL_SPACE_NR,
	VMALLOC_START_NR,
	VMALLOC_END_NR,
# ifdef CONFIG_HIGHMEM
	PKMAP_BASE_NR,
# endif
	FIXADDR_START_NR,
#endif
};



/* Multipliers for offsets within the PTEs */
#define PTE_LEVEL_MULT (PAGE_SIZE)
#define PMD_LEVEL_MULT (PTRS_PER_PTE * PTE_LEVEL_MULT)
#define PUD_LEVEL_MULT (PTRS_PER_PMD * PMD_LEVEL_MULT)
#define PGD_LEVEL_MULT (PTRS_PER_PUD * PUD_LEVEL_MULT)


extern pgd_t init_level4_pgt[];

/* references

   mm_struct -->  http://lxr.free-electrons.com/source/include/linux/mm_types.h#L396
*/
int walk_page_table(u64 vaddr, u64* paddr)
{
    pgd_t *pgd;
    pte_t *ptep;
    pud_t *pud;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
    p4d_t *p4d;
#endif
    
    pmd_t *pmd;
    struct mm_struct *mm = current->mm;

#ifdef DEBUG
    pr_info("mcas: walk page table %llx", vaddr);
#endif
    
    pgd = pgd_offset(mm, vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) {
      pr_err("mcas: pdg not found\n");
      return -1;
    }    

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)    
    p4d = p4d_offset(pgd, vaddr);    
    if (p4d_none(*p4d) || p4d_bad(*p4d)) {
      pr_err("mcas: p4d not found\n");
      return -1;
    }
    
    pud = pud_offset(p4d, vaddr);
#else
    pud = pud_offset(pgd, vaddr);
#endif
    if (pud_none(*pud) || pud_bad(*pud)) {
      pr_err("mcas: pud not found (0x%.16llx)\n", vaddr);
      return -1;
    }
    
    /* When huge pages are enabled on x86-64 (providing for 2MiB
       pages), this is achieved by setting the _PAGE_PSE flag on PMD
       entries. The PMD entries then no longer refer to a PTE page,
       but instead the page table structure is now terminated at the
       PMD and its physical address and flags refer to the physical
       page, leaving the remaining 21 bits (i.e. 2MiB) as an offset
       into the physical page. */
    
    pmd = pmd_offset(pud, vaddr);

    if (pmd_none(*pmd)) {
      pr_err("mcas: pmd not found\n");
      return -1;
    }

    if (pmd_large(*pmd)) {
#ifdef DEBUG
      pr_info("mcas: page table walker, page is huge.");
#endif
      *paddr = pmd_pfn(*pmd) << PAGE_SHIFT;
      return 0;
    }

    if(*((u64*)pmd) & (1<<7)) {
      pr_err("mcas: walk_page_table - bad PMD; not huge, yet flagged huge\n");
      return -1;
    }

    ptep = pte_offset_map(pmd, vaddr);
    if (!ptep) {
      pr_err("mcas: ptep not found\n");
      return -1;
    }

    *paddr = pte_pfn(*ptep) << PAGE_SHIFT;

    return 0;
}

