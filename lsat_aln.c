#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <zlib.h>
#include <unistd.h>
#include <ctype.h>
#include "lsat_aln.h"
#include "bntseq.h"
#include "frag_check.h"
#include "kseq.h"
KSEQ_INIT(gzFile, gzread)

int usage(void )		//aln usage
{
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage:   lsat aln [options] <ref_prefix> <in.fa/fq>\n\n");
	fprintf(stderr, "Options: -n              Do NOT excute soap2-dp program, when soap2-dp result existed already.\n");
    fprintf(stderr, "         -s              Seed information file has already existed.\n");
	fprintf(stderr, "         -a [STR]        The soap2-dp alignment result. When '-n' is used. [Def=\"seed_prefix.out.0\"]\n");
	fprintf(stderr, "         -m [INT][STR]   The soap2-dp option. Maximun #errors allowed. [Def=3e]\n");
	fprintf(stderr, "         -l [INT]        The length of seed. [Def=100]\n");
	fprintf(stderr, "         -o [STR]        The output file (SAM format). [Def=\"prefix_out.sam\"]\n");

	fprintf(stderr, "\n");
	return 1;
}

seed_msg *seed_init_msg(void)
{
	seed_msg *msg = (seed_msg*)malloc(sizeof(seed_msg));

	msg->n_read = 0;
	msg->n_seed = (int *)calloc(READ_INIT_MAX, sizeof(int));
    msg->last_len = (int *)malloc(READ_INIT_MAX * sizeof(int));
	msg->seed_max = 0;
    msg->read_max_len = 0;

	return msg;
}

void seed_free_msg(seed_msg *msg)
{
	free(msg->n_seed);
    free(msg->last_len);
	free(msg);
}

int split_seed(const char *prefix, seed_msg *s_msg, int seed_len)
{
	gzFile infp;
	kseq_t *seq;
	char out_f[1024], seed_head[1024], seed_seq[1024], seed_info[1024];
	FILE *outfp, *infofp;
	int m_read, n_seed, *new_p, i;

	if ((infp = gzopen(prefix, "r")) == NULL) {
		fprintf(stderr, "[lsat_aln] Can't open read file %s\n", prefix);
		exit(-1);
	}
	seq = kseq_init(infp);

	strcpy(out_f, prefix); strcat(out_f, ".seed");
	if ((outfp = fopen(out_f, "w")) == NULL) {
		fprintf(stderr, "[lsat_aln] Can't open seed file %s\n", out_f);
		exit(-1);
	}
    strcpy(seed_info, prefix); strcat(seed_info, ".seed.info");
	if ((infofp = fopen(seed_info, "w")) == NULL) {
		fprintf(stderr, "[lsat_aln] Can't open seed info file %s\n", seed_info);
		exit(-1);
	}
    fprintf(infofp, "%d\n", seed_len);

	fprintf(stderr, "[lsat_aln] Spliting seed ... ");
	seed_seq[seed_len] = '\n';
	m_read = READ_INIT_MAX;
	while (kseq_read(seq) >= 0)
	{
		n_seed = ((seq->seq.l / seed_len) + 1) >> 1;    //XXX
		if (n_seed > s_msg->seed_max) s_msg->seed_max = n_seed;
        if (seq->seq.l > s_msg->read_max_len) s_msg->read_max_len = seq->seq.l; //XXX
		if (s_msg->n_read == m_read-1)
		{
			m_read <<= 1;
			if ((new_p = (int*)realloc(s_msg->n_seed, m_read * sizeof(int))) == NULL)
			{
				free(s_msg->n_seed);
				fprintf(stderr, "[lsat_aln] Can't allocate more memory for n_seed[].\n");
				exit(1);
			}
			s_msg->n_seed = new_p;
            if ((new_p = (int*)realloc(s_msg->last_len, m_read * sizeof(int))) == NULL)
            {
                free(s_msg->last_len);
                fprintf(stderr, "[lsat_aln] Can't allocate more memory for last_len[].\n");
                exit(-1);
            }
			s_msg->last_len = new_p;
		}
		++s_msg->n_read;
		s_msg->n_seed[s_msg->n_read] = s_msg->n_seed[s_msg->n_read-1] + n_seed;
        s_msg->last_len[s_msg->n_read] = seq->seq.l - (n_seed * 2 - 1) * seed_len;
		for (i = 0; i < n_seed; ++i)
		{
			sprintf(seed_head, ">%s_%d:%d\n", seq->name.s, i, i*seed_len*2);
			strncpy(seed_seq, seq->seq.s+i*seed_len*2, seed_len);
			seed_seq[seed_len+1] = '\0';
			fputs(seed_head, outfp);
			fputs(seed_seq, outfp);
		}
        fprintf(infofp, "%d %d %d\n", n_seed, s_msg->last_len[s_msg->n_read], seq->seq.l);
	}
	fprintf(stderr, "done.\n");
	gzclose(infp);
	fclose(outfp);
    fclose(infofp);
	kseq_destroy(seq);
	
	return 0;
}

int split_seed_info(const char *prefix, seed_msg *s_msg, int *seed_len)
{
    char seed_info[1024];
    FILE *infofp;
    int m_read, n_seed, last_len, len, n;
    int *new_p;

    strcpy(seed_info, prefix); strcat(seed_info, ".seed.info");
    if ((infofp = fopen(seed_info, "r")) == NULL)
    {
        fprintf(stderr, "[split seed] Can't open %s.\n", seed_info); 
        exit(-1);
    }
    m_read = READ_INIT_MAX;
    fprintf(stderr, "[last_aln] Parsing seeds' information ... ");
    if (fscanf(infofp, "%d", seed_len) == EOF)
    {
        fprintf(stderr, "[split seed] INFO file error.[1]\n");
        exit(-1);
    }
    while ((n = fscanf(infofp, "%d %d %d", &n_seed, &last_len, &len)) != EOF)
    {
       if (n != 3)
       {
           fprintf(stderr, "[split seed] INFO file error.[2]\n");
           exit(-1);
       }
       if (n_seed > s_msg->seed_max) s_msg->seed_max = n_seed;
       if (len > s_msg->read_max_len) s_msg->read_max_len = len;
       if (s_msg->n_read == m_read-1)
       {
           m_read <<= 1;
           if ((new_p = (int*)realloc(s_msg->n_seed, m_read * sizeof(int))) == NULL)
           {
               free(s_msg->n_seed);
               fprintf(stderr, "[lsat aln] Can't allocate more memory for n_seed[].\n");
               exit(-1);
           }
           s_msg->n_seed =new_p;
           if ((new_p = (int*)realloc(s_msg->last_len, m_read * sizeof(int))) == NULL)
           {
               free(s_msg->last_len);
               fprintf(stderr, "[lsat aln] Can't allocate more memory for last_len[].\n");
               exit(-1);
           }
           s_msg->last_len = new_p;
       }
       ++s_msg->n_read;
       s_msg->n_seed[s_msg->n_read] = s_msg->n_seed[s_msg->n_read-1] + n_seed;
       s_msg->last_len[s_msg->n_read] = last_len;
       if (last_len != len - (n_seed * 2 - 1) * (*seed_len))
       {
           fprintf(stderr, "[split seed] INFO file error.[3]\n");
           exit(-1);
       }
    }
    
    fprintf(stderr, "done.\n");
    fclose(infofp);
    return 0;
}
aln_msg *aln_init_msg(int seed_max)
{
	aln_msg *msg;
	int i,j;
	msg = (aln_msg*)malloc(seed_max * sizeof(aln_msg));
	for (i = 0; i < seed_max; ++i)		//drop away seed whose number of alignments > PER_ALN_N
	{
		msg[i].read_id = -1;
    	msg[i].n_aln = 0;
		msg[i].skip = 0;
		msg[i].at = (aln_t*)malloc(PER_ALN_N * sizeof(aln_t));
		for (j = 0; j < PER_ALN_N; ++j)
		{
			msg[i].at[j].cigar = (uint32_t*)malloc(7 * sizeof(uint32_t));//XXX default value for 3-ed
			msg[i].at[j].cigar_len = 0;
			msg[i].at[j].cmax = 7;
			msg[i].at[j].bmax = 0;
		}
	}
	return msg;
}

