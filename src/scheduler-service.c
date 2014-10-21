#include <stdio.h>
#include <stdbool.h>
#include "memory_controller.h"
#include "utlist.h"
#include "utils.h"

#include "scheduler.h"


//////////////////////////////////////////////////
//////////////////////////////////////////////////
///////////      Constant Values        //////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
#define MAX_NUM_THREADS 16

extern int NUMCORES;
extern int NUM_CHANNELS ;// 1;
// number of ranks per channel
extern int NUM_RANKS ;// 2;
// number of banks per rank
extern int NUM_BANKS ;// 8;
// number of rows per bank
extern int NUM_ROWS ;// 32768;
// number of columns per rank
extern int NUM_COLUMNS ;// 128;
// cache-line size (bytes)
extern int CACHE_LINE_SIZE ;// 64;
// total number of address bits (i.e. indicates size of memory)
extern int ADDRESS_BITS ;// 32;

extern long long int CYCLE_VAL;
extern int PROCESSOR_CLK_MULTIPLIER;
extern int MAX_RETIRE;
extern int MAX_FETCH;
extern int ROBSIZE;
extern int PIPELINEDEPTH;

// RAS to CAS delay
extern int T_RCD ;// 44;
// PRE to RAS
extern int T_RP ;// 44;
// ColumnRD to Data burst
extern int T_CAS ;// 44;
// RAS to PRE delay
extern int T_RAS ;// 112;
// Row Cycle time
extern int T_RC ;// 156;
// ColumnWR to Data burst
extern int T_CWD ;// 20;
// write recovery time (COL_WR to PRE)
extern int T_WR ;// 48;
// write to read turnaround
extern int T_WTR ;// 24;
// rank to rank switching time
extern int T_RTRS ;// 8;
// Data transfer
extern int T_DATA_TRANS ;// 16;
// Read to PRE
extern int T_RTP ;// 24;
// CAS to CAS
extern int T_CCD ;// 16;
// Power UP time fast
extern int T_XP ;// 20;
// Power UP time slow
extern int T_XP_DLL ;// 40;
// Power down entry
extern int T_CKE ;// 16;
// Minimum power down duration
extern int T_PD_MIN ;// 16;
// rank to rank delay (ACTs to same rank)
extern int T_RRD ;// 20;
// refresh interval
extern int T_REFI;
// refresh cycle time
extern int T_RFC;
// four bank activation window
extern int T_FAW ;// 128;

extern int WQ_CAPACITY;

#define HI_WM (WQ_CAPACITY -5)
#define WA_WM (HI_WM - 5)
#define MID_WM (HI_WM - 8)
#define LO_WM (MID_WM-8)

#define HIGH_PRIO_AFTER_REFRESH T_RFC

//int types
typedef unsigned long long uint64_t;
typedef long long int64_t;
typedef unsigned long uint32_t;
typedef long int32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned char uint8_t;
typedef char int8_t;


//////////////////////////////////////////////////
//////////////////////////////////////////////////
///////////      utility methods        //////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////

//bit operation
#define EXP2(x) (1ull<<(x))
#define MASK(x) ((1ull<<(x)) - 1ull)
#define AT(v,x) ((v& EXP2(x)) >> x)
#define RANGE(v,f,t) ((v >> f) & MASK(t))
#define XORAT(v,x,b) (v ^ ((b&1) << x))
#define LROT(v,x,w) (((v << ((x)%w)) ^ (v >> (w-((x)%w)))) & MASK(w))
#define RROT(v,x,w) (((v >> ((x)%w)) ^ (v << (w-((x)%w)))) & MASK(w))
#define min(x,y) ((x > y)? y:x)
#define max(x,y) ((x < y)? y:x)

#define LINE "--------------------"

int log_2(const uint64_t x) {
	int i = 0;
	for (i = sizeof(uint64_t) * 8 - 1; i >= 0; i--) {
		if ((1ull << i) <= x) {
			break;
		}
	}
	return i;
}

/**
 * Xorshift
 * http://www.jstatsoft.org/v08/i14/
 */
uint64_t xor128(void) {
	static uint32_t x = 123456789;
	static uint32_t y = 362436069;
	static uint32_t z = 521288629;
	static uint32_t w = 88675123;
	uint64_t t;

	t = x ^ (x << 11);
	x = y;
	y = z;
	z = w;
	return w = (w ^ (w >> 19)) ^ (t ^ (t >> 8));
}

/**
 * Allocate Array of Array
 *
 * @param X
 * @param Y
 * @param size
 */
void* allocAA(const uint32_t X, const uint32_t Y, const size_t size) {
	void** aa = (void**) malloc(sizeof(void*) * X);
	for (int i = 0; i < X; ++i) {
		aa[i] = (void*) malloc(Y * size);
	}
	return aa;
}

/**
 * Allocate Array of Array
 *
 * @param X
 * @param Y
 * @param size
 */
void* allocDAA(const uint32_t X, const uint32_t* Y, const size_t size) {
	void** aa = (void**) malloc(sizeof(void*) * X);
	for (int i = 0; i < X; ++i) {
		aa[i] = (void*) malloc(Y[i] * size);
	}
	return aa;
}

/**
 * Allocate Array of Array of Array
 *
 * @param X
 * @param Y
 * @param Z
 * @param size
 */
void* allocAAA(const int32_t X, const int32_t Y, const int32_t Z, const size_t size) {
	void*** aaa = (void***) malloc(sizeof(void**) * X);
	for (int i = 0; i < X; ++i) {
		aaa[i] = (void**) allocAA(Y, Z, size);
	}
	return aaa;
}


//////////////////////////////////////////////////
//////////////////////////////////////////////////
////////////////      Counter        /////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////

typedef struct {
	char width;
	long max;
	long min;
	long value;
} CTR;

/**
 * get counter instance
 *
 * @params width width of a counter
 * @params s whether it is signed or not;
 */
CTR CTRinit(const char width, const int s) {
	static CTR c;
	if (s) {
		c.max = (1 << (width - 1)) - 1;
		c.min = -(1 << (width - 1));
	} else {
		c.max = (1 << (width)) - 1;
		c.min = 0;
	}
	c.value = 0;
	return c;
}

bool CTRsatMax(CTR* c) {
	return c->value == c->max;
}
bool CTRsatMin(CTR* c) {
	return c->value == c->min;
}

void CTRsetMin(CTR* c) {
	c->value = c->min;
}
void CTRsetMax(CTR* c) {
	c->value = c->max;
}

void CTRset(CTR* c, int value) {
	if (c->max < value) {
		c->value = c->max;
	} else if (c->min > value) {
		c->value = c->min;
	} else {
		c->value = value;
	}
}

void CTRdec(CTR* c) {
	if (c->min < c->value) {
		c->value--;
	}
}

void CTRinc(CTR* c) {
	if (c->max > c->value) {
		c->value++;
	}
}

long CTRget(CTR* c) {
	return c->value;
}


//////////////////////////////////////////////////
//////////////////////////////////////////////////
////////////////      History        /////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////

typedef struct history {
	unsigned int length;bool*hist;bool last;
	unsigned int pos;
} H;

/**
 * Folded History
 *
 * First update history, then update foldel history.
 */
typedef struct {
	unsigned long fh;
	unsigned int length;
	unsigned char width;
	unsigned char p1; // update pos
	unsigned char p2; // retire pos
	H*hist; // pointer to the history
} FH;

H Hinit(const unsigned int length) {
	static H h;

	h.length = length;
	h.hist = (bool*) malloc(sizeof(bool) * (length));
	h.last = false;
	return h;
}

void Hdelete(H* hist) {
	free(hist->hist);
}

FH FHinit(char width, int length, H* hist) {
	static FH fh;

	if (length <= 0) {
		length = 0;
	}
	if (width <= 0) {
		width = 0;
	}

	assert(length < hist->length);
	fh.fh = 0;
	fh.length = length;
	fh.width = width;
	fh.hist = hist;
	fh.p1 = 0;

	if (fh.width == 0) {
		fh.p2 = 0;
	} else {
		fh.p2 = length % width;
	}
	return fh;
}

bool Hread(H*hist, int at) {
	assert(at < hist->length);

	int p = (hist->pos + hist->length - at - 1) % hist->length;
	return hist->hist[p];
}

