/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/*
 *  termios structure
 */
#ifndef _SYS_TERMIOS_H_
#define _SYS_TERMIOS_H_

typedef unsigned int    tcflag_t;
typedef unsigned char   cc_t;
typedef unsigned int    speed_t;


/* 
 * Special Control Characters 
 *
 * Index into c_cc[] character array.
 *
 *	Name	     Subscript	Enabled by 
 */
#define	VEOF		0	/* ICANON */
#define	VEOL		1	/* ICANON */

#define	VEOL2		2	/* ICANON */

#define	VERASE		3	/* ICANON */

#define VWERASE 	4	/* ICANON */

#define VKILL		5	/* ICANON */

#define	VREPRINT 	6	/* ICANON */

/*			7	   spare 1 */

#define VINTR		8	/* ISIG */
#define VQUIT		9	/* ISIG */
#define VSUSP		10	/* ISIG */

#define VDSUSP		11	/* ISIG */

#define VSTART		12	/* IXON, IXOFF */
#define VSTOP		13	/* IXON, IXOFF */

#define	VLNEXT		14	/* IEXTEN */
#define	VDISCARD	15	/* IEXTEN */
#define	VFLUSH		VDISCARD /* for sun */

#define VMIN		16	/* !ICANON */
#define VTIME		17	/* !ICANON */

#define VSTATUS		18	/* ISIG */

/*			19	   spare 2 */

#define	NCCS		20

/*
 * Ioctl control packet
 */
struct termios {
	tcflag_t	c_iflag;	/* input flags */
	tcflag_t	c_oflag;	/* output flags */
	tcflag_t	c_cflag;	/* control flags */
	tcflag_t	c_lflag;	/* local flags */
	cc_t		c_cc[NCCS];	/* control chars */
	int		c_ispeed;	/* input speed */
	int		c_ospeed;	/* output speed */
};

#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE	(0377)
#endif	/* _POSIX_VDISABLE */

/*
 * Standard speeds
 */
#define B0	0
#define B50	50
#define B75	75
#define B110	110
#define B134	134
#define B150	150
#define B200	200
#define B300	300
#define B600	600
#define B1200	1200
#define	B1800	1800
#define B2400	2400
#define B4800	4800
#define B9600	9600
#define B19200	19200
#define B38400	38400

#endif /* _SYS_TERMIOS_H_ */
