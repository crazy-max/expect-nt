/*
UNIX: gcc -g -I/home/rganesan/tcl/expect-5.26
         -I/home/rganesan/tcl/tcl8.0/generic -o exp exp.c
         -L/home/rganesan/tcl/expect-5.26 -lexpect5.26
         -lm -L/home/rganesan/tcl/tcl8.0/unix -ltcl8.0
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "expect.h"
#include "exp_log.h"

void expect_init (void) ;
int main ( int argc, char **argv ) ;
int dev_login ( char * a_dev_id, char * a_login, char * a_passwd ) ;
int dev_cmd_execute ( char * a_cmd ) ;
void exp_error ( int a_line ) ;

void
expect_init ()
{
        char    l_fname [L_tmpnam]      ;

#ifdef XXX
        exp_loguser = 1 ;
#endif

        if ( tmpnam ( l_fname ) != l_fname )
                exp_error ( - __LINE__ ) ;

        printf ( "Temp file is %s.\n", l_fname ) ;
#ifdef UNIX
        exp_logfile = fopen ( l_fname, "wb" ) ;
#else
        /* exp_logfile = exp_popen ( l_fname, "wb" ) ; */ /* ==> 2 */
#endif

#ifdef XXX
        exp_logfile_all = 0 ;
#endif
        exp_timeout = 3600;

#ifndef UNIX
        exp_nt_debug = 1 ;
#endif
}


void
exp_error ( int a_line )
{
        printf ( "Error returned for line %d.\n", - a_line ) ;
        exit (1) ;
}

#ifdef UNIX
int     g_fd    ;
#else
ExpHandle g_fd ;
#define write(a,b,c)    exp_write(a,b,c)
#endif

int
main ( int a_argc, char ** a_argv )
{
        int     l_ret   ;

        printf ( "start.\n") ;

	exp_setdebug("debug.log", 1);

        expect_init () ;

        printf ( "expect_init successful.\n" ) ;

        l_ret = dev_login ( "zeego", "chaffee\r", "password\r" ) ;
        if ( !! l_ret )
                exp_error ( l_ret ) ;

        printf ( "dev_login susccessful.\n" ) ;

        l_ret = dev_cmd_execute ( "ls -al /tmp\r" ) ;
        if ( !! l_ret )
                exp_error ( l_ret ) ;

        printf ( "Command execution successful.\n" ) ;

        return 0 ;
}

int
dev_cmd_execute ( char * a_cmd )
{
        if ( write ( g_fd, a_cmd, strlen ( a_cmd ) ) == -1 )
                return ( - __LINE__ ) ;


        if ( exp_expectl ( g_fd, exp_regexp, "(.*)]", 1, exp_end ) )
                return 0 ;
        else
                return ( - __LINE__ ) ;

}


int
dev_login ( char * a_dev, char * a_login, char * a_passwd )
{
#ifdef UNIX
        g_fd = exp_spawnl ( "telnet", "telnet", a_dev, (char *)0 ) ;
#else
        g_fd = exp_spawnl ( "telnet.exe", "telnet.exe", a_dev, (char *)0 ) ;
#endif
        if ( g_fd == NULL )
                return ( - __LINE__ ) ;

        printf ( "exp_spawnl successful.\n" ) ;

        if ( exp_expectl ( g_fd, exp_glob, "login:", 0, exp_end ) )
                return ( - __LINE__ ) ;

        if ( write ( g_fd, a_login, strlen ( a_login ) ) == -1 )
                return ( - __LINE__ ) ;

        if ( exp_expectl ( g_fd, exp_glob, "Password:", 0, exp_end ) )
                return ( - __LINE__ ) ;

        if ( write ( g_fd, a_passwd, strlen ( a_passwd ) ) == -1 )
                return ( - __LINE__ ) ;

        if ( exp_expectl ( g_fd, exp_regexp, "(.*)]", 0, exp_end ) )
                return ( - __LINE__ ) ;

        return 0 ;
}


/*

Output:

start.
Temp file is \s4d..
expect_init successful.
spawn c:\Gans\telnet.exe device
Error returned for line 112.
Press any key to continue

*/
