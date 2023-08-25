//modified by: Carolina Martinez
//date: 3/17/22
//
//author: Gordon Griesel
//date: Spring 2022
//purpose: get openGL working on your personal computer
//
#include <iostream>
#include <fstream>
using namespace std;
#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cmath>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "fonts.h"
#include <GL/glx.h>
#include <pthread.h>

//MVC architecture
// M - model
// V - view (see our models)
// C - controller 

class Image {
public:
    int width, height, max;
    char *data;
    Image() { }
    Image(const char *fname) {
		bool isPPM = true;
		char str[1200];
		char newfile[200];
		ifstream fin;
		char *p = strstr((char *)fname, ".ppm");
		if(!p){
			isPPM = false;
			strcpy(newfile, fname);
			newfile[strlen(newfile)-4] = '\0';
			strcat(newfile, ".ppm");
			sprintf(str, "convert %s %s", fname, newfile);
			system(str);
			fin.open(newfile);
		} else{
			fin.open(fname);
		}
        //ifstream fin(fname);
        char p6[10];
        fin >> p6;
        fin >> width >> height;
        fin >> max;
        data = new char [width * height * 3];
		fin.read(data, 1);
        fin.read(data, width * height * 3);
        fin.close();
		if(!isPPM)
			unlink(newfile);
    }
} sprite("/home/stu/cmartinez/4490/proj/cat.png"),
  fbg("/home/stu/cmartinez/4490/proj/sky2.png"),
  tree("/home/stu/cmartinez/4490/proj/short1.png"),
  ground("/home/stu/cmartinez/4490/proj/ground.png"),
  intro("/home/stu/cmartinez/4490/proj/intro.png"),
  instruc("/home/stu/cmartinez/4490/proj/instruc.png");

  Image cats[10] = {"cat.png","cat2.png", "cat3.png", "cat4.png", "cat5.png"};
  int ncats = 3;

struct Vector {
    float x,y,z;
};

typedef double Flt;
//a game object
class Cat {
public:
	Flt pos[3]; //vector
	Flt vel[3]; //vector
	//Flt accel[0];
	float w, h;
	unsigned int color;
	bool alive_or_dead;
	Flt mass;
	void set_dimensions(int x, int y) {
		w = (float)x * 0.05;
		h = w;
		}
	Cat(){
		w = h = 4.0;
		pos[0] = 1.0;
		pos[1] = 200.0;
		vel[0] = 4.0;
		vel[1] = 0.0; 
	}
	Cat(int x, int y){
		w = h = 4.0;
		pos[0] = x;
		pos[1] = y;
		vel[0] = 0.0;
		vel[1] = 0.0; 
	}
}player;

enum {
	STATE_TOP,
	STATE_INTRO,
	STATE_INSTRUCTIONS,
	STATE_SHOW_OBJECTIVES,
	STATE_PLAY,
	STATE_GAME_OVER,
	STATE_BOTTOM
};


class Global {
public:
	int xres, yres;
	Cat cats[2];
    //box components
    float pos[2];
	char keys[65536];
    float w;
    float dir;
    int inside;
	int score;
	int state;
	unsigned int texid;
	unsigned int introid; //start screen
	unsigned int instid; //instruc screen
	unsigned int spriteid; //cat
	unsigned int bgid; //bg id
	unsigned int gnid; //ground id
	unsigned int treeid; //tree id
	int frameno;
	int jump;
	int hit;
	Flt gravity;
    Global(){
		memset(keys, 0, 65536);
		xres = 800;
		yres = 600;
		//box
		w = 20.0f;
		pos[0] = 0.0f+w; //w defines the position of the box
		pos[1] = yres/2.0f;
		dir = 25.0f;
		inside = 0;
		score = 0;
		gravity = 240.0;
		frameno = 1;
		state = STATE_INTRO;
	}
} g;

class X11_wrapper {
private:
	Display *dpy;
	Window win;
	GLXContext glc;
public:
	~X11_wrapper();
	X11_wrapper();
	void set_title();
	bool getXPending();
	XEvent getXNextEvent();
	void swapBuffers();
	void reshape_window(int width, int height);
	void check_resize(XEvent *e);
	void check_mouse(XEvent *e);
	int check_keys(XEvent *e);
} x11;

