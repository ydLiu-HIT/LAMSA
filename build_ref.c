/*
 * bulid_ref.c
 *
 * bulid index for reference sequence(soap2-dp)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <zlib.h>
#include "bntseq.h"
#include "build_ref.h"


int lsat_index_usage(void)
{
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage:   lsat index [option] <ref.fa>\n");
    fprintf(stderr, "                    bulid soap2-dp index for ref.fa\n\n");
    fprintf(stderr, "Option:  \n");
    fprintf(stderr, "         -i [INT]     Index program, <bwa(1)> or <soap2-dp(2)>. [Def=1]\n");
    return 1;
}

int bwa_build(char *prefix)
{
    char cmd[1024];
    sprintf(cmd, "./bwa_index.sh %s", prefix);
    fprintf(stderr, "[lsat_index] Executing bwa index ... ");
    if (system(cmd) != 0) { fprintf(stderr, "\n[lsat_index] Indexing undone, bwa index exit abnormally.\n"); exit(0); }
    fprintf(stderr, "done.\n");
    return 0;
}

int soap_bulid(char *prefix)
{
    char cmd[1024];
    sprintf(cmd, "./soap2dp_index.sh %s", prefix);
    fprintf(stderr, "[lsat_index] Executing soap2-dp-builder ... ");
    if (system(cmd) != 0 ) { fprintf(stderr, "\n[lsat_aln] Indexing undone, soap2-dp-builder exit abnormally.\n"); exit(0);}
    fprintf(stderr, " done.\n");
    return 0;
}

int lsat_index(int argc, char *argv[])
{
	char *prefix=0;
	int type=1, c;
	//XXX clock_t t;  
	
	while ((c = getopt(argc, argv, "i:")) >= 0)
	{
		switch (c)
		{
            case 'i':
                type = atoi(optarg);
                if (type > 2 || type < 1) return lsat_index_usage();
                break;
            default:
                return lsat_index_usage();
		}
	}

	if (optind + 1 > argc) return lsat_index_usage();
	prefix = strdup(argv[optind]);

	if (type==1) bwa_build(prefix);
    else if(type==2) soap_bulid(prefix);
    else return lsat_index_usage();


	//lsat_fa2pac
	gzFile fp = gzopen(prefix, "r");
	bns_fa2bnt(fp, prefix);

	free(prefix);
	return 0;
}
