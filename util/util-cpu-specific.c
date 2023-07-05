#include "util-cpu-specific.h"

// Find these numbers here:
//  https://software.intel.com/sites/default/files/managed/39/c5/325462-sdm-vol-1-2abcd-3abcd.pdf
//
// Find original code here:
//  https://github.com/clementine-m/msr-uncore-cbo
//
// Skylake client has 4 C-boxes (meaning 4 slices?)
//
// Coffeelake has 8 C-boxes which are not documented in the Intel manual
// The 8th one is weird and fortunately documented here: https://lore.kernel.org/patchwork/patch/1002033/

const unsigned long long MSR_UNC_PERF_GLOBAL_CTRL = 0xe01;

const unsigned long long *MSR_UNC_CBO_PERFEVTSEL0 = (unsigned long long []) {0x0E01, 0x0E11, 0x0E21, 0x0E31, 0x0E41, 0x0E51, 0x0E61, 0x0E71, 0x0E81, 0x0E91,
																0x0EA1, 0x0EB1, 0x0EC1, 0x0ED1, 0x0EE1, 0x0EF1, 0x0F01, 0x0F11, 0x0F21,	0x0F31,	0x0F41, 
																0x0F51, 0x0F61, 0x0F71, 0x0F81, 0x0F91, 0x0FA1, 0x0FB1};

const unsigned long long SELECT_EVENT_CORE = 0x408f34;

const unsigned long long *MSR_UNC_CBO_PERFCTR0 = (unsigned long long []) {0x0E00, 0x0E10, 0x0E20, 0x0E30, 0x0E40, 0x0E50, 0x0E60, 0x0E70, 0x0E80, 0x0E90,
																0x0EA0, 0x0EB0, 0x0EC0, 0x0ED0, 0x0EE0, 0x0EF0, 0x0F00, 0x0F10, 0x0F20, 0x0F30,	0x0F40,	
																0x0F50, 0x0F60, 0x0F70, 0x0F80, 0x0F90, 0x0FA0, 0x0FB0};

const unsigned long long ENABLE_CTRS = 0x20000000;

const unsigned long long DISABLE_CTRS = 0x80000000;

const unsigned long long RESET_CTRS = 0x30002;

#define PMON_GLOBAL_CTL_ADDRESS 0x700
const unsigned long long * CHA_CBO_EVENT_ADDRESS = (unsigned long long []) {0x0E01, 0x0E11, 0x0E21, 0x0E31, 0x0E41, 0x0E51, 0x0E61, 0x0E71, 0x0E81, 0x0E91,
																0x0EA1, 0x0EB1, 0x0EC1, 0x0ED1, 0x0EE1, 0x0EF1, 0x0F01, 0x0F11, 0x0F21,	0x0F31,	0x0F41, 
																0x0F51, 0x0F61, 0x0F71, 0x0F81, 0x0F91, 0x0FA1, 0x0FB1};

const unsigned long long * CHA_CBO_CTL_ADDRESS = (unsigned long long []) {0x0E00, 0x0E10, 0x0E20, 0x0E30, 0x0E40, 0x0E50, 0x0E60, 0x0E70, 0x0E80, 0x0E90,
																0x0EA0, 0x0EB0, 0x0EC0, 0x0ED0, 0x0EE0, 0x0EF0, 0x0F00, 0x0F10, 0x0F20, 0x0F30,	0x0F40,	
																0x0F50, 0x0F60, 0x0F70, 0x0F80, 0x0F90, 0x0FA0, 0x0FB0};

const unsigned long long * CHA_CBO_FILTER_ADDRESS = (unsigned long long []) {0x0E05, 0x0E15, 0x0E25, 0x0E35, 0x0E45, 0x0E55, 0x0E65, 0x0E75, 0x0E85, 0x0E95,
																0x0EA5, 0x0EB5, 0x0EC5, 0x0ED5, 0x0EE5, 0x0EF5, 0x0F05, 0x0F15, 0x0F25, 0x0F35, 0x0F45,
																0x0F55, 0x0F65, 0x0F75, 0x0F85, 0x0F95, 0x0FA5, 0x0FB5};

