               :#include "traffic.h"
               :#include "types.h"
               :#include "parse.h"
               :#include "error.h"
               :#include "log.h"
               :
               :#include <stdio.h>
               :#include <stdlib.h>
               :#include <string.h>
               :
               :int get_traffic_len(char const *input)
               :{
               :    int traffic_len = 0;
               :    while(*input != '\0')
               :    {
               :        if (*input == '\t')
               :        {
               :            traffic_len++;
               :        }
               :        input++;
               :    }
               :    return traffic_len;
               :}
               :
               :int parse_traffic(FILE *f, struct traffic_t *traffics, int tot_tors, int traffic_len, int coeff)
               :{ /* parse_traffic total:   2745  0.0018 */
               :    int pos = 0, result = 0;
               :    size_t len = 0;
               :    bw_t bandwidth;
               :    for (int src = 0; src < tot_tors; src++)
               :    {
               :        for (int dst = 0; dst < tot_tors - 1; dst++)
               :        {
               :            char *input = 0;
               :            if (getline(&input, &len, f) == -1)
               :            {
               :                error("Read file error!\n");
               :                exit(-1);
               :            }
               :            int cur_pos = src * (tot_tors - 1) + dst;
     2 1.3e-06 :            int name = 0;
               :            /* Skip the name */
               :            while (name <= 1)
               :            {
               :                if (*input == '\t')
               :                    name++;
     1 6.6e-07 :                input++;
               :            }
     3 2.0e-06 :            for (int i = 0; i < traffic_len; i++)
               :            {
   273 1.8e-04 :                result = read_bw(input, &bandwidth, &pos);
     1 6.6e-07 :                if (result <= 0)
               :                {
               :                    error("error when parsing traffic file\n");
               :                }
   517 3.4e-04 :                *((traffics->tms + i)->tm + cur_pos) = bandwidth * coeff;
  1948  0.0013 :                input += pos;
               :            }
               :        } 
               :    }
               :    return E_OK;
               :}
               :
               :struct traffic_t *traffic_load(char const *tracefile, struct network_t *network, int coeff)
               :{ /* traffic_load total:      1 6.6e-07 */
               :    printf("loading traffic\n");
               :    if (network == NULL)
               :    {
               :        error("Please init a network!\n");
               :        exit(-1);
               :    }
               :    FILE *f = fopen(tracefile, "r+");
               :    char *input = 0;
               :    size_t len = 0;
               :    if (getline(&input, &len, f) == -1)
               :    {
               :        error("Read file error!\n");
               :        exit(-1);
               :    }
               :    rewind(f);
               :
               :    int tot_tors = network->k * network->t_per_p;
               :    int traffic_len = get_traffic_len(input);
               :
               :    struct traffic_t *ret_traffic = (struct traffic_t *)malloc(sizeof(struct traffic_t));
               :    memset(ret_traffic, 0, sizeof(struct traffic_t));
               :    ret_traffic->tm_num = traffic_len - 1;
               :
               :    ret_traffic->tms = (struct tm_t *)malloc(sizeof(struct tm_t) * ret_traffic->tm_num);
               :    memset(ret_traffic->tms, 0, sizeof(struct tm_t) * ret_traffic->tm_num);
               :    for (int i = 0; i < ret_traffic->tm_num; i++)
               :    {
     1 6.6e-07 :        (ret_traffic->tms + i)->tm = (bw_t *)malloc(sizeof(bw_t) * (tot_tors * (tot_tors-1) + 1));
               :        memset((ret_traffic->tms + i)->tm, 0, sizeof(bw_t) * (tot_tors * (tot_tors-1) + 1));
               :    }
               :
               :    info("Loading traffic\n");
               :    if (parse_traffic(f, ret_traffic, tot_tors, ret_traffic->tm_num, coeff) != E_OK)
               :    {
               :        error("Unexpected input in traffic file!\n");
               :        exit(-1);
               :    }
               :    info("Load traffic done\n");
               :
               :    fclose(f);
               :    return ret_traffic;
               :}
               :
               :void update_tm(struct network_t *network, struct tm_t *tm)
     7 4.6e-06 :{ /* update_tm total: 226368  0.1498 */
               :    int tot_tors = network->k * network->t_per_p;
               :
               :    if (network->flows != NULL)
               :    {
               :        struct flow_t *flows = network->flows;
               :        memset(flows, 0, sizeof(struct flow_t) * network->num_flows);
 14829  0.0098 :        for (pair_id_t i = 0; i < network->num_flows; i++)
               :        {
  1373 9.1e-04 :            flows->id = i;
 81015  0.0536 :            flows->demand = tm->tm[i];
  8099  0.0054 :            flows->fixed = 0;
 13249  0.0088 :            flows->nlinks = 0;
 19777  0.0131 :            flows->bw = 0;
 35225  0.0233 :            flows->next = flows->prev = 0;
               :            memset(flows->links, 0, sizeof(struct link_t*) * MAX_PATH_LENGTH);
               :            flows++;
               :        }
               :        return;
               :    }
               :    else
               :    {
               :        network->num_flows = tot_tors * (tot_tors - 1);
               :
               :        struct flow_t *flows = (struct flow_t *)malloc(sizeof(struct flow_t) * network->num_flows);
               :        memset(flows, 0, sizeof(struct flow_t) * network->num_flows);
               :        network->flows = flows;
               :
    18 1.2e-05 :        for (pair_id_t i = 0; i < network->num_flows; i++)
               :        {
    19 1.3e-05 :            flows->id = i;
     7 4.6e-06 :            flows->demand = tm->tm[i];
               :            flows++;
               :        }
               :    }
   100 6.6e-05 :}
               :
               :void build_flow(struct network_t *network, struct traffic_t *traffic, int time)
    21 1.4e-05 :{ /* build_flow total:     21 1.4e-05 */
               :    struct tm_t *tm = traffic->tms + time;
               :    update_tm(network, tm);
               :}
               :
               :void print_flows(struct network_t *network)
               :{
               :    printf("%d\n", network->num_flows);
               :    for (int i = 0; i < network->num_flows; i++)
               :    {
               :        printf("%.0f ", network->flows[i].demand);
               :    }
               :    printf("\n");
               :}
/* 
 * Total samples for file : "/home/jiaqi/planning_config_dic/src/progressive-filling/traffic.c"
 * 
 * 176485  0.1168
 */


/* 
 * Command line: opannotate --source --output-dir=./ 
 * 
 * Interpretation of command line:
 * Output annotated source file with samples
 * Output all files
 * 
 * CPU: Intel Broadwell microarchitecture, speed 2900 MHz (estimated)
 * Counted CPU_CLK_UNHALTED events (Clock cycles when not halted) with a unit mask of 0x00 (No unit mask) count 100000
 */
