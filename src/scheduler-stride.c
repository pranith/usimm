#include <stdio.h>
#include "utlist.h"
#include "utils.h"
#include "params.h"
#include <stdlib.h>

#include "memory_controller.h"

#define MAXGHBSIZE 512
#define MAXINDEXTABLE 1024


//function to convert physical to DRAM address
dram_address_t calc_dram_addr_copy(long long int physical_address)
{


	long long int input_a, temp_b, temp_a;

	int channelBitWidth = log_base2(NUM_CHANNELS);
	int rankBitWidth = log_base2(NUM_RANKS);
	int bankBitWidth = log_base2(NUM_BANKS);
	int rowBitWidth = log_base2(NUM_ROWS);
	int colBitWidth = log_base2(NUM_COLUMNS);
	int byteOffsetWidth = log_base2(CACHE_LINE_SIZE);



	dram_address_t  this_a ;

	this_a.actual_address = physical_address;

	input_a = physical_address;

	input_a = input_a >> byteOffsetWidth;		  // strip out the cache_offset


	if(ADDRESS_MAPPING == 1)
	{
		temp_b = input_a;				
		input_a = input_a >> colBitWidth;
		temp_a  = input_a << colBitWidth;
		this_a.column = temp_a ^ temp_b;		//strip out the column address
		

		temp_b = input_a;   				
		input_a = input_a >> channelBitWidth;
		temp_a  = input_a << channelBitWidth;
		this_a.channel = temp_a ^ temp_b; 		// strip out the channel address


		temp_b = input_a;				
		input_a = input_a >> bankBitWidth;
		temp_a  = input_a << bankBitWidth;
		this_a.bank = temp_a ^ temp_b;		// strip out the bank address 


		temp_b = input_a;			
		input_a = input_a >> rankBitWidth;
		temp_a  = input_a << rankBitWidth;
		this_a.rank = temp_a ^ temp_b;     		// strip out the rank address


		temp_b = input_a;		
		input_a = input_a >> rowBitWidth;
		temp_a  = input_a << rowBitWidth;
		this_a.row = temp_a ^ temp_b;		// strip out the row number
	}
	else
	{
		temp_b = input_a;   	
		input_a = input_a >> channelBitWidth;
		temp_a  = input_a << channelBitWidth;
		this_a.channel = temp_a ^ temp_b; 		// strip out the channel address


		temp_b = input_a;
		input_a = input_a >> bankBitWidth;
		temp_a  = input_a << bankBitWidth;
		this_a.bank = temp_a ^ temp_b;		// strip out the bank address 


		temp_b = input_a;
		input_a = input_a >> rankBitWidth;
		temp_a  = input_a << rankBitWidth;
		this_a.rank = temp_a ^ temp_b;     		// strip out the rank address


		temp_b = input_a;
		input_a = input_a >> colBitWidth;
		temp_a  = input_a << colBitWidth;
		this_a.column = temp_a ^ temp_b;		//strip out the column address


		temp_b = input_a;
		input_a = input_a >> rowBitWidth;
		temp_a  = input_a << rowBitWidth;
		this_a.row = temp_a ^ temp_b;			// strip out the row number
	}
	return(this_a);
}


//GHB variables (global)
int GHBhead;
int GHBmaxed;


struct GHBentry
{
	int number;
	int thread_id;
	int channel;
	int rank;
	int bank;
	long long int row;
	long long int instruction_pc;
	long long int physical_address;
	struct GHBentry * link;
};


//GHB
struct GHBentry GHB[MAXGHBSIZE];


struct ToBeIssued
{
	int issue;
	int rank;
	int bank;
	long long int row;
};

struct ToBeIssued tbi[MAX_NUM_CHANNELS];



struct StrideTableentry
{
	int laststride;
	long long int prev_address;	
	int detected;
}; 


//variables required for stats print
int number_of_spec_activates;
int number_of_hits;