const unsigned long long * CHA_CBO_COUNTER_ADDRESS = (unsigned long long []) {0x0E08, 0x0E18, 0x0E28, 0x0E38, 0x0E48, 0x0E58, 0x0E68, 0x0E78, 0x0E88, 0x0E98,
																0x0EA8, 0x0EB8, 0x0EC8, 0x0ED8, 0x0EE8, 0x0EF8, 0x0F08, 0x0F18, 0x0F28, 0x0F38,	0x0F48,
																0x0F58, 0x0F68, 0x0F78, 0x0F88, 0x0F98, 0x0FA8, 0x0FB8};

/* MSR Values */
#define ENABLE_COUNT 0x20000000
#define DISABLE_COUNT 0x80000000
#define SELECTED_EVENT 0x441134 /* Event: LLC_LOOKUP Mask: Any request (All snooping signals) */
#define RESET_COUNTERS 0x30002
#define FILTER_BOX_VALUE 0x007E0000

//cpu-specific
#define NUMBER_SLICES 8
#define LINE 64

/*
 * Read an MSR on CPU 0
 */
static uint64_t rdmsr_on_cpu_0(uint32_t reg)
{
	uint64_t data;
	int cpu = 0;
	char *msr_file_name = "/dev/cpu/0/msr";

	static int fd = -1;

	if (fd < 0) {
		fd = open(msr_file_name, O_RDONLY);
		if (fd < 0) {
			if (errno == ENXIO) {
				fprintf(stderr, "rdmsr: No CPU %d\n", cpu);
				exit(2);
			} else if (errno == EIO) {
				fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n",
						cpu);
				exit(3);
			} else {
				perror("rdmsr: open");
				exit(127);
			}
		}
	}

	if (pread(fd, &data, sizeof data, reg) != sizeof data) {
		if (errno == EIO) {
			fprintf(stderr, "rdmsr: CPU %d cannot read "
							"MSR 0x%08" PRIx32 "\n",
					cpu, reg);
			exit(4);
		} else {
			perror("rdmsr: pread");
			exit(127);
		}
	}

	return data;
}

/*
 * Write to an MSR on CPU 0
 */
static void wrmsr_on_cpu_0(uint32_t reg, int valcnt, uint64_t *regvals)
{
	uint64_t data;
	char *msr_file_name = "/dev/cpu/0/msr";
	int cpu = 0;

	static int fd = -1;

	if (fd < 0) {
		fd = open(msr_file_name, O_WRONLY);
		if (fd < 0) {
			if (errno == ENXIO) {
				fprintf(stderr, "wrmsr: No CPU %d\n", cpu);
				exit(2);
			} else if (errno == EIO) {
				fprintf(stderr, "wrmsr: CPU %d doesn't support MSRs\n",
						cpu);
				exit(3);
			} else {
				perror("wrmsr: open");
				exit(127);
			}
		}
	}

	while (valcnt--) {
		data = *regvals++;
		if (pwrite(fd, &data, sizeof data, reg) != sizeof data) {
			if (errno == EIO) {
				fprintf(stderr,
						"wrmsr: CPU %d cannot set MSR "
						"0x%08" PRIx32 " to 0x%016" PRIx64 "\n",
						cpu, reg, data);
				exit(4);
			} else {
				perror("wrmsr: pwrite");
				exit(127);
			}
		}
	}

	return;
}

/*
 * Initialize uncore registers (CBo/CHA and Global MSR) before polling
 */
static void uncore_init() {

	int i;

	/* Setup monitoring session */

	/* Disable counters */
	uint64_t register_value[] = {DISABLE_COUNT};
	register_value[0]=DISABLE_COUNT;
	wrmsr_on_cpu_0(PMON_GLOBAL_CTL_ADDRESS,1,register_value);

	/* Select the event to monitor */
	register_value[0]=SELECTED_EVENT;
	for(i=0; i<NUMBER_SLICES; i++) {
		wrmsr_on_cpu_0(CHA_CBO_EVENT_ADDRESS[i], 1, register_value);
	}

	/* Reset CHA Counters */
	register_value[0]=RESET_COUNTERS;
	for(i=0; i<NUMBER_SLICES; i++) {
		wrmsr_on_cpu_0(CHA_CBO_CTL_ADDRESS[i],1,register_value);
	}

	/* Set Filter BOX */
	register_value[0]=FILTER_BOX_VALUE;
	for(i=0; i<NUMBER_SLICES; i++) {
		wrmsr_on_cpu_0(CHA_CBO_FILTER_ADDRESS[i], 1, register_value);
	}

	/* Enable counting */
	register_value[0]=ENABLE_COUNT;
	wrmsr_on_cpu_0(PMON_GLOBAL_CTL_ADDRESS, 1, register_value);
}

