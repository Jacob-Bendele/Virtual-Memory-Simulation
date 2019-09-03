// Jacob Bendele

// This code simulates the concept of Virtual Memory,
// Components such as TLB, Page Table, and Frame Table (main memory) are simulated.

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#define VPAGES 1024
#define TLB_SIZE 8
#define FRAMES 256

// Global variables were used as a basic way to avoid hiding the intention of counting Virtual Memory actions.
int TLB_Hit = 0;
int TLB_Miss = 0;
int TLB_ShootDown = 0;
int TLB_Writes = 0;
int pageTableHit = 0;
int pageFault = 0;
int pageTableAcc = 0;
int pageTableWrites = 0;
int pageTableEvics = 0;
int HDDWrites = 0;
int HDDReads = 0;

// Returns the TLB entry that corresponds to a virtual page.
// Returns -1 if the page's translation is not in the TLB.
int TLB_lookup(unsigned int TLB[][5], int size, unsigned int vpn)
{
	int i;
	
	for (i = 0; i < size; i++)
	{
		if (TLB[i][3] == vpn && TLB[i][0] == 1)
			return i;
	}
	
	return -1;
}

// Returns the index of the first available TLB entry.
// Returns -1 if all TLB entries are used.
int get_available_TLB_entry(unsigned int TLB[][5], int size)
{
	int i;
	
	for (i = 0; i < size; i++)
	{
		if (TLB[i][0] == 0)
		{
			TLB[i][0] = 1;
			return i;
		}	
	}
	
	return -1;
}

// Selects the TLB entry that will be evicted and returns the entry's index.
// Pre-condition: All TLB entries are full
// Criteria: Select a random entry with ref.bit=0; if all ref.bit=1, select a random entry
int select_TLB_shootdown_candidate(unsigned int TLB[][5], int size)
{
	int i, index;
	bool ref0 = false; 
	
	for (i = 0; i < size; i++)
		if (TLB[i][2] == 0 && TLB[i][0] == 1)
			ref0 = true; 
	
	while (ref0)
	{
		index = rand() % size;
		
		if (TLB[index][2] == 0 && TLB[index][0] == 1)
			return index;
	}
	
	return index = rand() % size;
}

// Perform a TLB shootdown (set V=0, copy D,R bits to the page table).
// Pre-condition: shootdown entry is currently valid
// Parameter index is the TLB entry to shoot down.
void TLB_shootdown(unsigned int TLB[][5], int tlb_size, unsigned int PageTable[][4], int page_table_size, int index)
{
	unsigned int Tag = TLB[index][3];
	TLB_ShootDown++;
	
	TLB[index][0] = 0;

	PageTable[Tag][1] = TLB[index][1];
	PageTable[Tag][2] = TLB[index][2];
}

// Copies a translation from the Page Table to the TLB.
// The first available TLB entry is used; otherwise, a TLB shootdown is performed.
// It copies the D,R bits and the frame number from the page table.
// Parameter: virtual page number
// Returns: 0 if no TLB shootdown occurs 1 if TLB shootdown occurs
int cache_translation_in_TLB(unsigned int TLB[][5], int tlb_size, unsigned int PageTable[][4], int page_table_size, unsigned int vpn)
{
	int index;
	TLB_Writes++;
	
	index = get_available_TLB_entry(TLB, tlb_size);
	
	if (index != -1)
	{
		TLB[index][1] = PageTable[vpn][1];
		TLB[index][2] = PageTable[vpn][2];
		TLB[index][3] = vpn;
		TLB[index][4] = PageTable[vpn][3];

		return 0;
	}
	
	else if (index == -1)
	{
		index = select_TLB_shootdown_candidate(TLB, tlb_size);
		TLB_shootdown(TLB, tlb_size, PageTable, page_table_size, index);
		TLB[index][0] = 1;
		TLB[index][1] = PageTable[vpn][1];
		TLB[index][2] = PageTable[vpn][2];
		TLB[index][3] = vpn;
		TLB[index][4] = PageTable[vpn][3];
		
		return 1;
	}
}

// Returns the index of the first available frame.
// Returns -1 if all frames are allocated.
int get_available_frame(unsigned int FrameTable[], int size)
{
	int i;
	
	for (i = 0; i < size; i++)
	{
		if (FrameTable[i] == 0)
		{	
			FrameTable[i] = 1;
			return i;
		}
	}
	
	return -1;
}

