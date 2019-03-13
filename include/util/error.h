#ifndef _UTIL_ERROR_H_
#define _UTIL_ERROR_H_

enum {
  /* Everything ... a ... ok! */
  E_OK = 0,

  /* Parsing errors */
  E_PARSE_ROUTING,
  E_PARSE_LINK,
  E_PARSE_FLOW,
  E_PARSE_UNEXPECTED,
};

#endif /* _ERROR_H_*/