unsigned long long FHread(FH* fh, int index, int n) {
	if(fh->width == 0){
		return 0;
	}
	return fh->fh ^ LROT((index & MASK(fh->width)),n,fh->width);
}

void Hupdate(H* hist, const bool t) {
	hist->hist[hist->pos] = t;
	hist->last = t;
	hist->pos = (hist->pos + 1);
	if (hist->pos >= hist->length) {
		hist->pos = 0;
	}
}

void FHupdate(FH* fh) {
	//rotate
	if (fh->width > 0 && fh->length > 0) {
		fh->fh = LROT(fh->fh,1,fh->width);

		//update
		if (fh->hist->last) {
			fh->fh = XORAT(fh->fh,fh->p1,1ul);
		}

		//retire
		if (Hread(fh->hist, fh->length)) {
			fh->fh = XORAT(fh->fh,fh->p2,1ul);
		}
	}
}

void Hprint(H* h) {
	for (int i = 1; i <= h->length; i++) {
		if (i % 50 == 1) {
			puts("");
		}
		int p = (h->pos - i + h->length) % h->length;
		printf("%d", (h->hist[p]) ? 1 : 0);
	}
	printf("\n");
}

void Hdump(H* h) {
	puts(LINE);
	printf("length: %d\n", h->length);
	printf("last  : %d\n", h->last);
	printf("pos   : %d\n", h->pos);
	printf("hist  : ");
	Hprint(h);
	puts(LINE);
}
void FHprint(FH* fh) {
	for (int i = fh->width - 1; i >= 0; i--) {
		printf("%d", (AT(fh->fh,i) == 0) ? 0 : 1);
	}
	puts("");
}
void FHdump(FH* fh) {
	puts(LINE);
	printf("length: %d\n", fh->length);
	printf("width : %d\n", fh->width);
	printf("up pos: %d\n", fh->p1);
	printf("dl pos: %d\n", fh->p2);
	printf("hist  : ");
	FHprint(fh);
	puts(LINE);
}



//////////////////////////////////////////////////
//////////////////////////////////////////////////
///////////////      LRU table       /////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
typedef struct {
	uint32_t ** lru;
	uint64_t** tag;
	void*** value;

	uint8_t assoc;
	uint8_t tag_width;
	uint8_t height;
	size_t entrysize;
} LRU;
LRU LRUinit(uint8_t tag_width, uint8_t assoc, uint8_t height_bit, size_t size) {
	LRU table;

	table.tag_width = tag_width;
	table.assoc = assoc;
	table.height = height_bit;
	table.entrysize = size;

	table.value = allocAA(assoc, (1 << table.height), sizeof(void*));
	table.tag = allocAA(assoc, (1 << table.height), sizeof(uint64_t));
	table.lru = allocAA(assoc, (1 << table.height), sizeof(uint32_t));
	for (int i = 0; i < assoc; ++i) {
		for (int j = 0; j < (1 << table.height); ++j) {
			table.value[i][j] = malloc(table.entrysize);
		}
	}

	return table;
}
int LRUbudgets(LRU* table){
	return table->assoc * (1 << table->height) * (table->tag_width + table->entrysize + log_2(table->assoc));
}

bool LRUfind(LRU* table, uint32_t index, void** value) {
	uint64_t tag = RANGE(index,table->height,table->tag_width);
	assert(tag < 1 << table->tag_width);
	assert(tag >= 0);
	int i = index & MASK(table->height);
	assert(i < 1 << table->height);
	assert(i >= 0);
	for (int t = 0; t < table->assoc; ++t) {
		if (table->tag[t][i] == tag) {
			*value = table->value[t][i];
			table->lru[t][i] = table->assoc;

			for (int t = 0; t < table->assoc; ++t) {
				if (table->lru[t][i] > 0) {
					--table->lru[t][i];
				}
			}
			return true;
		}
	}
	return false;
}

void LRUinsert(LRU* table, int index, void* value) {
	uint64_t tag = RANGE(index,table->height,table->tag_width);
	assert(tag < 1 << table->tag_width);
	assert(tag >= 0);
	int i = index & MASK(table->height);
	assert(i < 1 << table->height);
	assert(i >= 0);
	int ins = 0;

	for (int t = 0; t < table->assoc; ++t) {
		if (table->tag[t][i] == tag) {
			ins = t;
			break;
		}
		if (table->lru[t][i] == 0) {
			ins = t;
		}
	}

	memcpy(table->value[ins][i], value, table->entrysize);
	table->tag[ins][i] = tag;
	table->lru[ins][i] = table->assoc;

	for (int t = 0; t < table->assoc; ++t) {
		if (table->lru[t][i] > 0) {
			--table->lru[t][i];
		}
	}
	return;
}
typedef LRU SA;
SA SAinit(uint8_t tag_width, uint8_t assoc, uint8_t height_bit) {
	SA table = LRUinit(tag_width, assoc, height_bit, sizeof(uint64_t));
	return table;
}

bool SAfind(SA* table, int index, uint64_t* value) {
	void* r;
	bool find = LRUfind(table, index, &r);
	if (find) {
		*value = *((uint64_t*) r);
	} else {
		*value = 0;
	}
	return find;
}

void SAupdate(SA* table, int index, uint64_t value) {
	LRUinsert(table, index, (void*) &value);
	return;
}


//////////////////////////////////////////////////
//////////////////////////////////////////////////
/////////  utilities for the simulator   /////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////

#define forWQ(ch,el) for(request_t* el=write_queue_head[ch];el;el=el->next)
#define forRQ(ch,el) for(request_t* el=read_queue_head[ch];el;el=el->next)
#define forCh(L) for(int32_t L = 0; L < NUM_CHANNELS; ++L)
#define forRank(L) for(int32_t L = 0; L < NUM_RANKS; ++L)
#define forBank(L) for(int32_t L = 0; L < NUM_BANKS; ++L)
#define forThread(L) for(int32_t L = 0; L < NUMCORES; ++L)
#define M(REQ) int t = REQ->thread_id, c = REQ->dram_addr.channel, r = REQ->dram_addr.rank, b = REQ->dram_addr.bank, row = REQ->dram_addr.row;
#define SameCRBR(x,y) (x.channel == y.channel &&x.rank == y.rank && x.bank == y.bank && x.row == y.row)
#define SameRow(x,y) (x.rank == y.rank && x.bank == y.bank && x.row == y.row)
#define isPowerDownSlow(c,r) (dram_state[c][r][0].state == PRECHARGE_POWER_DOWN_SLOW)
#define isPowerDownFast(c,r) ((dram_state[c][r][0].state == PRECHARGE_POWER_DOWN_FAST) || (dram_state[c][r][0].state == ACTIVE_POWER_DOWN))
#define isPowerDown(c,r) (isPowerDownSlow(c,r) || isPowerDownFast(c,r))
#define forRB(R,B) forRank(R) forBank(B)
#define forCRB(C,R,B) forCh(C) forRank(R) forBank(B)

#define RANK(r) (r->dram_addr.rank)
#define BANK(r) (r->dram_addr.bank)
#define ROW(r) (r->dram_addr.row)

bool isRowHit(request_t*req) {
	return req->dram_addr.row
			== dram_state[req->dram_addr.channel][req->dram_addr.rank][req->dram_addr.bank].active_row;
}

bool isSameRow(request_t*req, request_t*req2) {
	return req != req2 && req->dram_addr.channel == req2->dram_addr.channel && req->dram_addr.row
			== req2->dram_addr.row && req->dram_addr.bank == req2->dram_addr.bank && req->dram_addr.rank
			== req2->dram_addr.rank;
}
bool isSameBank(request_t*req, request_t*req2) {
	return req != req2 && req->dram_addr.channel == req2->dram_addr.channel && req->dram_addr.bank
			== req2->dram_addr.bank && req->dram_addr.rank == req2->dram_addr.rank;
}
bool isSameRank(request_t*req, request_t*req2) {
	return req != req2 && req->dram_addr.channel == req2->dram_addr.channel && req->dram_addr.rank
			== req2->dram_addr.rank;
}

bool canAutoPre(request_t*req) {
	return (req->next_command == COL_READ_CMD || req->next_command == COL_WRITE_CMD) && is_autoprecharge_allowed(
			req->dram_addr.channel, req->dram_addr.rank, req->dram_addr.bank);
}

typedef struct {
	int32_t value;
} req_info;

