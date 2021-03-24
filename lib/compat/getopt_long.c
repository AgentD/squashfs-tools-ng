#include "compat.h"

#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

#ifndef HAVE_GETOPT_LONG
static void permute(char *const *argv, int dest, int src)
{
	char **av = (char **)argv;
	char *tmp = av[src];
	int i;
	for (i=src; i>dest; i--)
		av[i] = av[i-1];
	av[dest] = tmp;
}

int getopt_long(int argc, char *const *argv, const char *optstring, const struct option *longopts, int *idx)
{
	optarg = 0;
	if (longopts && argv[optind][0] == '-' &&
	    (argv[optind][1] == '-' && argv[optind][2]))
	{
		int colon = optstring[optstring[0]=='+'||optstring[0]=='-']==':';
		int i, cnt, match;
		char *arg, *opt, *start = argv[optind]+1;
		for (cnt=i=0; longopts[i].name; i++) {
			const char *name = longopts[i].name;
			opt = start;
			if (*opt == '-') opt++;
			while (*opt && *opt != '=' && *opt == *name)
				name++, opt++;
			if (*opt && *opt != '=') continue;
			arg = opt;
			match = i;
			if (!*name) {
				cnt = 1;
				break;
			}
			cnt++;
		}
		if (cnt==1) {
			i = match;
			opt = arg;
			optind++;
			if (*opt == '=') {
				if (!longopts[i].has_arg) {
					optopt = longopts[i].val;
					if (colon || !opterr)
						return '?';
					__getopt_msg(argv[0],
						": option does not take an argument: ",
						longopts[i].name,
						strlen(longopts[i].name));
					return '?';
				}
				optarg = opt+1;
			} else if (longopts[i].has_arg == required_argument) {
				if (!(optarg = argv[optind])) {
					optopt = longopts[i].val;
					if (colon) return ':';
					if (!opterr) return '?';
					__getopt_msg(argv[0],
						": option requires an argument: ",
						longopts[i].name,
						strlen(longopts[i].name));
					return '?';
				}
				optind++;
			}
			if (idx) *idx = i;
			if (longopts[i].flag) {
				*longopts[i].flag = longopts[i].val;
				return 0;
			}
			return longopts[i].val;
		}
		if (argv[optind][1] == '-') {
			optopt = 0;
			if (!colon && opterr)
				__getopt_msg(argv[0], cnt ?
					": option is ambiguous: " :
					": unrecognized option: ",
					argv[optind]+2,
					strlen(argv[optind]+2));
			optind++;
			return '?';
		}
	}
	return getopt(argc, argv, optstring);
}
#endif
