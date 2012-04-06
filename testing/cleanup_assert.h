// Copyright 2011 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