/*
 * Polling one address
 */
#define NUMBER_POLLING 1500
static void polling(void *address)
{
	unsigned long i;
	for (i = 0; i < NUMBER_POLLING; i++) {
		_mm_clflush(address);
	}
}

/*
 * Read the CBo counters' value and return the one with maximum number
 */
int find_CHA_CBO() {

	int i;
	unsigned long long* CHA_CBO_value = calloc(NUMBER_SLICES, sizeof(unsigned long long));

	/* Read CHA/CBo counter's value */
	for(i=0; i<NUMBER_SLICES; i++){
		CHA_CBO_value[i] = rdmsr_on_cpu_0(CHA_CBO_COUNTER_ADDRESS[i]);
	}

	/* Find maximum */
	unsigned long long max_value=0;
	int max_index=0;
	for(i=0; i<NUMBER_SLICES; i++){
		//printf(" %llu", CHA_CBO_value[i]);
		if(CHA_CBO_value[i]>max_value){
			max_value=CHA_CBO_value[i];
			max_index=i;
		}
	}
	return max_index;
}

static int skylake_i7_6700_cache_slice_alg(uint64_t i_addr)
{
	int bit0 = ((i_addr >> 6) & 1) ^ ((i_addr >> 10) & 1) ^ ((i_addr >> 12) & 1) ^ ((i_addr >> 14) & 1)
			 ^ ((i_addr >> 16) & 1) ^ ((i_addr >> 17) & 1) ^ ((i_addr >> 18) & 1) ^ ((i_addr >> 20) & 1)
			 ^ ((i_addr >> 22) & 1) ^ ((i_addr >> 24) & 1) ^ ((i_addr >> 25) & 1) ^ ((i_addr >> 26) & 1)
			 ^ ((i_addr >> 27) & 1) ^ ((i_addr >> 28) & 1) ^ ((i_addr >> 30) & 1) ^ ((i_addr >> 32) & 1)
			 ^ ((i_addr >> 33) & 1) ^ ((i_addr >> 35) & 1);

	int bit1 = ((i_addr >> 7) & 1) ^ ((i_addr >> 11) & 1) ^ ((i_addr >> 13) & 1) ^ ((i_addr >> 15) & 1)
			 ^ ((i_addr >> 17) & 1) ^ ((i_addr >> 19) & 1) ^ ((i_addr >> 20) & 1) ^ ((i_addr >> 21) & 1)
			 ^ ((i_addr >> 22) & 1) ^ ((i_addr >> 23) & 1) ^ ((i_addr >> 24) & 1) ^ ((i_addr >> 26) & 1)
			 ^ ((i_addr >> 28) & 1) ^ ((i_addr >> 29) & 1) ^ ((i_addr >> 31) & 1) ^ ((i_addr >> 33) & 1)
			 ^ ((i_addr >> 34) & 1) ^ ((i_addr >> 35) & 1);

	int result = ((bit1 << 1) | bit0);
	return result;
}

