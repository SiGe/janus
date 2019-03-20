# Things that can be done to improve the code
  - Probably can swap all malloc/memset pairs with calloc or some zmalloc
    sort of call.
  - Code is tightly coupled with jupiter topology (especially in the planning
    "plan.h" and "config.h", and other places) Relying on the fact that a
    alocated switch is a switch from the jupiter topology.  Have to fix it by
    relaying the calls to the network_t structure.

# To Improve
  - Adding Dynamic programming to the long-term planner
    - Dynamic programming (Reward is going to be n-step later)
    - Anytime that you have DP you have reinforcement learning

  - If you can provide some structure to your plans, then you don't need to
    search them both
  

# To Test
  - Testing just static traffic to flesh out the searching strategy
