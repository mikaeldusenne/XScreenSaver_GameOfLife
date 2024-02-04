/* xscreensaver
 *
 * Permission to use, copy, modify, distribute, and sell this softwa
re and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */



#include "screenhack.h"
#include <time.h>
#include "assert.h"
#include "stdint.h"


#define NBITS 12
#define NSTEP_MAX 50
int NSTEP = NSTEP_MAX;

int SZ = 2;

typedef uint64_t hash_t;
hash_t *zobrist_table;

/* #define SZ 2 */

int which_board=0;

typedef struct stack{
	int MAX_SIZE;
	int index;
        int *data;
}stack;

typedef int neighbors[8];

struct state {
 	Display *dpy;
	Window window;

	Pixmap pixmaps [NBITS];

	GC gc;
	
	int delay;
	unsigned long colors[NSTEP_MAX];
	double step_col[3];
	
	int xlim, ylim;
	stack ok_sizes;

	Colormap cmap;

	stack px;
	
	XColor xcol;

	int hash_activated;
	hash_t prev_hash[5];
	int ultimatum;

	double scrollcount,scrollspeed;
	
};

clock_t timecount;
int nb_gen;
int nx,ny, w, h, SQ;

/* board representation:
* 
* x x x x x x x x
* . . . . . . . x
* . . . . . . . x
* . . . . . . . x
* . . . . . . . x
* . . . . . . . x
* . . . . . . . x
* . . . . . . . x
* x x x x x x x x
* 
*/ 

/* char board[SIZE_MAX],b2[SIZE_MAX]; */
int **boards;

static unsigned long hex_to_long(const char* hex);
static void print_hex_value(unsigned long color);
static void render_console(int *b);
static void reinit_board(int size);


static void init_zobrist(void);
static hash_t hash_board(int *b);

static void init_stack(stack *s);
static void destroy_stack(stack *s);
static void stack_push(int c, stack *s);

static void stack_disp(stack *s);

static void scroll();
static inline void feed_neighbors(neighbors *nb, int i, int *b);

static void stack_disp(stack *s){
	int i;
	
	printf("-------- stack [ %d / %d ] --------",s->index, s->MAX_SIZE );

	for(i=0; i<s->index; i++){

		if( !(i%10)) printf("\n");

		printf("%d\t",s->data[i]);
		
	}
	printf("\n----------------------------------\n");
	
}

static void init_stack(stack *s){
	
	s->MAX_SIZE = 10;
	s->index = 0;
	s->data = calloc(s->MAX_SIZE, sizeof(int) );
}

static void destroy_stack(stack *s){
	free(s->data);
}


static void stack_push(int c, stack *s){

	if(s->index >= s->MAX_SIZE){
		s->MAX_SIZE *= 2;
		s->data = realloc(s->data, s->MAX_SIZE * sizeof(int));
	}

	s->data[s->index++] = c;

	/* stack_disp(s); */
	
}

static int stack_pop(stack *s){
	return s->data[--s->index];
}




static unsigned long hex_to_long(const char* hex){
	unsigned long cc;
	const char *p = hex+1;
	char part[] = "00";
	/* int r,g,b; */
	unsigned long c = 0;
	
	while( *p != '\0' ){
		part[0] = *(p++);
		part[1] = *(p++);
			
		cc = strtol(part, NULL, 16);
		/* printf("%s == %lu\n",part,cc); */
		c = (c<<8) + cc;
	}

	return c;
}

static void print_hex_value(unsigned long color){

	/* printf("%06x",color); */
}

static void reinit_board(int size){
	int i,k;
	timecount = 0;
	
	for(k=0; k<2; k++){
		for(i=0; i<SQ; i++){
			if( i < nx+1 || i+nx+1 >= SQ || i%(nx+1) == 0 ){
				boards[k][i] = -1;
			}
			else{
				// boards[k][i] = k ? 0 : ( ( (random()&1)) * (NSTEP-1) ) ;
                boards[k][i] = k ? 0 : ( ( (random()&3) ? 1 : 0) * (NSTEP-1) ) ;
			}
		}
	}

	nb_gen=0;
	/* fprintf(stderr,"REINIT BOARD"); */
}




