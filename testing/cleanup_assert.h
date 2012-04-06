#ifndef CLEANUP_ASSERT_H
#define CLEANUP_ASSERT_H

/* WARNING:

   Only the last parameter to these macros are evaluated only once.
   Make sure arguments to all other parameters are idempotent.

*/

#define CLEANUP_ASSERT_LT(a, b, clean) \
    do if ( (a) >= (b) ) { clean ASSERT_LT(a, b); } while (0)
#define CLEANUP_ASSERT_LE(a, b, clean) \
    do if ( (a) > (b) ) { clean ASSERT_LE(a, b); } while (0)
#define CLEANUP_ASSERT_EQ(a, b, clean) \
    do if ( (a) != (b) ) { clean ASSERT_EQ(a, b); } while (0)
#define CLEANUP_ASSERT_GE(a, b, clean) \
    do if ( (a) < (b) ) { clean ASSERT_GE(a, b); } while (0)
#define CLEANUP_ASSERT_GT(a, b, clean) \
    do if ( (a) <= (b) ) { clean ASSERT_GT(a, b); } while (0)
#define CLEANUP_ASSERT_TRUE(b, clean) \
    do if ( !(b) ) { clean ASSERT_TRUE(b); } while (0)
#define CLEANUP_ASSERT_FALSE(b, clean) \
    do if ( (b) ) { clean ASSERT_FALSE(b); } while (0)

#endif
