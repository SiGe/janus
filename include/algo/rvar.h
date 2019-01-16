#ifndef _RANDVAR_H_          
#define  _RANDVAR_H_         

#include <stdint.h>

struct rvar_t {
  float    low, high;        
  uint32_t num_samples;      
  float    *vals;            
};
    

#endif // _RANDVAR_H_   