//Function prototypes
void init_opengl(void);
void physics(void);
void render(void);
void *spriteThread(void *arg){
	//------------------------------------------------------------------
	//setup timers
	//const double OOBILLION = 1.0 / 1e9;
	struct timespec start, end;
	extern double timeDiff(struct timespec *start, struct timespec *end);
	extern void timeCopy(struct timespec *dest, struct timespec *source);
	//clock_gettime(CLOCK_REALTIME, &smokeStart);
	//-------------------------------------------------------------------
	clock_gettime(CLOCK_REALTIME, &start);
	double diff;
	while(1){
		//if some amount of time has passed, change the frame number.
		clock_gettime(CLOCK_REALTIME, &end);
		diff = timeDiff(&start, &end);
		if(diff >= 0.05){
			//enough time has passed
			++g.frameno;
			if(g.frameno > 20)
				g.frameno = 1;
			timeCopy(&start, &end);
		}
	}
	return (void *)0;
}

int main()
{
	//start thread
	pthread_t th;
	pthread_create(&th, NULL, spriteThread, NULL);

	srand(time(NULL));
	init_opengl();
    initialize_fonts();
	//main game loop
	int done = 0;
	while (!done) {
		//process events...
		while (x11.getXPending()) {
			XEvent e = x11.getXNextEvent();
			x11.check_resize(&e);
			x11.check_mouse(&e);
			done = x11.check_keys(&e);
		}
		physics();           //move things
		render();            //draw things
		x11.swapBuffers();   //make video memory visible
		usleep(200);         //pause to let X11 work better
	}
    cleanup_fonts();
	return 0;
}

X11_wrapper::~X11_wrapper()
{
	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);
}

