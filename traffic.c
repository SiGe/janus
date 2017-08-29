#include "traffic.h"
#include "types.h"
#include "parse.h"
#include "error.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void load_traffic(char const *tracefile, struct network_t *network, int coeff)
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

    info("Loading traffic\n");
    if (parse_traffic(f, ret_traffic, tot_tors, ret_traffic->tm_num, coeff) != E_OK)
    {
        error("Unexpected input in traffic file!\n");
        exit(-1);
    }
    info("Load traffic done\n");

    network->traffic = ret_traffic;
    fclose(f);
    return;
}

void build_flow(struct network_t *network, int time)
{
    struct tm_t *tm = network->traffic->tms + time;
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

void print_flows(struct network_t *network)
{
    printf("%d\n", network->num_flows);
    for (int i = 0; i < network->num_flows; i++)
    {
        printf("%.0f ", network->flows[i].demand);
    }
    printf("\n");
}
