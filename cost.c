#include "cost.h"

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

/* params
 * Traffic file
 * Coeff
 * Guarantee budget
 * Topology parameter: k, t_per_p, a_per_p, c_num
 * # of cache pods
 * Cache pod's agg switch id
 * ...
 * Minimum #, maximum #, step
 * # of agg pods
 * Webserver pod's agg switch id
 * ...
 * Minimum #, maximum #, step
 * Core switch first id
 * Minimum #, maximum #, step
 */

void parse_param(FILE *f, struct selected_param_t *param)
{
    memset(param, 0, sizeof(struct selected_param_t));
    param->traffic_file = (char *)malloc(100);
    memset(param->traffic_file, 0, 100);

    fscanf(f, "%s\n", param->traffic_file);
    fscanf(f, "%d\n", &param->coeff);
    fscanf(f, "%lf\n", &param->guarantee_bw);
    fscanf(f, "%d\t%d\t%d\t%d\n", &param->k, &param->t_per_p, &param->a_per_p, &param->c_num);

    fscanf(f, "%d\n", &param->group_len);
    param->groups = (struct param_symmetry_groups_t*)malloc(sizeof(struct param_symmetry_groups_t) * param->group_len);

    for (int i = 0; i < param->group_len; i++)
    {
        fscanf(f, "%d\n", &param->groups[i].id_length);
        param->groups[i].ids = (int *)malloc(sizeof(int) * param->groups[i].id_length);
        for (int j = 0; j < param->groups[i].id_length; j++)
            fscanf(f, "%d\n", &param->groups[i].ids[j]);
        fscanf(f, "%d\t%d\t%d\n", &param->groups[i].min, &param->groups[i].max, &param->groups[i].step);
    }

    param->max_node_num = 0;
    for (int i = 0; i < param->group_len; i++)
        param->max_node_num += param->groups[i].max * param->groups[i].id_length;
    
    if (1)
    {
        printf("---print parameters---\n");
        printf("traffic file: %s\n", param->traffic_file);
        printf("coeff: %d\n", param->coeff);
        printf("guarantee bandwidth: %lf\n", param->guarantee_bw);
        printf("topology paramter: %d, %d, %d, %d\n", param->k, param->t_per_p, param->a_per_p, param->c_num);
        printf("there are %d symmetry groups\n", param->group_len);
        for (int i = 0; i < param->group_len; i++)
        {
            printf("there are %d ids, they are ", param->groups[i].id_length);
            for (int j = 0; j < param->groups[i].id_length; j++)
                printf("%d ", param->groups[i].ids[j]);
            printf("\n");
            printf("min max step: %d %d %d\n", param->groups[i].min, param->groups[i].max, param->groups[i].step);
        }
    }
}