void aln_free_msg(aln_msg *a_msg, int seed_max)	//a_msg[seed_max]
{
	int i,j;
	for (i = 0; i < seed_max; ++i)
	{
		for (j = 0; j < PER_ALN_N; ++j)
		{
			free(a_msg[i].at[j].cigar);
		}
		free(a_msg[i].at);
	}
	free(a_msg);
}

//MIDNSHP=XB
//0123456789
void setCigar(aln_msg *a_msg, int seed_i, int aln_i, char *s_cigar)
{
	int op;
	long x, bi, bd;
	char *s, *t;

	a_msg[seed_i].at[aln_i].cigar_len=0;
	bi = bd = 0;
	for (s = s_cigar; *s; )
	{
		x = strtol(s, &t, 10);	
		if (x == 0)
		{
            fprintf(stderr, "%s\n",s);
			fprintf(stderr, "[lsat_aln] Cigar ERROR 1.\n");
			exit(-1);
		}
		op = toupper(*t);
		switch (op)
		{
			case 'M':	op = CMATCH;	break;
			case 'I':	op = CINS;		bi += x;	break;
			case 'D':	op = CDEL;		bd += x;	break;
			case 'N':	op = CREF_SKIP;		bd += x;	break;
			case 'S':	op = CSOFT_CLIP;		bi += x;	break;
			case 'H':	op = CHARD_CLIP;		bd += x;	break;
			case 'P':	op = CPAD;		bd += x;	break;
			case '=':	op = CEQUAL;	break;
			case 'X':	op = CDIFF;	break;
			case 'B':	op = CBACK;		bi += x;	break;	
			default:	fprintf(stderr, "[lsat_aln] Cigar ERROR 2.\n"); exit(-1); break;
		}
		if (a_msg[seed_i].at[aln_i].cigar_len == a_msg[seed_i].at[aln_i].cmax)
		{
			a_msg[seed_i].at[aln_i].cmax <<= 2 ;
			a_msg[seed_i].at[aln_i].cigar = (uint32_t*)realloc(a_msg[seed_i].at[aln_i].cigar, a_msg[seed_i].at[aln_i].cmax * sizeof(uint32_t));
		}
		a_msg[seed_i].at[aln_i].cigar[a_msg[seed_i].at[aln_i].cigar_len] = CIGAR_GEN(x, op);
		//modify variable directly OR use a auxiliary-variable
		++a_msg[seed_i].at[aln_i].cigar_len;
		s = t+1;
	}
	a_msg[seed_i].at[aln_i].len_dif = (int)(bd - bi);
	a_msg[seed_i].at[aln_i].bmax = (int)(bd > bi ? bd : bi);
}

void setAmsg(aln_msg *a_msg, int32_t read_x, int aln_y, int read_id, int chr, int64_t offset, char srand, int edit_dis, char *cigar)
{   //read_x: (除去unmap和repeat的)read序号, aln_y: read对应的比对结果序号(从1开始)
	if (aln_y > PER_ALN_N) {
		fprintf(stderr, "[lsat_aln] setAmsg ERROR!\n");
		exit(0);
	}
	a_msg[read_x-1].read_id = read_id;			//from 1
	a_msg[read_x-1].at[aln_y-1].chr = chr;
	a_msg[read_x-1].at[aln_y-1].offset = offset;	//1-base
	a_msg[read_x-1].at[aln_y-1].nsrand = ((srand=='+')?1:-1);
	a_msg[read_x-1].at[aln_y-1].edit_dis = edit_dis;
	a_msg[read_x-1].n_aln = aln_y;
	setCigar(a_msg, read_x-1, aln_y-1,  cigar);
}

//XXX diff of order of pre and curr?
int get_dis(aln_msg *a_msg, int pre, int pre_a, int i, int j, int *flag, int seed_len)    //(i,j)对应节点，来自pre的第pre_a个aln
{
    //if (pre < 0) {fprintf(stderr, "[lsat_aln] a_msg error.\n"); exit(0);}
	if (pre == -1) { *flag = MATCH; return 0; }	//for node-skip

	if ((pre == i) && (pre_a == j)) {*flag = MATCH; return 0;}

    if (a_msg[i].at[j].chr != a_msg[pre].at[pre_a].chr || a_msg[i].at[j].nsrand != a_msg[pre].at[pre_a].nsrand)	//different chr or different srnad
    {
        *flag = CHR_DIF;
    	return PRICE_DIF_CHR;
    }
    int64_t exp = a_msg[pre].at[pre_a].offset + a_msg[pre].at[pre_a].nsrand * (a_msg[i].read_id - a_msg[pre].read_id) * 2 * seed_len;	
    int64_t act = a_msg[i].at[j].offset;
	int64_t dis = a_msg[pre].at[pre_a].nsrand * (act-exp) - ((a_msg[pre].at[pre_a].nsrand==1)?(a_msg[pre].at[pre_a].len_dif):(a_msg[i].at[j].len_dif));

    if (dis > 10 && dis < DEL_THD) *flag = DELETION;
    else if (dis < -10 && dis >= (0-((a_msg[i].read_id-a_msg[pre].read_id)*2-1)*seed_len)) *flag = INSERT;
    else if (dis <= 10 && dis >= -10) *flag = MATCH; 
	else *flag = UNCONNECT;

    dis=(dis>0?dis:(0-dis)); //Absolute value
	//dis = offset_dis + pre.edit_dis + pre.cigar_len
	dis += (a_msg[pre].at[pre_a].edit_dis + a_msg[pre].at[pre_a].cigar_len);
    return adjest(dis);
}

