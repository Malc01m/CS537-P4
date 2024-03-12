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
        uint va = (i << PDXSHIFT) | (j << PTXSHIFT);
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

 struct wmapinfo *wminfo;

 //Check if argptr gets the pointer successfully
  if (argptr(0, (void*) & wminfo, sizeof(struct wmapinfo*)) < 0)
  {
    return -1;
  }

  struct proc *myProc = myproc();

  // Copy from kernel to user space
  if (copyout(myProc->pgdir, (uint)wminfo, (char *) & (myProc->wmap), sizeof(struct wmapinfo)) < 0)
  {
    return -1;
  }

  return 0;
}

int
sys_wmap(void) {

  // Args
  int addr;
  if (argint(0, &addr) != 0) {
    return FAILED;
  }
  int length; 
  if (argint(1, &length) != 0) {
    return FAILED;
  }
  int flags;
  if (argint(2, &flags) != 0) {
    return FAILED;
  }
  int fd;
  if (argint(3, &fd) != 0) {
    return FAILED;
  }

  cprintf("wmap debug: addr=%d, len=%d, flags=%d, fd=%d\n", 
    addr, length, flags, fd);

	// Vaildate length
	if (length <= 0) {
		return FAILED;
	}

	// Parse flags
	if ((flags >= 16) | (flags < 0)) {
		return FAILED;
	}
  
	int mapFixed = 0;
	if (flags >= 8) {
		mapFixed = 1;
		flags -= 8;
	}
  /*
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
  */

	// Get own process pointer
	struct proc* myProc = myproc();

  int thisMap = myProc->wmap.total_mmaps;
  if (thisMap >= (MAX_WMMAP_INFO - 1)) {
    // Too many maps
    return FAILED;
  }

  int useAddr;
  if (mapFixed) {
    useAddr = addr;
    // Check for collisions
    if (myProc->wmap.total_mmaps > 0) {
      int lastLen = myProc->wmap.length[thisMap - 1];
      int lastStart = myProc->wmap.addr[thisMap - 1];
      if (lastStart + lastLen > useAddr) {
        return FAILED;
      }
    }
  } else {
    useAddr = 0x60000000;
    // Check for collisions
    if (myProc->wmap.total_mmaps > 0) {
      int recheck = 0;
      do {
        for (int i = 0; i < myProc->wmap.total_mmaps; i++) {
          int lastStart = myProc->wmap.addr[i];
          int lastEnd = lastStart + myProc->wmap.length[i];
          int thisStart = useAddr;
          int thisEnd = useAddr + length;
          while (((thisStart > lastStart) && (thisStart < lastEnd)) ||
              ((lastStart > thisStart) && (lastStart < thisEnd))) {
            useAddr += PAGE_SIZE;
            recheck = 1;
            // Couldn't find a place
            if ((useAddr + PAGE_SIZE) >= 0x80000000) {
              return FAILED;
            }
          }
        }
      } while (recheck);
    }
  }

  // Set this map info, but don't alloc yet (lazy)
  myProc->wmap.addr[thisMap] = useAddr;
  myProc->wmap.length[thisMap] = length;
  myProc->wmap.n_loaded_pages[thisMap] = 0;
  myProc->wmap.total_mmaps++;

  return useAddr;
};

int
sys_wunmap(void) {
  // TODO: Not implemented
  return FAILED;
}

int
sys_wremap(void) {
  // TODO: Not implemented
  return FAILED;
}