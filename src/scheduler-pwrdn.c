#include <stdio.h>
#include "utlist.h"
#include "utils.h"

#include "memory_controller.h"
#include "params.h"


/*  A simple FCFS scheduler with an aggressive power-down policy.
    In any cycle, if the memory controller is unable to fire a
    command to a channel, it instead fires a POWER_DOWN_FAST
    command.                                                   */


extern long long int CYCLE_VAL;

/* A variable to keep track of whether I've already fired
   a power down command to a rank.  */
long long int pwrdn[MAX_NUM_CHANNELS][MAX_NUM_RANKS];

/* A stat to keep track of how long a rank stays in power-down mode.
   This matches up quite closely with a rank's time spent in ACT_PDN. */
long long int timedn[MAX_NUM_CHANNELS][MAX_NUM_RANKS];

// keep track of idle cycles
long long int timeidle[MAX_NUM_CHANNELS][MAX_NUM_RANKS];

  void
init_scheduler_vars ()
{
  int i, j;
  // initialize all scheduler variables here
  for (i = 0; i < MAX_NUM_CHANNELS; i++)
  {
    for (j = 0; j < MAX_NUM_RANKS; j++)
    {
      /* Initializing pwrdn and timedn arrays. */
      pwrdn[i][j] = 0;
      timedn[i][j] = 0;
      timeidle[i][j] = 0;
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
  int i;


  /* Since a refresh operation wakes a rank, mark the ranks as not being in power-dn mode. */
  if ((CYCLE_VAL % (8 * T_REFI)) == 0)
  {
    // printf("All banks powered up after refresh %lld.\n",CYCLE_VAL);
    for (i = 0; i < NUM_RANKS; i++)
    {
      if (pwrdn[channel][i])
      {
        timedn[channel][i] =
          timedn[channel][i] + CYCLE_VAL - pwrdn[channel][i];
        pwrdn[channel][i] = 0;
        timeidle[channel][i] = 0;
      }
    }
  }


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

    LL_FOREACH (write_queue_head[channel], wr_ptr)
    {
      if (wr_ptr->command_issuable)
      {
        if (issue_request_command(wr_ptr))
        {
          /* If the command was successful, mark that the rank has now been woken up.  Just book-keeping being done. */
          if (pwrdn[wr_ptr->dram_addr.channel][wr_ptr->dram_addr.rank])
          {
            timedn[wr_ptr->dram_addr.channel][wr_ptr->dram_addr.rank] = 
              timedn[wr_ptr->dram_addr.channel][wr_ptr->dram_addr.rank] + 
              CYCLE_VAL - pwrdn[wr_ptr->dram_addr.channel][wr_ptr->dram_addr.rank];

            pwrdn[wr_ptr->dram_addr.channel][wr_ptr->dram_addr.rank] = 0;
            // printf("Powering up c%d r%d in cycle %lld\n", wr_ptr->dram_addr.channel, wr_ptr->dram_addr.rank, CYCLE_VAL);
          }
          timeidle[wr_ptr->dram_addr.channel][wr_ptr->dram_addr.rank] = 0;
        }

        break;
      }
    }
    /* If you were unable to drain any writes this cycle, go ahead and try to power down. */
    for (i = 0; i < NUM_RANKS; i++)
    {
      if (!pwrdn[channel][i]) 
      {
        if (timeidle[channel][i] >= PWRN)
        {
          if (is_powerdown_fast_allowed (channel, i))
          {
            if (issue_powerdown_command (channel, i, PWR_DN_FAST_CMD))
            {
              pwrdn[channel][i] = CYCLE_VAL;
              timeidle[channel][i] = 0;
              // printf("Powered down c%d r%d in cycle %lld\n", channel, i, CYCLE_VAL);
              return;
            }
          }
        } 
        timeidle[channel][i]++;
      }
    }
    return;
  }

  // Draining Reads
  // look through the queue and find the first request whose
  // command can be issued in this cycle and issue it 
  // Simple FCFS 
  if (!drain_writes[channel])
  {
    LL_FOREACH (read_queue_head[channel], rd_ptr)
    {
      if (rd_ptr->command_issuable)
      {
        if (issue_request_command (rd_ptr))
        {
          /* If the command was successful, mark that the rank has now been woken up.  Just book-keeping being done. */
          if (pwrdn[rd_ptr->dram_addr.channel][rd_ptr->dram_addr.rank])
          {
            timedn[rd_ptr->dram_addr.channel][rd_ptr->dram_addr.rank] = 
              timedn[rd_ptr->dram_addr.channel][rd_ptr->dram_addr.rank] + 
              CYCLE_VAL - pwrdn[rd_ptr->dram_addr.channel][rd_ptr->dram_addr.rank];
            pwrdn[rd_ptr->dram_addr.channel][rd_ptr->dram_addr.rank] = 0;
            // printf("Powering up c%d r%d in cycle %lld\n", rd_ptr->dram_addr.channel, rd_ptr->dram_addr.rank, CYCLE_VAL);
          }
          timeidle[rd_ptr->dram_addr.channel][rd_ptr->dram_addr.rank] = 0;
        }
        break;
      }
    }
    /* If you were unable to issue any reads this cycle, go ahead and try to power down. */
    for (i = 0; i < NUM_RANKS; i++)
    {
      if (!pwrdn[channel][i]) 
      {
        if (timeidle[channel][i] >= PWRN) 
        {
          if (is_powerdown_fast_allowed (channel, i))
          {
            if (issue_powerdown_command (channel, i, PWR_DN_FAST_CMD))
            {
              pwrdn[channel][i] = CYCLE_VAL;
              timeidle[channel][i] = 0;
              // printf("Powered down c%d r%d in cycle %lld\n", channel, i, CYCLE_VAL);
              return;
            }
          }
        } 
        timeidle[channel][i]++;
      }
    }
    return;
  }
}

  void
scheduler_stats ()
{
  int i, j;
  printf ("Printing scheduler stats\n");
  for (i = 0; i < NUM_CHANNELS; i++)
  {
    for (j = 0; j < NUM_RANKS; j++)
    {
      printf ("Power down time c%d r%d  %lld\n", i, j, timedn[i][j]);
    }
  }
}