int get_abs_dis(aln_msg *a_msg, int pre, int pre_a, int i, int j, int *flag, int seed_len)    //(i,j)对应节点，来自pre的第pre_a个aln
{
    //if (pre < 0) {fprintf(stderr, "[lsat_aln] a_msg error.\n"); exit(0);}
	if (pre == -1) { *flag = MATCH; return 0; }	//for node-skip

	if ((pre == i) && (pre_a == j)) {*flag = MATCH; return 0;}

    if (a_msg[i].at[j].chr != a_msg[pre].at[pre_a].chr || a_msg[i].at[j].nsrand != a_msg[pre].at[pre_a].nsrand)	//different chr or different srnad
    {
        *flag = CHR_DIF;
    	return PRICE_DIF_CHR;
    }
    int64_t exp = a_msg[pre].at[pre_a].offset + a_msg[pre].at[pre_a].nsrand * (a_msg[i].read_id - a_msg[pre].read_id) * 2 * seed_len;	
    int64_t act = a_msg[i].at[j].offset;
	//int64_t dis = a_msg[pre].at[pre_a].nsrand * (act-exp) - ((a_msg[pre].at[pre_a].nsrand==1)?(a_msg[pre].at[pre_a].len_dif):(a_msg[i].at[j].len_dif));
	//have nothing to do with order of seeds.
	int64_t dis = a_msg[pre].at[pre_a].nsrand * ((a_msg[pre].read_id < a_msg[i].read_id)?(act-exp):(exp-act)) - (((a_msg[pre].at[pre_a].nsrand) * (a_msg[pre].read_id-a_msg[i].read_id) < 0)?(a_msg[pre].at[pre_a].len_dif):(a_msg[i].at[j].len_dif));

    if (dis > 10 && dis < DEL_THD) *flag = DELETION;
    else if (dis < -10 && dis >= (0-((a_msg[i].read_id-a_msg[pre].read_id)*2-1)*seed_len)) *flag = INSERT;
    else if (dis <= 10 && dis >= -10) *flag = MATCH; 
	else *flag = UNCONNECT;

    dis=(dis>0?dis:(0-dis)); //Absolute value
	//dis = offset_dis + pre.edit_dis + pre.cigar_len
	dis += (a_msg[pre].at[pre_a].edit_dis + a_msg[pre].at[pre_a].cigar_len);
    return dis;
}
//copy current line to the last row, add SKIP to current line, add SEED_I to last row
/*void copy_line(int ***line, int line_i, int *path_n, int **path_end, int **price, int seed_i, int p, int *x_l, int y_l)
{
    int i;

    if ((*path_n) == (*x_l))
    {
        fprintf(stderr, "realloc: ");
        (*line) = (int**)realloc((*line), 2 * (*x_l) * sizeof(int*));
        for (i = (*x_l); i < 2*(*x_l); ++i)
            (*line)[i] = (int*)malloc(y_l * sizeof(int));
        (*path_end) = (int*)realloc((*path_end), 2*(*x_l) * sizeof(int));
        (*price) = (int*)realloc((*price), 2*(*x_l) * sizeof(int));
        if ((*line) == NULL || (*path_end) == NULL || (*price) == NULL)
        {
            fprintf(stderr, "[copy line] Can't allocate more memory.\n");
            exit(-1);
        }
        (*x_l) <<= 1;
    }

    for (i = 0; i < (*path_end)[line_i]; ++i)
    {
        (*line)[(*path_n)][i] = (*line)[line_i][i];
    }
    (*line)[(*path_n)][i] = seed_i;
    (*path_end)[(*path_n)] = i+1;
    (*price)[(*path_n)] = (*price)[line_i]+p;
    ++(*path_n);

    (*price)[line_i]+=PRICE_SKIP;
}*/

//return: 1: yes, 0: no.
int check_in(int seed_i, int line_i, int ***line, int **price, int **path_end, int *path_n, aln_msg *a_msg, int seed_len, int *add_len, int *x_l, int y_l)
{
	int flag, dis, i;

    if ((*path_end)[line_i] == 0)  //only one node(-1) exists in this line.
    {
        ++(*add_len);
        copy_line(line, line_i, path_n, path_end, price, seed_i, PRICE_SKIP, x_l, y_l);
        return 0;
    }
    else
    {
        dis = get_abs_dis(a_msg, (*line)[line_i][(*path_end)[line_i]-1], 0, seed_i, 0, &flag, seed_len);
        if (flag == MATCH)
        {
            (*line)[line_i][(*path_end)[line_i]] = seed_i;
            ++(*path_end)[line_i];
            (*price)[line_i]+=dis;
            (*path_n)-=(*add_len); //delete all other "add"
            for (i = 0; i < line_i; ++i)
                (*price)[i] += PRICE_SKIP;  //add "skip"
            return 1;
        }
        else if (flag == INSERT || flag == DELETION)
        {
            ++(*add_len);
            copy_line(line, line_i, path_n, path_end, price, seed_i, dis, x_l, y_l);
            return 0;
        }
        else    //UNCONNECT || CHR_DIF
        {
            (*price)[line_i]+=PRICE_SKIP;
            return 0;
        }
    }
    /*
	get_dis(a_msg, line[line_i][path_end[line_i]-1], 0, seed_i, 0, &flag, seed_len);
    if (flag == MATCH)
    {

    }
	if (flag != UNCONNECT && flag != CHR_DIF)
	{
		line[line_i][path_end[line_i]] = seed_i;
		path_end[line_i]++;
		return 1;
	}
	else return 0;*/
}

void new_line(int seed_i, int **line, int *path_end, int *path_n)
{
	line[*path_n][0] = seed_i;
	path_end[*path_n] = 1;
	++(*path_n);
}

