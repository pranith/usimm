
#ifndef __CONFIG_FILE_IN_H__
#define __CONFIG_FILE_IN_H__

#include "params.h"

#define 	EOL 	10
#define 	CR 	13
#define 	SPACE	32
#define		TAB	9

typedef enum {
	processor_clk_multiplier_token,
	robsize_token,
	max_retire_token,
	max_fetch_token,
	pipelinedepth_token,

	num_channels_token,
	num_ranks_token,
	num_banks_token,
	num_rows_token,
	num_columns_token,
	cache_line_size_token,
	address_bits_token,

	dram_clk_frequency_token,
	t_rcd_token,
	t_rp_token,
	t_cas_token,
	t_rc_token,
	t_ras_token,
	t_rrd_token,
	t_faw_token,
	t_wr_token,
	t_wtr_token,
	t_rtp_token,
	t_ccd_token,
	t_rfc_token,
	t_refi_token,
	t_cwd_token,
	t_rtrs_token,
	t_pd_min_token,
	t_xp_token,
	t_xp_dll_token,
	t_data_trans_token,

	vdd_token,
	idd0_token,
	idd2p0_token,
	idd2p1_token,
	idd2n_token,
	idd3p_token,
	idd3n_token,
	idd4r_token,
	idd4w_token,
	idd5_token,

	wq_capacity_token,
	address_mapping_token,
	wq_lookup_latency_token,

	comment_token,
	unknown_token
}token_t;


token_t tokenize(char * input){
  size_t length;
  length = strlen(input);
  if(strncmp(input, "//",2) == 0) {
	return comment_token;
  } else if (strncmp(input, "PROCESSOR_CLK_MULTIPLIER",length) == 0) {
	return processor_clk_multiplier_token;
  } else if (strncmp(input, "ROBSIZE",length) == 0) {
	return robsize_token;
  } else if (strncmp(input, "MAX_RETIRE",length) == 0) {
	return max_retire_token;
  } else if (strncmp(input, "MAX_FETCH",length) == 0) {
	return max_fetch_token;
  } else if (strncmp(input, "PIPELINEDEPTH",length) == 0) {
	return pipelinedepth_token;
  } else if (strncmp(input, "NUM_CHANNELS",length) == 0) {
	return num_channels_token;
  } else if (strncmp(input, "NUM_RANKS",length) == 0) { 
	return num_ranks_token;
  } else if (strncmp(input, "NUM_BANKS",length) == 0) {
	return num_banks_token;
  } else if (strncmp(input, "NUM_ROWS",length) == 0) {
	return num_rows_token;
  } else if (strncmp(input, "NUM_COLUMNS",length) == 0) {
	return num_columns_token;
  } else if (strncmp(input, "CACHE_LINE_SIZE",length) == 0) {
	return cache_line_size_token;
  } else if (strncmp(input, "ADDRESS_BITS",length) == 0) {
	return address_bits_token;
  } else if (strncmp(input, "DRAM_CLK_FREQUENCY",length) == 0) {
	return dram_clk_frequency_token;
  } else if (strncmp(input, "T_RC",length) == 0) {
	return t_rc_token;
  } else if (strncmp(input, "T_RP",length) == 0) {
    return t_rp_token;
  } else if (strncmp(input, "T_CAS",length) == 0) {
    return t_cas_token;
  } else if (strncmp(input, "T_RCD",length) == 0) {
    return t_rcd_token;
  } else if (strncmp(input, "T_RAS",length) == 0) {
    return t_ras_token;
  } else if (strncmp(input, "T_RRD",length) == 0) {
    return t_rrd_token;
  } else if (strncmp(input, "T_FAW",length) == 0) {
    return t_faw_token;
  } else if (strncmp(input, "T_WR",length) == 0) {
	return t_wr_token;
  } else if (strncmp(input, "T_WTR",length) == 0) {
	return t_wtr_token;
  } else if (strncmp(input, "T_RTP",length) == 0) {
	return t_rtp_token;
  } else if (strncmp(input, "T_CCD",length) == 0) {
    return t_ccd_token;
  } else if (strncmp(input, "T_RFC",length) == 0) {
	return t_rfc_token;
  } else if (strncmp(input, "T_REFI",length) == 0) {
	return t_refi_token;
  } else if (strncmp(input, "T_CWD",length) == 0) {
	return t_cwd_token;
  } else if (strncmp(input, "T_RTRS",length) == 0) {
	return t_rtrs_token;
  } else if (strncmp(input, "T_PD_MIN",length) == 0) {
	return t_pd_min_token;
  } else if (strncmp(input, "T_XP",length) == 0) {
	return t_xp_token;
  } else if (strncmp(input, "T_XP_DLL",length) == 0) {
	return t_xp_dll_token;
  } else if (strncmp(input, "T_DATA_TRANS",length) == 0) {
	return t_data_trans_token;
  } else if (strncmp(input, "VDD",length) == 0) {
	return vdd_token;
  } else if (strncmp(input, "IDD0",length) == 0) {
	return idd0_token;
  } else if (strncmp(input, "IDD2P0",length) == 0) {
	return idd2p0_token;
  } else if (strncmp(input, "IDD2P1",length) == 0) {
	return idd2p1_token;
  } else if (strncmp(input, "IDD2N",length) == 0) {
	return idd2n_token;
  } else if (strncmp(input, "IDD3P",length) == 0) {
	return idd3p_token;
  } else if (strncmp(input, "IDD3N",length) == 0) {
	return idd3n_token;
  } else if (strncmp(input, "IDD4R",length) == 0) {
	return idd4r_token;
  } else if (strncmp(input, "IDD4W",length) == 0) {
	return idd4w_token;
  } else if (strncmp(input, "IDD5",length) == 0) {
	return idd5_token;
  } else if (strncmp(input, "WQ_CAPACITY",length) == 0) {
	return wq_capacity_token;
  } else if (strncmp(input, "ADDRESS_MAPPING",length) == 0) {
	return address_mapping_token;
  } else if (strncmp(input, "WQ_LOOKUP_LATENCY",length) == 0) {
	return wq_lookup_latency_token;
  }

  else {
	printf("PANIC :Unknown token %s\n",input);
	return unknown_token;
  }
}


