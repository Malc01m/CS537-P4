#include "wmap.h"
#include "types.h"
#include "user.h"
#include "defs.h"
#include "proc.h"
#include "memlayout.h"

#define PAGE_SIZE 4096

uint wmap(uint addr, int length, int flags, int fd) {

	// Vaildate length
	if (length <= 0) {
		printf(1, "wmap error: invalid length\n");
		return FAILED;
	}
	int pages = (length / PAGE_SIZE) + ((length % PAGE_SIZE) != 0);

	// Parse flags
	if ((flags >= 16) | (flags < 0)) {
		printf(1, "wmap error: invalid flag value\n");
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
			printf(1, "wmap error: flags cannot specify both shared and private\n");
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