req_info* get_req_info(request_t *req) {
	req_info *p = (req_info*) (req->user_ptr);
	return p;
}



//////////////////////////////////////////////////
//////////////////////////////////////////////////
/////////            IT-TAGE             /////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////


typedef uint64_t tag_t;
typedef uint64_t value_t;

typedef struct {
	value_t value;
	CTR counter;
} BLentry;

typedef struct {
	H* hist, *hist2; // per thread history
	FH** tagh, **tagh2;
	FH** idxh, **idxh2;

	CTR ** u;
	CTR ** conf;
	tag_t** tag;
	value_t** value;

	uint32_t* map; // table map
	uint32_t* height; // height of each tables
	uint8_t* height_bit; // height of each tables
	uint8_t* tag_width;
	uint32_t* hist_length;

	uint8_t tables; // number of tables
	uint8_t th; // number of threads

	CTR ALT;
	CTR TICK;

	uint64_t* t;//tag
	uint64_t* i;//index

	bool usealt, hit, althit;
	uint8_t predcomp, altcomp;
	value_t pred, altpred;

	uint64_t x, y, z, w;
	SA* blacklistFilter;
	bool use_blf;
} TAGE;

uint64_t TAGErand(TAGE*tage) {
	uint64_t t;

	t = tage->x ^ (tage->x << 11);
	tage->x = tage->y;
	tage->y = tage->z;
	tage->z = tage->w;
	return tage->w = (tage->w ^ (tage->w >> 19)) ^ (t ^ (t >> 8));
}