static void *
game_of_life_init (Display *dpy, Window window){
	int i,k;
	struct state *st = (struct state *) calloc (1, sizeof(*st));

	st->scrollspeed = .1;

	XGCValues gcv;
	XWindowAttributes xgwa;
	int perso_step=0;
	
	unsigned long *p;
	
	st->dpy = dpy;
	st->window = window;

	
	


	NSTEP = 2 + get_integer_resource (dpy, "trail", "Integer");
	if(NSTEP>NSTEP_MAX) NSTEP = NSTEP_MAX;
	/* fprintf(stderr,"-------->>>> %d\n\n",NSTEP);	 */
	

	XGetWindowAttributes (st->dpy, st->window, &xgwa);
	st->xlim = xgwa.width;
	st->ylim = xgwa.height;
	st->cmap = xgwa.colormap;
	/* st->grey_p = get_boolean_resource(st->dpy, "grey", "Boolean"); */
	/* gcv.foreground= get_pixel_resource(st->dpy, st->cmap, "foreground","Foreground"); */
	/* gcv.background= get_pixel_resource(st->dpy, st->cmap, "background","Background"); */

	/* st->fg = 37080; */
	/* st->bg = st->fg/2; */

	/* st->colors[0] = 16738UL; */
	/* st->colors[NSTEP-1] = 1545168UL; */
  
	/* st->colors[0] = (255UL << 8 ); */

	init_stack(&(st->ok_sizes));
	printf("----------------\n");
	for( i=2 ; i<st->xlim ; i++){
		if( ! ( st->xlim % i || st->ylim % i ) ){
			stack_push(i, &(st->ok_sizes));
			printf("%d\n",i);
		}
	}
	printf("----------------\n");

	st->colors[NSTEP-1] = hex_to_long("#2a89b7"); /* 1794d0 */
	st->colors[0] = hex_to_long("#010024"); /*  112592 004162 122238 */


	if(perso_step){
	  
		st->colors[2] = hex_to_long("#d0921a"); /*  d0921a   ff9011*/
		st->colors[1] = hex_to_long("#b9792f");
	}else{
		double rgba[3],rgbb[3];/* ,steprgb[3]; */

		for( i=0; i<3; i++){
			rgbb[i] = (st->colors[NSTEP-1] >> (i*8)) & 255;
			rgba[i] = (st->colors[0] >> (i*8)) & 255;
			st->step_col[i] = ((double)(rgba[i]-rgbb[i]))/NSTEP;
			/* printf("%d\n", (rgbb[i]-rgba[i])/NSTEP); */
		}
	  
		/* print_hex_value(st->colors[0]); */
		/* printf("\t"); */
		/* print_hex_value(st->colors[NSTEP-1]); */
		/* printf("\n"); */
		/* printf("rgba\trgbb\tstep\n"); */
		/* for(i=0;i<3;i++){ */

		/* 	  printf("%03d\t%03d\t%03d\n",rgba[i],rgbb[i],st->step_col[i]); */
	  
		/* } */
		/* printf("\nSTEPS: %d ; %d ; %d\n",st->step_col[0], st->step_col[1], st->step_col[2]); */
  
  
		for(i=1; i<NSTEP-1; i++){
			p = &(st->colors[i]);
			*p = 0;
			for(k=0;k<3;k++){
				rgba[k] -= st->step_col[k];
				*p = (*p << 8) + ( ((int)rgba[k]) & 255);
			}

			/* st->colors[i] =  */
		  
			/* st->colors[i] = st->colors[i-1]; */
			/* + (st->colors[0]-st->colors[NSTEP-1])/NSTEP ; */

		}

		for(i=0 ; i<NSTEP ; i++){

			print_hex_value(st->colors[i]);
			/* printf("\n"); */

		}
  
	}
	st->delay = get_integer_resource (st->dpy, "delay", "Integer");
	if (st->delay < 0) st->delay = 0;

	st->gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);

	SZ = get_integer_resource(dpy, "size", "Integer");
	w=SZ;
	h=SZ;
	
	nx = st->xlim ;
	ny = st->ylim ;

	/* printf("%d ; %d\n",nx,ny); */
  
	SQ = (nx+1)*(ny+2);

	boards = malloc( 2 * sizeof ( int* ));
	for( i=0; i<2; i++){
		boards[i] = malloc(SQ * sizeof(int));
	}
	
		
	/* st->px = calloc( nx*ny , sizeof(int) ); */
	init_stack(&(st->px));
	
	
	init_zobrist();
	
	nx = st->xlim / w;
	ny = st->ylim / h;
  
	SQ = (nx+1)*(ny+2);
	
	reinit_board(SZ);
	st->ultimatum=-1;
  
	return st;
}

static void init_zobrist(){
	int i,j;
	hash_t mask = ((1UL)<<15)-1;


	/* srand(time(NULL)); */
	
	zobrist_table = calloc( SQ, sizeof(hash_t));
	
	for( i=0 ; i<SQ ; i++){
		for(j=0; j<4;j++){
			zobrist_table[i] = (zobrist_table[i]<<16) | (random() & mask);
		}
	}
}

