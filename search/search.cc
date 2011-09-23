#include "../incl/search.hpp"
#include <cstdlib>
#include <cstring>

SearchStats::SearchStats(void) : expd(0), gend(0), reopnd(0), dups(-1) {
	start();
}

void SearchStats::start(void) {
	wallstrt = walltime();
	cpustrt = cputime();
}

void SearchStats::finish(void) {
	wallend = walltime();
	cpuend = cputime();
}

void SearchStats::output(FILE *f) {
	dfpair(f, "total raw cpu time", "%g", cpuend - cpustrt);
	dfpair(f, "total wall time", "%g", wallend - wallstrt);
	dfpair(f, "total nodes expanded", "%lu", expd);
	dfpair(f, "total nodes generated", "%lu", gend);
	dfpair(f, "total nodes duplicated", "%lu", dups);
	dfpair(f, "total nodes reopened", "%lu", reopnd);
}

Limit::Limit(void) : expd(0), gend(0) { }

Limit::Limit(int argc, char *argv[]) : expd(0), gend(0) {
	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-expd") == 0 && i < argc - 1) {
			expd = strtoul(argv[i+1], NULL, 10);
			i++;
		} else if (strcmp(argv[i], "-gend") == 0 && i < argc - 1) {
			gend = strtoul(argv[i+1], NULL, 10);
			i++;
		}
	}
}