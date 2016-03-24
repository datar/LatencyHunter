#pragma once

/* These are defined in socket.h, but older versions might not have all 3 */
#ifndef SIOCGSTAMPNS
# define SIOCGSTAMPNS 0x8907
#endif
#ifndef SIOCSHWTSTAMP
# define SIOCSHWTSTAMP 0x89b0
#endif
#ifndef SO_TIMESTAMP
  #define SO_TIMESTAMP            29
#endif
#ifndef SO_TIMESTAMPNS
  #define SO_TIMESTAMPNS          35
#endif
#ifndef SO_TIMESTAMPING
  #define SO_TIMESTAMPING         37
  # define SCM_TIMESTAMPING        SO_TIMESTAMPING
#endif