void read_config_file(FILE * fin)
{
	char 	c;
	char 	input_string[256];
	int	input_int;
	float   input_float;

	while ((c = fgetc(fin)) != EOF){
		if((c != EOL) && (c != CR) && (c != SPACE) && (c != TAB)){
			fscanf(fin,"%s",&input_string[1]);
			input_string[0] = c;
		} else {
			fscanf(fin,"%s",&input_string[0]);
		}
		token_t input_field = tokenize(&input_string[0]);

		switch(input_field)
		{
			case comment_token:
				while (((c = fgetc(fin)) != EOL) && (c != EOF)){
					/*comment, to be ignored */
				}
				break;

			case processor_clk_multiplier_token:
				fscanf(fin,"%d",&input_int);
				PROCESSOR_CLK_MULTIPLIER =  input_int;
				break;

			case robsize_token:
				fscanf(fin,"%d",&input_int);
				ROBSIZE =  input_int;
				break;

			case max_retire_token:
				fscanf(fin,"%d",&input_int);
				MAX_RETIRE =  input_int;
				break;

			case max_fetch_token:
				fscanf(fin,"%d",&input_int);
				MAX_FETCH =  input_int;
				break;

			case pipelinedepth_token:
				fscanf(fin,"%d",&input_int);
				PIPELINEDEPTH =  input_int;
				break;


			case num_channels_token:
				fscanf(fin,"%d",&input_int);
				NUM_CHANNELS =  input_int;
				break;


			case num_ranks_token:
				fscanf(fin,"%d",&input_int);
				NUM_RANKS =  input_int;
				break;

			case num_banks_token:
				fscanf(fin,"%d",&input_int);
				NUM_BANKS =  input_int;
				break;

			case num_rows_token:
				fscanf(fin,"%d",&input_int);
				NUM_ROWS =  input_int;
				break;

			case num_columns_token:
				fscanf(fin,"%d",&input_int);
				NUM_COLUMNS =  input_int;
				break;

			case cache_line_size_token:
				fscanf(fin,"%d",&input_int);
				CACHE_LINE_SIZE =  input_int;
				break;

			case address_bits_token:
				fscanf(fin,"%d",&input_int);
				ADDRESS_BITS =  input_int;
				break;

			case dram_clk_frequency_token:
				fscanf(fin,"%d",&input_int);
				DRAM_CLK_FREQUENCY =  input_int;
				break;

			case t_rcd_token:
				fscanf(fin,"%d",&input_int);
				T_RCD = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_rp_token:
				fscanf(fin,"%d",&input_int);
				T_RP = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_cas_token:
				fscanf(fin,"%d",&input_int);
				T_CAS = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_rc_token:
				fscanf(fin,"%d",&input_int);
				T_RC = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_ras_token:
				fscanf(fin,"%d",&input_int);
				T_RAS = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_rrd_token:
				fscanf(fin,"%d",&input_int);
				T_RRD = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_faw_token:
				fscanf(fin,"%d",&input_int);
				T_FAW = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_wr_token:
				fscanf(fin,"%d",&input_int);
				T_WR = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_wtr_token:
				fscanf(fin,"%d",&input_int);
				T_WTR = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_rtp_token:
				fscanf(fin,"%d",&input_int);
				T_RTP = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_ccd_token:
				fscanf(fin,"%d",&input_int);
				T_CCD = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_rfc_token:
				fscanf(fin,"%d",&input_int);
				T_RFC = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_refi_token:
				fscanf(fin,"%d",&input_int);
				T_REFI = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_cwd_token:
				fscanf(fin,"%d",&input_int);
				T_CWD = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_rtrs_token:
				fscanf(fin,"%d",&input_int);
				T_RTRS = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_pd_min_token:
				fscanf(fin,"%d",&input_int);
				T_PD_MIN = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_xp_token:
				fscanf(fin,"%d",&input_int);
				T_XP = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_xp_dll_token:
				fscanf(fin,"%d",&input_int);
				T_XP_DLL = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;

			case t_data_trans_token:
				fscanf(fin,"%d",&input_int);
				T_DATA_TRANS = input_int*PROCESSOR_CLK_MULTIPLIER;
				break;
			
			case vdd_token:
				fscanf(fin,"%f",&input_float);
				VDD = input_float;
				break;

			case idd0_token:
				fscanf(fin,"%f",&input_float);
				IDD0 = input_float;
				break;
			
			case idd2p0_token:
				fscanf(fin,"%f",&input_float);
				IDD2P0 = input_float;
				break;

			case idd2p1_token:
				fscanf(fin,"%f",&input_float);
				IDD2P1 = input_float;
				break;

			case idd2n_token:
				fscanf(fin,"%f",&input_float);
				IDD2N = input_float;
				break;

			case idd3p_token:
				fscanf(fin,"%f",&input_float);
				IDD3P = input_float;
				break;
			
			case idd3n_token:
				fscanf(fin,"%f",&input_float);
				IDD3N = input_float;
				break;

			case idd4r_token:
				fscanf(fin,"%f",&input_float);
				IDD4R = input_float;
				break;
			
			case idd4w_token:
				fscanf(fin,"%f",&input_float);
				IDD4W = input_float;
				break;
			
			case idd5_token:
				fscanf(fin,"%f",&input_float);
				IDD5 = input_float;
				break;

			case wq_capacity_token:
				fscanf(fin,"%d",&input_int);
				WQ_CAPACITY = input_int;
				break;

			case address_mapping_token:
				fscanf(fin,"%d",&input_int);
				ADDRESS_MAPPING= input_int;
				break;
			
			case wq_lookup_latency_token:
				fscanf(fin,"%d",&input_int);
				WQ_LOOKUP_LATENCY = input_int;
				break;

			case unknown_token:
			default:
				printf("PANIC: bad token in cfg file\n");
				break;

		}
	}
}