//method of main-line determination XXX
//2-node DP XXX
//partly 2-node DP, only for the UNMATCH nodes. XXX
/*int main_line_deter(aln_msg *a_msg, int n_seed, int seed_len, int **main_price, int **main_path, int *main_line)
{
	int i, j, pre_i, dis_con, dis_skip, flag;

	for (j = 0; j < n_seed; j++) {
		if (a_msg[j].n_aln == 1) {
			main_price[j][0] = 0;			//1 : connect
			main_price[j][1] = PRICE_SKIP;	//0 : skip
			main_path[j][1] = -1;
			main_path[j][0] = -1;
			pre_i = j;
			break;
		}
	}
	for (i = j+1; i < n_seed; i++)
	{
		if (a_msg[i].n_aln == 1)
		{
			// check for connect
			// NO check for flag XXX
			dis_con = get_abs_dis(a_msg, pre_i, 0, i, 0, &flag, seed_len);	
            if (flag == MATCH)
            {
                main_price[i][0] = dis_con + main_price[pre_i][0];
                main_path[i][0] = pre_i;
            }
            else
            {
                dis_skip = get_abs_dis(a_msg, main_path[pre_i][1], 0, i, 0, &flag, seed_len);
                if ((dis_con + main_price[pre_i][0]) > (dis_skip + main_price[pre_i][1])) {
                    main_price[i][0] = dis_skip + main_price[pre_i][1];
                    main_path[i][0] = main_path[pre_i][1];
                }
                else {
                    main_price[i][0] = dis_con + main_price[pre_i][0];
                    main_path[i][0] = pre_i;
                }
            }
			//check for skip
			if (main_price[pre_i][0] <= main_price[pre_i][1]) {
				main_price[i][1] = PRICE_SKIP + main_price[pre_i][0];
				main_path[i][1] = pre_i;
			} else {
				main_price[i][1] = PRICE_SKIP + main_price[pre_i][1];
				main_path[i][1] = main_path[pre_i][1];
			}
			pre_i = i;
		}
	}
	if (main_price[pre_i][0] > main_price[pre_i][1])
		pre_i = main_path[pre_i][1];
	i = 0;
	while (pre_i != -1)
	{
		main_line[i] = pre_i;
		i++;
		pre_i = main_path[pre_i][0];
	}
	return i;
}*/

int copy_line(int **line, int from, int to, int *path_end)
{
	int i;
	for (i = 0; i < path_end[from]; ++i)
	{
		line[to][path_end[to]+i] = line[from][i];
	}
	path_end[to] += path_end[from];
}

int main_line_deter(aln_msg *a_msg, int n_seed, int seed_len, int *m_i, int **line, int *path_end)
{
    int i, j, k, flag, con_flag;
    int path_n = 0, max_i = 0;

    //find all the small lines with MATCH-CONNECT nodes
    for (i = 0; i < n_seed; ++i)
    {
        if (a_msg[i].n_aln == 1)
        {
            flag = 0;
            for (j = path_n-1; j >= 0; --j)
            {
                get_abs_dis(a_msg, line[j][path_end[j]-1], 0, i, 0, &con_flag, seed_len);
                if (con_flag == MATCH)
                {
                    flag = 1;
                    line[j][path_end[j]] = i;
                    ++path_end[j];
					if (path_end[j] > path_end[max_i])
						max_i = j;
                    if (j != (path_n-1))
                    {
                        for (k = j+1; k != path_n; ++k)
                            path_end[k] = 0;
                        path_n = j+1;
                    }
					break;
				}
				if (path_end[j] > 5) break;
            }
            if (flag == 0)
                new_line(i, line, path_end, &path_n);
        }
    }
	//combine other small lines to "max_i" line
	path_end[path_n] = 0;
	for (i = 0; i < max_i; ++i)
	{
		get_abs_dis(a_msg, line[i][path_end[i]-1], 0, line[max_i][0], 0, &con_flag, seed_len);
		if (con_flag != UNCONNECT && con_flag != CHR_DIF)			
			copy_line(line, i, path_n, path_end);
	}
	copy_line(line, i, path_n, path_end);
	for (i = max_i+1; i < path_n; ++i)
	{
		get_abs_dis(a_msg, line[max_i][path_end[max_i]-1], 0, line[i][path_end[i]-1], 0, &con_flag, seed_len);
		if (con_flag != UNCONNECT && con_flag != CHR_DIF)
			copy_line(line, i, path_n, path_end);
	}

	(*m_i) = path_n;
	return path_end[path_n];
}