// Search the PageTable for VDR values passed as parameters.
// Return -1 if not found; otherwise, return the index of one such
// randomized entry (using rand function).
// Pre-condition: VDR are 0 or 1
int search_PageTable_by_VDR(unsigned int PageTable[][4], int size, int V, int D, int R)
{		
	int i, j = 0, retval;
	int *candidates;
	candidates = malloc(sizeof(int) * size);

	for (i = 0; i < size; i++)
	{
		if (PageTable[i][0] == V && PageTable[i][1] == D && PageTable[i][2] == R)
		{
			candidates[j] = i;
			j++;
		}
	}
	
	if (j == 0)
	{
		return -1;
	}
	
	retval = candidates[rand() % j];
	free(candidates);
	return retval;
}

// Selects the virtual page that will be replaced.
// Pre-condition: All the frames are allocated
// Criteria: Valid must be 1; choose in order as below
//     VDR.bits: 100   110   101   111
// Between pages with the same category, randomize (using rand).
unsigned int select_page_eviction_candidate(unsigned int PageTable[][4], int size)
{
	unsigned int candidate;
	
	if ((candidate = search_PageTable_by_VDR(PageTable, size, 1, 0, 0)) != -1)
		return candidate;
	
	else if ((candidate = search_PageTable_by_VDR(PageTable, size, 1, 1, 0)) != -1)
		return candidate;
	
	else if ((candidate = search_PageTable_by_VDR(PageTable, size, 1, 0, 1)) != -1)
		return candidate;
	
	else if ((candidate = search_PageTable_by_VDR(PageTable, size, 1, 1, 1)) != -1)
		return candidate;	
}

// Evict a page from RAM to the hard disk.
// If its translation is in the TLB, perform a TLB shootdown.
// If the page is dirty, write it to hard disk.
// Update the Frame Table and the page table.
// Pre-condition: the page is currently allocated in the RAM
// Returns (+1: TLB shootdown performed) (+10: hard disk write performed) These values were not used
// but were asked for in descriptions.
int page_evict(unsigned int PageTable[][4], int page_table_size, unsigned int TLB[][5], 
	           int tlb_size, int FrameTable[], int frame_table_size, int vpn)
{
	int total = 0, index;
	pageTableEvics++;
	
	index = TLB_lookup(TLB, tlb_size, vpn);
	
	if (index != -1)
	{
		TLB_shootdown(TLB, tlb_size, PageTable, page_table_size, index);
		total += 1;
	}
	
	// Page in ram is dirty write to Hdd
	if (PageTable[vpn][1] == 1)
	{
		HDDWrites++;
		total += 10;
	}
	
	//Remove from RAM
	FrameTable[PageTable[vpn][3]] = 0;
	
	// Page is set to HDD
	PageTable[vpn][0] = 0;
	
	return total;	
}

// Copies a page from the hard disk to the RAM. 
// Pre-conditions: Page currently not in RAM; page's translation is not in the TLB.
// Find a frame for the page; if all the frames are used, performa a page eviction.
// Find a TLB entry for the page; if the TLB is full, perform a TLB shootdown.
// Parameter read_write: indicates read access or write access
void cache_page_in_RAM(unsigned int PageTable[][4], int page_table_size, unsigned int TLB[][5],
	                  int tlb_size, unsigned int FrameTable[], int frame_table_size, unsigned int vpn, int read_write)
{
	int frame, candidate, frame1;
	
	HDDReads++;
	pageTableWrites++;
	
	frame = get_available_frame(FrameTable, frame_table_size);
	
	if (frame != -1)
	{
		PageTable[vpn][0] = 1;
		PageTable[vpn][1] = read_write;
		PageTable[vpn][2] = 1;
		PageTable[vpn][3] = frame;
		cache_translation_in_TLB(TLB, tlb_size, PageTable, page_table_size, vpn);
	}
	
	else if (frame == -1)
	{
		candidate = select_page_eviction_candidate(PageTable, page_table_size);
		page_evict(PageTable, page_table_size, TLB, tlb_size, FrameTable, frame_table_size, candidate);
		frame1 = get_available_frame(FrameTable, frame_table_size);
		PageTable[vpn][0] = 1;
		PageTable[vpn][1] = read_write;
		PageTable[vpn][2] = 1;
		PageTable[vpn][3] = frame1;
		cache_translation_in_TLB(TLB, tlb_size, PageTable, page_table_size, vpn);
	}
}

