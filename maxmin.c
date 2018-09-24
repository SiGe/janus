#include <stdint.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
//#include <time.h>
#include <sys/time.h>

#include "algorithm.h"
#include "error.h"
#include "log.h"
#include "parse.h"
#include "types.h"
#include "topo.h"
#include "thpool.h"
#include "cost.h"

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
  char *traffic_file = NULL;
  char *output_file = "./result_error_tot_thr.tsv";
  int coeff = 0;
  double guarantee = 6000000000;
  int failure_aware = 0;

  int symmetry_length = 0;

  char *selected_subplan_input = 0;
  int selected_error_flag = 0;
  char *noise_file = NULL;
  double noise_level = 0;

  struct timeval start, end;
  gettimeofday(&start, NULL);
  srand(47);

  while ((c = getopt(argc, argv, "f:y:x:mt:n:s:e:o:r:c:g:z:q:a:bi:j:")) != -1)
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
        case 'a':
            selected_subplan_input = optarg;
            break;
        case 'b':
            selected_error_flag = 1;
            break;
        case 'i':
            noise_file = optarg;
            break;
        case 'j':
            noise_level = atof(optarg);
            break;
        default:
            usage(argv[0]);
    }
  }

  if (error_time != -1)
  {
      error_cal_matrix(error_time, error_sample_num, x, y, symmetric_groups, error_file, traffic_file, output_file, coeff, failure_aware, symmetry_length, noise_file, noise_level);
      return EXIT_SUCCESS;
  }

  if (selected_subplan_input != 0)
  {
      if (file_name != 0)
      {
          if (selected_error_flag)
              selected_subplan_seq_cost_error(selected_subplan_input, file_name);
          else
              selected_subplan_seq_cost(selected_subplan_input, file_name);
          return EXIT_SUCCESS;
      }
      else
      {
          printf("please input file_name\n");
          return EXIT_FAILURE;
      }
  }
  
  if (coeff != 0)
  {
      //printf("execute all subplan seq cost\n");
      all_subplan_seq_cost(coeff, guarantee, file_name, symmetry_length, traffic_file);
      return EXIT_SUCCESS;
  }

  read_file(file_name, &output);
  //printf("read file done\n");

  struct network_t network = {0};
  if ((err = parse_input(output, &network)) != E_OK) {
    error("failed to read the data file: %d\n.", err);
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

  gettimeofday(&end, NULL);
  double delta_s = (double)(end.tv_sec - start.tv_sec) + ((double)(end.tv_usec - start.tv_usec)) / 1000000;
  //log_with_time("program exit, execution time is %lf\n", delta_s);

  return EXIT_SUCCESS;
}