int activates[MAX_NUM_CHANNELS][MAX_NUM_RANKS][MAX_NUM_BANKS];
		

//stride table
struct StrideTableentry ST[1024];
 


extern long long int CYCLE_VAL;

//previous read queue sizes
int prev_rqsize[MAX_NUM_CHANNELS];

//index table
struct GHBentry * IndexTable[MAXINDEXTABLE];

//ggenerate index for inex table
int generateindex (  int thread_id, long long int instruction_pc, long long int physical_address)
{
	long long int xorred;
	//printf("thread_id is %X, instr_pc is %llX, addr is %llX \n",thread_id, instruction_pc, physical_address);
	xorred = instruction_pc ^ physical_address;
	xorred = xorred & 0x00000000000000FF;
	thread_id = thread_id & 0x00000003;
	thread_id = thread_id << 8;
	//printf("index is %d %X\n",thread_id + (int)xorred);
	return (thread_id + (int)xorred);
}

//push entry into GHB
void push ( dram_address_t dram_addr,int thread_id, long long int instruction_pc, long long int physical_address )
{
	struct GHBentry * loop = NULL;
	int GHBnewhead = (GHBhead+1)%MAXGHBSIZE; //head incremented
	
	
	if(GHBhead == MAXGHBSIZE)
		GHBmaxed = 1;

	int index;
	
	if(GHBmaxed == 1)  //if GHB is maxed, then tail entry must be removed
	{
		index = generateindex(GHB[GHBnewhead].thread_id, GHB[GHBnewhead].instruction_pc, GHB[GHBnewhead].physical_address);
		loop = IndexTable[index];
		if(loop == &GHB[GHBnewhead])
		{
			IndexTable[index] = NULL;
		}
		else
		{

				if(loop->link == &GHB[GHBnewhead])
				{
					loop->link = NULL;
				
				}
			
		}
	}
	
	
	//insert GHB entry
	GHB[GHBnewhead].thread_id = thread_id;
	GHB[GHBnewhead].channel = dram_addr.channel;
	GHB[GHBnewhead].rank = dram_addr.rank;
	GHB[GHBnewhead].bank = dram_addr.bank;
	GHB[GHBnewhead].row = dram_addr.row;
	GHB[GHBnewhead].physical_address = physical_address;
	GHB[GHBnewhead].instruction_pc = instruction_pc;
	index = generateindex(thread_id, instruction_pc, physical_address);
	GHB[GHBnewhead].link = IndexTable[index];
	IndexTable[index] = &GHB[GHBnewhead];

	GHBhead = GHBnewhead;
	




}


void init_scheduler_vars()
{
	int i;
	// initialize all scheduler variables here
	for (i=0; i<MAX_NUM_CHANNELS;i++)
	{
		prev_rqsize[i] = 0;
	}
	number_of_spec_activates=0;
	number_of_hits=0;

	
	int o;
		int p;
		
		for(i=0;i<MAX_NUM_CHANNELS;i++){
		for (o=0; o<MAX_NUM_RANKS; o++) {
	  		for (p=0; p<MAX_NUM_BANKS; p++) {
	     			activates[i][o][p]=0;
					
	  		}
		}
		}
	GHBhead = -1;
	GHBmaxed = 0;
	for (i=0; i<MAXINDEXTABLE ; i++)
	{
		IndexTable[i] = NULL;
	}
	for (i=0; i<MAXGHBSIZE ; i++)
	{
		GHB[i].number = i;
	}

		for (i=0 ; i<MAX_NUM_CHANNELS ; i++)
	{
		tbi[i].issue = 0;
	}
	return;
}

// write queue high water mark; begin draining writes if write queue exceeds this value
#define HI_WM 40

// end write queue drain once write queue has this many writes in it
#define LO_WM 20

// 1 means we are in write-drain mode for that channel
int drain_writes[MAX_NUM_CHANNELS];