/*
int add_path(aln_msg *a_msg, path_msg **path, int **price, int *price_n, int start, int end, int rev, int seed_len)
{
	if (start == end) return 0;
	else if (start > end) {fprintf(stderr, "[add_path] error: start > end. %d %d\n", start, end); exit(-1);}

	int i, j, k, l, anchor, termi;
	int back_i, back_j;// for rev-path
	int con_flag, flag, min, dis;
	if (rev == 1) {//rev-path add seeds onto 'end' from end+1 to start 
		termi = start-1; anchor = end;
	} else {	//rev == -1/-2, add seeds onto 'start' from start+1 to end 
		termi = end+1; anchor = start;
	}
	price[anchor][0] = 0; price_n[anchor] = 1; back_i= anchor; back_j = 0;;
	if (rev == -1 || rev == 1)	//onepoint
	{
		for (i = anchor-rev; i != termi; i=i-rev)
		{
			price_n[i] = 0;
			for (j = 0; j < a_msg[i].n_aln; ++j)
			{
				flag = 0; min = -1;
                for (k = i+rev; k != anchor+rev; k+=rev)
                {
                    if (price_n[k] == 0) continue;
                    for (l = 0; l < a_msg[k].n_aln; ++l)
                    {
                        if (price[k][l] == -1) continue;
                        if (rev == 1) dis = price[k][l] + get_abs_dis(a_msg, i, j, k, l, &con_flag, seed_len);
                        else dis = price[k][l] + get_abs_dis(a_msg, k, l, i, j, &con_flag, seed_len);
                        if (con_flag != UNCONNECT && con_flag != CHR_DIF && ((min==-1)||(dis<min))) {
                            min = dis; flag = 1;
                            path[i][j].from.x = k;
                            path[i][j].from.y = l;
                            path[i][j].flag = con_flag;
							if (con_flag == MATCH)
								goto CON_END_1;
                        }
                    }
                }
				CON_END_1: if (flag) { ++price_n[i]; back_i = i; back_j = j; }
                price[i][j] = min;
            }
        }
    }
	else //rev == -2 : two endpoint XXX
	{
		int end_flag;
		for	(i = start+1; i != end+1; ++i)
		{
			price_n[i] = 0;
			for (j = 0; j < a_msg[i].n_aln; ++j)
			{
				flag = 0; min = -1;
                for (k = i-1; k != start-1; --k)
                {
                    if (price_n[k] == 0) continue;
                    for (l = 0; l < a_msg[k].n_aln; ++l) {
                        if (price[k][l] == -1) continue;
                        dis = price[k][l] + get_abs_dis(a_msg, k, l,  i, j, &con_flag, seed_len);
						if (con_flag == MATCH) {
							min = dis; flag = 1;
							path[i][j].from.x = k;
							path[i][j].from.y = l;
							path[i][j].flag = con_flag;
							goto CON_END_2;
						}
						else if (con_flag == DELETION || con_flag == INSERT) {
                            get_abs_dis(a_msg, i, j, end, 0, &end_flag, seed_len);	//connect-check to 'end'
                            if (end_flag != UNCONNECT && end_flag != CHR_DIF && ((min == -1)||(dis < min))) {
                                min = dis; flag = 1;
                                path[i][j].from.x = k;
                                path[i][j].from.y = l;
                                path[i][j].flag = con_flag;
                            }
                        }
                    }
                }
				CON_END_2: if (flag) price_n[i]++;
				price[i][j] = min;
			}
		}
	}
	if (rev == 1)	//repair the orientation and construce a minmum path, like reverse a single linked list.
	{
		int curr_p_x, curr_p_y, curr_f_x, curr_f_y, curr_flag, pre_p_x, pre_p_y, pre_f_x, pre_f_y, pre_flag;
		if (price_n[back_i] > 0)
		{
			pre_p_x = back_i;    pre_p_y = back_j; 
			pre_f_x = path[back_i][back_j].from.x;
			pre_f_y = path[back_i][back_j].from.y;
			pre_flag = path[back_i][back_j].flag;
			path[back_i][back_j].flag = PATH_END;
			while (pre_p_x != anchor) {
				curr_p_x = pre_f_x; curr_p_y = pre_f_y;
				curr_f_x = path[curr_p_x][curr_p_y].from.x; curr_f_y = path[curr_p_x][curr_p_y].from.y;
				curr_flag = path[curr_p_x][curr_p_y].flag;

				path[curr_p_x][curr_p_y].from.x = pre_p_x;
				path[curr_p_x][curr_p_y].from.y = pre_p_y;
				path[curr_p_x][curr_p_y].flag = pre_flag;

				pre_p_x = curr_p_x; pre_p_y = curr_p_y;
				pre_f_x = curr_f_x; pre_f_y = curr_f_y;
				pre_flag = curr_flag;
			}
		}
		else {fprintf(stderr, "[add_path] Bug: rev error.\n"); exit(-1);}
	}
	return 1;
}
*/
int add_path(aln_msg *a_msg, path_msg **path, int **price, int *price_n, int start, int end, int rev, int seed_len)
{
	if (start == end) return 0;
	else if (start > end) {fprintf(stderr, "[add_path] error: start > end. %d %d\n", start, end); exit(-1);}

	int i, j, k, l, anchor, termi;
	int last_i, last_j, start_dis, last_dis, last_start_dis, tmp_start_dis, tmp_i, tmp_j, tmp_dis, tmp_flag;
	int back_i, back_j;// for rev-path
	int con_flag, last_flag, flag, min, dis, tmp;
	if (rev == 1) {//rev-path add seeds onto 'end' from end+1 to start 
		termi = start-1; anchor = end;
	} else {	//rev == -1/-2, add seeds onto 'start' from start+1 to end 
		termi = end+1; anchor = start;
	}
	price[anchor][0] = 0; price_n[anchor] = 1; back_i= anchor; back_j = 0;;
	if (rev == -1 || rev == 1)	//onepoint
	{
		last_i = anchor; last_j = 0;
		for (i = anchor-rev; i != termi; i=i-rev)
		{
			price_n[i] = 0;
			flag = 0; min = -1;
			for (j = 0; j < a_msg[i].n_aln; ++j)
			{
				start_dis = get_abs_dis(a_msg, anchor, 0, i, j, &con_flag, seed_len);
				if (con_flag != UNCONNECT && con_flag != CHR_DIF) {
                    last_dis = get_abs_dis(a_msg, last_i, last_j, i, j, &last_flag, seed_len);
                    if (last_flag == MATCH)
                    {
                        path[i][j].from.x = last_i;
                        path[i][j].from.y = last_j;
                        path[i][j].flag = MATCH;
                        last_i = i; last_j = j; last_start_dis = start_dis;
                        price_n[i] = j+1;
                        back_i = i; back_j = j;
                        flag = 0;
                        break;
                    }
                    else if ((min == -1) || (last_dis < min)) {
                        min = last_dis;
                        flag = 1;
                        tmp_j = j;
                        tmp_start_dis = start_dis;
                        tmp_flag = last_flag;
                    }
				}
			}
			if (flag == 1)
			{
				if (tmp_flag != UNCONNECT)
				{
					path[i][tmp_j].from.x = last_i;
					path[i][tmp_j].from.y = last_j;
					path[i][tmp_j].flag = tmp_flag;
					last_i = i; last_j = tmp_j; last_start_dis = tmp_start_dis;
					price_n[i] = tmp_j+1;
					back_i = i; back_j = tmp_j;
				}
				else
				{
					if (tmp_start_dis < last_start_dis)
					{
						k = path[last_i][last_j].from.x; l = path[last_i][last_j].from.y;
						while (1)
						{
							get_abs_dis(a_msg, k, l, i, tmp_j, &last_flag, seed_len);
							if (last_flag != UNCONNECT)
							{
								path[i][tmp_j].from.x = k;
								path[i][tmp_j].from.y = l;
								path[i][tmp_j].flag = last_flag;
								last_i = i; last_j = tmp_j; last_start_dis = tmp_start_dis;
								price_n[i] = tmp_j+1;
								back_i = i; back_j = tmp_j;
								break;
							}
							tmp = k;
							k = path[k][l].from.x; l = path[tmp][l].from.y;
						}
					}
				}
			}
        }
    }
	else //rev == -2 : two endpoint XXX
	{
		int end_flag;
		last_i = start; last_j = 0;
		for	(i = start+1; i != end+1; ++i)
		{
			price_n[i] = 0;
			flag = 0; min = -1;
			for (j = 0; j < a_msg[i].n_aln; ++j)
			{
				start_dis = get_abs_dis(a_msg, start, 0, i, j, &con_flag, seed_len);
				if (con_flag != UNCONNECT && con_flag != CHR_DIF) 
				{
					get_abs_dis(a_msg, i, j, end, 0, &end_flag, seed_len);
					if (end_flag != UNCONNECT && end_flag != CHR_DIF) {
                        last_dis = get_abs_dis(a_msg, last_i, last_j, i, j, &last_flag, seed_len);
                        if (last_flag == MATCH) {
                            path[i][j].from.x = last_i;
                            path[i][j].from.y = last_j;                          
                            path[i][j].flag = MATCH;
                            last_i = i; last_j = j; last_start_dis = start_dis;
                            price_n[i] = j+1;
                            flag = 0;
                            break;
                        }
                        else if ((min == -1) || (last_dis < min)) {
                            min = last_dis;
                            flag = 1;
                            tmp_j = j;
                            tmp_start_dis = start_dis;
                            tmp_flag = last_flag;
                        }
                    }
                }
			}
			if (flag == 1)
			{
				if (tmp_flag != UNCONNECT)
				{
					path[i][tmp_j].from.x = last_i;
					path[i][tmp_j].from.y = last_j;
					path[i][tmp_j].flag = tmp_flag;
					last_i = i; last_j = tmp_j; last_start_dis = tmp_start_dis;
					price_n[i] = tmp_j+1;
				}
				else	//one of these two alns ([i,tmp_j], [last_i,last_j]) is wrong.
				{
					if (tmp_start_dis < last_start_dis)
					{
						k = path[last_i][last_j].from.x; l = path[last_i][last_j].from.y;
						while (1)
						{
							get_abs_dis(a_msg, k, l, i, tmp_j, &last_flag, seed_len);
							if (last_flag != UNCONNECT)
							{
								path[i][tmp_j].from.x = k;
								path[i][tmp_j].from.y = l;
								path[i][tmp_j].flag = last_flag;
								last_i = i; last_j = tmp_j; last_start_dis = tmp_start_dis;
								price_n[i] = tmp_j+1;
								break;
							}
							tmp = k;
							k = path[k][l].from.x; l = path[tmp][l].from.y;
						}
					}
				}
			}
		}
	}
	if (rev == 1)	//repair the orientation and construce a minmum path, like reverse a single linked list.
	{
		int curr_p_x, curr_p_y, curr_f_x, curr_f_y, curr_flag, pre_p_x, pre_p_y, pre_f_x, pre_f_y, pre_flag;
		if (price_n[back_i] > 0)
		{
			pre_p_x = back_i;    pre_p_y = back_j; 
			pre_f_x = path[back_i][back_j].from.x;
			pre_f_y = path[back_i][back_j].from.y;
			pre_flag = path[back_i][back_j].flag;
			path[back_i][back_j].flag = PATH_END;
			while (pre_p_x != anchor) {
				curr_p_x = pre_f_x; curr_p_y = pre_f_y;
				curr_f_x = path[curr_p_x][curr_p_y].from.x; curr_f_y = path[curr_p_x][curr_p_y].from.y;
				curr_flag = path[curr_p_x][curr_p_y].flag;

				path[curr_p_x][curr_p_y].from.x = pre_p_x;
				path[curr_p_x][curr_p_y].from.y = pre_p_y;
				path[curr_p_x][curr_p_y].flag = pre_flag;

				pre_p_x = curr_p_x; pre_p_y = curr_p_y;
				pre_f_x = curr_f_x; pre_f_y = curr_f_y;
				pre_flag = curr_flag;
			}
		}
		else {fprintf(stderr, "[add_path] Bug: rev error.\n"); exit(-1);}
	}
	return 1;
}
/********************************************/
/*	1. Use uniquely aligned seeds to:		*
 *		Determine the MAIN LINE.			*
 *	2. Add other seeds to the MAIN LINE.	*/
