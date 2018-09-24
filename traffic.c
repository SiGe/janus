#include "traffic.h"
#include "types.h"
#include "parse.h"
#include "error.h"
#include "log.h"
#include "algorithm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

double gaussrand()
{
	double V1, V2, S;
	double X;

    do {
        double U1 = (double)rand() / RAND_MAX;
        double U2 = (double)rand() / RAND_MAX;

        V1 = 2 * U1 - 1;
        V2 = 2 * U2 - 1;
        S = V1 * V1 + V2 * V2;
    } while(S >= 1 || S == 0);

    X = V1 * sqrt(-2 * log(S) / S);

	return X;
}

int get_traffic_len(char const *input)
{
    int traffic_len = 0;
    while(*input != '\0')
    {
        if (*input == '\t')
        {
            traffic_len++;
        }
        input++;
    }
    return traffic_len;
}

int parse_traffic(FILE *f, struct traffic_t *traffics, int tot_tors, int traffic_len, int coeff)
{
    int pos = 0, result = 0;
    size_t len = 0;
    bw_t bandwidth;
    for (int src = 0; src < tot_tors; src++)
    {
        for (int dst = 0; dst < tot_tors - 1; dst++)
        {
            char *input = 0;
            if (getline(&input, &len, f) == -1)
            {
                error("Read file error!\n");
                exit(-1);
            }
            int cur_pos = src * (tot_tors - 1) + dst;
            int name = 0;
            /* Skip the name */
            while (name <= 1)
            {
                if (*input == '\t')
                    name++;
                input++;
            }
            for (int i = 0; i < traffic_len; i++)
            {
                result = read_bw(input, &bandwidth, &pos);
                if (result <= 0)
                {
                    error("error when parsing traffic file\n");
                }
                *((traffics->tms + i)->tm + cur_pos) = bandwidth * coeff;
                input += pos;
            }
        } 
    }
    return E_OK;
}

struct traffic_t *traffic_load(char const *tracefile, struct network_t *network, int coeff)
{
    if (network == NULL)
    {
        error("Please init a network!\n");
        exit(-1);
    }
    FILE *f = fopen(tracefile, "r+");
    char *input = 0;
    size_t len = 0;
    if (getline(&input, &len, f) == -1)
    {
        error("Read file error!\n");
        exit(-1);
    }
    rewind(f);

    int tot_tors = network->k * network->t_per_p;
    int traffic_len = get_traffic_len(input);

    struct traffic_t *ret_traffic = (struct traffic_t *)malloc(sizeof(struct traffic_t));
    memset(ret_traffic, 0, sizeof(struct traffic_t));
    ret_traffic->tm_num = traffic_len - 1;

    ret_traffic->tms = (struct tm_t *)malloc(sizeof(struct tm_t) * ret_traffic->tm_num);
    memset(ret_traffic->tms, 0, sizeof(struct tm_t) * ret_traffic->tm_num);
    for (int i = 0; i < ret_traffic->tm_num; i++)
    {
        (ret_traffic->tms + i)->tm = (bw_t *)malloc(sizeof(bw_t) * (tot_tors * (tot_tors-1) + 1));
        memset((ret_traffic->tms + i)->tm, 0, sizeof(bw_t) * (tot_tors * (tot_tors-1) + 1));
    }

    if (parse_traffic(f, ret_traffic, tot_tors, ret_traffic->tm_num, coeff) != E_OK)
    {
        error("Unexpected input in traffic file!\n");
        exit(-1);
    }

    fclose(f);
    return ret_traffic;
}

void update_tm(struct network_t *network, struct tm_t *tm)
{
    int tot_tors = network->k * network->t_per_p;

    if (network->flows != NULL)
    {
        struct flow_t *flows = network->flows;
        memset(flows, 0, sizeof(struct flow_t) * network->num_flows);
        for (pair_id_t i = 0; i < network->num_flows; i++)
        {
            flows->id = i;
            flows->demand = tm->tm[i];
            flows->fixed = 0;
            flows->nlinks = 0;
            flows->bw = 0;
            flows->next = flows->prev = 0;
            memset(flows->links, 0, sizeof(struct link_t*) * MAX_PATH_LENGTH);
            flows++;
        }
        return;
    }
    else
    {
        network->num_flows = tot_tors * (tot_tors - 1);

        struct flow_t *flows = (struct flow_t *)malloc(sizeof(struct flow_t) * network->num_flows);
        memset(flows, 0, sizeof(struct flow_t) * network->num_flows);
        network->flows = flows;

        for (pair_id_t i = 0; i < network->num_flows; i++)
        {
            flows->id = i;
            flows->demand = tm->tm[i];
            flows++;
        }
    }
}