// Clears the reference bits of the TLB and the Page Table.
void reset_reference_bits(unsigned int TLB[][5], int tlb_size, unsigned int PageTable[][4], int page_table_size)
{
	int i;
	
	for (i = 0; i < tlb_size; i++)
		TLB[i][2] = 0;
	
	for (i = 0; i < page_table_size; i++)
		PageTable[i][2] = 0;
}

// Simulates a memory access; updates all the data and statistics.
void memory_access(unsigned int TLB[][5], int tlb_size, unsigned PageTable[][4], int page_table_size, unsigned int FrameTable[],
	               int frame_table_size, unsigned int address, int read_write)
{
	// Take the address and integer divide it by 1024 this will truncate to VPN
	int index;
	unsigned vpn = address / 1024;
	
	//TLB hit
	index = TLB_lookup(TLB, tlb_size, vpn);
	
	if (index != -1)
	{
		TLB_Hit++;
		if (read_write)
			TLB[index][1] = 1;
		
		TLB[index][2] = 1;
	}
	
	// Page fault
	else if (PageTable[vpn][0] == 0)
	{
		TLB_Miss++;
		pageFault++;
		pageTableAcc++;
		
		cache_page_in_RAM(PageTable, page_table_size, TLB, tlb_size, FrameTable, frame_table_size, vpn, read_write);
	}
	
	else if (PageTable[vpn][0] == 1)
	{
		TLB_Miss++;
		pageTableAcc++;
		pageTableHit++;
		
		cache_translation_in_TLB(TLB, tlb_size, PageTable, page_table_size, vpn);
	}
}

void drawOutput(int Addresses[],bool state[], int add_length)
{
	int t;
	printf("-----------------------------------------\n");
	printf("        VIRTUAL MEMORY SIMULATION\n");
	printf("-----------------------------------------\n");
	
	for (t = 0; t < add_length; t++)
	{
		printf("T: %6d     Address: %c %7d            VPN: %d\n", t + 1, (state[t]) ? 'w' : 'r', Addresses[t], (Addresses[t] / 1024));
	}
	
	printf("\n-----------------------------------------------------------------------------\n");
	printf("                              SIMULATION RESULTS\n");
	printf("-----------------------------------------------------------------------------\n");
	printf("TLB hits: %d\t\tTLB misses: %d\t\tTLB hit rate: %.2f%%\n", TLB_Hit, TLB_Miss, (((double)TLB_Hit / (TLB_Hit + TLB_Miss)) * 100));
	printf("TLB Shootdowns: %d\tTLB Writes: %d\n\n", TLB_ShootDown, TLB_Writes);
	printf("Pg Table accesses: %d\n", pageTableAcc);
	printf("Pg Table hits: %d\tPg faults: %d\t\tPg Table hit rate: %.2f%%\n", pageTableHit, pageFault,  (((double)pageTableHit / (pageTableHit + pageFault)) * 100));
	printf("Pg evictions: %d\t%cPg Table writes: %d\n\n", pageTableEvics, (add_length == 3000 || add_length == 500) ? ' ' : '\t', pageTableWrites);
	printf("Hard disk reads: %d\tHard disk writes: %d\n", HDDReads, HDDWrites);
	printf("-----------------------------------------------------------------------------\n\n");
	
	
}

void resetGlobals()
{
	
	TLB_Hit = 0;
	TLB_Miss = 0;
	TLB_ShootDown = 0;
	TLB_Writes = 0;
	pageTableHit = 0;
	pageFault = 0;
	pageTableAcc = 0;
	pageTableWrites = 0;
	pageTableEvics = 0;
	HDDWrites = 0;
	HDDReads = 0;
}

void testCase01(unsigned int TLB[][5], int tlb_size, unsigned PageTable[][4], int page_table_size, unsigned int FrameTable[], int frame_table_size)
{
	int Addresses[] = {51, 374, 846, 219, 433};
	bool state[] = {1, 0, 1, 1, 1};
	int i;
	
	resetGlobals();
	
	for (i = 0; i < 5; i++)
	{
		memory_access(TLB, TLB_SIZE, PageTable, VPAGES, FrameTable, FRAMES, Addresses[i], state[i]);
	}
	
	drawOutput(Addresses, state, 5);
}