/********************************************/      
int path_dp(aln_msg *a_msg, int n_seed, int **line, int *path_end, path_msg **path, int **price, int *price_n, int seed_len, int n_seq)
//int path_dp(aln_msg *a_msg, int n_seed, int *main_line, int **main_price, int **main_path, path_msg **path, int **price, int *price_n, int seed_len, int n_seq)
{
	int i, m_i, m_len;

	//1. Determine the main line: 
    m_len = main_line_deter(a_msg, n_seed, seed_len, &m_i, line, path_end);
    //m_len = main_line_deter(a_msg, n_seed, seed_len, main_price, main_path, main_line);
	if (m_len == 0)
	{
		fprintf(stderr, "[path_dp] no seed is uniquely aligned to the ref. %d %lld\n", a_msg[0].at[0].chr, a_msg[0].at[0].offset);
		return 0;
	}
   // printf("main line: ");
//	for (i = 0; i < m_len; ++i)
//		printf("%d ", a_msg[line[m_i][i]].read_id);
	/*
    for (i = 0; i < m_len; ++i)
    {
        fprintf(stdout, "%d ", a_msg[main_line[i]].read_id);
    }*/
 //   printf("\n");
	//2. Add other seeds to the main line.
	
   
	if (add_path(a_msg, path, price, price_n, 0, line[m_i][0], 1, seed_len) == 0) //rev-path one endpoint
		path[line[m_i][0]][0].flag = PATH_END;
	for (i = 0; i < m_len-1; i++)
	{
		//fprintf(stderr, "debug: %d %d %d %lld %lld %lld \n", i, line[m_i][i], line[m_i][i+1], a_msg[line[m_i][i]].read_id, a_msg[line[m_i][i+1]].read_id, a_msg[line[m_i][i]].at[0].offset);
		add_path(a_msg, path, price, price_n, line[m_i][i], line[m_i][i+1], -2, seed_len);	//two endpoint
	}
	add_path(a_msg, path, price, price_n, line[m_i][m_len-1], n_seed-1, -1, seed_len);	//one endpoint
    
    /*if (add_path(a_msg, path, price, price_n, 0, main_line[m_len-1], 1, seed_len) == 0)
        path[main_line[m_len-1]][0].flag = PATH_END;

    for (i = m_len-1; i > 0; --i)
        add_path(a_msg, path, price, price_n, main_line[i], main_line[i-1], -2, seed_len);
    add_path(a_msg, path, price, price_n, main_line[0], n_seed-1, -1, seed_len);*/

	return 1;
}