void build_flow(struct network_t *network, struct traffic_t *traffic, int time)
{
    struct tm_t *tm = traffic->tms + time;
    update_tm(network, tm);
}

void write_flows(struct network_t *network)
{
    printf("writing flows\n");
    FILE *fp = fopen("error_network_traffic.tsv", "w");
    if (fp == NULL)
        return;
    for (int i = 0; i < network->num_flows; i++)
    {
        fprintf(fp, "%.0f ", network->flows[i].demand);
    }
    fprintf(fp, "\n");
    printf("write flows done\n");
    fclose(fp);
}

void print_flows(struct network_t *network)
{
    printf("%d\n", network->num_flows);
    for (int i = 0; i < network->num_flows; i++)
    {
        printf("%.0f ", network->flows[i].demand);
    }
    printf("\n");
}

double tot_volume_tm(struct tm_t * tm, int tm_len)
{
    double tot_volume = 0;
    for (int i = 0; i < tm_len; i++)
        tot_volume += tm->tm[i];
    return tot_volume;
}

void build_flow_error_range(struct network_t *network, struct tm_t *tm, struct error_range_t *error, int error_seq, int tm_seq)
{
    struct tm_t *new_tm = malloc(sizeof(struct tm_t));
    new_tm->tm = malloc(sizeof(bw_t) * error->sd_pair_num);
    //printf("in build_flow_error_range, param is error_seq: %d, tm_seq: %d\n", error_seq, tm_seq);
    bw_t *error_tm = error->error_tms[error_seq - error->sample_str][tm_seq];
    for (int i = 0; i < error->sd_pair_num; i++)
    {
        new_tm->tm[i] = max(tm->tm[i] + error_tm[i], 0);
        //new_tm->tm[i] = tm->tm[i] + error_tm[i];
    }

    struct tm_t *new_tm_no_max = malloc(sizeof(struct tm_t));
    new_tm_no_max->tm = malloc(sizeof(bw_t) * error->sd_pair_num);
    for (int i = 0; i < error->sd_pair_num; i++)
    {
        new_tm_no_max->tm[i] = tm->tm[i] + error_tm[i];
    }

    double ratio = tot_volume_tm(new_tm_no_max, error->sd_pair_num) /
                   tot_volume_tm(new_tm, error->sd_pair_num) * 1.08;

    ratio = ratio > 0 ? ratio : 1;

    free(new_tm_no_max->tm);
    free(new_tm_no_max);

    for (int i = 0; i < error->sd_pair_num; i++)
    {
        new_tm->tm[i] *= ratio;
    }

    update_tm(network, new_tm);
    free(new_tm->tm);
    free(new_tm);
}

void build_flow_error_with_noise(struct network_t *network, struct tm_t *tm, struct error_t *error, int error_seq, 
        int tm_seq, double *noise_volume, double noise_level)
{
    struct tm_t *new_tm = malloc(sizeof(struct tm_t));
    new_tm->tm = malloc(sizeof(bw_t) * error->sd_pair_num);
    bw_t *error_tm = error->error_tms[error_seq][tm_seq];
    for (int i = 0; i < error->sd_pair_num; i++)
    {
        new_tm->tm[i] = max(tm->tm[i] + error_tm[i], 0);
        //new_tm->tm[i] = tm->tm[i] + error_tm[i];
    }

    struct tm_t *new_tm_no_max = malloc(sizeof(struct tm_t));
    new_tm_no_max->tm = malloc(sizeof(bw_t) * error->sd_pair_num);
    for (int i = 0; i < error->sd_pair_num; i++)
    {
        new_tm_no_max->tm[i] = tm->tm[i] + error_tm[i];
    }

    double ratio = tot_volume_tm(new_tm_no_max, error->sd_pair_num) /
                   tot_volume_tm(new_tm, error->sd_pair_num) * 1.08;

    ratio = ratio > 0 ? ratio : 1;

    free(new_tm_no_max->tm);
    free(new_tm_no_max);

    for (int i = 0; i < error->sd_pair_num; i++)
    {
        new_tm->tm[i] *= ratio;
    }

    /*
    double tot_volume = 0;
    double tot_volume_origin = 0;
    for (int i = 0; i < error->sd_pair_num; i++)
    {
        tot_volume_origin += new_tm->tm[i];
        new_tm->tm[i] += gaussrand() * noise_volume[i] * noise_level;
        new_tm->tm[i] = max(new_tm->tm[i], 0);
        tot_volume += new_tm->tm[i];
    }
    */

    /*
    ratio = 1;
    if (tot_volume == 0)
        ratio = 1;
    else
        ratio = tot_volume_origin / tot_volume;
    for (int i = 0; i < error->sd_pair_num; i++)
        new_tm->tm[i] *= ratio;
        */

    update_tm(network, new_tm);
    free(new_tm->tm);
    free(new_tm);
}