void testCase02(unsigned int TLB[][5], int tlb_size, unsigned PageTable[][4], int page_table_size, unsigned int FrameTable[], int frame_table_size)
{
	int Addresses[] = {178406, 442630, 507896, 212346, 439947};
	bool state[] = {1, 0, 1, 0, 0};
	int i;
	
	resetGlobals();
	
	for (i = 0; i < 5; i++)
	{
		memory_access(TLB, TLB_SIZE, PageTable, VPAGES, FrameTable, FRAMES, Addresses[i], state[i]);
	}
	
	drawOutput(Addresses, state, 5);
}

void testCase03(unsigned int TLB[][5], int tlb_size, unsigned PageTable[][4], int page_table_size, unsigned int FrameTable[], int frame_table_size)
{
	int Addresses[] = {845, 1857, 2103, 4081, 5068, 5535, 6345, 7304, 8704, 9315, 10374, 12160, 12961, 13553, 14405,
		               15578, 16960, 18067, 18734, 20470};
	bool state[] = {0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0};
	int i;
	
	resetGlobals();
	
	for (i = 0; i < 20; i++)
	{
		memory_access(TLB, TLB_SIZE, PageTable, VPAGES, FrameTable, FRAMES, Addresses[i], state[i]);
	
	}
	
	drawOutput(Addresses, state, 20);
}

void testCase04(unsigned int TLB[][5], int tlb_size, unsigned PageTable[][4], int page_table_size, unsigned int FrameTable[], int frame_table_size)
{
	int Addresses[500];
	bool state[500];
	int i;
	
	resetGlobals();
	
	
	for (i = 0; i < 500; i++)
	{
		Addresses[i] = rand() % 1048576;
		state[i] = rand() % 2;
		memory_access(TLB, TLB_SIZE, PageTable, VPAGES, FrameTable, FRAMES, Addresses[i], state[i]);
	
	}
	
	drawOutput(Addresses, state, 500);
}

void testCase05(unsigned int TLB[][5], int tlb_size, unsigned PageTable[][4], int page_table_size, unsigned int FrameTable[], int frame_table_size)
{
	int Addresses[500];
	bool state[500];
	int i;
	
	resetGlobals();
	
	
	for (i = 0; i < 500; i++)
	{
		Addresses[i] = rand() % 1048576;
		state[i] = 1;
		memory_access(TLB, TLB_SIZE, PageTable, VPAGES, FrameTable, FRAMES, Addresses[i], state[i]);
	
	}
	
	drawOutput(Addresses, state, 500);
}

void testCase06(unsigned int TLB[][5], int tlb_size, unsigned PageTable[][4], int page_table_size, unsigned int FrameTable[], int frame_table_size)
{
	int Addresses[1000];
	bool state[1000];
	int i, j = 0, ind = 0;
	
	resetGlobals();
	
	
	for (i = 0; i < 25; i++)
	{
		
		for (j = 0; j < 40; j++)
		{
			Addresses[ind] = rand() % ((1023 * (1 + i) + i + 1) + 1 - (1023 * i + i)) + (1023 * i + i);
			state[ind] = rand() % 2;
			ind ++;
		}
	}
	
	for (i = 0; i < 1000; i++)
	{
		memory_access(TLB, TLB_SIZE, PageTable, VPAGES, FrameTable, FRAMES, Addresses[i], state[i]);
	}
	
	drawOutput(Addresses, state, 1000);
}

void testCase07(unsigned int TLB[][5], int tlb_size, unsigned PageTable[][4], int page_table_size, unsigned int FrameTable[], int frame_table_size)
{
	int Addresses[3000];
	bool state[3000];
	int i, j = 0, ind = 0;
	
	resetGlobals();
	
	
	for (i = 0; i < 300; i++)
	{
		
		for (j = 0; j < 10; j++)
		{
			Addresses[ind] = rand() % ((1023 * (1 + i) + i + 1) + 1 - (1023 * i + i)) + (1023 * i + i);
			state[ind] = rand() % 2;
			ind ++;
		}
	}
	
	for (i = 0; i < 3000; i++)
	{
		memory_access(TLB, TLB_SIZE, PageTable, VPAGES, FrameTable, FRAMES, Addresses[i], state[i]);
	}
	
	drawOutput(Addresses, state, 3000);
}