static hash_t hash_board(int *b){
	/* clock_t t = clock(); */
	int i;
	hash_t h=0;

	for( i = nx+1 ; i < (SQ-nx-1) ; i++){
		if(b[i]==NSTEP-1) h ^= zobrist_table[i];
	}

	/* fprintf(stderr,"\n%f ~~~~~~~~\n",((float)(clock()-t))/CLOCKS_PER_SEC); */

	
	
	return h;
}

static void render_console(int *b){
	int i;
	printf("\n--------\n");

	for( i = nx+1 ; i < SQ-nx-1 ; i++){
		if(!(i%(nx+1)))printf("\n");
		printf("%c",b[i]==NSTEP-1?'#':'.');
	}
	
	printf("\n--------\n");
}

static unsigned long
game_of_life_draw (Display *dpy, Window window, void *closure){
 /* clock_t t = clock(); */
  struct state *st = (struct state *) closure;
  int x, y, i, k;
/*   XGCValues gcv; */
  
  int *b1 = boards[which_board],
	  *b2 = boards[1-which_board];
  
  int nb,diff=0,up=0,created=0;
  
 /* # ifndef DO_STIPPLE */
/*   XChangeGC (st->dpy, st->gc, GCForeground, &gcv); */
/* # else  /\* DO_STIPPLE *\/ */
/*   XChangeGC (st->dpy, st->gc, GCStipple|GCForeground|GCBackground, &gcv); */
/* # endif /\* DO_STIPPLE *\/ */

  /* printf("\n%d\n",which_board); */
  /* assert(which_board==0 || which_board==1); */

  /* st->scrollcount += st->scrollspeed; */
  
  /* if(st->scrollcount>=1){ */
  /* 	  y = st->scrollcount; */
  /* 	  for( i=1 ; i<ny+1 ; i++ ){ */
  /* 		  k = i * (nx+1); */
  /* 		  memcpy( &b1[k+1] , &b1[k+2] , sizeof(int) * (nx-1) ); */
  /* 		  for( x = g */
  /* 		  b1[ k + nx ] = random() & 1 ? NSTEP : 0 ; */
  /* 		  /\* printf("%d -> %d\n", k, b1[k+nx - 1]); *\/ */
  /* 	  } */
  /* 	  st->scrollcount = 0; */
  /* } */
  
  /* st->px.index=0; */

  
/*  int is[NSTEP] = {0}; */
  
  for( i = nx+1 ; i < SQ-nx-1 ; i++){
	  if(b1[i]!=-1){
		  nb=0;
		  if(b1[i-(nx+1)]   == NSTEP-1 ) nb++;
		  
		  if(b1[i+(nx+1)]   == NSTEP-1 ) nb++;
		  
		  if(b1[i-1]        == NSTEP-1 ) nb++;

		  if(b1[i+1]        == NSTEP-1 ) nb++;

		  if(b1[i-(nx+1)-1] == NSTEP-1 ) nb++;

		  if(b1[i+(nx+1)-1] == NSTEP-1 ) nb++;

		  if(b1[i-(nx+1)+1] == NSTEP-1 ) nb++;

		  if(b1[i+(nx+1)+1] == NSTEP-1 ) nb++;


		  
		  if(b1[i]==NSTEP-1){
			  if(nb<2 || nb>3){
				  b2[i] = b1[i]-1;
				  /* st->px[b2[i]][ is[b2[i]]++ ] = i; */

				  stack_push(i, &st->px);
				  
				  diff++;
			  }
			  else{
				  b2[i] = b1[i];
				  stack_push(i, &st->px);
				  /* st->px[b2[i]][ is[b2[i]]++ ] = i; */
			  }
		  }else{
			  if(nb==3){
				  b2[i] = NSTEP-1;
				  created++;
				  stack_push(i, &st->px);
				  /* st->px[b2[i]][ is[b2[i]]++ ] = i; */
			  }
			  else if(b1[i]){
				  b2[i] = b1[i]-1;
				  stack_push(i, &st->px);
				  /* st->px[b2[i]][ is[b2[i]]++ ] = i; */
			  }else{
				  b2[i] = 0;
				  stack_push(i, &st->px);
				  /* st->px[b2[i]][ is[b2[i]]++ ] = i; */
			  }
		  }

		  
		  if(b2[i]>0) up++;

	  }
  }

  /* fprintf(stderr,"%d up\n",up); */
  
  /* printf("%d",(nx*nx)/(diff ? diff : 1)); */

  /* if( st->ultimatum==-1 && diff && (nx*nx)/diff > 100 && !st->hash_activated){ */
  /* 	  st->hash_activated=5; */
  /* 	  st->prev_hash[0] = hash_board(b1); */
  /* } */


  
  if(st->hash_activated){
  	  hash_t h = hash_board(b2);
  	  int same = 0;
	  
  	  /* for(i=0;i<64;i++){ */
  	  /* 	  printf("%c", (h & (1ULL << i) ) ? '1' : '0'); */
  	  /* } */
  	  /* printf("\n"); */
  	  for(k=0;k<5;k++){
  		  if( h == st->prev_hash[k] ) same = 1;
  		  /* for(i=0;i<64;i++){ */
  		  /* 	  printf("%c", (st->prev_hash[k] & (1ULL << i) ) ? '1' : '0'); */
  		  /* } */
  		  /* printf("\n"); */
  	  }
  	  /* printf("--------\n"); */
  	  if(same ){

  		  st->ultimatum = 5;
  		  st->hash_activated=0;
  	  }else{
  		  st->prev_hash[--st->hash_activated] = h;
		  
  	  }
	  
  }

  

  if(!st->ultimatum){
  	  for( i = nx+1 ; i < SQ-nx-1 ; i++){
  		  if(b2[i]==NSTEP-1) b2[i]--;
  	  }
  }
  if(st->ultimatum > 0){
  	  st->ultimatum--;
  	  /* printf("@@@@@@@@  %d  @@@@@@@@",st->ultimatum); */
  }
  if(up==0){
	  /* SZ = st->ok_sizes.data[random() % st->ok_sizes.index]; */
	  SZ = st->ok_sizes.data[2];
	  w=SZ;
	  h=SZ;
	  
	  nx = st->xlim / w;
	  ny = st->ylim / h;
	  
	  /* printf("%d ; %d\n",nx,ny); */
	  
	  SQ = (nx+1)*(ny+2);
	  
	  reinit_board(SZ);
	  printf("\n%d\n",SZ);
  	  st->ultimatum=-1;
  }

  if( !(nb_gen & 63) ){

	  st->hash_activated = 5;
	  /* printf("generation %d\n",nb_gen); */
  }

  /* render_console(b1); */
  
  /* memcpy( b1, b2, sizeof(int)*SQ); */
  which_board = 1-which_board;
  /* b1=b2; */

  for( k=0; k<NSTEP; k++){
	  XSetForeground(st->dpy, st->gc,
			 st->colors[ k ]
		  );

	  
	  
	  for( i = 0; i < st->px.index; i++){
		  if( b2[ st->px.data[i] ] == k ){

			  x = ((st->px.data[i]%(nx+1))-1)*w;
			  y = (st->px.data[i]/(nx+1)-1)*h;
			  
			  XFillRectangle (
				  st->dpy,
				  st->window,
				  st->gc,
				  x, y, w, h
				  );
		  
		  }
		  
		  /* } */

	  }

  }

  /* XColor fgc,bgc; */


  /* px=calloc( 2+NSTEP , sizeof(unsigned long *)); */

  /* for( i=0 ; i<2+NSTEP ; i++){ */
  /* 	  px[i] = calloc( 10000, sizeof(unsigned long) ); */
  /* } */

  /* clock_t duration = clock()-t; */

  /* if( ! (nb_gen%10) ){ */
  /* 	  fprintf(stderr,"[ %f ]        \r",(1000.0f*(float)timecount)/10/CLOCKS_PER_SEC); */
  /* 	  timecount = 0; */
  /* }else{ */
  /* 	  timecount += duration; */
  /* } */
  
  


  nb_gen++;
  
  return st->delay;
}


static const char *game_of_life_defaults [] = {
  "*fpsSolid:	true",
  "*delay:	100000",
  "*size:	1",
  "*trail:	50",
#ifdef USE_IPHONE
  "*ignoreRotation: True",
#endif
  0
};

static XrmOptionDescRec game_of_life_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-size",		".size",	XrmoptionSepArg, "2" },
  { "-trail",		".trail",	XrmoptionSepArg, "50" },
  { 0, 0, 0, 0 }
};

static void
game_of_life_reshape (Display *dpy, Window window, void *closure,
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->xlim = w;
  st->ylim = h;
  /* reinit_board(); */
}

static Bool
game_of_life_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
game_of_life_free (Display *dpy, Window window, void *closure)
{
	free(boards[0]);
	free(boards[1]);
	free(zobrist_table);
	destroy_stack(&((struct state *)closure)->px);
}

XSCREENSAVER_MODULE ("GameOfLife", game_of_lifs)