int backtrack(aln_msg* a_msg, path_msg **path, int n_seed, int **price, int *price_n, int seed_len, frag_msg *f_msg)  //from end to start, find every fragment's postion
{
    if (n_seed == -1)
    {
        return 0;
    }
    //Determin the start point of backtrack.
    int i, j;//(path[i][min_i])
    for (i = n_seed-1; i >= 0; i--)
    {
    	if (price_n[i] > 0) {
			break;
    	}
    }
    //backtrack from (i, min_i)
    int last_x = i, last_y = price_n[i]-1, frag_num = 0, tmp, flag = 0;
    //first end
    frag_set_msg(a_msg, last_x, last_y, 1, f_msg, frag_num, seed_len);
	fprintf(stdout, "    end   %d ref %d read %d\n", a_msg[last_x].at[last_y].nsrand, a_msg[last_x].at[last_y].offset, a_msg[last_x].read_id);
    while (path[last_x][last_y].flag != PATH_END) {
    	if (path[last_x][last_y].flag != MATCH) {
    		//start
    		frag_set_msg(a_msg, last_x, last_y, 0, f_msg, frag_num, seed_len);
			fprintf(stdout, "    start %d ref %d read %d\n", a_msg[last_x].at[last_y].nsrand, a_msg[last_x].at[last_y].offset, a_msg[last_x].read_id);
    		flag = 1;
    		++frag_num;
    	}
    	tmp = last_x;
    	last_x = path[last_x][last_y].from.x;
    	last_y = path[tmp][last_y].from.y;

    	if (flag == 1) {
    		//next end
    		frag_set_msg(a_msg, last_x, last_y, 1, f_msg, frag_num, seed_len);
			fprintf(stdout, "    end   %d ref %d read %d\n", a_msg[last_x].at[last_y].nsrand, a_msg[last_x].at[last_y].offset, a_msg[last_x].read_id);
    		flag = 0; 
    	} else frag_set_msg(a_msg, last_x, last_y, 2, f_msg, frag_num, seed_len);	//seed
    }
	//start
    frag_set_msg(a_msg, last_x, last_y, 0, f_msg, frag_num, seed_len);
	fprintf(stdout, "    start %d ref %d read %d\n", a_msg[last_x].at[last_y].nsrand, a_msg[last_x].at[last_y].offset, a_msg[last_x].read_id);
	return 1;
}

int frag_cluster(const char *read_prefix, char *seed_result, seed_msg *s_msg, int seed_len, bntseq_t *bns, uint8_t *pac)
{
	FILE *result_p;
	char readline[1024];
	int n_read/*start from 1*/, n_seed, i;
    char srand;
	int read_id, chr, edit_dis;
    long long offset;
	char cigar[1024];
	
	aln_msg *a_msg;
	int **line, *path_end, **price, *price_n;
    int *main_line, **main_price, **main_path;
	path_msg **path;
	frag_msg *f_msg;
	gzFile readfp;
	kseq_t *read_seq_t;
	char *read_seq;

	//alloc mem and initialization
	a_msg = aln_init_msg(s_msg->seed_max);

	line = (int**)malloc(s_msg->seed_max * sizeof(int*));
    path_end = (int*)malloc(s_msg->seed_max * sizeof(int));

    main_line = (int*)malloc(s_msg->seed_max * sizeof(int));
    main_price = (int**)malloc(s_msg->seed_max * sizeof(int*));
    main_path = (int**)malloc(s_msg->seed_max * sizeof(int*));

	price = (int**)malloc(s_msg->seed_max * sizeof(int*));
	price_n = (int*)malloc(s_msg->seed_max * sizeof(int));
	path = (path_msg**)malloc(s_msg->seed_max * sizeof(path_msg*));

	f_msg = frag_init_msg(s_msg->seed_max);
	readfp = gzopen(read_prefix, "r");
	read_seq_t = kseq_init(readfp);
	read_seq = (char*)calloc(s_msg->read_max_len+1, sizeof(char));

	for (i = 0; i < s_msg->seed_max; ++i) {
		line[i] = (int*)malloc(s_msg->seed_max * sizeof(int));

        main_price[i] = (int*)malloc(s_msg->seed_max * sizeof(int));
        main_path[i] = (int*)malloc(s_msg->seed_max * sizeof(int));

		price[i] = (int*)malloc((PER_ALN_N+1) * sizeof(int));
		path[i] = (path_msg*)malloc((PER_ALN_N+1) * sizeof(path_msg));
	}
	if ((result_p = fopen(seed_result, "r")) == NULL) {
		fprintf(stderr, "[lsat_aln] Can't open seed result file %s.\n", seed_result); 
		exit(-1); 
	}

	n_read = 0;
	n_seed = 0;
	int multi_aln = 1, last_id = 0, REPEAT = 0, FLAG=0;

	//get seed msg of every read
	while (fgets(readline, 1024, result_p) != NULL)
	{
		//XXX for new-out.0 add 'cigar'
		sscanf(readline, "%d %d %lld %c %d %s", &read_id, &chr, &offset, &srand, &edit_dis, cigar);
		if (read_id == last_id) {		// seeds from same read
			if (++multi_aln > PER_ALN_N) {
				if (!REPEAT) {
					n_seed--;
					REPEAT = 1;
				}
				continue;
			} else setAmsg(a_msg, n_seed, multi_aln, read_id - s_msg->n_seed[n_read-1], chr, (int64_t)offset, srand, edit_dis, cigar);
		} else {		//get a new seed
			REPEAT = 0;
			if (read_id > s_msg->n_seed[n_read]) {	//new read
				if (last_id != 0) {
					fprintf(stdout, "read %d start %d n_seed %d\n", n_read, s_msg->n_seed[n_read-1]+1, n_seed);
					if (path_dp(a_msg, n_seed, line, path_end, path, price, price_n, seed_len, bns->n_seqs))
                   // if (path_dp(a_msg, n_seed, main_line, main_price, main_path, path, price, price_n, seed_len, bns->n_seqs))
						if (backtrack(a_msg, path, n_seed, price, price_n, seed_len, f_msg))
					/* SW-extenging */
							frag_check(bns, pac, read_prefix, read_seq, f_msg, a_msg, seed_len, s_msg->last_len[n_read]);
				}
				n_seed = 0;
				while (s_msg->n_seed[n_read] < read_id) {
					if (FLAG == 0) FLAG = 1;
					//else fprintf(stdout, "read %d n_seed 0\nfrag: 0\n\n", n_read);
					++n_read;
					if (kseq_read(read_seq_t) < 0) {
						fprintf(stderr, "[lsat_aln] Read file ERROR.\n");
						exit(-1);
					}
					read_seq = read_seq_t->seq.s;
				}
				FLAG = 0;
			}
			multi_aln = 1;
			last_id = read_id;
			if (n_seed >= s_msg->seed_max)	{
				fprintf(stderr, "[lsat_lan] bug: n_seed > seed_max\n");
				exit(-1);
			}
			++n_seed;
			setAmsg(a_msg, n_seed, multi_aln, read_id-s_msg->n_seed[n_read-1], chr, offset, srand, edit_dis, cigar);
		}
	}
	fprintf(stdout, "read %d start %d n_seed %d\n", n_read, s_msg->n_seed[n_read-1]+1, n_seed);
	if (path_dp(a_msg, n_seed, line, path_end, path, price, price_n, seed_len, bns->n_seqs))
    //if (path_dp(a_msg, n_seed, main_line, main_price, main_path, path, price, price_n, seed_len, bns->n_seqs))
		if (backtrack(a_msg, path, n_seed, price, price_n, seed_len, f_msg))
		/* SW-extenging */
			frag_check(bns, pac, read_prefix, read_seq, f_msg, a_msg, seed_len, s_msg->last_len[n_read]);

	fclose(result_p);
	aln_free_msg(a_msg, s_msg->seed_max);
	for (i = 0; i < s_msg->seed_max; ++i)
	{ free(line[i]); free(price[i]); free(path[i]);    free(main_price[i]); free(main_path[i]);}
	free(line); free(price); free(price_n); free(path); free(path_end);
    free(main_price); free(main_path);
	frag_free_msg(f_msg);
	gzclose(readfp);
	kseq_destroy(read_seq_t);
	//free(read_seq);

	return 0;
}

