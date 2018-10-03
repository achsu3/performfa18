#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define BIG_INPUT 1000
#define NUM_ESTIMATES 10
#define MAXCMDLEN 100

unsigned long do_factorial(unsigned long n)
{
	unsigned long fact = n;
	if (n == 0)
		return 1;

	while (--n) {
		fact *= n;
	}

	return fact;
}

void do_work(int iterations)
{
	int i;

	for (i=0;i < iterations; ++i) {
		do_factorial(BIG_INPUT*i);
	}
}

#define TIME_SUB_FLOAT(t2, t1, diff, res) \
	timersub(&t2, &t1, &diff); \
	res = (diff.tv_sec * 1000.0) + (diff.tv_usec / 1000.0);


unsigned long estimate_comp_time(int iterations)
{
	struct timeval start, end, diff;
	unsigned long curr_est, average = 0;
	int i;

	for (i=1; i <= NUM_ESTIMATES; ++i) {
		gettimeofday(&start, NULL);
		do_work(iterations);
		gettimeofday(&end, NULL);

		timersub(&end, &start, &diff);

		curr_est = (diff.tv_sec * (unsigned long)1000) +
			(diff.tv_usec / 1000);

		average += curr_est;
	}

	return (average / NUM_ESTIMATES) + 1;
}

int main(int argc, char **argv)
{
	int pid, observed_pid = -1;
	unsigned long num_jobs = 0;
	int c, iterations = 0;
	unsigned long period = 0, computation = 0;
	char cmd[MAXCMDLEN];
	FILE *fd;
	char *line = 0;
	ssize_t len = 0, read = 0;
	int err;

	struct timeval t0, wakeup_tv, proc_tv, diff;
	float t1, t2;


	/* get options for command line */
	while ((c = getopt(argc, argv, "n:p:j:c:")) != -1)
		switch(c)
		{
		case 'n':
			iterations = atoi(optarg);
			break;
		case 'p':
			period = atol(optarg);
			break;
		case 'j':
			num_jobs = atol(optarg);
			break;
		case 'c':
			computation = atol(optarg);
			break;
		case '?':
			fprintf(stderr,
				"Unknown option.\n" \
				"[Usage]: ./user_app -n iterations -p period" \
				" -j jobs\n");
			return 1;
		default:
			return 1;
		}

	if (period == 0 || iterations == 0 || num_jobs == 0) {
		fprintf(stderr, "Missing option.\n" \
			"[Usage]: ./user_app -n iterations -p period" \
			" -j jobs\n");
		return 1;
	}

	/* first assess the needed computation time */
	if (computation == 0) {
		computation = estimate_comp_time(iterations);
		printf("Estimated runtime for %d iterations is %lu msec\n",
		       iterations, computation);
	}

	/* start of the main stuff that the app needs to do */
	pid = getpid();

	/* register */
	memset(cmd, 0, MAXCMDLEN);
	sprintf(cmd, "echo \"R, %d, %lu, %lu\" > /proc/mp2/status",
		pid, period, computation);
	err = system(cmd);
	if (err < 0) {
		fprintf(stderr, "Failed regitration call\n");
		return -1;
	}

	/* check successful registration */
	fd = fopen("/proc/mp2/status", "r");
	if (!fd) {
		fprintf(stderr, "Could not open proc file to read\n");
		return -1;
	}

	while (getline(&line, &len, fd) != -1) {
		sscanf(line, "%i", &observed_pid);
		if (observed_pid == pid)
			break;
		observed_pid = -1;
	}
	if (line) free(line);
	fclose(fd);

	if (observed_pid == -1) {
		fprintf(stderr, "Admission check failed for the application\n");
		return -1;
	}

	/* now issue the first yield request */
	memset(cmd, 0, MAXCMDLEN);
	sprintf(cmd, "echo \"Y, %d\" > /proc/mp2/status", pid);
	/* record t0 timeval */
	gettimeofday(&t0, NULL);
	err = system(cmd);
	if (err < 0) {
		fprintf(stderr, "Failed yield call");
		return -1;
	}

	/* main loop */
	while(num_jobs > 0) {
		gettimeofday(&wakeup_tv, NULL);
		TIME_SUB_FLOAT(wakeup_tv, t0, diff, t1);
		printf("Wakeup time is %lf msec\n", t1);
		do_work(iterations);

		gettimeofday(&proc_tv, NULL);
		TIME_SUB_FLOAT(proc_tv, wakeup_tv, diff, t2);
		printf("Processing time is %lf msec\n", t2);

		num_jobs--;
		memset(cmd, 0, MAXCMDLEN);
		sprintf(cmd, "echo \"Y, %d\" > /proc/mp2/status", pid);
		err = system(cmd);
		if (err < 0) {
			fprintf(stderr, "Failed yield call");
			return -1;
		}
	}

	/* deregister */
	memset(cmd, 0, MAXCMDLEN);
	sprintf(cmd, "echo \"D, %d\" > /proc/mp2/status", pid);
	err = system(cmd);
	if (err < 0) {
		fprintf(stderr, "Failed yield call\n");
		return -1;
	}

	printf("Goodbye from application process\n");


	return 0;
}