/* Each cycle it is possible to issue a valid command from the read or write queues
   OR
   a valid precharge command to any bank (issue_precharge_command())
   OR 
   a valid precharge_all bank command to a rank (issue_all_bank_precharge_command())
   OR
   a power_down command (issue_powerdown_command()), programmed either for fast or slow exit mode
   OR
   a refresh command (issue_refresh_command())
   OR
   a power_up command (issue_powerup_command())
   OR
   an activate to a specific row (issue_activate_command()).

   If a COL-RD or COL-WR is picked for issue, the scheduler also has the
   option to issue an auto-precharge in this cycle (issue_autoprecharge()).

   Before issuing a command it is important to check if it is issuable. For the RD/WR queue resident commands, checking the "command_issuable" flag is necessary. To check if the other commands (mentioned above) can be issued, it is important to check one of the following functions: is_precharge_allowed, is_all_bank_precharge_allowed, is_powerdown_fast_allowed, is_powerdown_slow_allowed, is_powerup_allowed, is_refresh_allowed, is_autoprecharge_allowed, is_activate_allowed.
   */

void schedule(int channel)
{
	request_t * rd_ptr = NULL;
	request_t * wr_ptr = NULL;
	request_t * updater = NULL;
	int i=0;
	
	//find position in read queue where new entries start
	LL_FOREACH(read_queue_head[channel], updater)
	{
		if(i == prev_rqsize[channel])
			break;
		else
			i++;			
	}

	
	//push new entries into the GHB
	for(;updater;updater = updater->next)
	{
		
		push(updater->dram_addr,updater->thread_id, updater->instruction_pc, updater->physical_address);				
	
	} 
	prev_rqsize[channel] = read_queue_length[channel];


	// if in write drain mode, keep draining writes until the
	// write queue occupancy drops to LO_WM
	if (drain_writes[channel] && (write_queue_length[channel] > LO_WM)) {
	  drain_writes[channel] = 1; // Keep draining.
	}
	else {
	  drain_writes[channel] = 0; // No need to drain.
	}

	// initiate write drain if either the write queue occupancy
	// has reached the HI_WM , OR, if there are no pending read
	// requests
	if(write_queue_length[channel] > HI_WM)
	{
		drain_writes[channel] = 1;
	}
	else {
	  if (!read_queue_length[channel])
	    drain_writes[channel] = 1;
	}
	
	int j;
	
	int o;
		int p;
		
		int Isused[MAX_NUM_RANKS][MAX_NUM_BANKS];
		for (o=0; o<MAX_NUM_RANKS; o++) {
	  		for (p=0; p<MAX_NUM_BANKS; p++) {
	     			Isused[o][p]=0;
					
	  		}
		}
	if(drain_writes[channel])
	{
		
		//go through write queue
		LL_FOREACH(write_queue_head[channel], wr_ptr)
		{
			//label isused accordingly
			if(Isused[wr_ptr->dram_addr.rank][wr_ptr->dram_addr.bank]==0)
				Isused[wr_ptr->dram_addr.rank][wr_ptr->dram_addr.bank]=2;
			
			//initialise stride table
			ST[wr_ptr->instruction_pc%1024].laststride = 0;
			ST[wr_ptr->instruction_pc%1024].prev_address = 0;
			ST[wr_ptr->instruction_pc%1024].detected = 0;
			
			//first ready served
			if(wr_ptr->command_issuable && wr_ptr->next_command == COL_WRITE_CMD)
			{
				if(activates[channel][wr_ptr->dram_addr.rank][wr_ptr->dram_addr.bank] == 1)
					number_of_hits ++;
				issue_request_command(wr_ptr);
				tbi[channel].issue = 0;
				return;
			}
			
		}
		LL_FOREACH(write_queue_head[channel], wr_ptr)
		{
			
			//detect strides
			if(ST[wr_ptr->instruction_pc%1024].laststride == 0 && ST[wr_ptr->instruction_pc%1024].prev_address == 0)
			{
				ST[wr_ptr->instruction_pc%1024].prev_address = wr_ptr->physical_address;
			}
			else if ( ST[wr_ptr->instruction_pc%1024].laststride == 0 )
			{
				ST[wr_ptr->instruction_pc%1024].laststride = wr_ptr->physical_address - ST[wr_ptr->instruction_pc%1024].prev_address;
				ST[wr_ptr->instruction_pc%1024].prev_address = wr_ptr->physical_address;
			}
			else if (ST[wr_ptr->instruction_pc%1024].laststride == wr_ptr->physical_address - ST[wr_ptr->instruction_pc%1024].prev_address)
			{
				ST[wr_ptr->instruction_pc%1024].detected = 1;
				ST[wr_ptr->instruction_pc%1024].prev_address = wr_ptr->physical_address;	
			}

			if(wr_ptr->command_issuable)
			{
				if(wr_ptr->next_command == PRE_CMD)
					activates[channel][wr_ptr->dram_addr.rank][wr_ptr->dram_addr.bank] = 0 ;
				issue_request_command(wr_ptr);
				tbi[channel].issue = 0;
				return;
			}
			
		}
	
		
		

		
		
		
	}
	
	
		

		LL_FOREACH(read_queue_head[channel],rd_ptr)
		{
			ST[rd_ptr->instruction_pc%1024].laststride = 0;
			ST[rd_ptr->instruction_pc%1024].prev_address = 0;
			ST[rd_ptr->instruction_pc%1024].detected = 0;

			
			
			
			if(Isused[rd_ptr->dram_addr.rank][rd_ptr->dram_addr.bank]==0)
				Isused[rd_ptr->dram_addr.rank][rd_ptr->dram_addr.bank]=1;
			else if (Isused[rd_ptr->dram_addr.rank][rd_ptr->dram_addr.bank]==2)
				Isused[rd_ptr->dram_addr.rank][rd_ptr->dram_addr.bank]=3;


			if(rd_ptr->command_issuable && rd_ptr->next_command == COL_READ_CMD && !drain_writes[channel] && Isused[rd_ptr->dram_addr.rank][rd_ptr->dram_addr.bank]<2)
			{
				if(activates[channel][rd_ptr->dram_addr.rank][rd_ptr->dram_addr.bank] == 1)
					number_of_hits ++;
				issue_request_command(rd_ptr);
				prev_rqsize[channel] = prev_rqsize[channel] - 1;
				tbi[channel].issue = 0;
				return;
			}
		}
		

		LL_FOREACH(read_queue_head[channel],rd_ptr)
		{
			if(ST[rd_ptr->instruction_pc%1024].laststride == 0 && ST[rd_ptr->instruction_pc%1024].prev_address == 0)
			{
				ST[rd_ptr->instruction_pc%1024].prev_address = rd_ptr->physical_address;
			}
			else if ( ST[rd_ptr->instruction_pc%1024].laststride == 0 )
			{
				ST[rd_ptr->instruction_pc%1024].laststride = rd_ptr->physical_address - ST[rd_ptr->instruction_pc%1024].prev_address;
				ST[rd_ptr->instruction_pc%1024].prev_address = rd_ptr->physical_address;
			}
			else if (ST[rd_ptr->instruction_pc%1024].laststride == rd_ptr->physical_address - ST[rd_ptr->instruction_pc%1024].prev_address)
			{
				ST[rd_ptr->instruction_pc%1024].detected = 1;
				ST[rd_ptr->instruction_pc%1024].prev_address = rd_ptr->physical_address;	
			}

			if(rd_ptr->command_issuable && Isused[rd_ptr->dram_addr.rank][rd_ptr->dram_addr.bank]<2)
			{
				if(rd_ptr->next_command == PRE_CMD)
					activates[channel][rd_ptr->dram_addr.rank][rd_ptr->dram_addr.bank] = 0 ;
				issue_request_command(rd_ptr);
				tbi[channel].issue = 0;
				return;
			}
		}
		//precharge from write queue
		LL_FOREACH(write_queue_head[channel], wr_ptr)
		{
			if(wr_ptr->command_issuable && wr_ptr->next_command == PRE_CMD && Isused[wr_ptr->dram_addr.rank][wr_ptr->dram_addr.bank]%2==0)
			{
				activates[channel][wr_ptr->dram_addr.rank][wr_ptr->dram_addr.bank] =0;
				issue_request_command(wr_ptr);
				tbi[channel].issue = 0;
				return;
			}
			
		}
		
		//try from stride detector
		if(drain_writes[channel])
		{
		LL_FOREACH(write_queue_head[channel], wr_ptr)
		{
			
			if(ST[wr_ptr->instruction_pc%1024].detected == 1)
			{
				for(j=1;j<7;j++)
				{
					long long int next_physical= ST[wr_ptr->instruction_pc%1024].prev_address + j*ST[wr_ptr->instruction_pc%1024].laststride;
					dram_address_t next_dram_addr=calc_dram_addr_copy(next_physical);
					dram_address_t prev_address = calc_dram_addr_copy(ST[wr_ptr->instruction_pc%1024].prev_address);
					if (next_dram_addr.channel==channel)
					{
						if((prev_address.rank!=next_dram_addr.rank)||(prev_address.bank!=next_dram_addr.bank)||(prev_address.row!=next_dram_addr.row))
						{ 
							if(Isused[next_dram_addr.rank][next_dram_addr.bank]==0  && dram_state[channel][next_dram_addr.rank][next_dram_addr.bank].state == PRECHARGING && is_activate_allowed(channel, next_dram_addr.rank, next_dram_addr.bank))
							{
								issue_activate_command(channel, next_dram_addr.rank, next_dram_addr.bank, next_dram_addr.row);
							//free(next_dram_addr);
								activates[channel][next_dram_addr.rank][next_dram_addr.bank] =1;
								number_of_spec_activates = number_of_spec_activates+1;
								tbi[channel].issue = 0;
								return;
							}
							if(Isused[next_dram_addr.rank][next_dram_addr.bank]==0  && dram_state[channel][next_dram_addr.rank][next_dram_addr.bank].state == ROW_ACTIVE && is_precharge_allowed(channel, next_dram_addr.rank, next_dram_addr.bank))
							{
								issue_precharge_command(channel, next_dram_addr.rank, next_dram_addr.bank);
								activates[channel][next_dram_addr.rank][next_dram_addr.bank] =0;
								
								tbi[channel].issue = 0;					
								//free(next_dram_addr);
								return;
							}	
						}
						
					}
					else
					{
						tbi[next_dram_addr.channel].issue = 1;
						tbi[next_dram_addr.channel].rank = next_dram_addr.rank;
						tbi[next_dram_addr.channel].bank = next_dram_addr.bank;
						tbi[next_dram_addr.channel].row = next_dram_addr.row;
	
					}
				}
				ST[wr_ptr->instruction_pc%1024].detected = 0;

			}
			
			
			
		}
		}
		
		LL_FOREACH(read_queue_head[channel], rd_ptr)
		{
			
			if(ST[rd_ptr->instruction_pc%1024].detected == 1)
			{
				for(j=1;j<7;j++)
				{
					long long int next_physical= ST[rd_ptr->instruction_pc%1024].prev_address + j*ST[rd_ptr->instruction_pc%1024].laststride;
					dram_address_t next_dram_addr=calc_dram_addr_copy(next_physical);
					dram_address_t prev_address = calc_dram_addr_copy(ST[rd_ptr->instruction_pc%1024].prev_address);
					if (next_dram_addr.channel==channel)
					{
						if((prev_address.rank!=next_dram_addr.rank)||(prev_address.bank!=next_dram_addr.bank)||(prev_address.row!=next_dram_addr.row))
						{ 
							if(Isused[next_dram_addr.rank][next_dram_addr.bank]==0 && dram_state[channel][next_dram_addr.rank][next_dram_addr.bank].state == PRECHARGING && is_activate_allowed(channel, next_dram_addr.rank, next_dram_addr.bank))
							{
								issue_activate_command(channel, next_dram_addr.rank, next_dram_addr.bank, next_dram_addr.row);
								activates[channel][next_dram_addr.rank][next_dram_addr.bank] =1;
								number_of_spec_activates = number_of_spec_activates+1;
								tbi[channel].issue = 0;
								return;
							}
							if(Isused[next_dram_addr.rank][next_dram_addr.bank]==0  && dram_state[channel][next_dram_addr.rank][next_dram_addr.bank].state == ROW_ACTIVE && is_precharge_allowed(channel, next_dram_addr.rank, next_dram_addr.bank))
							{
								issue_precharge_command(channel, next_dram_addr.rank, next_dram_addr.bank);
								tbi[channel].issue = 0;
								activates[channel][next_dram_addr.rank][next_dram_addr.bank] =0;
								
								return;
							}
						}
						
					}
				}
				ST[rd_ptr->instruction_pc%1024].detected = 0;

			}
		}

				
		//try from GHB

		int index;
		struct GHBentry * loop = NULL;
		struct GHBentry *  head = NULL;


		head = &GHB[GHBhead];

		int r;
		for(r=0;r<3;r++)
		{

		loop = head->link;
		head = head->link;

 
		if(loop!=NULL)
			index = ((loop->number)+1)%MAXGHBSIZE;
		else
			return;
		
		for(j=0;j<20;j++)
		{
			if(index == GHBhead)
				break;
			
			if(Isused[GHB[index].rank][GHB[index].bank] != 0 || channel != GHB[index].channel)
			{
				index = (index + 1)%MAXGHBSIZE;
				continue;
			}
			
			
			if (dram_state[GHB[index].channel][GHB[index].rank][GHB[index].bank].state == ROW_ACTIVE)
			{
				if(dram_state[GHB[index].channel][GHB[index].rank][GHB[index].bank].active_row != GHB[index].row)
				{
					if(is_precharge_allowed(GHB[index].channel,GHB[index].rank,GHB[index].bank))
					{
						issue_precharge_command(GHB[index].channel,GHB[index].rank,GHB[index].bank);
						
						activates[GHB[index].channel][GHB[index].rank][GHB[index].bank] = 0 ;
				
						tbi[channel].issue = 0;	
						return;
					}
				}
				
			}
			index = (index + 1)%MAXGHBSIZE;
		}
		if(tbi[channel].issue == 1)
		{
			if(!Isused[tbi[channel].rank][tbi[channel].bank] && dram_state[channel][tbi[channel].rank][tbi[channel].bank].state == PRECHARGING && is_activate_allowed(channel, tbi[channel].rank, tbi[channel].bank))
			{
				issue_activate_command(channel, tbi[channel].rank, tbi[channel].bank, tbi[channel].row);
				//free(next_dram_addr);
				activates[channel][tbi[channel].rank][tbi[channel].bank] =1;
				number_of_spec_activates = number_of_spec_activates+1;
								
				tbi[channel].issue = 0;
				return;
			}
			if(!Isused[tbi[channel].rank][tbi[channel].bank]  && dram_state[channel][tbi[channel].rank][tbi[channel].bank].state == ROW_ACTIVE && is_precharge_allowed(channel, tbi[channel].rank, tbi[channel].bank))
			{
				issue_precharge_command(channel, tbi[channel].rank, tbi[channel].bank);
				tbi[channel].issue = 0;
				activates[channel][tbi[channel].rank][tbi[channel].bank] =0;			
				//free(next_dram_addr);
				return;
			}
				
		}		

		}	
}

void scheduler_stats()
{
	printf("\nNumber of speculative activates = %d ", number_of_spec_activates);
	printf("\nNumber of row hits = %d ", number_of_hits);
	

  /* Nothing to print for now. */
}