/* relative path convert for soap2-dp */
void relat_path(const char *ref_path, const char *soap_dir, char *relat_ref_path)	
{
	int i;
	char lsat_dir[1024], abs_soap_dir[1024], abs_ref_path[1024], ref_dir[1024], ref_file[1024];

	if (getcwd(lsat_dir, 1024) == NULL) { perror("getcwd error"); exit(-1); } 
	if (chdir(soap_dir) != 0) { perror("Wrong soap2-dp path"); exit(-1); }
	if (getcwd(abs_soap_dir, 1024) == NULL) { perror("getcwd error"); exit(-1); }

	//printf("ref: %s\n",ref_path);
	if (ref_path[0] == '.')
	{
		if (chdir(lsat_dir) != 0) { perror("Wrong soap2-dp path"); exit(-1); }
		strcpy(ref_dir, ref_path);
		for (i = strlen(ref_path)-1; i >= 0; i--)
			if (ref_path[i] == '/') { ref_dir[i] = '\0'; strncpy(ref_file, ref_path+i, 1024); }
		if (chdir(ref_dir) != 0) { perror("Wrong soap2-dp path"); exit(-1); }
		if (getcwd(abs_ref_path, 1024) == NULL) { perror("getcwd error"); exit(-1); }
		strcat(abs_ref_path, ref_file);
	}
	else
		strcpy(abs_ref_path, ref_path);

	if (chdir(lsat_dir) != 0) { perror("chdir error"); exit(-1); }
	//printf("soap: %s\nref: %s\n", abs_soap_dir, abs_ref_path);
	int dif=-1;
	for (i = 0; i < strlen(abs_soap_dir); ++i)
	{
		if (abs_soap_dir[i] != abs_ref_path[i]) break;
		if (abs_soap_dir[i] == '/') dif = i;
	}
	//printf("i: %d dif: %d\n", i, dif);
	if(dif == -1)
	{
		fprintf(stderr, "[lsat_aln] dir bug\n");
		exit(-1);
	}
	strcpy(relat_ref_path, "./");
	for (i = dif; i < strlen(abs_soap_dir); ++i)
	{
		if (abs_soap_dir[i] == '/')
			strcat(relat_ref_path, "../");
	}
	strcat(relat_ref_path, abs_ref_path+dif+1);
}

int lsat_soap2_dp(const char *ref_prefix, const char *read_prefix, char *opt_m)
{
	char relat_ref_path[1024], relat_read_path[1024];
	char lsat_dir[1024];

	if (getcwd(lsat_dir, 1024) == NULL) { perror("getcwd error"); exit(-1); } 
	relat_path(ref_prefix, SOAP2_DP_DIR, relat_ref_path);
	relat_path(read_prefix, SOAP2_DP_DIR, relat_read_path);
	if (chdir(SOAP2_DP_DIR) != 0) { perror("Wrong soap2-dp dir"); exit(-1); }

	char soap2_dp_cmd[1024];
	sprintf(soap2_dp_cmd, "./soap2-dp single %s.index %s.seed -h 2 %s > %s.seed.aln", relat_ref_path, relat_read_path, opt_m, relat_read_path);
	fprintf(stderr, "[lsat_aln] Executing soap2-dp ... ");
	if (system (soap2_dp_cmd) != 0)
		exit(-1);
	fprintf(stderr, "done.\n");

	if (chdir(lsat_dir) != 0) { perror("chdir error"); exit(-1); }
	return 0;
}

int lsat_aln_core(const char *ref_prefix, const char *read_prefix, int seed_info, int no_soap2_dp, char *seed_result, char *opt_m, int opt_l)
{
	seed_msg *s_msg;
	bntseq_t *bns;
    int seed_len;

	/* split-seeding */
	s_msg = seed_init_msg();
    if (seed_info)
        split_seed_info(read_prefix, s_msg, &seed_len);
    else
    {
        seed_len = opt_l;
        split_seed(read_prefix, s_msg, opt_l);
    }

	if (!strcmp(seed_result, ""))
	{
		strcpy(seed_result, read_prefix);
		strcat(seed_result, ".seed.out.0");
	}
	//excute soap2-dp program
	if (!no_soap2_dp) lsat_soap2_dp(ref_prefix, read_prefix, opt_m);

	/* frag-clustering */
	/* SW-extending for per-frag */
	fprintf(stderr, "[lsat_aln] Restoring ref-indices ... ");
	bns = bns_restore(ref_prefix);
	uint8_t *pac = (uint8_t*)calloc(bns->l_pac/4+1, 1);
	fread(pac, 1, bns->l_pac/4+1, bns->fp_pac);	fprintf(stderr, "done.\n");
	fprintf(stderr, "[lsat_aln] Clustering frag ... ");
	frag_cluster(read_prefix, seed_result, s_msg, opt_l, bns, pac);	fprintf(stderr, "done.\n");

	seed_free_msg(s_msg);
	free(pac);
	bns_destroy(bns);
	return 0;
}

int lsat_aln(int argc, char *argv[])
{
	int c;
	int no_soap2_dp=0, seed_info=0, opt_l=0;
	char result_f[1024]="", opt_m[100];
	char *ref, *read;
	
	opt_l = SEED_LEN;
	strcpy(opt_m, "-m 3e ");

	while ((c =getopt(argc, argv, "nsa:m:l:")) >= 0)
	{
		switch (c)
		{
			case 'n':
				no_soap2_dp = 1;
				break;
            case 's':
                seed_info = 1;
                break;
			case 'a':
				strcpy(result_f, optarg);	//soap2-dp alignment result
				break;
			case 'm':
				sprintf(opt_m, "-m %s ", optarg);
				break;
			case 'l':
				opt_l = atoi(optarg);
				break;
			default:
				return usage();
		}
	}
    if (argc - optind != 2)
		return usage();

	ref = strdup(argv[optind]);
	read =strdup(argv[optind+1]);

	lsat_aln_core(ref, read, seed_info, no_soap2_dp, result_f, opt_m, opt_l);
	
	return 0;
}
