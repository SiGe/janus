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

int network_slo_violation(struct network_t *network, double y) {
  struct flow_t *flow = 0;
  int vio_num = 0;
  for (int i = 0; i < network->num_flows; ++i) {
    flow = &network->flows[i];
    if ((flow->demand > flow->bw) && (flow->bw < y)) {
        vio_num += 1;
    }
    if ((flow->demand < flow->bw)) {
        printf("More bandwith than demand\n");
        printf("%lf %lf\n", flow->demand, flow->bw);
        panic("More bandwith than demand");
    }
  }
  //printf("%d ToR pairs violating %f bandwidth, %d ToR pairs permitted\n", vio_num, y, x);
  return vio_num;
  //printf("%d", vio_num);
}

double network_max_link_throughput(struct network_t *network) {
    //printf("Getting maxlink util\n");
    struct link_t *link= 0;
    double max_link_throughput = 0.0;
    for(int i = 0; i < network->num_links; ++i) {
        link = &network->links[i];
        if (link->used / link->capacity > max_link_throughput)
            max_link_throughput = link->used / link->capacity;
    }
    //printf("%f\n", max_link_throughput);
    //printf("Getting maxlink done\n");
    return max_link_throughput;
}

double tot_throughput(struct network_t *network)
{
    struct flow_t *flow = 0;
    double ret = 0.0;
    for (int i = 0; i < network->num_flows; i++)
    {
        flow = &network->flows[i];
        ret += flow->demand;
    }
    return ret;
}

const char *usage_message = "" \
  "usage: %s <routing-file>\n" \
  "routing-file has the following format:\n\n" \
  "\tr\n"\
  "\t[num-flows]\n"\
  "\t[links on path of flow 0]\n"\
  "\t[links on path of flow 1]\n"\
  "\t[...]\n"\
  "\t[links on path of flow n]\n"\
  "\tl\n"\
  "\t[link capacities, space separated]\n"\
  "\tf\n"\
  "\t[flow demands, space separated]\n\n";

void usage(const char *fname) {
  printf(usage_message, fname);
  exit(EXIT_FAILURE);
}

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

void parallel_calculate(void *vargp)
{
    struct parallel_param_t *param = vargp;
    struct network_t *test_network = network_watchtower_gen(8, 12, 6, 6);

    int symmetry_list[] = {12, 18, 24, 30, 36, 6};

    printf("%d\n", param->idx);

    int node_num = 0;
    for (int i = 0; i < param->groups_len; i++)
        node_num += param->symmetric_subplan[i];

    if (param->symmetry_length)
        node_num += param->symmetric_subplan[0] * param->symmetry_length;

    int *update_nodes = malloc(sizeof(int) * node_num);
    int update_nodes_pos = 0;
    for (int i = 0; i < param->groups_len; i++)
    {
        for (int j = 0; j < param->symmetric_subplan[i]; j++)
        {
            update_nodes[update_nodes_pos] = param->update_groups[i] + j;
            update_nodes_pos++;
        }
    }

    if (param->symmetry_length)
    {
        for (int i = 0; i < param->symmetry_length; i++)
        {
            for (int j = 0; j < param->symmetric_subplan[0]; j++)
            {
                update_nodes[update_nodes_pos] = symmetry_list[i] + j;
                update_nodes_pos++;
            }
        }
    }

    struct traffic_t *traffic = param->traffic;
    network_update(test_network, update_nodes, node_num);
    for (int i = 0; i < traffic->tm_num; i++)
    {
        network_reset(test_network);
        build_flow(test_network, traffic, i);
        maxmin(test_network);
        int ret = network_slo_violation(test_network, param->guarantee);
        param->data[i] = ret;
    }
    network_free(test_network);
    free(vargp);
}

