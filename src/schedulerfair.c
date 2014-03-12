#include <stdio.h>
#include "utlist.h"
#include "utils.h"

#include "memory_controller.h"

#define MAX_THREADS 100
#define MAX_CREDITS 1024

extern long long int CYCLE_VAL;

// currency used to arbitrate data bus usage between threads 
int dbus_credits[MAX_NUM_CHANNELS][MAX_THREADS];

// used to make sure each thread gets one dbus_credit per cycle
long long int last_cycle_credited[MAX_NUM_CHANNELS];

// fair scheduler stats
long long int count_col_read[MAX_NUM_CHANNELS][MAX_THREADS];
long long int credits_at_read[MAX_NUM_CHANNELS][MAX_THREADS];

void init_scheduler_vars()
{
  // initialize all scheduler variables here

  int i,j;
  for(i=0; i<MAX_NUM_CHANNELS; i++)
    {
      for(j=0; j<MAX_THREADS; j++)
	{
	  // all threads start out with maximum credits
	  dbus_credits[i][j] = MAX_CREDITS;
	  count_col_read[i][j] = 0;
	  credits_at_read[i][j] = 0;
	}
    }

  for(i=0; i<MAX_NUM_CHANNELS; i++)
    {
      last_cycle_credited[i] = CYCLE_VAL;
    }

  return;
}

// write queue high water mark; begin draining writes if write queue exceeds this value
#define HI_WM 40

// end write queue drain once write queue has this many writes in it
#define LO_WM 20

// when switching to write drain mode, write at least this many times before switching back to read mode
#define MIN_WRITES_ONCE_WRITING_HAS_BEGUN 1

// 1 means we are in write-drain mode for that channel
int drain_writes[MAX_NUM_CHANNELS];

// how many writes have been performed since beginning current write drain
int writes_done_this_drain[MAX_NUM_CHANNELS];

// flag saying that we're only draining the write queue because there are no reads to schedule
int draining_writes_due_to_rq_empty[MAX_NUM_CHANNELS];

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
  // increase all threads' credits by one credit per cycle of simulation
  long long int since_last_credit = CYCLE_VAL - last_cycle_credited[channel];
  last_cycle_credited[channel] = CYCLE_VAL;
  int i;
  for(i=0; i<MAX_THREADS; i++)
    {
      dbus_credits[channel][i] += (int)since_last_credit;

      // just to make sure that dbus_credits is in the right range even if last_cycle_credit makes it overflow
      if((dbus_credits[channel][i] < 0) || (dbus_credits[channel][i] > MAX_CREDITS))
        {
          dbus_credits[channel][i] = MAX_CREDITS;
        }
    }

  request_t * rd_ptr = NULL;
  request_t * wr_ptr = NULL;
  
  // begin write drain if we're above the high water mark
  if((write_queue_length[channel] > HI_WM) && (!drain_writes[channel]))
    {
      drain_writes[channel] = 1;
      writes_done_this_drain[channel] = 0;
    }

  // also begin write drain if read queue is empty
  if((read_queue_length[channel] < 1) && (write_queue_length[channel] > 0) && (!drain_writes[channel]))
    {
      drain_writes[channel] = 1;
      writes_done_this_drain[channel] = 0;
      draining_writes_due_to_rq_empty[channel] = 1;
    }

  // end write drain if we're below the low water mark
  if((drain_writes[channel]) && (write_queue_length[channel] <= LO_WM) && (!draining_writes_due_to_rq_empty[channel]))
    {
      drain_writes[channel] = 0;
    }

  // end write drain that was due to read_queue emptiness only if at least one write has completed
  if((drain_writes[channel]) && (read_queue_length[channel] > 0) && (draining_writes_due_to_rq_empty[channel]) && (writes_done_this_drain[channel] > MIN_WRITES_ONCE_WRITING_HAS_BEGUN))
    {
      drain_writes[channel] = 0;
      draining_writes_due_to_rq_empty[channel] = 0;
    }

  // make sure we don't try to drain writes if there aren't any
  if(write_queue_length[channel] == 0)
    {
      drain_writes[channel] = 0;
    }

  // drain from write queue now
  if(drain_writes[channel])
    {
      // prioritize open row hits
      LL_FOREACH(write_queue_head[channel], wr_ptr)
	{
	  // if COL_WRITE_CMD is the next command, then that means the appropriate row must already be open
	  if(wr_ptr->command_issuable && (wr_ptr->next_command == COL_WRITE_CMD))
	    {
	      writes_done_this_drain[channel]++;
	      issue_request_command(wr_ptr);
	      return;
	    }
	}

      // if no open rows, just issue any other available commands
      LL_FOREACH(write_queue_head[channel], wr_ptr)
	{
	  if(wr_ptr->command_issuable)
	    {
	      issue_request_command(wr_ptr);
	      return;
	    }
	}

      // nothing issuable this cycle
      return;
    }

  // do a read
      
  // find the thread with the most credits that has an issuable read
  int top_credits = -1;
  request_t *top_read = NULL;
  
  LL_FOREACH(read_queue_head[channel],rd_ptr)
    {
      if(rd_ptr->command_issuable)
	{
	  int current_credits = dbus_credits[channel][rd_ptr->thread_id];
	  
	  // if it's an open row hit, COL_READ_CMD will be the next command
	  if(rd_ptr->next_command == COL_READ_CMD)
	    {
	      // count this thread as having 50% more credits if it's an open-row hit
	      current_credits += current_credits/2;
	    }

	  // update the top credits seen so far	  
	  if(current_credits > top_credits)
	    {
	      top_credits = current_credits;
	      top_read = rd_ptr;
	    }
	}
    }

  // if an issuable read was found, issue it
  if(top_read != NULL)
    {
      // only decrease credits when it actually does a column read (i.e., the read actually happens)
      if(top_read->next_command == COL_READ_CMD)
	{
	  // update stats
	  count_col_read[channel][top_read->thread_id]++;
	  credits_at_read[channel][top_read->thread_id] += dbus_credits[channel][top_read->thread_id];

	  // update credits
	  dbus_credits[channel][top_read->thread_id] /= 2;
	}
      
      issue_request_command(top_read);

    }

  return;
}

void scheduler_stats()
{
  /* Nothing to print for now. */

  printf("Average number of credits when performing a COL_READ_CMD\n");
  for(int i=0; i<MAX_NUM_CHANNELS; i++)
    {
      if(count_col_read[i][0]==0)
	{
	  break;
	}

      printf("Channel %d\n", i);

      for(int j=0; j<MAX_THREADS; j++)
	{
	  if(count_col_read[i][j]==0)
	    {
	      break;
	    }
	  
	  printf("\tThread %d credits: %f\n", j, ((float)credits_at_read[i][j])/((float)count_col_read[i][j]));
	}
    }
}

