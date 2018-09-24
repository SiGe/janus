#include "../parse.h"
#include "../error.h"
#include "../topo.h"

#include "acutest.h"

void test_parse_input_01(void) {
  char *output = 0; struct network_t network = {0};
  read_file("../data/test_input_01.dat", &output);
  int err = parse_input(output, &network);
  TEST_CHECK_(err == E_OK, "got %d want %d", err, E_OK);

  TEST_CHECK(network.num_flows == 4);
  TEST_CHECK(network.num_links == 10);

  TEST_CHECK(network.routing[(MAX_PATH_LENGTH+1)*0] == 4);
  TEST_CHECK(network.routing[(MAX_PATH_LENGTH+1)*1] == 1);
  TEST_CHECK(network.routing[(MAX_PATH_LENGTH+1)*2] == 0);
  TEST_CHECK(network.routing[(MAX_PATH_LENGTH+1)*3] == 3);

  // TEST_CHECK(network.flows[0].nlinks == 4);
  // TEST_CHECK(network.flows[1].nlinks == 1);
  // TEST_CHECK(network.flows[2].nlinks == 0);
  // TEST_CHECK(network.flows[3].nlinks == 3);

  // TEST_CHECK(network.links[0].nactive_flows == 1);
  // TEST_CHECK(network.links[1].nactive_flows == 2);
  // TEST_CHECK(network.links[2].nactive_flows == 1);
  // TEST_CHECK(network.links[3].nactive_flows == 1);
  // TEST_CHECK(network.links[4].nactive_flows == 1);
  // TEST_CHECK(network.links[5].nactive_flows == 0);
  // TEST_CHECK(network.links[6].nactive_flows == 0);
  // TEST_CHECK(network.links[7].nactive_flows == 0);
  // TEST_CHECK(network.links[8].nactive_flows == 1);
  // TEST_CHECK(network.links[9].nactive_flows == 1);

  for (int i = 0; i < network.num_links; ++i) {
    TEST_CHECK(network.links[i].capacity == (i+1));
    TEST_CHECK(network.links[i].id == (i));
  }

  for (int i = 0; i < network.num_flows; ++i) {
    TEST_CHECK(network.flows[i].demand == (i+1) * 10);
    TEST_CHECK(network.flows[i].id == i);
  }

  network_free(&network);
}

void test_parse_input_02(void) {
  char *output = 0; struct network_t network = {0};
  read_file("../data/test_input_02.dat", &output);
  TEST_CHECK(parse_input(output, &network) == E_OK);

  TEST_CHECK(network.num_flows == 4032);
  TEST_CHECK(network.num_links == 144);

  network_free(&network);
}

void test_parse_input(void) {
  test_parse_input_01();
  test_parse_input_02();
}


TEST_LIST = {
  {"parse", test_parse_input},
  {0},
};