X11_wrapper::X11_wrapper()
{
	GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
	int w = g.xres, h = g.yres;
	dpy = XOpenDisplay(NULL);
	if (dpy == NULL) {
		cout << "\n\tcannot connect to X server\n" << endl;
		exit(EXIT_FAILURE);
	}
	Window root = DefaultRootWindow(dpy);
	XVisualInfo *vi = glXChooseVisual(dpy, 0, att);
	if (vi == NULL) {
		cout << "\n\tno appropriate visual found\n" << endl;
		exit(EXIT_FAILURE);
	} 
	Colormap cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
	XSetWindowAttributes swa;
	swa.colormap = cmap;
	swa.event_mask =
		ExposureMask | KeyPressMask | KeyReleaseMask |
		ButtonPress | ButtonReleaseMask |
		PointerMotionMask |
		StructureNotifyMask | SubstructureNotifyMask;
	win = XCreateWindow(dpy, root, 0, 0, w, h, 0, vi->depth,
		InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
	set_title();
	glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
	glXMakeCurrent(dpy, win, glc);
}

void X11_wrapper::set_title()
{
	//Set the window title bar.
	XMapWindow(dpy, win);
	XStoreName(dpy, win, "Project");
}

bool X11_wrapper::getXPending()
{
	//See if there are pending events.
	return XPending(dpy);
}

XEvent X11_wrapper::getXNextEvent()
{
	//Get a pending event.
	XEvent e;
	XNextEvent(dpy, &e);
	return e;
}

void X11_wrapper::swapBuffers()
{
	glXSwapBuffers(dpy, win);
}

void X11_wrapper::reshape_window(int width, int height)
{
	//window has been resized.
	g.xres = width;
	g.yres = height;
	//
	glViewport(0, 0, (GLint)width, (GLint)height);
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	glOrtho(0, g.xres, 0, g.yres, -1, 1);
	g.cats[0].set_dimensions(g.xres, g.yres);
}

void X11_wrapper::check_resize(XEvent *e)
{
	//The ConfigureNotify is sent by the
	//server if the window is resized.
	if (e->type != ConfigureNotify)
		return;
	XConfigureEvent xce = e->xconfigure;
	if (xce.width != g.xres || xce.height != g.yres) {
		//Window size did change.
		reshape_window(xce.width, xce.height);
	}
}
//-----------------------------------------------------------------------------

void X11_wrapper::check_mouse(XEvent *e)
{
	static int savex = 0;
	static int savey = 0;

	//Weed out non-mouse events
	if (e->type != ButtonRelease &&
		e->type != ButtonPress &&
		e->type != MotionNotify) {
		//This is not a mouse event that we care about.
		return;
	}
	//
	if (e->type == ButtonRelease) {
		return;
	}
	if (e->type == ButtonPress) {
		if (e->xbutton.button==1) {
			//Left button was pressed.
			int y = g.yres - e->xbutton.y;
            int x = e->xbutton.x;
            if(x >= g.cats[0].pos[0]-g.w && x <= g.cats[0].pos[0]+g.w){
                if(y >= g.cats[0].pos[1]-g.w && y <= g.cats[0].pos[1]+g.w){
                    //g.inside++;
					g.score ++;
                    //printf("hits: %i\n", g.inside);
                }
            }
			else{
				
				g.state = STATE_GAME_OVER;

			}
            
            return;
		}
		if (e->xbutton.button==3) {
			//Right button was pressed.
			return;
		}
	}
	if (e->type == MotionNotify) {
		//The mouse moved!
		if (savex != e->xbutton.x || savey != e->xbutton.y) {
			savex = e->xbutton.x;
			savey = e->xbutton.y;
			//Code placed here will execute whenever the mouse moves.
		}
	}
}

int X11_wrapper::check_keys(XEvent *e)
{
	if (e->type != KeyPress && e->type != KeyRelease)
		return 0;
	int key = XLookupKeysym(&e->xkey, 0);
	if(e->type == KeyPress) g.keys[key] = 1;
	//if(e->type == KeyRelease) g.keys[key] = 0;
	if (e->type == KeyPress) {
		switch (key) {
			case XK_Right:
				if(g.state == STATE_INTRO){
					g.state = STATE_INSTRUCTIONS;
				}
				break;
			case XK_p:
				if(g.state == STATE_GAME_OVER){
					//glClear(GL_COLOR_BUFFER_BIT);
					g.score = 0;
					g.state = STATE_PLAY;
				}
				break;
			case XK_a:
				if(g.state == STATE_INSTRUCTIONS){
					g.state = STATE_PLAY;
				}
				break;
			case XK_Escape:
				//Escape key was pressed
				return 1;
		}
	}
	return 0;
}

void init_opengl(void)
{
	//OpenGL initialization
	glViewport(0, 0, g.xres, g.yres);
	//Initialize matrices
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	//Set 2D mode (no perspective)
	glOrtho(0, g.xres, 0, g.yres, -1, 1);
    //

    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	//Set the screen background color
	glClearColor(0.1, 0.1, 0.1, 1.0);

	// bg
	    glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &g.bgid);
	glBindTexture(GL_TEXTURE_2D, g.bgid);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, fbg.width, fbg.height, 0,
                        GL_RGB, GL_UNSIGNED_BYTE, fbg.data);

	//state start
		glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &g.introid);
	glBindTexture(GL_TEXTURE_2D, g.introid);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, intro.width, intro.height, 0,
                        GL_RGB, GL_UNSIGNED_BYTE, intro.data);

	//state instructions
			glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &g.instid);
	glBindTexture(GL_TEXTURE_2D, g.instid);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, instruc.width, instruc.height, 0,
                        GL_RGB, GL_UNSIGNED_BYTE, instruc.data);

	//ground
		glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &g.gnid);
	glBindTexture(GL_TEXTURE_2D, g.gnid);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, ground.width, ground.height, 0,
                        GL_RGB, GL_UNSIGNED_BYTE, ground.data);
	
	//trees
	unsigned char *data3 = new unsigned char [tree.width*tree.height*6];
	for(int i=0; i<tree.height; i++){
		for(int j=0; j<tree.width; j++){
			//stepping through data stream
			int offset = i*tree.width*6 + j*6;
			int offset2 = i*tree.width*6 + j*6;
			data3[offset2+0] = tree.data[offset+0];
			data3[offset2+1] = tree.data[offset+1];
			data3[offset2+2] = tree.data[offset+2];
			data3[offset2+3] = tree.data[offset+3];
			data3[offset2+4] = tree.data[offset+4];
			data3[offset2+5] = ((unsigned char)tree.data[offset+0] != 255 && 
								(unsigned char)tree.data[offset+1] != 255 &&
								(unsigned char)tree.data[offset+2] != 255 && 
								(unsigned char)tree.data[offset+3] != 255 &&
								(unsigned char)tree.data[offset+4] != 255 &&  
								(unsigned char)tree.data[offset+5] != 255);
		}
	}
	//trees
		glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &g.treeid);
	glBindTexture(GL_TEXTURE_2D, g.treeid);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, tree.width, tree.height, 0,
                        GL_RGB, GL_UNSIGNED_BYTE, data3);
	delete [] data3;

	//bee sprite
	//make a new data stream, with 4 color components
	//add an alpha channel
	unsigned char *data2 = new unsigned char [sprite.width*sprite.height*4];
	for(int i=0; i<sprite.height; i++){
		for(int j=0; j<sprite.width; j++){
			//stepping through data stream
			int offset = i*sprite.width*3 + j*3;
			int offset2 = i*sprite.width*4 + j*4;
			data2[offset2+0] = sprite.data[offset+0];
			data2[offset2+1] = sprite.data[offset+1];
			data2[offset2+2] = sprite.data[offset+2];
			data2[offset2+3] = ((unsigned char)sprite.data[offset+0] != 255 && 
								(unsigned char)sprite.data[offset+1] != 255 && 
								(unsigned char)sprite.data[offset+2] != 255);
		}
	}
	glGenTextures(1, &g.spriteid);
	glBindTexture(GL_TEXTURE_2D, g.spriteid);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, 4, sprite.width, sprite.height, 0,
								GL_RGBA, GL_UNSIGNED_BYTE, data2);
	delete [] data2;
	g.cats[0].set_dimensions(g.xres, g.yres);

}

