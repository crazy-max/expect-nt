#include "expect.h"
#include <signal.h>

int
test1(void)
{
    void *ehandle;
    int ret;
    int status;

    /* This should give us a NULL value */
    ehandle = exp_spawnl("bad.exe", "zeego", NULL);
    if (ehandle) {
	printf("Problem: running zeego should have failed\n");
	return 1;
    }

    exp_nt_debug = 0;

    ehandle = exp_spawnl("telnet.exe", "telnet.exe", "zeego", NULL);
    printf("ehandle: %p\n", ehandle);
    exp_nt_debug = 0;
    if (ehandle == NULL) {
	return 1;
    }

    exp_timeout = 10000;
    ret = exp_expectl(ehandle,
	exp_glob,"login:",0,
	exp_glob,"Unable to connect",1,
	exp_glob,"Host not found",2,
	exp_glob,"Connection closed by foreign host",3,
	exp_end);

    printf("exp_expectl() => %d\n", ret);
    if (ret != 0) {
	return 2;
    }

    printf("***Saw login:\n");
    exp_printf(ehandle, "chaffee\r");

    ret = exp_expectl(ehandle,
	exp_glob,"Password:",0,
	exp_glob,"Connection closed by foreign host",3,
	exp_end);

    if (ret != 0) {
	return 3;
    }

    printf("***Saw Password:\n");
    exp_printf(ehandle, "wrong\r");

    ret = exp_expectl(ehandle,
	exp_glob,"Login incorrect",0,
	exp_glob,"Connection closed by foreign host",3,
	exp_end);

    if (ret != 0) {
	return 4;
    }
    printf("***Saw Login incorrect\n");

    exp_closefd(ehandle);

    exp_waitpid(exp_pid, &status, 0);

    return 0;
}

int
test2(void)
{
    void *ehandle;
    int status;

    ehandle = exp_spawnl("sleep.exe", "sleep.exe", "100", NULL);
    printf("ehandle: %p\n", ehandle);
    if (ehandle == NULL) {
	printf("No sleep.exe in your path\n");
	return 1;
    }

    exp_waitpid(exp_pid, &status, WNOHANG);

    exp_kill(exp_pid, SIGBREAK);
    exp_waitpid(exp_pid, &status, 0);

    return 0;
}

int
test3(void)
{
    void *ehandle;
    int ret;
    int status;

    ehandle = exp_popen("echo yes");
    printf("ehandle: %p\n", ehandle);
    if (ehandle == NULL) {
	printf("exp_popen failed\n");
	return 1;
    }
    ret = exp_expectl(ehandle, exp_glob, "yes", 0, exp_end);
    if (ret != 0) {
	printf("exp_expectl failed\n");
	return 2;
    }

    /* Flush the rest of the output so we can't hang */
    exp_timeout = 1;
    ret = exp_expectl(ehandle, exp_glob, "no", 0, exp_end);
    if (ret == 0) {
	printf("exp_expectl failed\n");
	return 3;
    }
    exp_waitpid(exp_pid, &status, 0);

    return 0;
}

int
test4(void)
{
    void *ehandle;

    ehandle = exp_spawnl("bogus.exe", "bogus.exe", NULL);
    if (ehandle == NULL) {
	exp_perror("test4");
	return 0;
    }
    return 1;
}

int
test5(void)
{
    void *ehandle;
    int ret;
    int status;

    exp_nt_debug = 1;
    ehandle = exp_spawnl("testsig.exe", "testsig.exe", NULL);
    exp_nt_debug = 0;
    if (ehandle == NULL) {
	exp_perror("testsig");
	return 0;
    }
    exp_kill(exp_pid, SIGINT);
    ret = exp_expectl(ehandle, exp_glob, "Received Ctrl-C", 0, exp_end);
    if (ret == 0) {
	printf("exp_expectl failed\n");
	return 1;
    }
    exp_kill(exp_pid, SIGBREAK);
    exp_waitpid(exp_pid, &status, 0);

    return 1;
}