void all_subplan_seq_cost(int coeff, double guarantee, char* file_name, int symmetry_length)
{
  int symmetric_groups_norm[4] = {4,4,4,4};
  //int symmetric_groups_norm[4] = {5,5,5,5};
  int update_groups_norm[4] = {0, 12, 42, 48};
  int symmetric_groups_para[4] = {4,0,4,4};
  int update_groups_para[4] = {0, 6, 42, 48};

  int *symmetric_groups;
  int *update_groups;
  int group_len = 0;
  
  if (symmetry_length == 0)
  {
      symmetric_groups = symmetric_groups_norm;
      update_groups = update_groups_norm;
      group_len = 4;
  }
  else
  {
      symmetric_groups = symmetric_groups_para;
      update_groups = update_groups_para;
      group_len = 4;
  }

  int tot_subplans = 0;
  int **symmetric_subplans = generate_subplan(symmetric_groups, group_len, &tot_subplans);
  threadpool thpool = thpool_init(sysconf(_SC_NPROCESSORS_ONLN) - 2);
  //threadpool thpool = thpool_init(1);
  struct network_t *test_network = network_watchtower_gen(8, 12, 6, 6);
  struct traffic_t *traffic = traffic_load("./traffic/webserver_traffic_30s_8p_12t_sorted.tsv", test_network, coeff);
  int **data = malloc(sizeof(int *) * tot_subplans);

  for (int i = 0; i < tot_subplans; i++)
  {
      data[i] = malloc(sizeof(int) * traffic->tm_num);
      struct parallel_param_t *param = malloc(sizeof(struct parallel_param_t));
      param->symmetric_subplan = symmetric_subplans[i];
      param->groups_len = group_len;
      param->update_groups = update_groups;
      param->traffic = traffic;
      param->data = data[i];
      param->idx = i;
      param->guarantee = guarantee;
      param->symmetry_length = symmetry_length;
      thpool_add_work(thpool, parallel_calculate, param);
  }
  thpool_wait(thpool);
  FILE *f = fopen(file_name, "w+");
  for (int i = 0; i < tot_subplans; i++)
  {
      for (int j = 0; j < traffic->tm_num; j++)
      {
          fprintf(f, "%d ", data[i][j]);
      }
      fprintf(f, "\n");
  }
  fclose(f);
}

void single_plan_seq_cost()
{
  int update_nodes[] = {0, 6, 7};
  struct network_t *test_network = network_watchtower_gen(8, 12, 6, 6);
  network_update(test_network, update_nodes, 10);
  struct traffic_t *traffic = traffic_load("../traffic/webserver_traffic_30s_8p_12t_sorted.tsv", test_network, 2500);
  for (int i = 0; i < traffic->tm_num; i++)
  {
      network_reset(test_network);
      build_flow(test_network, traffic, i);
      maxmin(test_network);
      printf("%d ", network_slo_violation(test_network, 6000000000));
  }
  printf("\n");
  network_free(test_network);
} 

void plan_debug()
{
    int update_nodes[] = {0, 6, 7};
    struct network_t *test_network = network_watchtower_gen(8, 12, 6, 6);
    network_update(test_network, update_nodes, 3);
    struct traffic_t *traffic = traffic_load("../traffic/webserver_traffic_30s_8p_12t_sorted.tsv", test_network, 2500);
    int time = 944;
    network_reset(test_network);
    print_links(test_network);
    build_flow(test_network, traffic, time);
    maxmin(test_network);
    printf("%d\n", network_slo_violation(test_network, 6000000000));
    network_free(test_network);
}

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
};

