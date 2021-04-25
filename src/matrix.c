/* matrixgl - Cross-platform matrix screensaver
 * Copyright (C) Alexander Zolotov, Eugene Zolotov 2003.
 * Based on matrixgl 1.0 (see http://knoppix.ru/matrixgl.shtml)
 * -------------------------------------------
 * Written By:  Alexander Zolotov  <nightradio@gmail.com> 2003.
 *       and :  Eugene Zolotov     <sentinel@knoppix.ru> 2003.
 * Modified By: Vincent Launchbury <vincent@doublecreations.com> 2008-2010.
 * -------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  US
 */

#ifndef NIX_MODE
   #define WIN32_MODE
#endif

/* Includes */
#ifdef WIN32_MODE
   #include <windows.h>
#else /* NIX_MODE */
   #include <X11/Xlib.h>
   #include <X11/Xutil.h>
   #define __USE_XOPEN_EXTENDED
   #include <unistd.h>
   #include <getopt.h>
   #include <time.h>
#endif /* NIX_MODE */

#include <stdio.h>   /* Always a good idea. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <GL/gl.h>   /* OpenGL itself. */
#include <GL/glu.h>  /* GLU support library. */
#ifdef NIX_MODE
   #include <GL/glx.h>
   #include "vroot.h"
#else /* WIN32_MODE */
   #include <GL/glut.h> /* GLUT support library. */
#endif /* WIN32_MODE */

#include "matrix.h"     /* Prototypes */
#include "matrix1.h"    /* Font data */
#include "matrix2.h"    /* Image data */
#ifdef NIX_MODE
#include "config.h"  /* Autoconf stuff */
#endif /* NIX_MODE */

#define abs(a) (((a)>0)?(a):(-(a)))

#ifdef DEBUG
#define DEBUGMESS(s) fprintf(stderr, s);
#else

#define DEBUGMESS(s)
#endif

/* Global Variables */
unsigned char flare[16]={0,0,0,0,0,96}; /* Node flare texture */
#define rtext_x 90
#define _rtext_x rtext_x/2
#define text_y 70
#define _text_y text_y/2
#define _text_x text_x/2

struct black_column {
    long pindex;
    int col_len;
    int speed;
    int lead;
};
#define ColSize sizeof(struct black_column)

struct black_column *colms;

int density=30;
int text_x;
unsigned char *text;       /* Random Characters (0-NUM_CHARS-1) */
unsigned char *text_light; /* Alpha 255-white, 254-none */
float *bump_pic;           /* Z-Offset (calculated from pic from matrix2.h) */

int pic_offset;            /* Which image to show */
long timer=-1;             /* Controls pic fade in/out */
int mode2=0;               /* Initial speed boost (inits the light table) */
int pic_mode=1;            /* 1-fade in; 2-fade out (controls pic_fade) */
int pic_fade=0;            /* Makes all chars lighter/darker */
int classic=0;             /* classic mode (no 3d) */
int paused=0;
int num_pics=11 -1;         /* # 3d images (0 indexed) */
GLenum color=GL_RED;     /* Color of text */


#ifdef NIX_MODE
Display                 *dpy;
GLint                   att[] = {GLX_RGBA, GLX_DOUBLEBUFFER, None};
XVisualInfo             *vi;
XWindowAttributes       gwa;
XSetWindowAttributes    swa;
Window                  win;
GLXContext              glc;
XEvent                  xev;
int x,y;
#endif

/* FPS Stats */
time_t last;               /* Tims since last stat was printed */
int showfps=0;
unsigned long sleeper = 30000;
float fps=0;
int frames=0;              /* # frames shown since [last] */
int fpoll=2;               /* Print stats every [fpoll] seconds */
float maxfps=60.0;


#ifdef WIN32_MODE
/* 
 * int main(int argc, char **argv)
 */
