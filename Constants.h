#pragma once

#ifndef SO_TIMESTAMPING
# define SO_TIMESTAMPING         37
# define SCM_TIMESTAMPING        SO_TIMESTAMPING
#endif

#ifndef SO_TIMESTAMPNS
# define SO_TIMESTAMPNS 35
#endif

#ifndef SIOCGSTAMPNS
# define SIOCGSTAMPNS 0x8907
#endif

#ifndef SIOCSHWTSTAMP
# define SIOCSHWTSTAMP 0x89b0
#endif

static const unsigned char sync[] = {
	0x00, 0x01, 0x00, 0x01,
	0x5f, 0x44, 0x46, 0x4c,
	0x54, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x01, 0x01,

	/* fake uuid */
	0x00, 0x01,
	0x02, 0x03, 0x04, 0x05,

	0x00, 0x01, 0x00, 0x37,
	0x00, 0x00, 0x00, 0x08,
	0x00, 0x00, 0x00, 0x00,
	0x49, 0x05, 0xcd, 0x01,
	0x29, 0xb1, 0x8d, 0xb0,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x01,

	/* fake uuid */
	0x00, 0x01,
	0x02, 0x03, 0x04, 0x05,

	0x00, 0x00, 0x00, 0x37,
	0x00, 0x00, 0x00, 0x04,
	0x44, 0x46, 0x4c, 0x54,
	0x00, 0x00, 0xf0, 0x60,
	0x00, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0xf0, 0x60,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x04,
	0x44, 0x46, 0x4c, 0x54,
	0x00, 0x01,

	/* fake uuid */
	0x00, 0x01,
	0x02, 0x03, 0x04, 0x05,

	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};