void selected_parallel_calculate(void *vargp)
{
    struct selected_paral_param_t *param = vargp;
    struct network_t *test_network = network_watchtower_gen(
                                    param->k,
                                    param->t_per_p,
                                    param->a_per_p,
                                    param->c_num);
    printf("%d\n", param->para_idx);
    //for (int i = 0; i < param->group_len; i++)
    //    printf("%d ", param->groups[i].upgrade_num);

    int *upgrade_nodes = malloc(sizeof(int) * param->tot_node_num);
    int upgrade_nodes_pos = 0;
    for (int i = 0; i < param->group_len; i++)
    {
        for (int j = 0; j < param->groups[i].id_length; j++)
        {
            for (int k = 0; k < param->groups[i].upgrade_num; k++)
            {
                upgrade_nodes[upgrade_nodes_pos] = param->groups[i].ids[j] + k;
                upgrade_nodes_pos ++;
            }
        }
    }
    if (upgrade_nodes_pos != param->tot_node_num)
    {
        printf("calculation is wrong here\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < param->tot_node_num; i++)
    {
        param->upgrade_nodes[i] = upgrade_nodes[i];
    }
    for (int i = param->tot_node_num; i < param->upgrade_node_len; i++)
    {
        param->upgrade_nodes[i] = -1;
    }

    struct traffic_t *traffic = param->traffic;
    network_update(test_network, upgrade_nodes, param->tot_node_num);
    for (int i = 0; i < traffic->tm_num; i++)
    {
        network_reset(test_network);
        build_flow(test_network, traffic, i);
        maxmin(test_network);
        int ret = network_slo_violation(test_network, param->guarantee_bw);
        param->ret_data[i] = ret;
    }
    network_free(test_network);
    free(vargp);

    /*
    for (int i = 0; i < param->tot_node_num; i++)
        printf("%d ", upgrade_nodes[i]);
    printf("\n");
    */

}

void selected_subplan_seq_cost(char *input_param_file, char *output_result)
{
    FILE *f = fopen(input_param_file, "r");
    struct selected_param_t *param = malloc(sizeof(struct selected_param_t));
    parse_param(f, param);
    fclose(f);
    struct network_t *test_network = network_watchtower_gen(param->k, param->t_per_p, param->a_per_p, param->c_num);
    struct traffic_t *traffic = traffic_load(param->traffic_file, test_network, param->coeff);

    int subplan_num = 1;
    int *subplan_lengths = malloc(sizeof(int) * param->group_len);
    for (int i = 0; i < param->group_len; i++)
    {
        struct param_symmetry_groups_t tmp = param->groups[i];
        subplan_lengths[i] = ((int)(tmp.max - tmp.min) / tmp.step) + 1;
        subplan_num *= subplan_lengths[i];
    }
    printf("there are %d total subplans\n", subplan_num);
    printf("each length is :");
    for (int i = 0; i < param->group_len; i++)
        printf("%d ", subplan_lengths[i]);
    printf("\n");

    threadpool thpool = thpool_init(sysconf(_SC_NPROCESSORS_ONLN) - 1);
    //threadpool thpool = thpool_init(1);
    int **data = malloc(sizeof(int *) * subplan_num);
    int **nodes = malloc(sizeof(int *) * subplan_num);
    for (int i = 0; i < subplan_num; i++)
    {
        nodes[i] = malloc(sizeof(int) * param->max_node_num);
        memset(nodes[i], -1, param->max_node_num);
    }

    for (int i = 0; i < subplan_num; i++)
    {
        struct selected_paral_param_t *p = malloc(sizeof(struct selected_paral_param_t));

        p->k = param->k;
        p->t_per_p = param->t_per_p;
        p->a_per_p = param->a_per_p;
        p->c_num = param->c_num;
        p->guarantee_bw = param->guarantee_bw;
        p->traffic = traffic;

        p->group_len = param->group_len;
        p->groups = malloc(sizeof(struct paral_symmetry_groups_t) * p->group_len);

        p->tot_node_num = 0;

        int nodes_pos = 0;

        for (int j = 0; j < p->group_len; j++)
        {
            struct paral_symmetry_groups_t *tmp = p->groups + j;
            struct param_symmetry_groups_t tmp_2 = param->groups[j];
            tmp->id_length = tmp_2.id_length;
            tmp->ids = tmp_2.ids;
            tmp->upgrade_num = i;
            for (int k = p->group_len - 1; k > j; k--)
            {
                tmp->upgrade_num /= subplan_lengths[k];
                //printf("%d ", tmp->upgrade_num);
            }
            tmp->upgrade_num %= subplan_lengths[j];
            tmp->upgrade_num = tmp->upgrade_num * tmp_2.step + tmp_2.min;

            p->tot_node_num += tmp->upgrade_num * tmp_2.id_length;
        }

        p->para_idx = i;

        data[i] = malloc(sizeof(int) * traffic->tm_num);
        p->ret_data = data[i];

        p->upgrade_nodes = nodes[i];
        p->upgrade_node_len = param->max_node_num;

        printf("idx: %d, total upgrade %d nodes, upgrade set: ", i, p->tot_node_num);
        for (int j = 0; j < p->group_len; j++)
            printf("%d ", p->groups[j].upgrade_num);
        printf("\n");
        thpool_add_work(thpool, selected_parallel_calculate, p);
    }
    thpool_wait(thpool);
    f = fopen(output_result, "w+");
    for (int i = 0; i < subplan_num; i++)
    {
        for (int j = 0; j < param->max_node_num; j++)
        {
            if (nodes[i][j] != -1)
                fprintf(f, "%d ", nodes[i][j]);
            else
                break;
        }
        fprintf(f, "\n");
        for (int j = 0; j < traffic->tm_num; j++)
            fprintf(f, "%d ", data[i][j]);
        fprintf(f, "\n");
    }
    fclose(f);
}

void parse_param_error(FILE *f, struct selected_param_error_t *param)
{
    memset(param, 0, sizeof(struct selected_param_error_t));
    parse_param(f, &param->norm_para);
    fscanf(f, "%d\n", &param->cur_time);
    fscanf(f, "%d\n", &param->predict_tm_num);
    param->update_nums = malloc(sizeof(int) * param->norm_para.group_len);
    for (int i = 0; i < param->norm_para.group_len; i++)
        fscanf(f, "%d\n", &param->update_nums[i]);
    param->error_folder= (char *)malloc(100);
    fscanf(f, "%s\n", param->error_folder);
    fscanf(f, "%d\n", &param->x);
    if (1)
    {
        printf("curr time: %d\n", param->cur_time);
        printf("predict tm numn: %d\n", param->predict_tm_num);
        printf("each group's update num: ");
        for (int i = 0; i < param->norm_para.group_len; i++)
            printf("%d ", param->update_nums[i]);
        printf("\n");
        printf("error file: %s\n", param->error_folder);
        printf("threshold x: %d\n", param->x);
    }
}

void selected_parallel_calculate_error(void *vargp)
{
    struct selected_paral_param_error_t *p = vargp;
    printf("%d\n", p->para_idx);
    struct network_t *network = network_watchtower_gen(
            p->k,
            p->t_per_p,
            p->a_per_p,
            p->c_num);
    network_update(network, p->upgrade_node_ids, p->upgrade_id_length);
    clock_t start, end;
    for (int i = 0; i < p->sample_num; i++)
    {
        p->ret_data[i] = 0;
        for (int j = 0; j < p->error->predict_num; j++)
        {
            //printf("%d %d %d\n", p->para_idx, i, j);
            struct tm_t *tm = p->tms[j];
            network_reset(network);
            start = clock();
            build_flow_error_range(network, tm, p->error, p->error_seqs[i], j);
            end = clock();
            p->build_flow_time += ((double)(end - start)) / CLOCKS_PER_SEC;
            //write_flows(network);
            start = clock();
            maxmin(network);
            int ret = network_slo_violation(network, p->guarantee_bw);
            end = clock();
            p->simulate_network_time += ((double)(end - start)) / CLOCKS_PER_SEC;
            if (ret > p->x)
                p->ret_data[i]++;
        }
    }
    /*
    for (int i = 0; i < p->sample_num; i++)
        printf("%d ", p->ret_data[i]);
    printf("\n");
    */
    free(p);
    network_free(network);
}

void selected_subplan_seq_cost_error(char *input_param_file, char *output_result)
{
    struct selected_param_error_t *param = malloc(sizeof(struct selected_param_error_t));
    FILE *f = fopen(input_param_file, "r");
    parse_param_error(f, param);
    fclose(f);
    struct selected_param_t *norm_para = &param->norm_para;
    struct network_t *test_network = network_watchtower_gen(
            norm_para->k,
            norm_para->t_per_p,
            norm_para->a_per_p,
            norm_para->c_num);
    struct traffic_t *traffic = traffic_load(
            norm_para->traffic_file, 
            test_network, 
            norm_para->coeff);

    int subplan_num = 1;
    int *subplan_lengths = malloc(sizeof(int) * norm_para->group_len);
    int **subplan_offsets = malloc(sizeof(int *) * norm_para->group_len);
    int maximum_length = 0;
    log_with_time("Assigning subplans\n");
    for (int i = 0; i < norm_para->group_len; i++)
    {
        printf("%d\n", i);
        struct param_symmetry_groups_t tmp = norm_para->groups[i];
        printf("%d %d %d\n", tmp.min, tmp.max, tmp.step);
        // Here has a concern that if I have only one core left, but core_min is 2
        if (param->update_nums[i] > tmp.max)
            param->update_nums[i] = tmp.max;
        if (param->update_nums[i] <= tmp.min)
            subplan_lengths[i] = 1;
        else
        {
            subplan_lengths[i] = ((int)(param->update_nums[i] - tmp.min) / tmp.step) + 1;
            if (param->update_nums[i] % tmp.step != 0)
                //Here add the maxmum possible to ease the calculation.
                subplan_lengths[i]++;
        }
        subplan_num *= subplan_lengths[i];
        subplan_offsets[i] = malloc(sizeof(int) * subplan_lengths[i]);
        for (int j = 0; j < subplan_lengths[i]; j++)
        {
            if (param->update_nums[i] <= tmp.min)
                subplan_offsets[i][j] = tmp.min;
            else
            {
                int off = j * tmp.step + tmp.min;
                subplan_offsets[i][j] = off > param->update_nums[i] ? param->update_nums[i] : off;
            }
        }
        maximum_length += param->update_nums[i] * tmp.id_length;
    }
    log_with_time("Assign subplans complete\n");
    printf("there are %d total subplans\n", subplan_num);
    printf("each length and offset is:\n");
    for (int i = 0; i < norm_para->group_len; i++)
    {
        printf("%d: ", subplan_lengths[i]);
        for (int j = 0; j < subplan_lengths[i]; j++)
            printf("%d ", subplan_offsets[i][j]);
        printf("\n");
    }
    printf("maximum length is %d\n", maximum_length);

    threadpool thpool = thpool_init(sysconf(_SC_NPROCESSORS_ONLN) - 1);
    //threadpool thpool = thpool_init(1);
    

    // Used to write to file
    int **nodes = malloc(sizeof(int *) * subplan_num);
    for (int i = 0; i < subplan_num; i++)
    {
        nodes[i] = malloc(sizeof(int) * maximum_length);
        memset(nodes[i], -1, sizeof(int) * maximum_length);
    }

    int *error_seqs = malloc(sizeof(int) * param->predict_tm_num);
    for (int i = 0; i < param->predict_tm_num; i++)
        error_seqs[i] = param->cur_time - param->predict_tm_num / 2 + i;
    struct error_range_t *error = malloc(sizeof(struct error_range_t));
    log_with_time("parsing error\n");
    parse_error_range(param->error_folder, error, error_seqs[0], error_seqs[param->predict_tm_num-1]);
    log_with_time("parse error done \n");

    log_with_time("Building ewma flow\n");
    struct tm_t **tms = build_flow_ewma(traffic, param->cur_time, error->sd_pair_num);
    log_with_time("Build ewma flow done\n");

    int **data = malloc(sizeof(int *) * subplan_num);

    log_with_time("Predicting and simulate cost\n");
    struct selected_paral_param_error_t **param_vector = NULL;
    param_vector = malloc(sizeof(struct selected_paral_param_error_t*) * subplan_num);
    for (int i = 0; i < subplan_num; i++)
    {
        data[i] = malloc(sizeof(int) * param->predict_tm_num);
        struct selected_paral_param_error_t *p = 
            malloc(sizeof(struct selected_paral_param_error_t));
        param_vector[i] = p;
        p->ret_data = data[i];
        p->k = norm_para->k;
        p->t_per_p = norm_para->t_per_p;
        p->a_per_p = norm_para->a_per_p;
        p->c_num = norm_para->c_num;
        p->guarantee_bw = norm_para->guarantee_bw;
        p->traffic = traffic;
        p->para_idx = i;

        p->tms = tms;
        p->sample_num = param->predict_tm_num;
        p->error_seqs = error_seqs;
        p->x = param->x;
        p->error = error;
        p->upgrade_id_length = 0;
        p->upgrade_node_ids = nodes[i];
        p->build_flow_time = 0;
        p->simulate_network_time = 0;
        int pos = 0;
        //deal with update node ids
        // Decide how many nodes to upgrade here
        for (int j = 0; j < norm_para->group_len; j++)
        {
            int upgrade_num = i;
            for (int k = norm_para->group_len - 1; k > j; k--)
                upgrade_num /= subplan_lengths[k];
            upgrade_num %= subplan_lengths[j];
            p->upgrade_id_length += subplan_offsets[j][upgrade_num] * norm_para->groups[j].id_length;
            for (int m = 0; m < norm_para->groups[j].id_length; m++)
            {
                for (int n = 0; n < subplan_offsets[j][upgrade_num]; n++)
                {
                    p->upgrade_node_ids[pos] = norm_para->groups[j].ids[m]+n;
                    pos++;
                }
            }
        }
        //printf("subplan %d, %d ndoes are upgraded, they are:\n", i, p->upgrade_id_length);
        //for (int j = 0; j < p->upgrade_id_length; j++)
        //    printf("%d ", p->upgrade_node_ids[j]);
        //printf("\n");
        //TODO: node ids are already assigned, next should calculate each subplan's error cost

        thpool_add_work(thpool, selected_parallel_calculate_error, p);
    }
    thpool_wait(thpool);
    log_with_time("Predict and simulate cost done\n");
    f = fopen(output_result, "w+");
    for (int i = 0; i < subplan_num; i++)
    {
        for (int j = 0; j < maximum_length; j++)
        {
            if (nodes[i][j] != -1)
                fprintf(f, "%d ", nodes[i][j]);
            else
                break;
        }
        fprintf(f, "\n");
        for (int j = 0; j < param->predict_tm_num; j++)
        {
            fprintf(f, "%d ", data[i][j]);
        }
        fprintf(f, "\n");
    }
    fclose(f);
    double tot_build_flow_time = 0;
    double tot_simulate_time = 0;
    for (int i = 0; i < subplan_num; i++)
    {
        tot_build_flow_time += param_vector[i]->build_flow_time;
        tot_simulate_time += param_vector[i]->simulate_network_time;
    }
    printf("Build flow time is %f\n", tot_build_flow_time);
    printf("Simulate time is %f\n", tot_simulate_time);
}

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

void all_subplan_seq_cost(int coeff, double guarantee, char* file_name, int symmetry_length, char *traffic_file)
{
  //int symmetric_groups_norm[4] = {4,4,4,4};
  int symmetric_groups_norm[4] = {5,5,5,5};
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
  threadpool thpool = thpool_init(sysconf(_SC_NPROCESSORS_ONLN));
  //threadpool thpool = thpool_init(1);
  struct network_t *test_network = network_watchtower_gen(8, 12, 6, 6);
  if (traffic_file == NULL)
      traffic_file = "./traffic/webserver_traffic_30s_8p_12t_sorted.tsv";
  struct traffic_t *traffic = traffic_load(traffic_file, test_network, coeff);
  //struct traffic_t *traffic = traffic_load("./traffic/hadoop_traffic_30s_8p_12t_sorted_new.tsv", test_network, coeff);
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

    network_update(network, update_nodes, node_num);
    for (int i = 0; i < param->sample_num; i++)
    {
        param->data[i] = 0;
        for (int j = 0; j < param->errors->predict_num; j++)
        {
            struct tm_t *tm = param->tms[j];
            network_reset(network);
            if (param->noise_volume != NULL)
                build_flow_error_with_noise(network, tm, param->errors, param->error_seqs[i], j, param->noise_volume, param->noise_level);
            else
            {
                //printf("building error wo noise\n");
                build_flow_error(network, tm, param->errors, param->error_seqs[i], j);
            }
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
        int symmetry_length, char* noise_file, double noise_level)
{

    struct error_t *error = malloc(sizeof(struct error_t));
    //parse_error("./error_2500.txt", error);
    parse_error(error_file, error);

    struct network_t *network = network_watchtower_gen(8, 12, 6, 6);
    //struct traffic_t *traffic = traffic_load("../traffic/webserver_traffic_30s_8p_12t_sorted.tsv", network, 2500);
    struct traffic_t *traffic = traffic_load(traffic_file, network, coeff);

    int tot_subplans = 0;
    int update_groups_norm[4] = {0, 12, 42, 48};
    int update_groups_sym[4] = {0, 12, 42, 48};
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
    threadpool thpool = thpool_init(sysconf(_SC_NPROCESSORS_ONLN));
    //threadpool thpool = thpool_init(19);
    //threadpool thpool = thpool_init(1);

    printf("Init done, parallizing\n");

    int **ret_data = malloc(sizeof(int *) * tot_subplans);

    int *error_seqs = malloc(sizeof(int) * sample_num);
    for (int i = 0; i < sample_num; i++)
        error_seqs[i] = error_time - sample_num / 2 + i - 100;

    struct tm_t **tms = NULL;
    if (noise_file == NULL)
    {
        tms = build_flow_ewma(traffic, error_time, error->sd_pair_num);
    }
    else
    {
        tms = build_flow_ewma_noise(traffic, error_time, error->sd_pair_num, noise_file, coeff, noise_level);
    }

    double *noise_volume = NULL;
    /*
    if (noise_file != NULL)
    {
        noise_volume = malloc(sizeof(double) * error->sd_pair_num);
        FILE *f = fopen(noise_file, "r+");
        for (int i = 0; i < error->sd_pair_num; i++)
        {
            fscanf(f, "%lf\n", &noise_volume[i]);
            noise_volume[i] *= coeff;
        }
    }
    */

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
        param->noise_volume = noise_volume;
        param->noise_level = noise_level;

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
    printf("Error parallize done\n");
}