void error_parallel_calculate(void *vargp)
{
    struct error_para_param_t *param = vargp;

    struct network_t *network = network_watchtower_gen(8, 12, 6, 6);

    int symmetry_list[] = {12, 18, 24, 30, 36, 6};

    int node_num = 0;
    for (int i = 0; i < param->groups_len; i++)
        node_num += param->symmetric_subplan[i];

    if (param->failure_aware)
        node_num ++;

    if (param->symmetry_length)
        node_num += param->symmetric_subplan[0] * param->symmetry_length;

    int *update_nodes = malloc(sizeof(int) * node_num);
    int update_nodes_pos = 0;
    for (int i = 0; i < param->groups_len; i++)
    {
        for (int j = 0; j < param->symmetric_subplan[i]; j++)
        {
            update_nodes[update_nodes_pos] = param->update_groups[i] + j;
            update_nodes_pos++;
        }
    }
    if (param->failure_aware)
    {
        update_nodes[update_nodes_pos] = param->failure_aware;
    }
    if (param->symmetry_length)
    {
        for (int i = 0; i < param->symmetry_length; i++)
        {
            for (int j = 0; j < param->symmetric_subplan[0]; j++)
            {
                update_nodes[update_nodes_pos] = symmetry_list[i] + j;
                update_nodes_pos++;
            }
        }
    }

    /*
    printf("%d\t", node_num);
    for (int i = 0; i < param->groups_len; i++)
    {
        printf("%d ", param->symmetric_subplan[i]);
    }
    printf("\t");

    for (int i = 0; i < node_num; i++)
    {
        printf("%d ", update_nodes[i]);
    }
    printf("\n");

    return;
    */

    network_update(network, update_nodes, node_num);
    for (int i = 0; i < param->sample_num; i++)
    {
        param->data[i] = 0;
        for (int j = 0; j < param->errors->predict_num; j++)
        {
            struct tm_t *tm = param->tms[j];
            network_reset(network);
            build_flow_error(network, tm, param->errors, param->error_seqs[i], j);
            //printf("%d %d\n", i, j);
            //write_flows(network);
            maxmin(network);
            int ret = network_slo_violation(network, param->y);
            if (ret > param->x)
                param->data[i] += 1;
        }
    }
    free(vargp);
}

void error_cal_matrix(int error_time, int sample_num, int x, double y, int *symmetric_groups, 
        char *error_file, char *traffic_file, char *output_file, int coeff, int failure_aware,
        int symmetry_length)
{

    struct error_t *error = malloc(sizeof(struct error_t));
    //parse_error("./error_2500.txt", error);
    parse_error(error_file, error);

    struct network_t *network = network_watchtower_gen(8, 12, 6, 6);
    //struct traffic_t *traffic = traffic_load("../traffic/webserver_traffic_30s_8p_12t_sorted.tsv", network, 2500);
    struct traffic_t *traffic = traffic_load(traffic_file, network, coeff);

    int tot_subplans = 0;
    int update_groups_norm[4] = {0, 12, 42, 48};
    int update_groups_sym[4] = {0, 6, 42, 48};
    int groups_len = 0;
    int *update_groups;
    int **symmetric_subplans;
    if (symmetry_length == 0)
    {
        symmetric_subplans = generate_subplan(symmetric_groups, 4, &tot_subplans);
        update_groups = update_groups_norm;
        groups_len = 4;
    }
    else
    {
        symmetric_subplans = generate_subplan(symmetric_groups, 4, &tot_subplans);
        update_groups = update_groups_sym;
        groups_len = 4;
    }
    //threadpool thpool = thpool_init(sysconf(_SC_NPROCESSORS_ONLN) - 2);
    threadpool thpool = thpool_init(19);

    printf("Init done, parallizing\n");

    int **ret_data = malloc(sizeof(int *) * tot_subplans);

    int *error_seqs = malloc(sizeof(int) * sample_num);
    for (int i = 0; i < sample_num; i++)
        error_seqs[i] = error_time - sample_num / 2 + i - 100;

    struct tm_t **tms = build_flow_ewma(traffic, error_time, error->sd_pair_num);

    for (int i = 0; i < tot_subplans; i++)
    {
        ret_data[i] = malloc(sizeof(int) * sample_num);
        struct error_para_param_t *param = malloc(sizeof(struct error_para_param_t));

        param->data = ret_data[i];
        param->tms = tms;
        param->symmetric_subplan = symmetric_subplans[i];
        param->groups_len = groups_len;
        param->update_groups = update_groups;
        param->sample_num = sample_num;
        param->error_seqs = error_seqs;
        param->y = y;
        param->x = x;
        param->errors = error;
        param->failure_aware = failure_aware;
        param->symmetry_length = symmetry_length;

        thpool_add_work(thpool, error_parallel_calculate, param);
    }
    thpool_wait(thpool);

    FILE *f = fopen(output_file, "w+");
    for (int i = 0; i < tot_subplans; i++)
    {
        for (int j = 0; j < sample_num; j++)
        {
            fprintf(f, "%d ", ret_data[i][j]);
        }
        fprintf(f, "\n");
    }
    fclose(f);

    for (int i = 0; i < tot_subplans; i++)
    {
        free(ret_data[i]);
    }
    free(ret_data);
}