static int coffee_lake_i7_9700_cache_slice_alg(uint64_t i_addr)
{
	int bit0 = ((i_addr >> 6) & 1) ^ ((i_addr >> 10) & 1) ^ ((i_addr >> 12) & 1) ^ ((i_addr >> 14) & 1)
			 ^ ((i_addr >> 16) & 1) ^ ((i_addr >> 17) & 1) ^ ((i_addr >> 18) & 1) ^ ((i_addr >> 20) & 1)
			 ^ ((i_addr >> 22) & 1) ^ ((i_addr >> 24) & 1) ^ ((i_addr >> 25) & 1) ^ ((i_addr >> 26) & 1)
			 ^ ((i_addr >> 27) & 1) ^ ((i_addr >> 28) & 1) ^ ((i_addr >> 30) & 1) ^ ((i_addr >> 32) & 1)
			 ^ ((i_addr >> 33) & 1);

	int bit1 = ((i_addr >> 9) & 1) ^ ((i_addr >> 12) & 1) ^ ((i_addr >> 16) & 1) ^ ((i_addr >> 17) & 1)
			 ^ ((i_addr >> 19) & 1) ^ ((i_addr >> 21) & 1) ^ ((i_addr >> 22) & 1) ^ ((i_addr >> 23) & 1)
			 ^ ((i_addr >> 25) & 1) ^ ((i_addr >> 26) & 1) ^ ((i_addr >> 27) & 1) ^ ((i_addr >> 29) & 1)
			 ^ ((i_addr >> 31) & 1) ^ ((i_addr >> 32) & 1);

	int bit2 = ((i_addr >> 10) & 1) ^ ((i_addr >> 11) & 1) ^ ((i_addr >> 13) & 1) ^ ((i_addr >> 16) & 1)
			 ^ ((i_addr >> 17) & 1) ^ ((i_addr >> 18) & 1) ^ ((i_addr >> 19) & 1) ^ ((i_addr >> 20) & 1)
			 ^ ((i_addr >> 21) & 1) ^ ((i_addr >> 22) & 1) ^ ((i_addr >> 27) & 1) ^ ((i_addr >> 28) & 1)
			 ^ ((i_addr >> 30) & 1) ^ ((i_addr >> 31) & 1) ^ ((i_addr >> 33) & 1) ^ ((i_addr >> 34) & 1);

	int result = ((bit2 << 2) | (bit1 << 1) | bit0);
	return result;
}

uint64_t
rte_xorall64(uint64_t ma) {
	return __builtin_parityll(ma);
}

/* Calculate slice based on the physical address - Haswell */

/* Number of hash functions/Bits for slices */
#define bitNum 3
/* Bit0 hash */
#define hash_0 0x1B5F575440
/* Bit1 hash */
#define hash_1 0x2EB5FAA880
/* Bit2 hash */
#define hash_2 0x3CCCC93100

uint8_t
calculateSlice_HF_haswell(uint64_t pa) {
	uint8_t sliceNum=0;
	sliceNum= (sliceNum << 1) | (rte_xorall64(pa&hash_2));
	sliceNum= (sliceNum << 1) | (rte_xorall64(pa&hash_1));
	sliceNum= (sliceNum << 1) | (rte_xorall64(pa&hash_0));
	return sliceNum;
}

/* Calculate the slice based on a given virtual address - Haswell and SkyLake */

uint8_t
calculateSlice_uncore(void* va) {
	/*
	 * The registers' address and their values would be selected in msr-utils.c
	 * check_cpu.sh will automatically define the proper architecture (i.e., #define HASWELL or #define SKYLAKE).
	 */
	uncore_init();
	polling(va);
	return find_CHA_CBO();
}


/* 
 * Calculate virtual slice based on the virtual address - SkyLake 
 * Since SkyLake has 18 slices and 8 cores, we tag every slice with a number between 0 to 7, which represent the virtual slice
 * The mapping with virtual slice number and number of cores is similar to Haswell architecture, i.e., slice ith is the closest to core ith.
 * The mapping for virtual slices is as follows:
 * VS0 -> S0/S2/S6
 * VS1 -> S4/S1
 * VS2 -> S8/S11
 * VS3 -> S12/S13
 * VS4 -> S10/S7/S9
 * VS5 -> S14/S16
 * VS6 -> S3/S5
 * VS7 -> S15/S17
 */

