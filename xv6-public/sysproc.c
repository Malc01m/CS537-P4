#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "wmap.h"

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
  return FAILED;

return SUCCESS;
}