void testCase08(unsigned int TLB[][5], int tlb_size, unsigned PageTable[][4], int page_table_size, unsigned int FrameTable[], int frame_table_size)
{
	int Addresses[160];
	bool state[160];
	int i, j = 0, ind = 0;
	
	resetGlobals();
	
	
	for (i = 0; i < 8; i += 2)
	{
		
		for (j = 0; j < 40; j++)
		{
			Addresses[ind] = rand() % ((1023 * (1 + i) + i + 1) + 1 - (1023 * i + i)) + (1023 * i + i);
			state[ind] = rand() % 2;
			ind ++;
		}
	}
	
	for (i = 0; i < 160; i++)
	{
		memory_access(TLB, TLB_SIZE, PageTable, VPAGES, FrameTable, FRAMES, Addresses[i], state[i]);
	}
	
	drawOutput(Addresses, state, 160);
}
int main()
{
	srand(time(0));
	
	unsigned int PageTable[VPAGES][4] = {0};
	unsigned int TLB [TLB_SIZE][5] = {0};
	unsigned int FrameTable[FRAMES] = {0};
	
	int input;
	bool flag = true;
	
	
	while (flag)
	{
		printf("ENTER 1 - 8 TO RUN CORRESPONDING TEST CASE 9 TO EXIT\n");
		scanf("%d", &input);
		if (input == 9)
		{
			flag = false; 
			break;
		}
		
		else if (input < 1 || input > 9)
		{
			continue;
		}
		
		switch (input)
		{
			case 1:
				memset(PageTable, 0, sizeof(PageTable[0][0]) * VPAGES * 4);
				memset(TLB, 0, sizeof(TLB[0][0]) * TLB_SIZE * 5);
				memset(FrameTable, 0, sizeof(FrameTable));			
				testCase01(TLB, TLB_SIZE, PageTable, VPAGES, FrameTable, FRAMES);
				break;
			case 2:
				memset(PageTable, 0, sizeof(PageTable[0][0]) * VPAGES * 4);
				memset(TLB, 0, sizeof(TLB[0][0]) * TLB_SIZE * 5);
				memset(FrameTable, 0, sizeof(FrameTable));
				testCase02(TLB, TLB_SIZE, PageTable, VPAGES, FrameTable, FRAMES);
				break;
			case 3:
				memset(PageTable, 0, sizeof(PageTable[0][0]) * VPAGES * 4);
				memset(TLB, 0, sizeof(TLB[0][0]) * TLB_SIZE * 5);
				memset(FrameTable, 0, sizeof(FrameTable));			
				testCase03(TLB, TLB_SIZE, PageTable, VPAGES, FrameTable, FRAMES);
				break;
			case 4:
				memset(PageTable, 0, sizeof(PageTable[0][0]) * VPAGES * 4);
				memset(TLB, 0, sizeof(TLB[0][0]) * TLB_SIZE * 5);
				memset(FrameTable, 0, sizeof(FrameTable));			
				testCase04(TLB, TLB_SIZE, PageTable, VPAGES, FrameTable, FRAMES);
				break;
			case 5:
				memset(PageTable, 0, sizeof(PageTable[0][0]) * VPAGES * 4);
				memset(TLB, 0, sizeof(TLB[0][0]) * TLB_SIZE * 5);
				memset(FrameTable, 0, sizeof(FrameTable));			
				testCase05(TLB, TLB_SIZE, PageTable, VPAGES, FrameTable, FRAMES);
				break;
			case 6:
				memset(PageTable, 0, sizeof(PageTable[0][0]) * VPAGES * 4);
				memset(TLB, 0, sizeof(TLB[0][0]) * TLB_SIZE * 5);
				memset(FrameTable, 0, sizeof(FrameTable));			
				testCase06(TLB, TLB_SIZE, PageTable, VPAGES, FrameTable, FRAMES);
				break;
			case 7:
				memset(PageTable, 0, sizeof(PageTable[0][0]) * VPAGES * 4);
				memset(TLB, 0, sizeof(TLB[0][0]) * TLB_SIZE * 5);
				memset(FrameTable, 0, sizeof(FrameTable));			
				testCase07(TLB, TLB_SIZE, PageTable, VPAGES, FrameTable, FRAMES);
				break;
			case 8:
				memset(PageTable, 0, sizeof(PageTable[0][0]) * VPAGES * 4);
				memset(TLB, 0, sizeof(TLB[0][0]) * TLB_SIZE * 5);
				memset(FrameTable, 0, sizeof(FrameTable));			
				testCase08(TLB, TLB_SIZE, PageTable, VPAGES, FrameTable, FRAMES);
				break;
		}
	}
}