void print_params()
{
	printf("----------------------------------------------------------------------------------------\n");
	printf("------------------------\n");
	printf("- SIMULATOR PARAMETERS -\n");
	printf("------------------------\n");
	printf("\n-------------\n");
	printf("- PROCESSOR -\n");
	printf("-------------\n");
	printf("PROCESSOR_CLK_MULTIPLIER:   %6d\n", PROCESSOR_CLK_MULTIPLIER);
	printf("ROBSIZE:                    %6d\n", ROBSIZE);
	printf("MAX_FETCH:                  %6d\n", MAX_FETCH);
	printf("MAX_RETIRE:                 %6d\n", MAX_RETIRE);
	printf("PIPELINEDEPTH:              %6d\n", PIPELINEDEPTH);          

	printf("\n---------------\n");
	printf("- DRAM Config -\n");
	printf("---------------\n");
	printf("NUM_CHANNELS:               %6d\n", NUM_CHANNELS);
  printf("NUM_RANKS:                  %6d\n", NUM_RANKS);
  printf("NUM_BANKS:                  %6d\n", NUM_BANKS);
  printf("NUM_ROWS:                   %6d\n", NUM_ROWS);
  printf("NUM_COLUMNS:                %6d\n", NUM_COLUMNS);

	printf("\n---------------\n");
	printf("- DRAM Timing -\n");
	printf("---------------\n");
	printf("T_RCD:                      %6d\n", T_RCD);
  printf("T_RP:                       %6d\n", T_RP);
  printf("T_CAS:                      %6d\n", T_CAS);
  printf("T_RC:                       %6d\n", T_RC);
  printf("T_RAS:                      %6d\n", T_RAS);
  printf("T_RRD:                      %6d\n", T_RRD);
  printf("T_FAW:                      %6d\n", T_FAW);
  printf("T_WR:                       %6d\n", T_WR);
  printf("T_WTR:                      %6d\n", T_WTR);
  printf("T_RTP:                      %6d\n", T_RTP);
  printf("T_CCD:                      %6d\n", T_CCD);
  printf("T_RFC:                      %6d\n", T_RFC);
  printf("T_REFI:                     %6d\n", T_REFI);
  printf("T_CWD:                      %6d\n", T_CWD);
  printf("T_RTRS:                     %6d\n", T_RTRS);
  printf("T_PD_MIN:                   %6d\n", T_PD_MIN);
  printf("T_XP:                       %6d\n", T_XP);
  printf("T_XP_DLL:                   %6d\n", T_XP_DLL);
  printf("T_DATA_TRANS:               %6d\n", T_DATA_TRANS);

	printf("\n---------------------------\n");
	printf("- DRAM Idd Specifications -\n");
	printf("---------------------------\n");

	printf("VDD:                        %05.2f\n", VDD);
  printf("IDD0:                       %05.2f\n", IDD0);
  printf("IDD2P0:                     %05.2f\n", IDD2P0);
  printf("IDD2P1:                     %05.2f\n", IDD2P1);
  printf("IDD2N:                      %05.2f\n", IDD2N);
  printf("IDD3P:                      %05.2f\n", IDD3P);
  printf("IDD3N:                      %05.2f\n", IDD3N);
  printf("IDD4R:                      %05.2f\n", IDD4R);
  printf("IDD4W:                      %05.2f\n", IDD4W);
  printf("IDD5:                       %05.2f\n", IDD5);

	printf("\n-------------------\n");
	printf("- DRAM Controller -\n");
	printf("-------------------\n");
	printf("WQ_CAPACITY:                %6d\n", WQ_CAPACITY);
  printf("ADDRESS_MAPPING:            %6d\n", ADDRESS_MAPPING);
  printf("WQ_LOOKUP_LATENCY:          %6d\n", WQ_LOOKUP_LATENCY);
	printf("\n----------------------------------------------------------------------------------------\n");


}
	

#endif // __CONFIG_FILE_IN_H__