void build_flow_error(struct network_t *network, struct tm_t *tm, struct error_t *error, int error_seq, int tm_seq)
{
    struct tm_t *new_tm = malloc(sizeof(struct tm_t));
    new_tm->tm = malloc(sizeof(bw_t) * error->sd_pair_num);
    bw_t *error_tm = error->error_tms[error_seq][tm_seq];
    for (int i = 0; i < error->sd_pair_num; i++)
    {
        new_tm->tm[i] = max(tm->tm[i] + error_tm[i], 0);
        //new_tm->tm[i] = tm->tm[i] + error_tm[i];
    }

    struct tm_t *new_tm_no_max = malloc(sizeof(struct tm_t));
    new_tm_no_max->tm = malloc(sizeof(bw_t) * error->sd_pair_num);
    for (int i = 0; i < error->sd_pair_num; i++)
    {
        new_tm_no_max->tm[i] = tm->tm[i] + error_tm[i];
    }

    double ratio = tot_volume_tm(new_tm_no_max, error->sd_pair_num) /
                   tot_volume_tm(new_tm, error->sd_pair_num) * 1.08;

    ratio = ratio > 0 ? ratio : 1;

    free(new_tm_no_max->tm);
    free(new_tm_no_max);

    for (int i = 0; i < error->sd_pair_num; i++)
    {
        new_tm->tm[i] *= ratio;
    }

    update_tm(network, new_tm);
    free(new_tm->tm);
    free(new_tm);
}

struct tm_t **build_flow_ewma_noise(struct traffic_t *traffic, int time, int sd_pair_num, char *noise_file, int coeff, double noise_level)
{
    double alpha = 0.52, beta = 0.25;
    int seq_len = 10;
    struct tm_t **ret_tms = malloc(sizeof(struct tm_t*) * 8);

    FILE *f = fopen(noise_file, "r+");
    bw_t *noise_sigma = malloc(sizeof(bw_t) * sd_pair_num);
    double tot_volume[8] = {0};
    double tot_volume_origin[8] = {0};
    for (int i = 0; i < sd_pair_num; i++)
    {
        fscanf(f, "%lf\n", &noise_sigma[i]);
        noise_sigma[i] *= coeff;
    }

    bw_t *seq = malloc(sizeof(bw_t) * seq_len);
    for (int i = 0; i < 8; i++)
    {
        ret_tms[i] = malloc(sizeof(struct tm_t));
        ret_tms[i]->tm = malloc(sizeof(bw_t) * sd_pair_num);
    }
    for (int i = 0; i < sd_pair_num; i++)
    {
        for (int j = 0; j < seq_len; j++)
        {
            seq[j] = traffic->tms[time-seq_len+j].tm[i];
        }
        bw_t *new_tm = double_ewma(seq, seq_len, alpha, 8, beta);
        for (int j = 0; j < 8; j++)
        {
            // Add noise_level of the average traffic between this tor pair
            ret_tms[j]->tm[i] = max(new_tm[j] + gaussrand() * noise_sigma[i] * noise_level, 0);
            tot_volume[j] += ret_tms[j]->tm[i];
            tot_volume_origin[j] += new_tm[j];
        }
        free(new_tm);
    }

    // uniform noise
    /*
    for (int i = 0; i < 8; i++)
    {
        double ratio = 1;
        if (tot_volume[i] == 0)
            ratio = 1;
        else
            ratio = tot_volume_origin[i] / tot_volume[i];
        for (int j = 0; j < sd_pair_num; j++)
            ret_tms[i]->tm[j] *= ratio;
    }
    */
    return ret_tms;
}

struct tm_t **build_flow_ewma(struct traffic_t *traffic, int time, int sd_pair_num)
{
    double alpha = 0.52, beta = 0.25;
    int seq_len = 10;
    struct tm_t **ret_tms = malloc(sizeof(struct tm_t*) * 8);
    //bw_t tms[8] = {0};
    bw_t *seq = malloc(sizeof(bw_t) * seq_len);
    for (int i = 0; i < 8; i++)
    {
        ret_tms[i] = malloc(sizeof(struct tm_t));
        ret_tms[i]->tm = malloc(sizeof(bw_t) * sd_pair_num);
    }
    for (int i = 0; i < sd_pair_num; i++)
    {
        for (int j = 0; j < seq_len; j++)
        {
            seq[j] = traffic->tms[time-seq_len+j].tm[i];
        }
        bw_t *new_tm = double_ewma(seq, seq_len, alpha, 8, beta);
        for (int j = 0; j < 8; j++)
        {
            ret_tms[j]->tm[i] = new_tm[j];
        }
        free(new_tm);
    }
    return ret_tms;
}
