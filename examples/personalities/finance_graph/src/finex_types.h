#ifndef __PERSONALITY_FINEX_TYPES__
#define __PERSONALITY_FINEX_TYPES__

namespace finex
{

struct Transaction
{
  unsigned char * key_source;
  unsigned char * key_target;
  unsigned char   date[16];
  double          time;
  double          amount;
  unsigned char   currency[6];
};

}

#endif // __PERSONALITY_FINEX_TYPES__