int __stdcall WinMain(HINSTANCE hInst,HINSTANCE hPrev,LPSTR lpCmd,int nShow)
{
   int argc=1;
   int i=0,a=0,s=0;
   char win[100];
   char *cfile = malloc(strlen(win)+25);
   FILE *config;
   GetWindowsDirectory(win, 100);
   if(lpCmd[1]=='p') exit(EXIT_SUCCESS);
   /* Run the config dialog, assuming the user has 
    * copied it to %WINDIR% */
   if(lpCmd[1]=='c') {
      sprintf(cfile, "%s\\matrixgl_config.exe", win);
      _execl(cfile);
      exit(EXIT_SUCCESS);
   }

   glutInit(&argc, &lpCmd);
   srand(GetTickCount());
   pic_offset=(rtext_x*text_y)*(rand()%num_pics); /* Start at rand pic */

   /* Read in config */
   sprintf(cfile, "%s\\matrixgl_config", win);
   fprintf(stderr, "cfile=%s\n", cfile);
   config = fopen(cfile, "r");
   if (config) {
      char vals[5];
      fgets(vals, 5, config);
      /* Static mode */
      if (vals[0]!='0') {
            pic_fade=0;
            pic_offset=0;
            pic_mode=!pic_mode;
            classic=1;
      }
      /* Show credits on startup */
      if (vals[1]!='0') {
            if (!classic) cbKeyPressed('c', 0, 0); /* Start at credits */
      }
      /* Set color */
      if (vals[2]=='1') color=GL_GREEN;
      if (vals[2]=='2') color=GL_RED;
      if (vals[2]=='3') color=GL_BLUE;

   } else {
      fprintf(stderr, "Config: Not found\n");
   }
   free(cfile);
#else /* NIX_MODE */
int main(int argc,char **argv) 
{
   int i=0,a=0;
   int opt,spot;
   short allowroot=0; /* If they insist... */
   enum {PREVIEW, ROOT, FS, WINDOWED} wuse=WINDOWED; /* Windown type */
   Window wid=0;      /* ID of window, used to grab preview size */

   static struct option long_opts[] =
   {
      {"static",    no_argument,       0, 's'},
      {"credits",   no_argument,       0, 'c'},
      {"color",     required_argument, 0, 'C'},
      {"help",      no_argument,       0, 'h'},
      {"version",   no_argument,       0, 'v'},
      {"window-id", required_argument, 0, 'W'},
      {"root",      no_argument,       0, 'R'},
      {"fs",        no_argument,       0, 'F'},
      {"fullscreen",no_argument,       0, 'F'},
      {"allow-root",no_argument,       0, 'Z'},
      {"fps",       optional_argument, 0, 'f'},
      {"limit",     required_argument, 0, 'l'},
      {"density",   required_argument, 0, 'd'},
      {0, 0, 0, 0}
   };
   int opti = 0;
   pic_offset=(rtext_x*text_y)*(rand()%num_pics); /* Start at rand pic */
   while ((opt = getopt_long_only(argc, argv, "schvl:f::C:FRW:Z", long_opts, &opti))) {
      if (opt == EOF) break;
      switch (opt) {
         case 'Z':
            if (!opti) break; /* Short opt not allowed */
            allowroot=1;
            break;
         case 'W':
            wuse=PREVIEW;
            wid=htoi(optarg);
            break;
         case 'R':
            wuse=ROOT;
         case 'F':
            wuse=FS;
            break;
         case 's':
            pic_fade=0;
            pic_offset=0;
            pic_mode=!pic_mode;
            classic=1;
            break;
         case 'c':
            if (classic) break; /* Can't be used with 's' */
            cbKeyPressed('c', 0, 0); /* Start at credits */
            break;
         case 'C':
            if (!strcmp("green", (char *)optarg))
               color=GL_GREEN;
            else if (!strcmp("red", (char *)optarg))
               color=GL_RED;
            else if (!strcmp("blue", (char *)optarg))
               color=GL_BLUE;
            else {
               fprintf(stderr, "Error: Color must be green, red or blue\n");
               color=GL_GREEN;
            }
            break;
         case 'f':
            showfps=1;
            if (optarg) fpoll=clamp(atoi(optarg), 1, 20);
            else if (optind>0 && optind<argc && atoi(argv[optind])) 
               fpoll=clamp(atoi(argv[optind]), 1, 20);
            break;
         case 'l':
            maxfps=clamp(atoi(optarg), 1, 60);
            break;
         case 'd':
            density=clamp(atoi(optarg), 1, 200);
            break;
         case 'h':
            fputs("Usage: matrixgl [OPTIONS]...\n\
3D Matix Screensaver based on The Matrix Reloaded\n\
 -C --color=COL         Set color to COL (must be green, red or blue)\n\
 -c --credits           Show the credits on startup\n\
 -d --density=DENS      Set density of 'rain' (default: 30)\n\
 -F --fs --fullscreen   Run in fullscreen window\n\
 -f --fps[=SEC]         Print fps stats every SEC seconds (default: 2)\n\
 -h --help              Show the help screen\n\
 -l --limit=LIM         Limit framerate to LIM fps (default: 32)\n",
               stdout);
            fputs("\
 -x --rate=RATE         Set speedrate of falling 'rain' (default: 100)\n\
 -s --static            Run in static mode (no 3D images)\n\
 -v --version           Print version info\n\
 --allow-root           Allow to be run as root\n",
               stdout);
            fputs("Long options may be passed with a single dash.\n\n\
In-Screensaver Keys:\n\
 'c' - View creidts\n\
 's' - Toggle static mode (3D)\n\
 'n' - Next image\n\
 'p' - Pause screensaver\n\
 'q' - Quit\n\
Note: Keys won't work in xscreensaver (see manpage)\n\n\
Report bugs to <" PACKAGE_BUGREPORT ">\n\
Home Page: http://www.sourceforge.net/projects/matrixgl/\n",
             stdout);
            exit(EXIT_SUCCESS);
         case 'v':
            fputs("matrixgl version " VERSION "\n\
Based on matrixgl 1.0 (see http://knoppix.ru/matrixgl.shtml) \n\
Written By:  Alexander Zolotov  <nightradio@gmail.com> 2003.\n\
      and :  Eugene Zolotov     <sentinel@knoppix.ru> 2003.\n\
Modified By: Vincent Launchbury <vincent@doublecreations.com> 2008-2010.\n\n",
               stdout);
            fputs("Send bug reports to <" PACKAGE_BUGREPORT ">\n\
To assist us best, please run the script ./gen-bug-report.sh in the \
source directory and follow the instructions, before sending your \
bug report.\n",
               stdout);
            exit(EXIT_SUCCESS);
      }
   }
   /* Don't run as root, unless specifically requested */
   if (!allowroot && getuid()==0) {
      fprintf(stderr, "Error: You probably don't want to run this as "
         "root.\n (Use --allow-root if you really want to..)\n");
      exit(EXIT_FAILURE);
   }
#endif /* NIX_MODE */

#ifdef WIN32_MODE
   glutInit(&argc, &lpCmd);
#else /* NIX_MODE */
   srand(time(NULL));
#endif


#ifdef NIX_MODE
   /* Set up X Window stuff */
   dpy = XOpenDisplay(NULL);
   if(dpy == NULL) {
      fprintf(stderr, "Can't connect to X server\n");
      exit(EXIT_FAILURE); 
   }
   swa.event_mask = KeyPressMask;
   /* Bypass window manager for fullscreen */
   swa.override_redirect = (wuse==FS)?1:0;

   /* Use screen dimensions */
   y = DisplayHeight(dpy, DefaultScreen(dpy));
   x = DisplayWidth(dpy, DefaultScreen(dpy));

   /* Take height of preview win in xscreensaver */
   if (wuse==PREVIEW) {
      XGetWindowAttributes(dpy, wid, &gwa);
      x = gwa.width;
      y = gwa.height;
   /* Or scale down for windowed mode */
   } else if (wuse==WINDOWED) {
      x/=2;
      y/=2;
   }

   /* Create window, and map it */
   win = XCreateWindow(dpy, DefaultRootWindow(dpy),
       0, 0, x, y, 0, 0, InputOutput, 
       CopyFromParent, CWEventMask | CWOverrideRedirect, &swa);
   XMapRaised(dpy, win);

   /* Handle fullscreen.. */
   if (wuse==FS) {
      Pixmap blank;
      Cursor cursor;
      XColor c;

      /* Since we bypassed the window manager,
         we have to steal keyboard control */
      XGrabKeyboard(dpy, win, 1, GrabModeSync, GrabModeAsync, CurrentTime);

      /* X has no function to hide the cursor, 
       * so we have to create a blank one */
      blank = XCreatePixmapFromBitmapData(dpy, win,"\000", 1, 1, 0, 0, 1);
      cursor = XCreatePixmapCursor(dpy, blank, blank, &c, &c, 0, 0);
      XFreePixmap (dpy, blank);
      XDefineCursor(dpy, win, cursor);
   }
   XStoreName(dpy, win, "HellGL " VERSION);

   /* Set up glX */
   vi = glXChooseVisual(dpy, 0, att);
   glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
   glXMakeCurrent(dpy, win, glc);
#endif /* NIX_MODE */

/* Allocations for dynamic width */
#ifdef WIN32_MODE
   text_x = ceil(70 * ((float)glutGet(GLUT_SCREEN_WIDTH) 
      / glutGet(GLUT_SCREEN_HEIGHT)));
#else /* NIX_MODE */
   text_x = ceil(70 * ((float)x/y));
#endif /* NIX_MODE */

   /* Initializations */
   if (text_x % 2 == 1) text_x++;
   if (text_x < 90) text_x=90; /* Sanity check */

DEBUGMESS("Got to one.\n")

   colms = tmalloc(density*ColSize);

/*   text= tmalloc(rtext_x*text_y);
   text_light = tmalloc(rtext_x*text_y);
   bump_pic = tmalloc(sizeof(float) * rtext_x*text_y);*/

   text= tmalloc(text_x*(text_y+1));
   text_light = tmalloc(text_x*(text_y+1));
   bump_pic = tmalloc(sizeof(float) * (text_x*(text_y+1)));



   /* Init the light tables */
   for(i=0; i<text_x*(text_y+1); i++) {
      text_light[i]=0;
      text[i]=rand()%NUM_CHARS;
   }

   for(i=0; i<density; i++) {
      colms[i].pindex=rand()%(rtext_x*text_y);
      colms[i].col_len=10+rand()%40;
      colms[i].speed=rand()%3+1;

      spot=colms[i].col_len;
      for (a=colms[i].pindex; a%text_x>0 && spot>0; a-=text_x){
         text_light[a]=253;
	 spot--;
      }
    }
DEBUGMESS("Got through init function\n")

   mode2=1;

#ifdef NIX_MODE
   ourInit();
DEBUGMESS("OurInit done.\n")
   cbResizeScene(x,y);
DEBUGMESS("The error is in the main loop.\n")
   while(1) {
      /* FPS and frame limiting */
      if (!frames) time(&last);
      else if (time(NULL)-last >= fpoll) {
         fps = frames/(time(NULL)-last);
         if (showfps) {
            printf("FPS:%5.1f (%ld) (%3d frames in %2d seconds)\n", 
               fps, sleeper, frames, (int)(time(NULL)-last));
            fflush(stdout); /* Don't buffer fps stats (do not remove!) */
         }
         frames=-1;
 /* Limit framerate */
         if (fps > maxfps) {
            /* Framerate too fast. Calculate how many microseconds per-frame
             * too fast it's going, and add that to the per-frame sleeper */
            sleeper += 1000000 / maxfps - 1000000 / fps;
            /* sleeper = clamp(sleeper, 0, 1000000);*/
         } else if (fps != maxfps) {
            /* Framerate too slow. Calculate how many microseconds per-frame
             * too slow it's going, and minus that from the per-frame sleeper */
            sleeper -= 1000000 / fps - 1000000 / maxfps;
         }
         /* Limit framerate older code
         if (fps > maxfps) {
            sleeper+=50000*fps/maxfps;
         } else if (sleeper >= 50000) {
            sleeper-=50000*(1.0-fps/maxfps);
         }
         if (sleeper > 100000000) sleeper=100000000; */
      }
DEBUGMESS("Clear of frame limiter code!\n")

      /* Check events */
      if(XCheckWindowEvent(dpy, win, KeyPressMask, &xev)) {
         cbKeyPressed(get_ascii_keycode(&xev),0,0);
      }

      /* Update viewport on window size change */
      XGetWindowAttributes(dpy, win, &gwa);
      glViewport(0, 0, gwa.width, gwa.height);

      /* Render frame */
DEBUGMESS("About to render frame...\n")
      cbRenderScene();
DEBUGMESS("cbRenderScene fine.\n")
      usleep(sleeper);
      glFinish();
DEBUGMESS("Entering scroll\n")
      scroll(0);
DEBUGMESS("Done with scroll\n")
      frames++; /* Finished drawing a frame */
   } 
#else /* WIN32_MODE */
   glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
   glutEnterGameMode();
   /* Register the callback functions */
   glutDisplayFunc(&cbRenderScene);
   glutIdleFunc(&cbRenderScene);
   glutReshapeFunc(&cbResizeScene);
   glutKeyboardFunc(&cbKeyPressed);
   glutPassiveMotionFunc(MouseFunc);   
   ourInit();
   glutMainLoop(); /* Pass off control to OpenGL. */
#endif /* WIN32_MODE */
   return 0;
}


/* Draw character #num on the screen. */
static void draw_char(int mode, long num, float light, float x, float y, float z)
{
   float tx,ty;
/*   long num2;  */
   if (mode==1) light/=255;

/*   num2=num/10;  */
   ty=(float)num/70;
   tx=(float)num/10;

   glColor4f(1.0,0.0,0.0,light);

   glTexCoord2f(tx, ty); glVertex3f( x, y, z); 
   glTexCoord2f(tx+0.1, ty); glVertex3f( x+1, y, z);
   glTexCoord2f(tx+0.1, ty+0.166); glVertex3f( x+1, y-1, z);
   glTexCoord2f(tx, ty+0.166); glVertex3f( x, y-1, z);
}


/* Draw flare around white characters */
static void draw_flare(float x,float y,float z)
{
/*   glColor4f(0.769,0.231,0.0,.35); */
   glColor4f(1.0,0.0,0.0,.75);

   glTexCoord2f(0, 0); glVertex3f( x-1, y+1, z); 
   glTexCoord2f(0.75, 0); glVertex3f( x+2, y+1, z);
   glTexCoord2f(0.75, 0.75); glVertex3f( x+2, y-2, z);
   glTexCoord2f(0, 0.75); glVertex3f( x-1, y-2, z);
}

/* Draw colored text on screen */
static void draw_text1(void)
{
   float x,y;
   long p=0,b=0;
   int c,c_pic;
   if(!paused && pic_mode==1 && ++pic_fade>253)
      pic_fade=253;
   if(!paused && pic_mode==2 && --pic_fade<0)
      pic_fade=0;
   for(y=_text_y;y>-_text_y;y--){
      for(x=-_text_x;x<_text_x;x++){
         if (x>-_rtext_x-1 && x<_rtext_x) {
            /* Main text */
            c=text_light[p]-(text[p]>>1);
            c+=pic_fade;if(c>253) c=253;
            /* 3D Image offsets */
            c_pic=pic[b+pic_offset]-(253-pic_fade);
            if(c_pic<0) {c_pic=0;} c-=c_pic;if(c<0) {c=0;}

            if(!paused)bump_pic[p]=(float)(255-c_pic)/32;

            /*if (p<5)
               fprintf(stderr, "c = %i\n",c);*/

            if(c>10 && text[p]) draw_char(1, text[p]+1,c,x,y, bump_pic[p]);
            b++;
         }
         else {
            c=text_light[p]-(text[p]>>1);c+=pic_fade;if(c>253) c=253;
            if (c>10 && text[p]) draw_char(1, text[p]+1, c,x,y,8);
         }
         p++;
      }
   }
}

/* Draw white characters and flares for each column */
static void draw_text2(int mode)
{
   float x,y;
   long p=0;

   for(y=_text_y;y>=-_text_y;y-=1.0){
      for(x=-_text_x;x<_text_x;x+=1.0){
         if(text_light[p]==253 && text_light[p+text_x]<10 
               && x>-_text_x+3) {
            /* White character */
            if (!mode) {
              draw_char(2, text[p]%NUM_CHARS,0.5,x,y,
                  ((x>-text_x-1 && x<text_x )?8-bump_pic[p]:8));
            /* Flare */
            } else {
               draw_flare(x,y,
                  ((x>-text_x-1 && x<text_x )?8-bump_pic[p]:8));
            }
         }
      p++;
      }
   }
}

static void scroll(int mode)
{
   static char odd=0;
   int a, s=0, ycor, line,spot,sofar,i,j;
   if (paused)return;

   for (i=0; i<density; i++) {
      spot=colms[i].pindex;
      sofar=colms[i].speed;
      text_light[spot]=0;
      while (sofar) {
         if (spot>0)
            text_light[spot]=0;
         sofar--;
         spot-=text_x;
      }

      if (colms[i].pindex%text_x<=colms[i].speed) {
         colms[i].pindex=rand()%(text_y*text_x); /*+.5*text_y*text_x;*/
	 colms[i].speed=rand()%3+1;
         colms[i].col_len=rand()%40+10;
      }
      else {
         colms[i].pindex-=colms[i].speed*text_x;
      }

   }

   for (i=0; i<density; i++) {
      spot=colms[i].pindex-text_x;
      sofar=colms[i].col_len;
      while (spot>=text_x*text_y && sofar) {
         spot-=text_x;
         sofar--;
      }
      while (sofar>0 && spot>text_x) {
         text_light[spot]=253;
         spot-=text_x;
         sofar--;
      }
   }

   for (i=0; i<density*10; i++)
      text[rand()%(text_x*text_y)]=rand()%NUM_CHARS;

   if (odd) {
      if(timer==0 && !classic)  pic_mode=1;  /* pic fade in */
      if(timer>10) {mode2=0;mode=0;} /* Initialization */
#ifdef NIX_MODE
      if(timer>140 && timer<145 && !classic) pic_mode=2; /* pic fade out */
      if (timer > 140 && (pic_offset==(num_pics+1)*(rtext_x*text_y) || \
	  pic_offset==(num_pics+2)*(rtext_x*text_y))) {
#else /* WIN32_MODE */
	  if(timer>75 && timer<80 && !classic) pic_mode=2; /* pic fade out */
          if (timer > 75 && (pic_offset==(num_pics+1)*(rtext_x*text_y) || \
	      pic_offset==(num_pics+2)*(rtext_x*text_y)) ) {
#endif
            pic_offset+=rtext_x*text_y; /* Go from 'knoppix.ru' -> 'DC' */
            timer=-1;pic_mode=1; /* Make DC dissapear quickly , changed */
         }
#ifdef NIX_MODE
      if(timer>210) {
#else /* WIN32_MODE */
	  if(timer>100) {
#endif
         timer=-1;  /* back to start */
         pic_offset+=rtext_x*text_y; /* Next pic */
         if(pic_offset>(rtext_x*text_y*(num_pics))) pic_offset=0; 
      }
      timer++;
   }
   odd =!odd;
#ifdef WIN32_MODE
   if(!mode) glutTimerFunc(60,scroll,mode2);
#else
   mode=mode;
#endif
}

nix_static void cbRenderScene(void)
{
   glBindTexture(GL_TEXTURE_2D,1);
   glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, 
      GL_NEAREST_MIPMAP_LINEAR);
   glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
   glMatrixMode(GL_MODELVIEW);
   glTranslatef(0.0f,0.0f,-89.0F);
   glClear(GL_COLOR_BUFFER_BIT);

   glBegin(GL_QUADS); 
DEBUGMESS("Entering draw_text1\n")
   draw_text1();
DEBUGMESS("Exiting draw_text1, about to execute glEnd()\n")
   glEnd();

DEBUGMESS("Got through glEnd, Was it bind texture?\n")
   glBindTexture(GL_TEXTURE_2D,2);
DEBUGMESS("It wasn't bind texture.\n")
   glBegin(GL_QUADS);
DEBUGMESS("Entering draw_text2\n")
   draw_text2(0);
DEBUGMESS("Exiting draw_text2\n")
   glEnd();

   glBindTexture(GL_TEXTURE_2D,3);
   glBegin(GL_QUADS);
DEBUGMESS("Entering draw_text2\n")
   draw_text2(1);
DEBUGMESS("Exiting draw_text2\n")
   glEnd();

   glLoadIdentity();
   glMatrixMode(GL_PROJECTION);

#ifdef WIN32_MODE
   glutSwapBuffers();
#else /* NIX_MODE */
   glXSwapBuffers(dpy, win); 
#endif /* NIX_MODE */
} 


#ifdef WIN32_MODE
void MouseFunc(int x, int y)
{
	/* A work around for really buggy
	mouse drivers that:
		a) Randomly generate a mouse event when the
		 coords haven't changed and
	   b) Even worse, generate a mouse event with
		 different coords, when the mouse hasn't moved.
	*/
   static short xx=0,yy=0, t=0;
   if (!t) {t++;xx=x;yy=y;}
   else if (abs(xx-x)>8||abs(yy-y)>8)exit(EXIT_SUCCESS);
}
#endif


nix_static void cbKeyPressed(unsigned char key, unused int x, unused int y)
{
   switch (key) {
      case 'X':
         break; /* Do nothing */
      case 113: case 81: case 27: /* q,ESC */
#ifdef NIX_MODE
         glXMakeCurrent(dpy, None, NULL);
         glXDestroyContext(dpy, glc);
         XDestroyWindow(dpy, win);
         XCloseDisplay(dpy);
         XFree(vi);
#endif /* NIX_MODE */
         free(colms);
         free(text);
         free(text_light);
         free(bump_pic);
         exit(EXIT_SUCCESS); 
      case 'n': /* n - Next picture. */
         if (classic) break;
         pic_offset+=rtext_x*text_y;
         pic_mode=1;
         timer=10;
         if(pic_offset>(rtext_x*text_y*(num_pics))) pic_offset=0; 
         break;
      case 'c': /* Show "Credits" */
         classic=0;
         pic_offset=(num_pics+1)*(rtext_x*text_y);
         pic_mode=1;
         timer=-1;
         break;
      case 's': /* Stop 3D Images, classic style. */
         if (!classic) {
            pic_fade=0;
            pic_offset=0;
         } 
         pic_mode=!pic_mode;
         classic=!classic;
         break;
      case 'p': /* Pause */
         paused=!paused;
#ifdef WIN32_MODE
         if (!paused) glutTimerFunc(60,scroll,mode2);
#endif /* WIN32_MODE */
         break;
   }
}




nix_static void cbResizeScene(int Width, int Height)
{
   glViewport(0, 0, Width, Height);
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluPerspective(45.0f,(GLfloat)Width/(GLfloat)Height,0.1f,200.0f);
   glMatrixMode(GL_MODELVIEW);
}


static void ourInit(void) 
{  /* Make Textures */
   glPixelTransferf(GL_RED_SCALE, 1.15f); /* Give red a bit of a boost */

   glBindTexture(GL_TEXTURE_2D,1);
   gluBuild2DMipmaps(GL_TEXTURE_2D,GL_RGBA8, 512, 256, color,
      GL_UNSIGNED_BYTE, (void *)font);

   glBindTexture(GL_TEXTURE_2D,2);
   gluBuild2DMipmaps(GL_TEXTURE_2D,GL_RGBA8, 512, 256, GL_LUMINANCE,
      GL_UNSIGNED_BYTE, (void *)font);

   glBindTexture(GL_TEXTURE_2D,3);
   gluBuild2DMipmaps(GL_TEXTURE_2D,GL_RGBA8, 4, 4, GL_LUMINANCE, 
      GL_UNSIGNED_BYTE, (void *)flare);

   /* Some pretty standard settings for wrapping and filtering. */
   glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
   glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
   glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_DECAL);
   glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
   glShadeModel(GL_SMOOTH);

   /* A handy trick -- have surface material mirror the color. */
   glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
   glEnable(GL_COLOR_MATERIAL);

   glEnable(GL_BLEND);
   glEnable(GL_TEXTURE_2D);
   glDisable(GL_LIGHTING);
   glBlendFunc(GL_SRC_ALPHA,GL_ONE); 
}

/* malloc w/ error checking  */
static void *tmalloc(size_t n)
{
   void *p = malloc(n);
   if (!p && n != 0)
      exit(EXIT_FAILURE);
   return p;
}

#ifdef NIX_MODE
static int htoi(char *str)
{
  int i,sum=0,d,sl;
  sl=strlen(str);

  if(str[0]!='0' || str[1]!='x') return -1;
  for(i=2;i<sl;i++){
    d=0;
    if(str[i]>='0' && str[i]<='9') d=(int)(str[i]-'0');
    if(str[i]>='A' && str[i]<='F') d=(int)(str[i]-'A'+10);
    if(str[i]>='a' && str[i]<='f') d=(int)(str[i]-'a'+10);
    sum+=d;
    sum=sum<<4;
  }

  return(sum>>4);
}

static char get_ascii_keycode(XEvent *ev)
{
   char keys[256], *s;
   int count;
   KeySym k;

   if (ev) {
      count = XLookupString((XKeyEvent *)ev, keys, 256, &k,NULL);
      keys[count] = '\0';
      if (count == 0) {
         s = XKeysymToString(k);
         strcpy(keys, (s)?s:"");
      }
      if (count==1) return *keys;
   }
   return 'X';
}
#endif