int
test6(void)
{
    void *ehandle;
    int ret;
    int status;
    struct exp_case ecases[20];

    exp_nt_debug = 0;

    ehandle = exp_spawnl("telnet.exe", "telnet.exe", "zeego", NULL);
    printf("ehandle: %p\n", ehandle);
    exp_nt_debug = 0;
    if (ehandle == NULL) {
	return 1;
    }

    exp_timeout = 10000;

    ecases[0].pattern = "login:";
    ecases[0].re = NULL;
    ecases[0].type = exp_glob;
    ecases[0].value = 0;

    ecases[1].pattern = "Unable to connect";
    ecases[1].re = NULL;
    ecases[1].type = exp_glob;
    ecases[1].value = 1;

    ecases[2].pattern = "Host not found";
    ecases[2].re = NULL;
    ecases[2].type = exp_glob;
    ecases[2].value = 2;

    ecases[3].pattern = "Connection closed by foreign host";
    ecases[3].re = NULL;
    ecases[3].type = exp_glob;
    ecases[3].value = 3;

    ecases[4].type = exp_end;

    ret = exp_expectv(ehandle, ecases);

    printf("exp_expectv() => %d\n", ret);
    if (ret != 0) {
	return 2;
    }

    printf("***Saw login:\n");
    exp_printf(ehandle, "chaffee\r");

    ret = exp_expectl(ehandle,
	exp_glob,"Password:",0,
	exp_glob,"Connection closed by foreign host",3,
	exp_end);

    if (ret != 0) {
	return 3;
    }

    printf("***Saw Password:\n");
    exp_printf(ehandle, "wrong\r");

    ret = exp_expectl(ehandle,
	exp_glob,"Login incorrect",0,
	exp_glob,"Connection closed by foreign host",3,
	exp_end);

    if (ret != 0) {
	return 4;
    }
    printf("***Saw Login incorrect\n");

    exp_closefd(ehandle);

    exp_waitpid(exp_pid, &status, 0);

    return 0;
}

int
test7(void)
{
    void *ehandle;
    char buffer[1024];
    int n;

    exp_nt_debug = 0;

    ehandle = exp_spawnl("telnet.exe", "telnet.exe", "zeego", NULL);
    printf("ehandle: %p\n", ehandle);
    exp_nt_debug = 0;
    if (ehandle == NULL) {
	return 1;
    }

    n = exp_read(ehandle, buffer, sizeof(buffer)-1);
    buffer[n] = 0;
    printf("exp_read returns %d: %s\n", n, buffer);

    exp_closefd(ehandle);
    return 0;
}

int
test8(void)
{
    void *ehandle;

    exp_nt_debug = 1;

    ehandle = exp_spawnl("telnet.exe", "telnet.exe", "zeego", NULL);
    printf("ehandle: %p\n", ehandle);
    exp_nt_debug = 0;
    if (ehandle == NULL) {
	return 1;
    }
    exp_kill(exp_pid, SIGINT);
    exp_kill(exp_pid, SIGBREAK);

    exp_closefd(ehandle);
    return 0;
}

int
test9(void)
{
    void *ehandle;

    exp_nt_debug = 1;

    ehandle = exp_spawnl("telnet.exe", "telnet.exe", "zeego", NULL);
    printf("ehandle: %p\n", ehandle);
    exp_nt_debug = 0;
    if (ehandle == NULL) {
	return 1;
    }

    exp_closefd(ehandle);
    return 0;
}

void
result(int id, int ret)
{
    if (ret == 0) {
	printf("Test %d was successful\n", id);
    } else {
	printf("Test %d was unsuccessful (returned %d)\n", id, ret);
    }
}

int
main(int argc, char **argv)
{
    int i;

    /* Call this with 1 to turn on verbose debugging info */
    exp_setdebug(NULL, 1);

    for (i = 0; i < 100; i++) {
	printf("-------------------- Iteration %d ------------------------\n", i);
	result(1, test1());
	result(2, test2());
	result(3, test3());
	result(4, test4());
	result(5, test5());
	result(6, test6());
	result(7, test7());
	result(8, test8());
	result(9, test9());
    }

    return 0;
}
