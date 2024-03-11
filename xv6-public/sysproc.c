#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "wmap.h"

#define PAGE_SIZE 4096

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

//Custom Syscalls
int
sys_getpgdirinfo(void){
  struct pgdirinfo *info;
  struct pgdirinfo localinfo;
  int i,j;

  if (argptr(0, (void *)&info, sizeof(struct pgdirinfo*))<0)
    return -1;
  
memset(&localinfo, 0, sizeof(localinfo));

pde_t *pgdir = myproc()->pgdir;
uint count = 0;

// Loop through page directory
for (i = 0; i < NPDENTRIES && count < MAX_UPAGE_INFO; i++)
{
  pde_t pde = pgdir[i];
  // Check for entry in page table
  if (pde & PTE_P)
  {
    pte_t *pgtab = (pte_t *)P2V(PTE_ADDR(pde));
    for (j = 0; j < NPTENTRIES && count < MAX_UPAGE_INFO; j++)
    {
      pte_t pte = pgtab[j];
      // Ensure page table is present and available to the user
      if ((pte & PTE_P) && (pte & PTE_U)) 
      {
        uint va = (i << PDXSHIFT) | (pte & PTE_U);
        localinfo.va[count] = va;
        localinfo.pa[count] = PTE_ADDR(pte) | (va & 0xFFF);
        count++;
      }
    }
  }
}
localinfo.n_upages = count;

if (copyout(myproc()-> pgdir, (uint)info, (char *)&localinfo, sizeof(localinfo)) < 0)
  return -1;

return 0;
}

int 
sys_getwmapinfo(void) {

  // Arg
  struct wmapinfo *wminfo;
  if (argptr(1, (void*) &wminfo, sizeof(struct wmapinfo*)) > 0) {
    return FAILED;
  }

  // Get PCB
  struct proc *myProc = myproc();

  // Hand off wmap pointer
  wminfo = myProc->wmap;

  return SUCCESS;
  
}

int
sys_wmap(void) {

  // Args
  uint addr;
  if (argptr(1, (void*) &addr, sizeof(uint)) > 0) {
    return FAILED;
  }
  int length; 
  if (argptr(2, (void*) &length, sizeof(int)) > 0) {
    return FAILED;
  }
  int flags;
  if (argptr(3, (void*) &flags, sizeof(int)) > 0) {
    return FAILED;
  }
  int fd;
  if (argptr(4, (void*) &fd, sizeof(int)) > 0) {
    return FAILED;
  }

	// Vaildate length
	if (length <= 0) {
		return FAILED;
	}
	int pages = (length / PAGE_SIZE) + ((length % PAGE_SIZE) != 0);

	// Parse flags
	if ((flags >= 16) | (flags < 0)) {
		return FAILED;
	}
	int mapFixed = 0;
	if (flags >= 8) {
		mapFixed = 1;
		flags -= 8;
	}
	int mapAnonymous = 0;
	if (flags >= 4) {
		mapAnonymous = 1;
		flags -= 4;
	}
	int mapShared = 0;
	if (flags >= 2) {
		mapShared = 1;
		flags -= 2;
	}
	int mapPrivate = 0;
	if (flags >= 1) {
		if (mapShared) {
			return FAILED;
		}
		mapPrivate = 1;
	}

	// Get own process pointer
	struct proc* myProc = myproc();
	
	// Allocate a page
	char *mem = kalloc();
	// For each page, place an entry in the page table
	mappages(myProc->pgdir, 0x60000000, 4096, V2P(mem), PTE_W | PTE_U);
	
	for (int i = 0; i < (pages - 1); i++) {
		mem = kalloc();
		mappages(myProc->pgdir, 0x60000000 + (PAGE_SIZE * (i + 1)), 4096, V2P(mem), PTE_W | PTE_U);
	}
};
