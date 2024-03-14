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
    return FAILED;
  
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
  return FAILED;

return 0;
}

int 
sys_getwmapinfo(void) {

 struct wmapinfo *wminfo;

 //Check if argptr gets the pointer successfully
  if (argptr(0, (void*) & wminfo, sizeof(struct wmapinfo*)) < 0)
  {
    return FAILED;
  }

  struct proc *myProc = myproc();

  // Copy from kernel to user space
  if (copyout(myProc->pgdir, (uint)wminfo, (char *) & (myProc->wmap), sizeof(struct wmapinfo)) < 0)
  {
    return FAILED;
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

  cprintf("wmap debug: addr=%x, len=%d, flags=%d, fd=%d\n", 
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
  // Private is true if shared isn't
	if (flags >= 1) {
		if (mapShared) {
			return FAILED;
		}
	}

	// Get own process pointer
	struct proc* myProc = myproc();

  int thisMap = myProc->wmap.total_mmaps;
  if (thisMap >= (MAX_WMMAP_INFO - 1)) {
    // Too many maps
    return FAILED;
  }

  if (mapFixed) {

    // Check addr within range
    if ((addr < 0x60000000) || (addr > 0x80000000)) {
      return FAILED;
    }

    // Check for collisions - can't move
    for (int i = 0; i < myproc()->wmap.total_mmaps; i++) {
      int otherStart = myproc()->wmap.addr[i];
      int otherEnd = otherStart + myproc()->wmap.length[i];
      // Check if any other mapping starts or ends within the span of this mapping
      if (((otherStart <= (addr + length - 1)) && (otherStart >= addr)) ||
          ((otherEnd <= (addr + length - 1)) && (otherEnd >= addr))) {
        cprintf("wmap debug: Failed due to insufficient space for fixed addr\n");
        return FAILED;
      }
    }

  } else {

    // Completely ignore address suggestion
    // Nothing's sorted, so we'll just decide space 
    // is full when we hit the end while incrementing and
    // failing to find a space with no collisions
    addr = 0x60000000;

    // Check for collisions
    if (myProc->wmap.total_mmaps > 0) {
      int recheck;
      do {
        recheck = 0;
        for (int i = 0; i < myproc()->wmap.total_mmaps; i++) {

          int otherStart = myproc()->wmap.addr[i];
          int otherEnd = otherStart + myproc()->wmap.length[i];

          // Check if any other mapping starts or ends within the span of this mapping
          if (((otherStart <= (addr + length - 1)) && (otherStart >= addr - 1)) ||
              ((otherEnd <= (addr + length - 1)) && (otherEnd >= addr - 1))) {

            addr += PAGE_SIZE;
            if ((addr + length) >= 0x80000000) {
              cprintf("wmap debug: Failed due to insufficient space for non-fixed addr\n");
              return FAILED;
            } else {
              recheck = 1;
              break;
            }

          }
        }
      } while (recheck);

    }
  }

  // Set this map info, but don't alloc yet (lazy)
  myProc->wmap.addr[thisMap] = addr;
  myProc->wmap.length[thisMap] = length;
  myProc->wmap.n_loaded_pages[thisMap] = 0;
  myProc->wmap.total_mmaps++;
  myProc->wmap.anon[thisMap] = mapAnonymous;

  // Implementing File-Backed Mapping- BW
  if (!mapAnonymous) {
    filedup(myProc->ofile[fd]); // Keep fd alive
  }
  myProc->wmap.fptr[thisMap] = mapAnonymous ? 0 : myProc->ofile[fd];
  myProc->wmap.shared[thisMap] = mapShared;

  return addr;
};

int
sys_wunmap(void) {
  int addr;

  if (argint(0, &addr) < 0)
  {
    return FAILED;
  }

  struct proc *currproc = myproc();
  int finder = -1;

  //Try to find the mapping by the address
  for (int i = 0; i < currproc->wmap.total_mmaps; i++)
  {
    if (currproc->wmap.addr[i] == addr)
    {
      finder = i;
      break;
    }
  }

  if (finder == -1)
  {
    return FAILED;
  }

  // START File backed mapping section
  if (!currproc->wmap.anon[finder] && currproc->wmap.shared[finder])
  {
    struct file *f = currproc->wmap.fptr[finder];

    if (f)
    {
      filewrite(f, (char *)addr,currproc->wmap.length[finder]);
    }
  }
  // END File backed mapping section

  // Deallocate memory region
  uint oldsz = currproc->wmap.addr[finder] + currproc->wmap.length[finder];
  uint newsz = currproc->wmap.addr[finder];
  if ((deallocuvm(currproc->pgdir, oldsz, newsz) == 0))
  {
    return FAILED;
  }

  //Remove mapping
  for (int i = finder; i < currproc->wmap.total_mmaps -1; i++)
  {
    currproc->wmap.addr[i] = currproc->wmap.addr[i + 1];
    currproc->wmap.length[i] = currproc->wmap.length[i + 1];
    currproc->wmap.n_loaded_pages[i] = currproc->wmap.n_loaded_pages[i + 1];
    currproc->wmap.anon[i] = currproc->wmap.anon[i + 1];
    // File backed mapping related elements -BW
    currproc->wmap.fptr[i] = currproc->wmap.fptr[i + 1];
    currproc->wmap.shared[i] = currproc->wmap.shared[i];
  }
  currproc->wmap.total_mmaps--;
  return SUCCESS;
}

int
sys_wremap(void) {

  // Args
  int oldaddr;
  if (argint(0, &oldaddr) != 0) {
    return FAILED;
  }
  int oldsize; 
  if (argint(1, &oldsize) != 0) {
    return FAILED;
  }
  int newsize;
  if (argint(2, &newsize) != 0) {
    return FAILED;
  }
  int flags;
  if (argint(3, &flags) != 0) {
    return FAILED;
  }

  cprintf("wremap debug: oldaddr=%x, oldsize=%d, newsize=%d, flags=%d\n",
    oldaddr, oldsize, newsize, flags);

  // Find our index
  int foundInd = -1;
  for (int i = 0; i < myproc()->wmap.total_mmaps; i++) {
    if (myproc()->wmap.addr[i] == oldaddr) {
      foundInd = i;
    }
  }
  if (foundInd == -1) {
    cprintf("wremap debug: Failed due to addr not found\n");
    return FAILED;
  }

  if (flags == 0) {
    // Check for collisions - can't move
    for (int i = 0; i < myproc()->wmap.total_mmaps; i++) {
      // Don't test collision with self - will always fail
      if (i != foundInd) {
        int otherStart = myproc()->wmap.addr[i];
        int otherEnd = otherStart + myproc()->wmap.length[i];
        // Check if any other mapping starts or ends within the span of this mapping
        if (((otherStart <= (oldaddr + newsize - 1)) && (otherStart >= oldaddr)) ||
            ((otherEnd <= (oldaddr + newsize - 1)) && (otherEnd >= oldaddr))) {
          cprintf("wremap debug: Failed due to no space\n");
          return FAILED;
        }
      }
    }

    // We're ok to expand
    myproc()->wmap.length[foundInd] = newsize;
    
  } else if (flags == MREMAP_MAYMOVE) {

    // Can we stay in place?
    int canStay = 1;
    for (int i = 0; i < myproc()->wmap.total_mmaps; i++) {
      // Don't test collision with self - will always fail
      if (i != foundInd) {
        int otherStart = myproc()->wmap.addr[i];
        int otherEnd = otherStart + myproc()->wmap.length[i];
        // Check if any other mapping starts or ends within the span of this mapping
        if (((otherStart <= (oldaddr + newsize - 1)) && (otherStart >= oldaddr)) ||
            ((otherEnd <= (oldaddr + newsize - 1)) && (otherEnd >= oldaddr))) {
          canStay = 0;
          break;
        }
      }
    }

    if (canStay) {

      // Resize in place
      myproc()->wmap.length[foundInd] = newsize;

    } else {

      // Start back at the top, so we know space is full 
      // if we search past the end
      oldaddr = 0x6000000;

      int recheck;
      do {
        recheck = 0;

        // Check for collisions, move if needed
        for (int i = 0; i < myproc()->wmap.total_mmaps; i++) {
          // Again, don't check for collision w/ self
          if (i != foundInd) {

            int otherStart = myproc()->wmap.addr[i];
            int otherEnd = otherStart + myproc()->wmap.length[i];

            // Check if any other mapping starts or ends within the span of this mapping
            if (((otherStart <= (oldaddr + newsize)) && (otherStart >= oldaddr)) ||
                ((otherEnd <= (oldaddr + newsize)) && (otherEnd >= oldaddr))) {

              oldaddr += PAGE_SIZE;
              if ((oldaddr + newsize) >= 0x80000000) {
                cprintf("wremap debug: Failed due to no space\n");
                return FAILED;
              } else {
                recheck = 1;
                break;
              } 
            }
          }
        }
      } while (recheck);

      // De-alloc any yielded space
      if (oldsize > newsize) {
        uint oldsz = oldaddr + oldsize;
        uint newsz = oldaddr + newsize;
        if ((deallocuvm(myproc()->pgdir, oldsz, newsz) == 0)) {
          return FAILED;
        }
      }
      
      myproc()->wmap.length[foundInd] = newsize;
      myproc()->wmap.addr[foundInd] = oldaddr;

    }
    
  } else {
    cprintf("wremap debug: Failed due to bad flags\n");
    return FAILED;
  }

  cprintf("wremap debug: Success, addr %x\n", oldaddr);
  return oldaddr;
}