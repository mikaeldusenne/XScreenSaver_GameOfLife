#!/usr/bin/env python3
"""Measure the renderer CPU kernel with X11 calls replaced by local recording.

Requires Python 3 and a C compiler; no X server or Python packages are needed.
The pixel assertions validate the benchmark's rendering scenarios, not X11.
"""
from pathlib import Path
import argparse
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1] / 'game_of_life.c')
parser.add_argument('--cc', default='cc')
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
s=args.source.read_text()
helper=s[s.index('static void flush_rectangles'):s.index('static unsigned long\ngame_of_life_draw')]
preamble=r'''
#define _POSIX_C_SOURCE 200809L
const char *__asan_default_options(void){return "detect_leaks=0";}
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#define NSTEP_MAX 50
#define RECT_BATCH_SIZE 256
typedef struct {short x,y;unsigned short width,height;} XRectangle;
struct state {void *dpy;int window,gc;unsigned long colors[50];XRectangle rectangles[50][256];int rectangle_count[50],redraw,xlim,ylim;};
int nx,ny,w,h,NSTEP=50;
unsigned long foreground;unsigned long *pixels;int rasterize;
volatile unsigned long checksum;long calls,rects;
static void XSetForeground(void*d,int g,unsigned long c){foreground=c;}
static void XFillRectangle(void*d,int win,int gc,int x,int y,unsigned width,unsigned height){
 calls++;rects++;checksum+=foreground+width+height+x+y;
 if(rasterize)for(unsigned yy=y;yy<y+height;yy++)for(unsigned xx=x;xx<x+width;xx++)pixels[yy*(nx*w+3)+xx]=foreground;
}
static void XFillRectangles(void*d,int win,int gc,XRectangle*r,int n){
 calls++;rects+=n;
 for(int i=0;i<n;i++){
 checksum+=foreground+r[i].width+r[i].height+r[i].x+r[i].y;
 if(rasterize)for(unsigned yy=r[i].y;yy<r[i].y+r[i].height;yy++)for(unsigned xx=r[i].x;xx<r[i].x+r[i].width;xx++)pixels[yy*(nx*w+3)+xx]=foreground;
 }
}
static void baseline(struct state*st,const int*previous,const int*next){
 for(int k=0;k<NSTEP;k++){
 XSetForeground(0,0,st->colors[k]);
 for(int y=0;y<ny;y++)for(int x=0;x<nx;x++)if(next[(y+1)*(nx+1)+x+1]==k)XFillRectangle(0,0,0,x*w,y*h,w,h);
 }
}
static double now(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec*1e-9;}
static unsigned rng=1234567;
static unsigned rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
'''
main=r'''
int main(void){
 struct state *st=calloc(1,sizeof(*st));for(int k=0;k<50;k++)st->colors[k]=k;
 /* Pixel equivalence: trails, zeros, same colours, long runs, batches and exposure. */
 nx=319;ny=181;w=2;h=3;st->xlim=nx*w+3;st->ylim=ny*h+2;
 int size=(nx+1)*(ny+2)+1;int*a=calloc(size,sizeof(int)),*b=calloc(size,sizeof(int));
 int np=st->xlim*st->ylim;unsigned long *expected=calloc(np,sizeof(long)),*actual=calloc(np,sizeof(long));
 rasterize=1;
 for(int frame=0;frame<80;frame++){
  for(int y=0;y<ny;y++)for(int x=0;x<nx;x++){
   int i=(y+1)*(nx+1)+x+1;
   if(frame==0||frame%9==0)b[i]=(x/30+y)%50;
   else if(frame%11==0)b[i]=0;
   else b[i]=rnd()%10==0?rnd()%50:a[i];
  }
  pixels=expected;baseline(st,a,b);
  pixels=actual;st->redraw=(frame==0||frame%7==0);render_frame(st,a,b);
  assert(!memcmp(expected,actual,np*sizeof(long)));
  memcpy(a,b,size*sizeof(int));
 }
 printf("Pixel equivalence: 80 frames, 319x181 cells, trails=50, size=2x3, exposure and margins: PASS\n");
 free(a);free(b);free(expected);free(actual);
 rasterize=0;nx=640;ny=360;w=h=1;st->xlim=nx;st->ylim=ny;
 size=(nx+1)*(ny+2)+1;a=calloc(size,sizeof(int));b=calloc(size,sizeof(int));
 for(int scenario=0;scenario<3;scenario++){
  for(int y=0;y<ny;y++)for(int x=0;x<nx;x++){
   int i=(y+1)*(nx+1)+x+1;a[i]=rnd()%50;
   b[i]=(scenario==0|| (scenario==1&&rnd()%10))?a[i]:(a[i]+1)%50;
  }
  double times[2];long requests[2];
  for(int mode=0;mode<2;mode++){
   calls=rects=0;double begin=now();
   for(int frame=0;frame<60;frame++)if(mode)render_frame(st,a,b);else baseline(st,a,b);
   times[mode]=now()-begin;requests[mode]=calls/60;
  }
  printf("%s: baseline %.3f ms/frame, optimized %.3f ms/frame, %.2fx; X drawing calls %ld -> %ld/frame\n",scenario==0?"unchanged":scenario==1?"10% dirty":"all dirty",times[0]*1000/60,times[1]*1000/60,times[0]/times[1],requests[0],requests[1]);
 }
 free(a);free(b);free(st);return checksum==0;
}
'''
with tempfile.TemporaryDirectory(prefix='.renderer-bench-', dir=args.source.resolve().parent) as folder:
    folder = Path(folder)
    source = folder / 'renderer.c'
    executable = folder / 'renderer'
    source.write_text(preamble + helper + main)
    flags = ['-O1', '-g', '-fsanitize=address,undefined'] if args.sanitize else ['-O3']
    subprocess.run([args.cc, '-std=c99', *flags, str(source), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
