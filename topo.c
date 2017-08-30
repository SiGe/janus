#include "types.h"
#include "topo.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINK_CAPACITY 1000000000.0

/* 
 * Update network topology using given nodes.
 * Capacity calculation is based on previous update plan.
 * Node id starts with a0.
 */
void network_update(struct network_t *network, int *node_ids, int node_nums)
{
    int k = network->k; int t_per_p = network->t_per_p;
    int a_per_p = network->a_per_p;
    int c_num = network->c_num;
    int tot_tors = k * t_per_p;
    int *capacity_reduce = (int *)malloc(sizeof(int) * (k+1));
    memset(capacity_reduce, 0, sizeof(int) * (k+1));

    for (int i = 0; i < node_nums; i++)
    {
        int node_id = node_ids[i];
        /* This is a aggregate switch */
        if (node_id < k * a_per_p)
        {
            int node_pod = node_id / a_per_p;
            capacity_reduce[node_pod]++;
        }
        /* This is a core switch */
        else
        {
            capacity_reduce[k]++;
        }
    }
    for (int i = 0; i < k; i++)
    {
        if (capacity_reduce[i] > 0)
        {
            for (int tor = i * t_per_p; tor < (i + 1) * t_per_p; tor++)
            {
                struct link_t *link = network->links + 2 * tor;
                link->capacity = (a_per_p - capacity_reduce[i]) * LINK_CAPACITY;
                link++;
                link->capacity = (a_per_p - capacity_reduce[i]) * LINK_CAPACITY;
            }
        }
    }
    if (capacity_reduce[k] > 0)
    {
        for (int agg = 0; agg < k; agg++)
        {
            struct link_t *link = network->links + 2 * agg + 2 * tot_tors;
            link->capacity = (a_per_p - capacity_reduce[agg]) * (c_num - capacity_reduce[k]) * LINK_CAPACITY;
            link++;
            link->capacity = (a_per_p - capacity_reduce[agg]) * (c_num - capacity_reduce[k]) * LINK_CAPACITY;
        }
    }
    return;
}

/* 
 * Restore the origin capacity of the network
 */
void network_restore(struct network_t *network)
{
    int k = network->k;
    int t_per_p = network->t_per_p;
    int a_per_p = network->a_per_p;
    int c_num = network->c_num;
    int tot_tors = k * t_per_p;

    for (int i = 0; i < tot_tors * 2; i++)
    {
        struct link_t *link = network->links + i;
        link->id = i;
        link->capacity = LINK_CAPACITY * a_per_p;
        link->used = 0;
    }
    for (int i = 0; i < 2 * k; i++)
    {
        struct link_t *link = network->links + tot_tors * 2 + i;
        link->id = i + tot_tors * 2;
        link->capacity = LINK_CAPACITY * a_per_p * c_num;
        link->used = 0;
    }
}

/*
 * Generate Google's watchtower topology based on given parameters.
 */
struct network_t *network_watchtower_gen(int k, int t_per_p, int a_per_p, int c_num)
{
    /* Initialize network */
    struct network_t *ret_network = (struct network_t *)malloc(sizeof(struct network_t));
    memset(ret_network, 0, sizeof(struct network_t));

    ret_network->k = k;
    ret_network->t_per_p = t_per_p;
    ret_network->a_per_p = a_per_p;
    ret_network->c_num = c_num;

    int tot_tors = k * t_per_p;

    /* Generate link in this network */
    ret_network->num_links = (tot_tors + k) * 2;
    ret_network->links = (struct link_t *)malloc(sizeof(struct link_t) * ret_network->num_links);
    memset(ret_network->links, 0, sizeof(struct link_t) * ret_network->num_links);

    for (int i = 0; i < tot_tors * 2; i++)
    {
        struct link_t *link = ret_network->links + i;
        link->id = i;
        link->capacity = LINK_CAPACITY * a_per_p;
        link->used = 0;
    }
    for (int i = 0; i < 2 * k; i++)
    {
        struct link_t *link = ret_network->links + tot_tors * 2 + i;
        link->id = i + tot_tors * 2;
        link->capacity = LINK_CAPACITY * a_per_p * c_num;
        link->used = 0;
    }

    /* Generate routing */
    ret_network->routing = (link_id_t *)malloc(sizeof(link_id_t) * (MAX_PATH_LENGTH+1) * tot_tors * (tot_tors - 1));
    memset(ret_network->routing, 0, sizeof(link_id_t) * (MAX_PATH_LENGTH+1) * tot_tors * (tot_tors - 1));

    link_id_t *routing = ret_network->routing;
    for (int src = 0; src < tot_tors; src++)
    {
        for (int dst = 0; dst < tot_tors; dst++)
        {
            if (src == dst)
                continue;
            int src_pod = src / t_per_p;
            int dst_pod = dst / t_per_p;
            if (src_pod == dst_pod)
            {
                *routing = 2;
                *(routing+1) = 2 * src;
                *(routing+2) = 2 * dst + 1;
            }
            else
            {
                *routing = 4;
                *(routing+1) = 2 * src;
                *(routing+2) = 2 * (src_pod + tot_tors);
                *(routing+3) = 2 * (dst_pod + tot_tors) + 1;
                *(routing+4) = 2 * dst + 1;
            }
            routing += (MAX_PATH_LENGTH + 1);
        }
    }

    return ret_network;
}

void print_routing(struct network_t *network)
{
    int tot_tors = network->k * network->t_per_p;
    for (int i = 0; i < tot_tors * (tot_tors - 1); i++)
    {
        for (int j = 1; j <= network->routing[i * (MAX_PATH_LENGTH+1)]; j++)
        {
            printf("%d\t", network->routing[i * (MAX_PATH_LENGTH+1) + j]);
        }
        printf("\n");
    }
}

void print_links(struct network_t *network)
{
    for (int i = 0; i < network->num_links; i++)
    {
        printf("%.0f ", network->links[i].capacity);
    }
    printf("\n");
}

void network_reset(struct network_t *network)
{
    network->fixed_flow_end = 0;
    network->fixed_link_end = 0;
    network->smallest_flow = NULL;
    network->smallest_link = NULL;
    struct link_t *link = 0;
    for (int i = 0; i < network->num_links; i++)
    {
        link = network->links + i;
        link->used = 0;
        link->nactive_flows = 0;
        link->nflows = 0;
        free(link->flows);
        link->flows = NULL;
        link->next = NULL;
        link->prev = NULL;
    }
}

void network_free(struct network_t *network) {
  if (network->routing) {
    free(network->routing);
    network->routing = 0;
  }

  if (network->links) {
    for (int i = 0; i < network->num_links; ++i) {
      free(network->links[i].flows);
    }
    free(network->links);
    network->links = 0;
  }

  if (network->flows) {
    free(network->flows);
    network->flows = 0;
  }
}