#define GRAVITY 1.0

void physics()
{
	/*if(g.keys[XK_j] == 1)
	{
		player.vel[1] += 2;
	}
	player.vel[1] -= GRAVITY;
	player.pos[1] += player.vel[1];
	if (player.pos[1] < 0) {
		player.pos[1] = 0;
		player.vel[1] = 0.0;
	}*/
	//movement
	g.cats[0].pos[0] += g.cats[0].vel[0];
	g.cats[0].pos[1] += g.cats[0].vel[1];

    //boundary test
	if (g.cats[0].pos[0] >= g.xres) {
		g.cats[0].pos[0] = g.xres;
		g.cats[0].vel[0] = 0.0;
	}
	if (g.cats[0].pos[0] <= 0) {
		g.cats[0].pos[0] = 0.0;
		g.cats[0].vel[0] = 0.0;
	}
	if (g.cats[0].pos[1] >= g.yres) {
		g.cats[0].pos[1] = g.yres;
		g.cats[0].vel[1] = 0.0;
	}
	if (g.cats[0].pos[1] <= 0) {
		g.cats[0].pos[1] = 0.0;
		g.cats[0].vel[1] = 0.0;
	}
	//moving randomly
	Flt cx = g.xres/2.0;
	Flt cy = g.yres/2.0;
	cx = g.xres * (300.0/400.0);
	cy = g.yres * (90.0/170.0);
	Flt dx = cx - g.cats[0].pos[0];
	Flt dy = cy - g.cats[0].pos[1];
	Flt dist = (dx*dx + dy*dy);
	if(dist < 0.01)
		dist = 0.01; //clamp
	//change in velocity based on a force
	g.cats[0].vel[0] += (dx / dist) * g.gravity;
	g.cats[0].vel[1] += (dy / dist) * g.gravity;
	g.cats[0].vel[0] += ((Flt)rand() / (Flt)RAND_MAX) * 0.5 - 0.25; 
	g.cats[0].vel[1] += ((Flt)rand() / (Flt)RAND_MAX) * 0.5 - 0.25;
}


void drawCat(int i){
	//Draw bee.
	glPushMatrix();
	glColor3ub(255, 255, 0);
	//glTranslatef(player.pos[0] + 200, player.pos[1] + 200, 0.0f);
	glTranslatef(g.cats[0].pos[0] + ((Flt)rand() / (Flt)RAND_MAX), g.cats[0].pos[1] + ((Flt)rand() / (Flt)RAND_MAX) , 0.0f);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.0f);
	glColor4ub(255,255,255,255);
	//
	glBindTexture(GL_TEXTURE_2D, g.spriteid);
	//make texture coordinates based on frame #
	float tx1 = 0.0 + (float)((g.frameno-1) % 4) * 0.25f;
	float tx2 = tx1 + 0.25f;
	float ty1 = 0.0f + (float)((g.frameno-1) / 2 * 0.5f);
	float ty2 = ty1 + 0.5;
	if(g.cats[0].vel[0] > 0.0){
		float tmp = tx1;
		tx1 = tx2;
		tx2 = tmp;
	}
	glBegin(GL_QUADS);
	//cats
		/*glTexCoord2f(tx1, ty2); glVertex2f(-g.cats[0].w, -g.cats[0].h);
		glTexCoord2f(tx1, ty1); glVertex2f(-g.cats[0].w,  g.cats[0].h);
		glTexCoord2f(tx2, ty1); glVertex2f( g.cats[0].w,  g.cats[0].h);
		glTexCoord2f(tx2, ty2); glVertex2f( g.cats[0].w, -g.cats[0].h);*/
		glTexCoord2f(tx1, ty2); glVertex2f(-g.cats[i].w, -g.cats[i].h);
		glTexCoord2f(tx1, ty1); glVertex2f(-g.cats[i].w,  g.cats[i].h);
		glTexCoord2f(tx2, ty1); glVertex2f( g.cats[i].w,  g.cats[i].h);
		glTexCoord2f(tx2, ty2); glVertex2f( g.cats[i].w, -g.cats[i].h);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_ALPHA_TEST);

	//show sprite bound box
	if(g.keys[XK_b] == 1)
	{
		glColor3ub(255, 255, 0);
		glBegin(GL_LINE_LOOP);
			glVertex2f(-g.cats[i].w, -g.cats[i].h);
			glVertex2f(-g.cats[i].w,  g.cats[i].h);
			glVertex2f( g.cats[i].w,  g.cats[i].h);
			glVertex2f( g.cats[i].w, -g.cats[i].h);
		glEnd();
	}
	glPopMatrix();
}