uint32_t stat_count[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
uint32_t stat_count2[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
uint8_t tage_table_map[] = { 0, 1, 1, 2, 2, 2, 3, 3 ,4, 4};
uint8_t tage_height_bit[] = { 12, 11, 11, 10, 10, 7, 7, 8 };
uint8_t tage_tag_width[] = { 10, 11, 13, 15, 17, 11, 13, 13 };
TAGE TAGEinit(uint8_t tables, uint8_t threads, uint32_t s, uint32_t e, const int32_t* hist_length, const int32_t* height_bit,const bool use_bl) {
	TAGE t;

	t.x = 123456789ull;
	t.y = 362436069ull;
	t.z = 521288629ull;
	t.w = 88675123ull;
	t.th = threads;
	t.tables = tables;

	t.map = malloc(sizeof(uint32_t) * t.tables);
	t.height = malloc(sizeof(uint32_t) * t.tables);
	t.height_bit = malloc(sizeof(uint8_t) * t.tables);
	t.tag_width = malloc(sizeof(uint8_t) * t.tables);
	t.hist_length = malloc(sizeof(uint32_t) * t.tables);

	if (hist_length == NULL) {
		t.hist_length[0] = 0;
/*		for (int i = 1; i < t.tables; ++i) {
			t.hist_length[i] = (int) ((double) s * pow(((double) e / (double) s),
					((double) (i - 1) / (double) (t.tables - 2))) + 0.5);
		}*/
	} else {
		for (int i = 0; i < t.tables; ++i) {
			if (e < hist_length[i]) {
				e = hist_length[i];
			}
			t.hist_length[i] = hist_length[i];
		}
	}
/*

	for (int i = 0; i < t.tables; ++i) {
		printf("%lu,", t.hist_length[i]);
	}
	puts("");
*/

	for (int i = 0; i < t.tables; ++i) {
		t.map[i] = tage_table_map[i];
		t.height[i] = 1 << height_bit[t.map[i]];
		t.height_bit[i] = height_bit[t.map[i]];
		t.tag_width[i] = tage_tag_width[t.map[i]];
	}

	t.hist = malloc(sizeof(H) * t.th);
	t.hist2 = malloc(sizeof(H) * t.th);
	t.idxh = allocAA(t.th, t.tables, sizeof(FH));
	t.tagh = allocAA(t.th, t.tables, sizeof(FH));
	t.idxh2 = allocAA(t.th, t.tables, sizeof(FH));
	t.tagh2 = allocAA(t.th, t.tables, sizeof(FH));

	for (int i = 0; i < t.th; ++i) {
		t.hist[i] = Hinit(e + 1);
		t.hist2[i] = Hinit(e + 1);
		for (int j = 0; j < t.tables; ++j) {
			//printf("%d\n",t.hist_length[j]);
			t.idxh[i][j] = FHinit(t.height_bit[j], min(t.hist_length[j],t.hist_length[j]), &t.hist[i]);
			t.tagh[i][j] = FHinit(t.tag_width[j], min(t.hist_length[j]-1,t.hist_length[j]-1), &t.hist[i]);

			t.idxh2[i][j] = FHinit(t.height_bit[j], min(t.hist_length[j],8), &t.hist2[i]);
			t.tagh2[i][j] = FHinit(t.tag_width[j], min(t.hist_length[j]-1,8-1), &t.hist2[i]);
		}
	}

	t.u = allocDAA(t.tables, t.height, sizeof(CTR));
	t.conf = allocDAA(t.tables, t.height, sizeof(CTR));

	t.tag = allocDAA(t.tables, t.height, sizeof(tag_t));
	t.value = allocDAA(t.tables, t.height, sizeof(value_t));

	for (int i = 0; i < t.tables; ++i) {
		for (int j = 0; j < t.height[i]; ++j) {
			t.u[i][j] = CTRinit(1, 0);
			t.conf[i][j] = CTRinit(2, 0);
			t.tag[i][j] = 0;
			t.value[i][j] = 0;
		}
	}

	t.TICK = CTRinit(8, 0);
	t.ALT = CTRinit(4, 1);

	t.t = malloc(sizeof(tag_t) * t.tables);
	t.i = malloc(sizeof(uint32_t) * t.tables);

	t.blacklistFilter = malloc(sizeof(LRU));
	*t.blacklistFilter = LRUinit(12, 16, 6, sizeof(BLentry));
	t.use_blf = use_bl;
	return t;
}

int TAGEbudget(TAGE*t) {
	int sum = 0,b = 0;
	puts(LINE);
	puts("TAGE budgets");
	puts(LINE);
	printf("prediction components\n");
	int c = -1;
	for (int i = 0; i < t->tables; ++i) {
		if(c != t->map[i]){
			c = t->map[i];
			b = (1 << t->height_bit[i]) * (1 + 2 + t->tag_width[i] +12);
			sum += b;
		}
	}
	printf("table total:%d\n",sum);
	b  = t->th * t->hist->length;
	printf("history 1:%d\n",b);
	sum += b;
	b = (t->use_blf)?LRUbudgets(t->blacklistFilter):0;
	printf("blacklist filter: %d\n",b);
	sum += b;
	printf("sum:%d\n",sum);
	puts(LINE);
	return sum;
}
void TAGEdelete(TAGE*t) {
	//under construction
	for (int i = 0; i < t->tables; ++i) {
		printf("%lu,%lu\n", stat_count[i], stat_count2[i]);
	}
}
bool TAGEeq(value_t a, value_t b) {
	return a == b;
}

bool TAGEhit(value_t a, value_t b) {
	if (a >= b * 0.5 - 1 && a <= b * 1.5 + 1) {
		return true;
	} else {
		return false;
	}
}

void __TAGEcalc(TAGE*t, uint8_t th, uint32_t ind) {
	for (int i = 0; i < t->tables; ++i) {

		t->i[i] = FHread(&t->idxh[th][i], FHread(&t->idxh2[th][i], ind, i), 0);
		if (false && i == 7) {
			printf("%d'", FHread(&t->idxh[th][i], 0, 0));
			printf("%d,", FHread(&t->idxh2[th][i], 0, 0));
			printf("%d,", FHread(&t->idxh[th][i], FHread(&t->idxh2[th][i], 0, 0), 0));
			printf("%d\n", FHread(&t->idxh[th][i], FHread(&t->idxh2[th][i], ind, i), 0));
		}
		assert(t->height[i] > t->i[i]);
		assert(t->i[i] >= 0);
		t->t[i] = FHread(&t->tagh[th][i], FHread(&t->tagh2[th][i], ind >> t->height_bit[i], i), 0);
		assert((1 << t->tag_width[i]) > t->t[i]);
		assert(t->t[i] >= 0);
	}
}

void __TAGElookup(TAGE*t, uint8_t th, uint32_t ind) {
	__TAGEcalc(t, th, ind);

	t->hit = false;
	t->usealt = false;
	t->althit = false;
	t->altcomp = 0;
	t->predcomp = 0;
	bool alt = false;

	for (int i = 0; i < t->tables; ++i) {
		if (t->tag[t->map[i]][t->i[i]] == t->t[i]) {//tag hit
			t->pred = t->value[t->map[i]][t->i[i]];
			t->hit = true;
			alt = CTRsatMin(&t->conf[t->map[i]][t->i[i]]);
			t->predcomp = i;
		}
	}

	if (alt) {
		for (int i = 1; i < t->predcomp; ++i) {
			if (t->tag[t->map[i]][t->i[i]] == t->t[i] && CTRsatMax(&t->conf[t->map[i]][t->i[i]])) {
				t->altpred = t->value[t->map[i]][t->i[i]];
				t->althit = true;
				t->altcomp = i;
			}
		}
	}

	if (CTRget(&t->ALT) >= 0 && t->althit) {//alt
		t->usealt = true;
	} else {
		t->usealt = false;
	}

}

bool TAGEfind(TAGE*t, uint8_t th, uint32_t ind, uint64_t*value, uint64_t*conf) {
	__TAGElookup(t, th, ind);

	BLentry*entry;
	bool blhit = t->use_blf && LRUfind(t->blacklistFilter, ind ^ (th << 5), (void*) &entry);
	if (blhit && CTRget(&entry->counter) > 7) {
		*value = entry->value;
		return true;
	}

	if (!t->hit) {//not found
		*value = 0;
		*conf = 0;
		return false;
	} else if (t->usealt) {
		//printf("usealt:%d\n", t->altcomp);
		*conf = CTRget(&t->conf[t->map[t->altcomp]][t->i[t->altcomp]]);
		*value = t->altpred;
	} else {
		//printf("usepred:%d\n", t->predcomp);
		*conf = CTRget(&t->conf[t->map[t->predcomp]][t->i[t->predcomp]]);
		*value = t->pred;
	}
	return true;
}

typedef bool(*tageHitFunk_t)(value_t, value_t);

void TAGEupdate(TAGE*t, uint8_t th, uint32_t ind, uint64_t value, uint64_t newvalue, tageHitFunk_t isHit) {
	__TAGElookup(t, th, ind);

	uint64_t pred = (t->usealt) ? t->altpred : t->pred;
	uint8_t comp = (t->usealt) ? t->altcomp : t->predcomp;

	//printf("%d,%d,%d\n",pred,t->usealt,comp);
	BLentry*entry;
	bool blhit = t->use_blf && LRUfind(t->blacklistFilter, ind ^ (th << 5), (void*) &entry);

	if (blhit) {
		//puts("bl hit");
		if (isHit(entry->value, value)) {
			CTRinc(&entry->counter);
			//printf("bl:%ld\n",CTRget(&entry->counter));
		}
		if (isHit(pred, value)) {
			CTRdec(&entry->counter);
		}
		entry->value = newvalue;
		if (CTRget(&entry->counter) > 7) {
			return;//filtered
		}
	}

	if (t->althit && t->altpred != t->pred) {
		if (isHit(t->altpred, value)) {
			CTRinc(&t->ALT);
		} else if (isHit(t->pred, value)) {
			CTRdec(&t->ALT);
		}
	}

	//printf("%d\t%d,%d\t%d\t%d\n",ind,value,pred,comp,isHit(pred, value));
	if (isHit(pred, value)) {// correct
		//if(t->usealt)printf("hit:%d,%d\n",comp,pred);
		CTRinc(&t->conf[t->map[comp]][t->i[comp]]);
		t->value[comp][t->i[comp]] = newvalue;
	} else {//mispred
		if (!blhit && t->use_blf) {
			BLentry ent;
			ent.value = value;
			ent.counter = CTRinit(4, 0);
			LRUinsert(t->blacklistFilter, ind ^ (th << 5), (void*) &ent);
		}
		//alloc
		bool resetu = false;
		//printf("miss:%d,%d\n",comp,pred);
		int T = 2; // try to allocate T entries
		int c = t->predcomp + 1;
		if(!t->hit){
			c = 0;
		}
		int rand = TAGErand(t);
		if (c < t->tables - 1 && (rand & 3) == 0) {
			c++;
		} else if (c < t->tables - 2 && (rand & 3) == 1) {
			c+=2;
		}
		for (; c < t->tables; ++c) {
			// try to alloc on comp c;
			if (CTRsatMin(&t->u[t->map[c]][t->i[c]])) {
				//allocate
				t->tag[t->map[c]][t->i[c]] = t->t[c];
				t->value[t->map[c]][t->i[c]] = value;
				CTRsetMin(&t->u[t->map[c]][t->i[c]]);
				CTRsetMin(&t->conf[t->map[c]][t->i[c]]);

				CTRdec(&t->TICK);
				stat_count2[c]++;
				T--;
				if (T <= 0)
					break;
				c += 1;
			} else {
				stat_count[c]++;
				CTRinc(&t->TICK);
				if (CTRsatMax(&t->TICK)) {
					resetu = true;
				}
			}
		}

		//reset u bit
		if (resetu) {
			CTRsetMin(&t->TICK);
			for (int i = 0; i < t->tables; ++i) {
				for (int j = 0; j < t->height[i]; ++j) {
					CTRsetMin(&t->u[t->map[i]][j]);
				}
			}
		}

		//conf
		if (isHit(t->pred, value)) {
			CTRinc(&t->conf[t->map[t->predcomp]][t->i[t->predcomp]]);
		} else if (!CTRsatMin(&t->conf[t->map[t->predcomp]][t->i[t->predcomp]])) {
			CTRdec(&t->conf[t->map[t->predcomp]][t->i[t->predcomp]]);
		} else {
			t->tag[t->map[t->predcomp]][t->i[t->predcomp]] = t->t[t->predcomp];
			t->value[t->map[t->predcomp]][t->i[t->predcomp]] = value;
			CTRsetMin(&t->u[t->map[t->predcomp]][t->i[t->predcomp]]);
			CTRsetMin(&t->conf[t->map[t->predcomp]][t->i[t->predcomp]]);
		}

	}

	if (!isHit(t->altpred, value) && isHit(t->pred, value)) {
		CTRinc(&t->u[t->map[t->predcomp]][t->i[t->predcomp]]);
	}
}

void TAGEupdateHist(TAGE*t, uint8_t th, bool h) {
	Hupdate(&t->hist[th], h);
	for (int j = 0; j < t->tables; ++j) {
		FHupdate(&t->idxh[th][j]);
		FHupdate(&t->tagh[th][j]);
	}
}
void TAGEupdateHist2(TAGE*t, uint8_t th, bool h) {
	Hupdate(&t->hist2[th], h);
	for (int j = 0; j < t->tables; ++j) {
		FHupdate(&t->idxh2[th][j]);
		FHupdate(&t->tagh2[th][j]);
	}
}

void TAGEFlushHist(TAGE*t, uint8_t th) {
	for (int i = 0; i < 5; ++i) {
		Hupdate(&t->hist[th], 0);
		for (int j = 0; j < t->tables; ++j) {
			FHupdate(&t->idxh[th][j]);
			FHupdate(&t->tagh[th][j]);
		}
	}
}
void TAGEFlushHist2(TAGE*t, uint8_t th) {
	for (int i = 0; i < 5; ++i) {
		Hupdate(&t->hist2[th], 0);
		for (int j = 0; j < t->tables; ++j) {
			FHupdate(&t->idxh2[th][j]);
			FHupdate(&t->tagh2[th][j]);
		}
	}
}

#define PREACTWIDTH 6
#define PREACTTH 28




CTR PREACT;
typedef struct {
	int32_t wr_drain;
	int32_t num_reqests_thread[MAX_NUM_THREADS];
} ChannelInfo;
ChannelInfo ch_info[MAX_NUM_CHANNELS];

typedef struct {
	int32_t weight;
	int32_t num_reqests;
	int32_t num_write;

	int32_t num_reqests_thread[MAX_NUM_THREADS];
	int64_t last_refresh;

	int64_t act_rec[4]; //for check FAW
	bool go_refreshing;

} RankInfo;
RankInfo rank_info[MAX_NUM_CHANNELS][MAX_NUM_RANKS];

int num_ACT(int ch, int r) {
	int c = 0;
	for (int a = 0; a < 4; ++a) {
		if (rank_info[ch][r].act_rec[a] >= CYCLE_VAL) {
			c++;
		}
	}
	return c;
}
int num_ACT_future(int ch, int r) {
	int c = 0;
	for (int a = 0; a < 4; ++a) {
		if (rank_info[ch][r].act_rec[a] >= CYCLE_VAL + T_RP) {
			c++;
		}
	}
	return c;
}

typedef struct {
	int32_t num_reqests;
	int32_t preact_row;
	bool preacted, t_preacted;
} BankInfo;
BankInfo bank_info[MAX_NUM_CHANNELS][MAX_NUM_RANKS][MAX_NUM_BANKS];

//--------------------for thread--------------------//
#define MAX_PRIO ((NUMCORES >= 8)?128:((NUMCORES >= 4)?64:32))
#define MAX_VALUE ((1<<12)-1)
typedef struct {
	int32_t num_reqests;
	int32_t value;

	//for value estimation
	//12 + 1 + 1 + 1 + 32+7+ 13= 67 bits
	int32_t estimated_value;
	bool estimated;
	uint32_t last_index;
	bool pcbit, membit;
	uint32_t last_id;
	int32_t step_count;

	//for preactivation prediction
	//1 +1 + 1 + 1 +  (2+1+3+17) + 32 = 59 bits
	uint32_t preact_last_index;
	uint32_t last_mem;
	bool preact_pcbit, preact_membit;
	dram_address_t preact_addr;
	bool preact_predicted;

	request_t* req; // latest
	request_t* oldest; //oldest pointer

	int32_t prio;//priority tickets

	int32_t traffic_light;
	int64_t cycle_counter;
} ThreadInfo;
ThreadInfo thread_info[MAX_NUM_THREADS];

//-------------------predictors---------------------//
TAGE value_est;
TAGE preact_pred;

//-----------------------------------------------//
#define row_counter_assoc 128
typedef struct {
	uint32_t rank; //4bit
	uint32_t bank; //4bit
	uint32_t row; //15-17 bit
	uint32_t value;
} FA;
FA WrowCounter[MAX_NUM_CHANNELS][row_counter_assoc];

uint32_t getWRowCount(int c, int r, int b, int row) {
	for (int i = 0; i < row_counter_assoc; ++i) {
		if (WrowCounter[c][i].rank == r && WrowCounter[c][i].bank == b && WrowCounter[c][i].row == row) {
			return WrowCounter[c][i].value;
		}
	}
	return 0;
}
bool is_there_W_for_bank(int c, int r, int b) {
	for (int i = 0; i < row_counter_assoc; ++i) {
		if (WrowCounter[c][i].rank == r && WrowCounter[c][i].bank == b && WrowCounter[c][i].value > 0) {
			return true;
		}
	}
	return false;
}

uint32_t getWRowHitCount(int c, int r, int b) {
	int row = dram_state[c][r][b].active_row;
	return getWRowCount(c, r, b, row);
}
uint32_t getRRowHitCount(int c, int r, int b) {
	int hit = 0;
	forRQ(c,req) {
		if (RANK(req) == r && BANK(req) == b && isRowHit(req)) {
			hit++;
		}
	}
	return hit;
}

//--------------------for cmd--------------------//

void issue_pre(int c, int r, int b) {
	bank_info[c][r][b].preacted = false;
	bank_info[c][r][b].t_preacted = false;
}

void issue_act(int c, int r, int b, int row) {
	for (int a = 0; a < 4; ++a) {
		if (rank_info[c][r].act_rec[a] < CYCLE_VAL) {
			rank_info[c][r].act_rec[a] = CYCLE_VAL + T_FAW;
			return;
		}
	}
	assert(false);
}

void issue_write(request_t*req) {
	M(req)


	switch (req->next_command) {
	case COL_WRITE_CMD:
		for (int i = 0; i < row_counter_assoc; ++i) {
			if (SameRow(WrowCounter[c][i],req->dram_addr)) {
				WrowCounter[c][i].value--;
				break;
			}
		}
		rank_info[c][r].num_write--;

		break;
	case ACT_CMD:
		issue_act(c, r, b, row);

		break;
	case PRE_CMD:
		issue_pre(c, r, b);
		break;
	default:
		assert(false);
	}

	issue_request_command(req);
}

void issue_read(request_t*req) {
	M(req)

	switch (req->next_command) {
	case COL_READ_CMD:
		ch_info[c].num_reqests_thread[t]--;
		rank_info[c][r].num_reqests--;
		rank_info[c][r].num_reqests_thread[t]--;
		bank_info[c][r][b].num_reqests--;
		thread_info[t].num_reqests--;
		int32_t v = get_req_info(req)->value;

		rank_info[c][r].weight -= v;
		thread_info[t].value -= v;

		if (thread_info[t].num_reqests == 0) {
			int d = thread_info[t].last_id - req->instruction_id;
			if (d < 0) {
				d += ROBSIZE;
			}
			thread_info[t].step_count = (ROBSIZE - d) / MAX_RETIRE;
		}

		if (thread_info[t].req == req) {
			thread_info[t].req = NULL;
		}

		if (thread_info[t].oldest == req) {
			thread_info[t].oldest = NULL;
			int32_t oldest = (req->instruction_id + ROBSIZE);
			forCh(channel)
				forRQ(channel,r) {
					if (r->thread_id == req->thread_id) {
						int32_t id = r->instruction_id;
						if (id < req->instruction_id) {
							id += ROBSIZE;
						}
						if (id < oldest) {
							oldest = id;
							thread_info[t].oldest = r;
						}
					}
				}
		}
		if (thread_info[t].prio > 0) {
			thread_info[t].prio--;
		}

		break;
	case ACT_CMD:
		issue_act(c, r, b, row);
		break;
	case PRE_CMD:
		issue_pre(c, r, b);
		break;
	default:
		assert(false);
		break;
	}

	issue_request_command(req);
}

void issue_refresh(int c, int r) {
	rank_info[c][r].last_refresh = CYCLE_VAL;
	issue_refresh_command(c, r);
}

//-------------------------------------------//

void add_new_write(request_t *req) {
	M(req)
	req->user_ptr = malloc(sizeof(req_info));
	get_req_info(req)->value = 0;

	rank_info[c][r].num_write++;
	int ins = -1;
	for (int i = 0; i < row_counter_assoc; ++i) {
		if (SameRow(WrowCounter[c][i],req->dram_addr)) {
			WrowCounter[c][i].value++;
			ins = -1;
			break;
		}
		if (ins == -1 && WrowCounter[c][i].value == 0) {
			ins = i;
		}
	}
	if (ins >= 0) {
		WrowCounter[c][ins].value = 1;
		WrowCounter[c][ins].rank = r;
		WrowCounter[c][ins].bank = b;
		WrowCounter[c][ins].row = row;
	}
}

void weight_estimater_update(request_t *req) {
	M(req)

	//calculate actual request weight for the prior read request.
	uint64_t duration = 0; //duration between the prior request and new one
	thread_info[t].step_count -= T_CAS + T_DATA_TRANS + PIPELINEDEPTH;
	if (thread_info[t].num_reqests > 0 || thread_info[t].step_count < 0) { //prior instruction is in rob
		if (req->instruction_id > thread_info[t].last_id) {
			duration = (req->instruction_id - thread_info[t].last_id) / MAX_RETIRE;
		} else {
			duration = (req->instruction_id + ROBSIZE - thread_info[t].last_id) / MAX_RETIRE;
		}
	} else {
		duration = thread_info[t].step_count;
	}
	thread_info[t].cycle_counter += duration;
	if (duration > MAX_VALUE) {
		duration = MAX_VALUE;
	} else {
		//duration = duration;
	}

	//value adjustment
	if (thread_info[t].req != NULL) { //last rpquest is not served yet
		int diff = duration - (get_req_info(thread_info[t].req)->value);
		rank_info[c][thread_info[t].req->dram_addr.rank].weight += diff;
		thread_info[t].value += diff;
		get_req_info(thread_info[t].req)->value = duration;
	}


	//estimate was correct
	if (TAGEhit(thread_info[t].estimated_value, duration)) {
		TAGEupdate(&value_est, t, thread_info[t].last_index, duration, (thread_info[t].estimated_value + duration) / 2,
				TAGEhit);
	} else {
		TAGEupdate(&value_est, t, thread_info[t].last_index, duration, duration, TAGEhit);
	}

	TAGEupdateHist(&value_est, t, thread_info[t].pcbit);
	TAGEupdateHist2(&value_est, t, thread_info[t].membit);

	thread_info[t].step_count = 0;
	thread_info[t].last_id = req->instruction_id;
	thread_info[t].last_index = (req->instruction_pc >> 2) ^ (req->instruction_pc >> 15);
	thread_info[t].pcbit = (req->instruction_pc >> 2) & 1;
	thread_info[t].membit = (req->dram_addr.row) & 1;

	uint64_t tagepred = 0, conf = 0;
	thread_info[t].estimated = TAGEfind(&value_est, t, thread_info[t].last_index, &tagepred, &conf);
	if (conf < 2) {
		thread_info[t].estimated = false;
	}
	if (!thread_info[t].estimated) {
		tagepred = 2;
	}

	thread_info[t].estimated_value = tagepred;
	get_req_info(req)->value = thread_info[t].estimated_value;
	rank_info[c][r].weight += thread_info[t].estimated_value;
}

uint64_t getaddress(dram_address_t add) {
	uint64_t a = 0;
	a += add.row;
	a += (add.bank) * NUM_ROWS;
	a += (add.rank) * NUM_ROWS * NUM_BANKS;
	a += (add.channel) * NUM_ROWS * NUM_BANKS * NUM_RANKS;
	return a;
}

dram_address_t parseaddress(uint64_t a) {
	dram_address_t addr;
	addr.channel = a / (NUM_ROWS * NUM_BANKS * NUM_RANKS);
	addr.rank = (a / (NUM_ROWS * NUM_BANKS)) % NUM_RANKS;
	addr.bank = (a / NUM_ROWS) % NUM_BANKS;
	addr.row = (a) % NUM_ROWS;
	addr.column = 0;
	addr.actual_address = 0;

	return addr;
}

void preact_predictor_update(request_t *req) {
	M(req)

	bool f = false;
	if (bank_info[c][r][b].preacted) {
		if (bank_info[c][r][b].preact_row == row) {
			CTRinc(&PREACT);
		} else {
			CTRdec(&PREACT);
			CTRdec(&PREACT);
			CTRdec(&PREACT);
		}
		f = true;
		bank_info[c][r][b].t_preacted = false;
		bank_info[c][r][b].preacted = false;
	}

	if (TAGEeq(getaddress(thread_info[t].preact_addr), getaddress(req->dram_addr))) {
		TAGEupdate(&preact_pred, t, thread_info[t].preact_last_index, getaddress(req->dram_addr),
				getaddress(req->dram_addr), TAGEeq);
	} else {
		TAGEupdate(&preact_pred, t, thread_info[t].preact_last_index, getaddress(req->dram_addr),
				getaddress(req->dram_addr), TAGEeq);
	}

	TAGEupdateHist(&preact_pred, t, thread_info[t].preact_membit & 1);
	TAGEupdateHist2(&preact_pred, t, thread_info[t].preact_pcbit & 1);
	thread_info[t].preact_last_index = (req->instruction_pc >> 2) ^ req->thread_id;
	uint64_t tagepred = 0, conf = 0;
	thread_info[t].preact_predicted = TAGEfind(&preact_pred, t, thread_info[t].preact_last_index, &tagepred, &conf);
	if (conf < 3) {
		thread_info[t].preact_predicted = false;
	}
	thread_info[t].preact_addr = parseaddress(tagepred);
	thread_info[t].preact_pcbit = (req->instruction_pc >> 2) & 1;
	thread_info[t].preact_membit = (req->dram_addr.row) & 1;

}

void add_new_read(request_t *req) {
	M(req)
	req->user_ptr = malloc(sizeof(req_info));
	get_req_info(req)->value = 0;

	preact_predictor_update(req);
	weight_estimater_update(req);

	thread_info[t].num_reqests++;
	thread_info[t].req = req;
	if (thread_info[t].oldest == NULL) {
		thread_info[t].oldest = req;
	}

	ch_info[c].num_reqests_thread[t]++;
	rank_info[c][r].num_reqests++;
	rank_info[c][r].num_reqests_thread[t]++;
	bank_info[c][r][b].num_reqests++;

}

//-------------------------------------------//
bool is_prio_thread(int t) {
	if (thread_info[t].prio > MAX_PRIO * 2 / 4) {
		return true;
	}

	if (thread_info[t].value >= MAX_VALUE || thread_info[t].estimated_value >= MAX_VALUE) {
		return true;
	}

	return false;
}

void step(int ch) {
	if (ch == 0) {
		if (CYCLE_VAL % (NUMCORES * T_DATA_TRANS * 3) == 0) {
			forThread(t) {
				if (thread_info[t].prio < MAX_PRIO) {
					thread_info[t].prio++;
				}
			}
		}
		//update duration counter
		forThread(t) {
			if (thread_info[t].num_reqests == 0) {
				thread_info[t].step_count += PROCESSOR_CLK_MULTIPLIER;

				if (thread_info[t].step_count > MAX_VALUE << 1) {
					thread_info[t].step_count = MAX_VALUE << 1;
				}
			}
			thread_info[t].traffic_light = 0;
		}
		//find stopping thread -> turn light red
		forCh(c) {
			forRank(r) {
				if ((forced_refresh_mode_on[c][r]) || rank_info[c][r].last_refresh + T_RFC > CYCLE_VAL) {
					forThread(t) {
						if (rank_info[c][r].num_reqests_thread[t] > 0) {
							thread_info[t].traffic_light = 1;
						}
					}
				}
			}
			if (ch_info[c].wr_drain == 1) {
				forThread(t) {
					if (ch_info[c].num_reqests_thread[t] > 0 && !is_prio_thread(t)) {
						thread_info[t].traffic_light = 1;
					}
				}
			}
		}
	}
}

//---------------------------------------------//
bool try_write(int ch) {
	request_t* req = NULL;
	int score = -1;
	forWQ(ch,r) {
		if (rank_info[ch][RANK(r)].go_refreshing) {
			continue;
		}
		if (r->command_issuable && isRowHit(r)) {
			int s = rank_info[ch][RANK(r)].num_write;

			if (s > score) {
				score = s;
				req = r;
			}
		}
	}
	if (req != NULL) {
		issue_write(req);
		if (canAutoPre(req)) { //precharge
			bool pre = is_there_W_for_bank(ch, RANK(req), BANK(req)) && num_ACT(ch, RANK(req)) < 3;
			forWQ(ch,r) {
				if (isSameRow(req, r)) {
					pre = false;
				}
			}
			if (pre) {
				issue_pre(ch, req->dram_addr.rank, req->dram_addr.bank);
				issue_autoprecharge(ch, req->dram_addr.rank, req->dram_addr.bank);
			}
		}
		return true; //row hit
	}

	return false;
}

bool try_write_act_pre(int ch) {
	int max_act = 0, max_pre = 0;
	int max_act_i = -1, max_pre_i = -1;
	for (int i = 0; i < row_counter_assoc; ++i) {
		int rank = WrowCounter[ch][i].rank;
		int bank = WrowCounter[ch][i].bank;
		int row = WrowCounter[ch][i].row;

		int s = WrowCounter[ch][i].value;
		if (rank_info[ch][rank].go_refreshing) {
			continue;
		}
		if (s > max_act && is_activate_allowed(ch, rank, bank)) {
			if (getWRowHitCount(ch, rank, bank) == 0) {
				if (!ch_info[ch].wr_drain && bank_info[ch][rank][bank].num_reqests > 0) {
					continue;
				}
				max_act = s;
				max_act_i = i;
			}
		}

		if (s > max_pre && ((is_precharge_allowed(ch, rank, bank) && dram_state[ch][rank][bank].active_row != row
				&& dram_state[ch][rank][bank].active_row != -1))) {
			if (getWRowHitCount(ch, rank, bank) == 0) {
				if (!ch_info[ch].wr_drain && bank_info[ch][rank][bank].num_reqests > 0) {
					continue;
				}
				max_pre = s;
				max_pre_i = i;
			}
		}

	}

	if (max_act_i >= 0) {
		//activate
		int rank = WrowCounter[ch][max_act_i].rank;
		int bank = WrowCounter[ch][max_act_i].bank;
		int row = WrowCounter[ch][max_act_i].row;
		if (is_activate_allowed(ch, rank, bank)) {
			issue_act(ch, rank, bank, row);
			issue_activate_command(ch, rank, bank, row);
		} else {
			assert(false);
		}
		return true;
	}

	if (max_pre_i >= 0) {
		//activate
		int rank = WrowCounter[ch][max_pre_i].rank;
		int bank = WrowCounter[ch][max_pre_i].bank;
		int row = WrowCounter[ch][max_pre_i].row;
		if (is_precharge_allowed(ch, rank, bank) && dram_state[ch][rank][bank].active_row != row
				&& dram_state[ch][rank][bank].active_row != -1) {
			issue_pre(ch, rank, bank);
			issue_precharge_command(ch, rank, bank);
		} else {
			assert(false);
		}
		return true;
	}

	return false;
}

int calc_read_score(request_t* req) {
	M(req)

	int32_t max_value_thread = 0, second_max = 1, max = 0;
	for (int i = 0; i < NUMCORES; ++i) {
		if (thread_info[i].value > max) {
			second_max = max;
			max_value_thread = i;
			max = thread_info[i].value;
		} else if (thread_info[i].value > second_max) {
			second_max = thread_info[i].value;
		}
	}

	int32_t v = 4;
	int64_t slow_thread = 0, s = 1ll << 62;

	for (int i = 0; i < NUMCORES; ++i) {
		if (thread_info[i].cycle_counter < s) {
			slow_thread = i;
			s = thread_info[i].cycle_counter;
		}
	}
	if(t == slow_thread){
		v += 1;
	}

	if (thread_info[t].traffic_light == 1) {
		v = 0;
	}


	if (t == max_value_thread) {
		v += 4;
	}


	if (thread_info[t].num_reqests == 1) {
		v += 2;
	}
	if (thread_info[t].oldest == req) {
		v += 1;
	}


	return v;
}

int calc_read_score2(request_t* req) {
	M(req)

	int32_t v = 1;

	if (thread_info[t].traffic_light == 1) {
		v = 0;
	}

	if (thread_info[t].num_reqests == 1) {
		v += 2;
	}
	if (thread_info[t].oldest == req) {
		v += 1;
	}

	return v;
}

bool prio_read(request_t* req) {
	M(req)
	return is_prio_thread(t);
}

bool try_prio_read(int ch) {
	request_t* req = NULL;
	int score = -1;

	forRQ(ch,r) {
		int s = calc_read_score2(r);
		if (prio_read(r) && r->command_issuable && isRowHit(r)) {
			if (s > score) {
				score = s;
				req = r;
			}
		}
	}
	if (req != NULL) {
		issue_read(req);
		return true;
	}
	return false;
}
bool try_prio_act_pre(int ch) {
	request_t *pre_r = NULL, *act_r = NULL;
	int prescore = -1, actscore = -1;

	forRQ(ch,r)
		if (prio_read(r)) {
			int s = calc_read_score2(r);
			switch (r->next_command) {
			case COL_READ_CMD:
				break;
			case ACT_CMD:
				if (r->command_issuable) {
					if (s > actscore) {
						actscore = s;
						act_r = r;
					}
				}
				break;
			case PRE_CMD:
				if (r->command_issuable) {
					if (s > prescore) {
						prescore = s;
						pre_r = r;
					}
				}
				break;
			default:
				assert(false);
				break;
			}
		}

	if (actscore >= prescore && act_r) {
		issue_read(act_r);
		return true;
	} else if (pre_r) {
		issue_read(pre_r);
		return true;
	}

	return false;
}
bool try_read(int ch) {
	request_t* req = NULL, *pre_r = NULL, *act_r = NULL, *ref_ptr = NULL;
	int score = -1, prescore = -1, actscore = -1, refscore = -1;

	forRQ(ch,r) {
		int s = calc_read_score(r);
		if (rank_info[ch][RANK(r)].go_refreshing) {
			if (refscore < s && r->command_issuable && isRowHit(r)) {
				refscore = s;
				ref_ptr = r;
			}
			continue;
		}
		switch (r->next_command) {
		case COL_READ_CMD:
			if (r->command_issuable) {
				if (s > score) {
					score = s;
					req = r;
				}
			}
			break;
		case ACT_CMD:
			if (r->command_issuable) {
				if (s > actscore) {
					actscore = s;
					act_r = r;
				}
			}
			break;
		case PRE_CMD:
			if (r->command_issuable) {
				if (s > prescore) {
					prescore = s;
					pre_r = r;
				}
			}
			break;
		default:
			assert(false);
			break;
		}
	}

	//refresh rank
	if (refscore > score) {
		req = ref_ptr;
		score = refscore;
	}

	if (req != NULL) {
		issue_read(req);
		//try auto precharge
		if (canAutoPre(req)) {
			bool pre = (pre_r != NULL) && isSameBank(pre_r, req);
			pre |= bank_info[ch][RANK(req)][BANK(req)].num_reqests > 0 && num_ACT(ch, RANK(req)) < 3;
			forRQ(ch,ptr) {
				if (isSameRow(req, ptr)) {
					pre = false;
				}
			}
			forThread(t) {
				if (bank_info[ch][RANK(req)][BANK(req)].num_reqests == 0 && thread_info[t].preact_predicted
						&& SameCRBR(thread_info[t].preact_addr,req->dram_addr)) {
					pre = false;
				}
			}
			if (pre) {
				issue_pre(ch, req->dram_addr.rank, req->dram_addr.bank);
				issue_autoprecharge(ch, req->dram_addr.rank, req->dram_addr.bank);
			}
		}
		return true;
	}

	return false;
}

bool try_read_act_pre(int ch) {
	request_t *pre_r = NULL, *act_r = NULL;
	int prescore = -1, actscore = -1;

	forRQ(ch,r) {
		int s = calc_read_score(r);
		if (rank_info[ch][RANK(r)].go_refreshing) {
			continue;
		}
		switch (r->next_command) {
		case COL_READ_CMD:
			break;
		case ACT_CMD:
			if (r->command_issuable) {
				if (s > actscore) {
					actscore = s;
					act_r = r;
				}
			}
			break;
		case PRE_CMD:
			if (r->command_issuable) {
				if (s > prescore) {
					prescore = s;
					pre_r = r;
				}
			}
			break;
		default:
			assert(false);
			break;
		}
	}

	if (actscore >= prescore && act_r != NULL) {
		issue_read(act_r);
		return true;
	}
	if (pre_r != NULL) {
		issue_read(pre_r);
		return true;
	}

	return false;
}

bool try_precharge(int ch) {
	int rr = xor128() % NUM_RANKS;
	int rb = xor128() % NUM_BANKS;
	forRB(rank,bank)
		{
			int r = (rank + rr) % NUM_RANKS;
			int b = (bank + rb) % NUM_BANKS;
			if (!rank_info[ch][r].go_refreshing) {
				bool pre = bank_info[ch][r][b].num_reqests == 0 && !is_there_W_for_bank(ch, r, b) && num_ACT_future(ch,
						r) < 2 && !bank_info[ch][r][b].t_preacted;
				forThread(t) {
					if (thread_info[t].preact_predicted && thread_info[t].preact_addr.channel == ch
							&& thread_info[t].preact_addr.rank == r && thread_info[t].preact_addr.bank == b
							&& thread_info[t].preact_addr.row == dram_state[ch][r][b].active_row) {
						pre = false;
					}
				}

				if (pre && is_precharge_allowed(ch, r, b)) {
					issue_pre(ch, r, b);
					issue_precharge_command(ch, r, b);
					return true;
				}
			}
		}
	return false;
}

int32_t distance(int id_old, int id_new) {
	if (id_old == id_new) {
		return 0;
	} else if (id_old > id_new) {
		return id_new + ROBSIZE - id_old;
	} else {
		return id_new - id_old;
	}
}

//speculative activation
bool try_activate(int ch) {
	int p = MAX_VALUE;
	dram_address_t* cand = NULL;
	forThread(t) {
		//for this channel
		if (thread_info[t].preact_addr.channel != ch) {
			continue;
		}
		//no read for bank
		if (bank_info[ch][thread_info[t].preact_addr.rank][thread_info[t].preact_addr.bank].num_reqests > 0) {
			continue;
		}
		if (thread_info[t].preact_predicted
				&& !bank_info[ch][thread_info[t].preact_addr.rank][thread_info[t].preact_addr.bank].preacted) {
			int est = thread_info[t].estimated_value;

			if (thread_info[t].oldest) {
				int d = distance(thread_info[t].oldest->instruction_id, thread_info[t].last_id);
				if (thread_info[t].estimated_value + d / MAX_RETIRE > ROBSIZE / MAX_RETIRE) {
					est += thread_info[t].num_reqests * 100;
				}
			}

			if (thread_info[t].num_reqests <= 2 && p > est) {
				if (num_ACT_future(ch, thread_info[t].preact_addr.rank) < 2) {
					if (is_activate_allowed(ch, thread_info[t].preact_addr.rank, thread_info[t].preact_addr.bank)) {
						cand = &thread_info[t].preact_addr;
						p = thread_info[t].estimated_value;
					}
				}
			}
		}

	}
	if (cand) {
		if (is_activate_allowed(ch, cand->rank, cand->bank)) {
			if (CTRget(&PREACT) >= PREACTTH) {
				issue_act(cand->channel, cand->rank, cand->bank, cand->row);
				issue_activate_command(cand->channel, cand->rank, cand->bank, cand->row);
				bank_info[cand->channel][cand->rank][cand->bank].t_preacted = true;
			}
			bank_info[cand->channel][cand->rank][cand->bank].preacted = true;
			bank_info[cand->channel][cand->rank][cand->bank].preact_row = cand->row;
		}
		return true;
	}

	return false;
}

request_t** ordering;
//--------------------sub functions-----------------------//
// insert new request
void checkNewRequest(int channel) {
	if (channel != 0) {
		return;
	}
	forCh(ch) {
		forWQ(ch,r) {
			if (r->user_ptr == NULL)
				add_new_write(r);
		}
	}

	int size = ROBSIZE;
	forThread(t) {
		for (int i = 0; i < size; ++i) {
			ordering[i] = NULL;
		}
		forCh(ch) {
			forRQ(ch,r) {
				if (r->user_ptr == NULL && r->thread_id == t) {
					//new;
					for (int i = 0; i < size; ++i) {
						if (ordering[i] == NULL) {
							ordering[i] = r;
							break;
						} else {
							int id = r->instruction_id;
							int id2 = ordering[i]->instruction_id;
							bool fl = false;
							if (id < id2 - MAX_FETCH * PROCESSOR_CLK_MULTIPLIER) {
								id2 -= ROBSIZE;
								fl = true;
							}
							if (id2 < id - MAX_FETCH * PROCESSOR_CLK_MULTIPLIER) {
								id -= ROBSIZE;
							}
							if (id < id2) {
								for (int j = size - 1; j > i; --j) {
									ordering[j] = ordering[j - 1];
								}
								ordering[i] = r;
								break;
							}
						}
					}
				}
			}
		}

		for (int i = 0; i < size; ++i) {
			if (ordering[i] != NULL) {
				add_new_read(ordering[i]);
			} else {
				break;
			}
		}
	}
}

void checkWriteLevel(int channel) {
	if (ch_info[channel].wr_drain) {
		if (write_queue_length[channel] > MID_WM) {
			forRank(r) {
				rank_info[channel][r].go_refreshing = false;
			}
			ch_info[channel].wr_drain = 1; // Keep draining.
		} else if (write_queue_length[channel] > LO_WM) {
			forThread(t) {
				if (is_prio_thread(t) && thread_info[t].num_reqests > 0) {
					if (thread_info[t].traffic_light != 1) {
						if (ch_info[channel].num_reqests_thread[t] > 0) {
							ch_info[channel].wr_drain = 0;
							return;
						}
					}
				}
			}

			//check if there are enough row locality in write requests.
			static const int th = 3;
			bool cont = false;
			for (int i = 0; i < row_counter_assoc; ++i) {
				if (WrowCounter[channel][i].value >= th) {
					cont = true;
				}
			}
			ch_info[channel].wr_drain = (cont) ? 2 : 0;

		} else {
			ch_info[channel].wr_drain = 0;
		}
	}

	if (ch_info[channel].wr_drain != 1) {
		//get into write drain mode.
		if (write_queue_length[channel] > HI_WM) {
			forRank(r) {
				rank_info[channel][r].go_refreshing = false;
			}
			ch_info[channel].wr_drain = 1;
		}
	}
}

void checkRefresh(int ch) {
	forRank(r) {
		if (forced_refresh_mode_on[ch][r]) {
			rank_info[ch][r].go_refreshing = false;
		}
	}

	if (CYCLE_VAL % (T_REFI / 2) >= PROCESSOR_CLK_MULTIPLIER) {
		return;
	}

	//now is the time to try refresh
	forRank(r) {
		int32_t num = 8 - num_issued_refreshes[ch][r];
		//rest time before force refreshing deadline.
		int64_t rest = (next_refresh_completion_deadline[ch][r] - CYCLE_VAL);

		if (rank_info[ch][r].go_refreshing) {
			continue;
		}
		if (num_issued_refreshes[ch][r] >= 8) {
			continue;
		}

		//high pace
		if (num * 2 < rest / T_REFI) {
			continue;
		}

		//this rank had better drain write than refresh
		if (ch_info[ch].wr_drain == 1) {
			continue;
		} else if (rank_info[ch][r].num_write > HI_WM / NUM_RANKS) {
			continue;
		}

		//have priority read requests
		bool f = false;
		forThread(t) {
			if (is_prio_thread(t) && rank_info[ch][r].num_reqests_thread[t] > 0) {
				f = true;
			}
		}
		if (f) {
			continue;
		}

		//otherwise
		rank_info[ch][r].go_refreshing = true;
	}
}

//--------------------main functions-----------------------//
void schedule(int channel) {
	checkNewRequest(channel);
	step(channel);
	checkWriteLevel(channel);
	checkRefresh(channel);

	//refresh
	if (ch_info[channel].wr_drain != 1) {
		int s = xor128() % NUM_RANKS;
		forRank(rank) {
			int r = (rank + s) % NUM_RANKS;
			if (rank_info[channel][r].go_refreshing) {
				if (is_refresh_allowed(channel, r)) {
					issue_refresh(channel, r);
					rank_info[channel][r].go_refreshing = false;
					return;
				}
			}
		}
	}

	//priority column read
	if (try_prio_read(channel)) {
		return;
	}

	//column write when the state is drain strong
	if (ch_info[channel].wr_drain) {
		if (try_write(channel)) {
			return;
		}
	}

	//priority activate and precharge
	if (try_prio_act_pre(channel)) {
		return;
	}

	//column read
	if (try_read(channel)) {
		return;
	}

	//column write
	if (try_write(channel)) {
		return;
	}

	//activate and precharge for write request when the state is write drain
	if (ch_info[channel].wr_drain) {
		if (try_write_act_pre(channel)) {
			return;
		}
	}

	//activate and precharge for read request
	if (try_read_act_pre(channel)) {
		return;
	}
	//activate and precharge for write request
	if (try_write_act_pre(channel)) {
		return;
	}

	//precharge not refereed row
	if (try_precharge(channel)) {
		return;
	}
	//speculative activation
//	if (try_activate(channel)) {
//		return;
//	}
}

void init_scheduler_vars() {

	PREACT = CTRinit(PREACTWIDTH, 1);
	CTRsetMin(&PREACT);

	forThread(t) {
		thread_info[t].prio = MAX_PRIO - 1;
	}
	forCh(c) {
		ch_info[c].wr_drain = 0;

		forRank(r) {
			rank_info[c][r].go_refreshing = false;
			rank_info[c][r].last_refresh = -T_RFC;
		}
	}

	//initialize TAGE predictor
	const static int32_t t_hist_length[] = { 0, 0, 1, 2, 3, 5, 9, 16, 32, 64 };
	const static int32_t t_height_bit[] = { 12, 11, 11, 10, 10 };
	value_est = TAGEinit(10, MAX_NUM_THREADS, 1, 32, t_hist_length, t_height_bit, true);
	const static int32_t t_hist_length2[] = { 0, 0, 1, 2, 3, 5, 16, 32 };
	const static int32_t t_height_bit2[] = { 10, 10, 10, 10 };
	preact_pred = TAGEinit(8, MAX_NUM_THREADS, 1, 32, t_hist_length2, t_height_bit2, false);

	ordering = malloc(sizeof(request_t*) * ROBSIZE);
}

void scheduler_stats() {
}