struct max_link_util_param_t
{
    struct traffic_t *traffic;
    int time;
    struct error_t *errors;
    double *data;
    int *error_seqs;
    int sample_num;
};

//void max_link_util_calculation(struct traffic_t *traffic, int time, struct error_t *errors,
//        double *data, int *error_seqs, int sample_num)
void max_link_util_calculation(void* vargp)
{
    struct max_link_util_param_t *param = vargp;
    printf("%d\n", param->time);
    struct network_t *network = network_watchtower_gen(8, 12, 6, 6);
    struct tm_t **tms = build_flow_ewma(param->traffic, param->time, param->errors->sd_pair_num);
    //printf("Start calculating\n");

    param->data[param->time] = 0;
    for (int i = 0; i < param->sample_num; i++)
    {
        for (int j = 0; j < param->errors->predict_num; j++)
        {
            struct tm_t *tm = tms[j];
            network_reset(network);
            //printf("%d\n", param->error_seqs[i]);
            build_flow_error(network, tm, param->errors, param->error_seqs[i], j);
            //printf("get TM done\n");
            //maxmin(network);
            //double ret = network_max_link_throughput(network);
            double ret = tot_throughput(network);
            param->data[param->time] += ret;
        }
    }
    //printf("Calculate done\n");
    param->data[param->time] /= param->errors->predict_num * param->sample_num;
    free(param->error_seqs);
    free(vargp);
}