void render()
{
	glClear(GL_COLOR_BUFFER_BIT);
	Rect r;
	if(g.state == STATE_INTRO){
		glColor3ub(255, 255, 255);
		glBindTexture(GL_TEXTURE_2D, g.introid);
		glBegin(GL_QUADS);
			glTexCoord2f(0,1); glVertex2i(0,      0);
			glTexCoord2f(0,0); glVertex2i(0,      g.yres);
			glTexCoord2f(1,0); glVertex2i(g.xres, g.yres);
			glTexCoord2f(1,1); glVertex2i(g.xres, 0);
		glEnd();
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	if(g.state == STATE_INSTRUCTIONS)
	{
		glColor3ub(255, 255, 255);
		glBindTexture(GL_TEXTURE_2D, g.instid);
		glBegin(GL_QUADS);
			glTexCoord2f(0,1); glVertex2i(0,      0);
			glTexCoord2f(0,0); glVertex2i(0,      g.yres);
			glTexCoord2f(1,0); glVertex2i(g.xres, g.yres);
			glTexCoord2f(1,1); glVertex2i(g.xres, 0);
		glEnd();
		glBindTexture(GL_TEXTURE_2D, 0);

	}

	if(g.state == STATE_GAME_OVER){
		//show intro
		r.bot = g.yres / 2;
		r.left = g.xres / 2;
		r.center = 1;
		ggprint8b(&r, 40, 0x00ffffff,"GAME OVER. You've dropped your food :(");
		ggprint8b(&r, 20, 0x00ffffff,"You have fed the cat %i times.", g.score);
		ggprint8b(&r, 0, 0x00ff0000,"Press P to try again.");
		return;
	}

	if(g.state == STATE_PLAY){

		static float xb = 0.0f;
		static float xr = 0.0f;

		//bg
		glColor3ub(255, 255, 255);
		glBindTexture(GL_TEXTURE_2D, g.bgid);
		glColor4ub(255,255,255,255);
		glBegin(GL_QUADS);
			glTexCoord2f(xb+0,1); glVertex2i(0,      0);
			glTexCoord2f(xb+0,0); glVertex2i(0,      g.yres);
			glTexCoord2f(xb+1,0); glVertex2i(g.xres, g.yres);
			glTexCoord2f(xb+1,1); glVertex2i(g.xres, 0);
		glEnd();
		xb += 0.009;
		glBindTexture(GL_TEXTURE_2D, 0);

		//trees
		xr += 0.01;
		//glColor3ub(255,255,0);
		/*glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.0f);
		glColor4ub(255,255,255,255);
		glBindTexture(GL_TEXTURE_2D, g.treeid);
		glBegin(GL_QUADS);
			glTexCoord2f(xr+0,1); glVertex2i(0,      0);
			glTexCoord2f(xr+0,0); glVertex2i(0,      g.yres);
			glTexCoord2f(xr+1,0); glVertex2i(g.xres, g.yres);
			glTexCoord2f(xr+1,1); glVertex2i(g.xres, 0);
		glEnd();
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_ALPHA_TEST);*/

		//draw the ground
		glBindTexture(GL_TEXTURE_2D, g.gnid);
		glBegin(GL_QUADS);
			glTexCoord2f(xr+0,1); glVertex2i(0,      0);
			glTexCoord2f(xr+0,0); glVertex2i(0,      40);
			glTexCoord2f(xr+1,0); glVertex2i(g.xres, 40);
			glTexCoord2f(xr+1,1); glVertex2i(g.xres, 0);
		glEnd();
		glBindTexture(GL_TEXTURE_2D, 0);
		xr += 0.0;

		//score
		Rect r;
		r.bot = g.yres -20 ;
		r.left = g.xres / 2;
		r.center = 1;
		ggprint8b(&r, 0, 0x00000000,"score: %i\n", g.score);

		//drawCat();
		for(int i=0; i<ncats; i++){
			drawCat(i);
		}
	}

}






