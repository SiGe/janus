#ifndef _COST_H_
#define _COST_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#include "algorithm.h"
#include "error.h"
#include "log.h"
#include "parse.h"
#include "types.h"
#include "topo.h"
#include "traffic.h"
#include "thpool.h"

struct paral_symmetry_groups_t {
    int id_length;
    int *ids;
    int upgrade_num;
};

// structure used to pass to parallel calculator
struct selected_paral_param_t {
    // topology
    int k, t_per_p, a_per_p, c_num;
    struct traffic_t *traffic;

    // symmetry groups
    struct paral_symmetry_groups_t *groups;
    int group_len;
    int tot_node_num;

    // budget
    double guarantee_bw;

    // control and ret data
    int para_idx;
    int *ret_data;

    int *upgrade_nodes;
    int upgrade_node_len;
};

struct param_symmetry_groups_t {
    int id_length;
    int *ids;
    int min, max, step;
};

// structure used to pass parameter between python and c
struct selected_param_t {
    char *traffic_file;
    int coeff;
    double guarantee_bw;

    int k, t_per_p, a_per_p, c_num;

    int group_len;
    struct param_symmetry_groups_t *groups;

    int max_node_num;
};

struct selected_paral_param_error_t {
    // topology
    int k, t_per_p, a_per_p, c_num;
    struct traffic_t *traffic;

    struct tm_t **tms;
    int *error_seqs;
    int sample_num;

    // control and ret data
    int para_idx;
    int *ret_data;

    // budget
    double guarantee_bw;
    int x;

    struct error_range_t *error;
    int *upgrade_node_ids;
    int upgrade_id_length;
    double build_flow_time;
    double simulate_network_time;
};

struct selected_param_error_t {
    struct selected_param_t norm_para;
    
    int cur_time;
    int predict_tm_num;
    int *update_nums;
    char *error_folder;
    // tor pair violated threshold
    int x;
};

struct parallel_param_t {
  int *symmetric_subplan;
  int groups_len;

  int *update_groups;

  struct traffic_t *traffic;

  int *data;
  int idx;

  double guarantee;

  int symmetry_length;
};

struct error_para_param_t
{
    int *data;

    struct tm_t **tms;
    int sample_num;
    
    int *symmetric_subplan;
    int groups_len;    

    int *update_groups;

    double y;
    int x;

    struct error_t *errors;
    int *error_seqs;

    int failure_aware;

    int symmetry_length;

    double *noise_volume;
    double noise_level;
};

int network_slo_violation(struct network_t *network, double y);

void selected_subplan_seq_cost(char *input_param_file, char *output_result);

void selected_subplan_seq_cost_error(char *input_param_file, char *output_result);

void all_subplan_seq_cost(int coeff, double guarantee, char* file_name, int symmetry_length, char* traffic_file);

void error_cal_matrix(int error_time, int sample_num, int x, double y, int *symmetric_groups, 
        char *error_file, char *traffic_file, char *output_file, int coeff, int failure_aware,
        int symmetry_length, char *noise_file, double noise_level);

#endif