void max_link_util_cal_matrix(int sample_num, char *output_file)
{

    struct error_t *error = malloc(sizeof(struct error_t));
    parse_error("../error_2500_new.txt", error);

    struct network_t *network = network_watchtower_gen(8, 12, 6, 6);
    struct traffic_t *traffic = traffic_load("../traffic/webserver_traffic_30s_8p_12t_sorted.tsv", network, 2500);

    printf("%d\n", error->predict_num);

    threadpool thpool = thpool_init(sysconf(_SC_NPROCESSORS_ONLN) - 1);

    double *ret_data = malloc(sizeof(double) * traffic->tm_num);
    memset(ret_data, 0, sizeof(double) * traffic->tm_num);
    printf("Start calculating\n");

    for (int time = 200; time < 2500; time++)
    {
        int *error_seqs = malloc(sizeof(int) * sample_num);
        for (int i = 0; i < sample_num; i++)
            error_seqs[i] = time - sample_num / 2 + i - 100;
        struct max_link_util_param_t *param = malloc(sizeof(struct max_link_util_param_t));
        param->traffic = traffic;
        param->time = time;
        param->errors = error;
        param->data = ret_data;
        param->error_seqs = error_seqs;
        param->sample_num = sample_num;
        thpool_add_work(thpool, max_link_util_calculation, param);
    }
    thpool_wait(thpool);

    double *real_data = malloc(sizeof(double) * traffic->tm_num);
    memset(real_data, 0, sizeof(double) * traffic->tm_num);
    for (int time = 200; time < 2500; time++)
    {
        network_reset(network);
        build_flow(network, traffic, time);
        real_data[time] = tot_throughput(network);
    }

    double *ewma_data = malloc(sizeof(double) * traffic->tm_num);
    memset(ewma_data, 0, sizeof(double) * traffic->tm_num);
    for (int time = 200; time < 2500; time++)
    {
        struct tm_t **tms = build_flow_ewma(traffic, time, 96 * 95);
        for (int i = 0; i < 8; i++)
        {
            network_reset(network);
            update_tm(network, tms[i]);
            ewma_data[time] += tot_throughput(network);
        }
        ewma_data[time] /= 8;
    }

    FILE *f = fopen(output_file, "w+");
    for (int i = 0; i < traffic->tm_num; i++)
    {
        fprintf(f, "%f\t%f\t%f\n", ret_data[i], real_data[i], ewma_data[i]);
    }
    //fprintf(f, "\n");
    fclose(f);

    free(ret_data);
}
int main(int argc, char **argv) {
  char *output = 0;
  int err = 0;
  int c = 0;
  int max_link_flag = 0;
  double y = -1;
  int x = 0;
  char *file_name = NULL;

  int error_time = -1;
  int error_sample_num = 200;
  int symmetric_groups[4] = {0};

  char *error_file = "./error_2500_new.txt";
  char *traffic_file = "../traffic/webserver_traffic_30s_8p_12t_sorted.tsv";
  char *output_file = "./result_error_tot_thr.tsv";
  int coeff = 0;
  double guarantee = 6000000000;
  int failure_aware = 0;

  int symmetry_length = 0;

  srand(47);

  while ((c = getopt(argc, argv, "f:y:x:mt:n:s:e:o:r:c:g:z:q:")) != -1)
  {
    switch (c)
    {
        case 'f':
            file_name = optarg;
            break;
        case 'y':
            y = atof(optarg);
            break;
        case 'x':
            x = atoi(optarg);
            break;
        case 'm':
            max_link_flag = 1;
            break;
        case 't':
            error_time = atoi(optarg);
            break;
        case 'n':
            error_sample_num = atoi(optarg);
            break;
        case 's':
            for (int i = 0; i < 4; i++)
            {
                symmetric_groups[i] = optarg[2*i] - '0';
                printf("%d\n", symmetric_groups[i]);
            }
            break;
        case 'e':
            error_file = optarg;
            break;
        case 'o':
            output_file = optarg;
            break;
        case 'r':
            traffic_file = optarg;
            break;
        case 'c':
            coeff = atoi(optarg);
            break;
        case 'g':
            guarantee = atof(optarg);
            break;
        case 'z':
            failure_aware = atoi(optarg+1) + 3;
            if (*optarg == 'c')
                failure_aware += 48;
            break;
        case 'q':
            symmetry_length = atoi(optarg);
            break;
        default:
            usage(argv[0]);
    }
  }

  if (error_time != -1)
  {
      error_cal_matrix(error_time, error_sample_num, x, y, symmetric_groups, error_file, traffic_file, output_file, coeff, failure_aware, symmetry_length);
      return EXIT_SUCCESS;
  }
  
  if (coeff != 0)
  {
      all_subplan_seq_cost(coeff, guarantee, file_name, symmetry_length);
      return EXIT_SUCCESS;
  }

  info("reading data file.");
  read_file(file_name, &output);

  struct network_t network = {0};
  if ((err = parse_input(output, &network)) != E_OK) {
    error("failed to read the data file: %d.", err);
    return EXIT_FAILURE;
  };

  maxmin(&network);
  //network_print_flows(&network);
  //network_slo_violation(&network, atof(argv[2]));
  if (max_link_flag)
      printf("%f", network_max_link_throughput(&network));
  else if (y >= 0)
      printf("%d", network_slo_violation(&network, y));
  network_free(&network);

  return EXIT_SUCCESS;
}
