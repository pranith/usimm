#include <stdio.h>
#include "utlist.h"
#include "utils.h"

#include "memory_controller.h"
#include "params.h"

extern long long int CYCLE_VAL;
#define MAX_THREADS  64

double threshold_open;
long long accesses[MAX_NUM_CHANNELS][MAX_THREADS];
long int hits[MAX_NUM_CHANNELS][MAX_THREADS];

/* Keeping track of how many preemptive precharges are performed. */
long long int num_aggr_precharge = 0;

  void
init_scheduler_vars ()
{
  threshold_open = T_RP / (T_RP+T_RCD);
  // initialize all scheduler variables here

  for (int i = 0; i < MAX_NUM_CHANNELS; i++)
  {
    for (int j = 0; j < MAX_THREADS; j++)
    {
      hits[i][j] = 0;
      accesses[i][j] = 0;
    }
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

  void
schedule (int channel)
{
  request_t *rd_ptr = NULL;
  request_t *wr_ptr = NULL;


  // if in write drain mode, keep draining writes until the
  // write queue occupancy drops to LO_WM
  if (drain_writes[channel] && (write_queue_length[channel] > LO_WM))
  {
    drain_writes[channel] = 1;	// Keep draining.
  }
  else
  {
    drain_writes[channel] = 0;	// No need to drain.
  }

  // initiate write drain if either the write queue occupancy
  // has reached the HI_WM , OR, if there are no pending read
  // requests
  if (write_queue_length[channel] > HI_WM)
  {
    drain_writes[channel] = 1;
  }
  else
  {
    if (!read_queue_length[channel])
      drain_writes[channel] = 1;
  }


  // If in write drain mode, look through all the write queue
  // elements (already arranged in the order of arrival), and
  // issue the command for the first request that is ready
  if (drain_writes[channel])
  {
    // prioritize open row hits
    LL_FOREACH (write_queue_head[channel], wr_ptr)
    {
      // if COL_WRITE_CMD is the next command, then that means the appropriate row must already be open
      if (wr_ptr->command_issuable
          && (wr_ptr->next_command == COL_WRITE_CMD))
      {
        hits[channel][wr_ptr->thread_id]++;
        accesses[channel][wr_ptr->thread_id]++;
        issue_request_command (wr_ptr);

        // issue auto-precharge if possible
        if ((hits[channel][wr_ptr->thread_id] / accesses[channel][wr_ptr->thread_id]) >= threshold_open && 
            is_autoprecharge_allowed(channel, rd_ptr->dram_addr.rank, rd_ptr->dram_addr.bank))
          issue_autoprecharge(channel, wr_ptr->dram_addr.rank, wr_ptr->dram_addr.bank);
 
        return;
      }
    }

    LL_FOREACH (write_queue_head[channel], wr_ptr)
    {
      // if no open rows, just issue any other available commands
      if (wr_ptr->command_issuable)
      {
        issue_request_command (wr_ptr);
        accesses[channel][wr_ptr->thread_id]++;
        break;
      }
    }
  }

  // Draining Reads
  // look through the queue and find the first request whose
  // command can be issued in this cycle and issue it 
  // Simple FCFS 
  if (!drain_writes[channel])
    accesses[channel][wr_ptr->thread_id]++;
  {
    LL_FOREACH (read_queue_head[channel], rd_ptr)
    {
      // if COL_WRITE_CMD is the next command, then that means the appropriate row must already be open
      if (rd_ptr->command_issuable
          && (rd_ptr->next_command == COL_READ_CMD))
      {
        hits[channel][rd_ptr->thread_id]++;
        accesses[channel][rd_ptr->thread_id]++;
        issue_request_command (rd_ptr);
        // issue auto-precharge if possible
        if ((hits[channel][rd_ptr->thread_id] / accesses[channel][rd_ptr->thread_id]) >= threshold_open && 
            is_autoprecharge_allowed(channel, rd_ptr->dram_addr.rank, rd_ptr->dram_addr.bank))
          issue_autoprecharge(channel, rd_ptr->dram_addr.rank, rd_ptr->dram_addr.bank);

        return;
      }
    }

    LL_FOREACH (read_queue_head[channel], rd_ptr)
    {
      // no hits, so just issue other available commands
      if (rd_ptr->command_issuable)
      {
        issue_request_command (rd_ptr);
        accesses[channel][rd_ptr->thread_id]++;
        break;
      }
    }
  }

  // if no commands have been issued, check and issue precharge commands
  if (!command_issued_current_cycle[channel])
  {
    for (int i = 0; i < NUM_RANKS; i++)
    {
      for (int j = 0; j < NUM_BANKS; j++)
      {			/* For all banks on the channel.. */
        if (is_precharge_allowed (channel, i, j))
        {		/* See if precharge is doable. */
          if (issue_precharge_command (channel, i, j))
            num_aggr_precharge++;
        }
      }
    }
  }
}

  void
scheduler_stats ()
{
  printf ("Number of aggressive precharges: %lld\n", num_aggr_precharge);
}