uint8_t
calculateVirtualSlice_uncore(void* va) {
	uint8_t slice = calculateSlice_uncore(va);
	uint8_t virtualSlice=0;

	if (slice==0 || slice ==2 || slice==6){
		virtualSlice=0;
	} else if (slice==4 || slice==1){
		virtualSlice=1;
	} else if (slice==8 || slice==11){
		virtualSlice=2;
	} else if (slice==12 || slice==13){
		virtualSlice=3;
	} else if (slice==10 || slice==7 || slice==9){
		virtualSlice=4;
	} else if (slice==14 || slice==16){
		virtualSlice=5;
	} else if (slice==3 || slice==5){
		virtualSlice=6;
	} else if (slice==15 || slice==17){
		virtualSlice=7;
	}

	return virtualSlice;
}

/* Find the next chunk that is mapped to the input slice number - with Haswell hash function */

uint64_t
sliceFinder_HF_haswell(uint64_t pa, uint8_t desiredSlice) {
	uint64_t offset=0;
	while(desiredSlice!=calculateSlice_HF_haswell(pa+offset)) {
		/* Slice mapping will change for each cacheline which is 64 Bytes */
		offset+=LINE;
	}
	return offset;
}

/* Find the next chunk that is mapped to the input slice number - with Haswell/Skylake uncore performance counters */

uint64_t
sliceFinder_uncore(void* va, uint8_t desiredSlice) {
	uint64_t offset=0;
	while(desiredSlice!=calculateSlice_uncore(va+offset)) {
		/* Slice mapping will change for each cacheline which is 64 Bytes */
		offset+=LINE;
	}
	return offset;
}

/* 
 * Get the page frame number
 */
static uint64_t get_page_frame_number_of_address(void *address)
{
	/* Open the pagemap file for the current process */
	FILE *pagemap = fopen("/proc/self/pagemap", "rb");

	/* Seek to the page that the buffer is on */
	uint64_t offset = (uint64_t)((uint64_t)address >> PAGE_SHIFT) * (uint64_t)PAGEMAP_LENGTH;
	if (fseek(pagemap, (uint64_t)offset, SEEK_SET) != 0) {
		fprintf(stderr, "Failed to seek pagemap to proper location\n");
		exit(1);
	}

	/* The page frame number is in bits 0-54 so read the first 7 bytes and clear the 55th bit */
	uint64_t page_frame_number = 0;
	int ret = fread(&page_frame_number, 1, PAGEMAP_LENGTH - 1, pagemap);
	page_frame_number &= 0x7FFFFFFFFFFFFF; // Mastik uses 0x3FFFFFFFFFFFFF
	fclose(pagemap);
	return page_frame_number;
}

/*
 * Get the physical address of a page
 */
uint64_t get_physical_address(void *address)
{
	/* Get page frame number */
	unsigned int page_frame_number = get_page_frame_number_of_address(address);

	/* Find the difference from the buffer to the page boundary */
	uint64_t distance_from_page_boundary = (uint64_t)address % getpagesize();

	/* Determine how far to seek into memory to find the buffer */
	uint64_t physical_address = (uint64_t)((uint64_t)page_frame_number << PAGE_SHIFT) + (uint64_t)distance_from_page_boundary;

	return physical_address;
}

static void check_reversed_function_correct(uint64_t slice_id, void *va)
{
	uncore_init();
	polling(va);
	uint64_t ground_truth = find_CHA_CBO();
	uint64_t pa = get_physical_address(va);

	if (slice_id != ground_truth) {
		printf("%" PRIu64 " should be %" PRIu64 "\n", slice_id, ground_truth);
	}
}

/*
 * Calculate the slice based on a given virtual address - Haswell and SkyLake 
 */
uint64_t get_cache_slice_index(void *va)
{
	// uncore_init();
	// polling(va);
	// uint64_t slice_id = find_CBO();

	uint64_t pa = get_physical_address(va);
	uint64_t slice_id = calculateSlice_HF_haswell(pa);

	// // Code to verify that our slice mapping function works as expected
	// check_reversed_function_correct(slice_id, va);

	// // Code to print the binary representation of an address and its slice ID
	// char hex_key[17];
	// char bin_key[65];
	// sprintf(hex_key, "%09lX", pa);
	// hexToBin(hex_key, bin_key);
	// printf("%s %d\n", bin_key, slice_id);

	return slice_id;
}
