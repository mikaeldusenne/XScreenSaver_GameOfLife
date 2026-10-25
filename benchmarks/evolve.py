#!/usr/bin/env python3
"""Differential and CPU benchmark of generation kernels; requires a C99 compiler."""
from pathlib import Path
import argparse
import subprocess
import tempfile
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc',default='cc')
parser.add_argument('--sanitize',action='store_true')
args=parser.parse_args()
source=Path(__file__).resolve().parents[1]/'game_of_life.c'
s=source.read_text()
loop='  for( i = nx+1 ; i < SQ-nx-1 ; i++){\n\t  if(b1[i]!=-1){\n\t\t  nb=0;\n\t\t  if(b1[i-(nx+1)]   == NSTEP-1 ) nb++;\n\t\t  \n\t\t  if(b1[i+(nx+1)]   == NSTEP-1 ) nb++;\n\t\t  \n\t\t  if(b1[i-1]        == NSTEP-1 ) nb++;\n\n\t\t  if(b1[i+1]        == NSTEP-1 ) nb++;\n\n\t\t  if(b1[i-(nx+1)-1] == NSTEP-1 ) nb++;\n\n\t\t  if(b1[i+(nx+1)-1] == NSTEP-1 ) nb++;\n\n\t\t  if(b1[i-(nx+1)+1] == NSTEP-1 ) nb++;\n\n\t\t  if(b1[i+(nx+1)+1] == NSTEP-1 ) nb++;\n\n\n\t\t  \n\t\t  if(b1[i]==NSTEP-1){\n\t\t\t  if(nb<2 || nb>3){\n\t\t\t\t  b2[i] = b1[i]-1;\n\n\t\t\t\t  diff++;\n\t\t\t  }\n\t\t\t  else{\n\t\t\t\t  b2[i] = b1[i];\n\n\t\t\t  }\n\t\t  }else{\n\t\t\t  if(nb==3){\n\t\t\t\t  b2[i] = NSTEP-1;\n\t\t\t\t  created++;\n\n\t\t\t  }\n\t\t\t  else if(b1[i]){\n\t\t\t\t  b2[i] = b1[i]-1;\n\n\t\t\t  }else{\n\t\t\t\t  b2[i] = 0;\n\n\t\t\t  }\n\t\t  }\n\n\t\t  \n\t\t  if(b2[i]>0) up++;\n\n\t  }\n  }\n\n'
head='''#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
const char *__asan_default_options(void){return "detect_leaks=0";}
typedef uint64_t hash_t;
#define NSTEP_MAX 50
int validate_changes=1;int NSTEP,nx,ny,SQ;hash_t *zobrist_table;
'''
helper=s[s.index('struct evolution {'):s.index('/* Return a timer delay')]
ref='''static struct evolution reference(const int*b1,int*b2,int collect_hash){int i,nb,diff=0,up=0,created=0;'''+loop+'''struct evolution r={up,0,0};if(validate_changes||collect_hash)for(int y=0;y<ny;y++)for(int x=1;x<=nx;x++){int i=(y+1)*(nx+1)+x;if(validate_changes)r.changed|=b1[i]!=b2[i];if(collect_hash&&b2[i]==NSTEP-1)r.hash^=zobrist_table[i];}return r;}\n'''
main=r'''
static unsigned seed=123;static unsigned rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
volatile uint64_t checksum;
int main(void){
 for(int scenario=0;scenario<60;scenario++){
 nx=scenario<3?scenario+1:37+scenario;ny=scenario%11+1;NSTEP=scenario%3==0?2:scenario%3==1?3:50;SQ=(nx+1)*(ny+2);
 int*a=malloc((SQ+1)*sizeof(int)),*b=malloc((SQ+1)*sizeof(int)),*c=malloc((SQ+1)*sizeof(int));zobrist_table=malloc((SQ+1)*sizeof(hash_t));
 for(int i=0;i<=SQ;i++){a[i]=b[i]=c[i]=-1;zobrist_table[i]=((hash_t)rnd()<<32)|rnd();}
 for(int trial=0;trial<50;trial++){
 for(int y=0;y<ny;y++)for(int x=1;x<=nx;x++)a[(y+1)*(nx+1)+x]=rnd()%NSTEP;
 struct evolution r=reference(a,b,trial&1),s=evolve_board(a,c,trial&1);
 assert(r.occupied==s.occupied&&r.changed==s.changed&&r.hash==s.hash);
 for(int i=0;i<=SQ;i++)assert(b[i]==c[i]);
 }
 free(a);free(b);free(c);free(zobrist_table);
 }
 puts("3000 differential boards: states, occupancy, change flag and hash PASS (trails 2/3/50, narrow grids and sentinels)");
 validate_changes=0;nx=640;ny=360;NSTEP=50;SQ=(nx+1)*(ny+2);int*a=malloc((SQ+1)*sizeof(int)),*b=malloc((SQ+1)*sizeof(int));zobrist_table=calloc(SQ+1,sizeof(hash_t));
 for(int density=0;density<3;density++){
 for(int i=0;i<=SQ;i++)a[i]=b[i]=-1;
 for(int y=0;y<ny;y++)for(int x=1;x<=nx;x++)a[(y+1)*(nx+1)+x]=rnd()%100<(density==0?1:density==1?25:75)?49:rnd()%49;
 for(int hash=0;hash<2;hash++){
 double times[2];for(int mode=0;mode<2;mode++){double begin=now();for(int f=0;f<200;f++){struct evolution r=mode?evolve_board(a,b,hash):reference(a,b,hash);checksum+=r.occupied+r.changed+r.hash+b[100];}times[mode]=now()-begin;}
 printf("density %d%% hash %d: %.3f -> %.3f ms/generation, %.2fx\n",density==0?1:density==1?25:75,hash,times[0]*5,times[1]*5,times[0]/times[1]);
 }
 }
 free(a);free(b);free(zobrist_table);return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='.evolve-bench-',dir=source.parent) as directory:
    directory=Path(directory)
    c=directory/'compare.c'
    exe=directory/'compare'
    c.write_text(head+helper+ref+main)
    flags=['-O1','-g','-fsanitize=address,undefined'] if args.sanitize else ['-O3']
    subprocess.run([args.cc,'-std=c99',*flags,str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
