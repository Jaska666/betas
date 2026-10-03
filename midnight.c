/*
 * midnight - a bouncing-logo screensaver in the style of the classic DVD
 * player screensaver.
 *
 * The "MIDNIGHT" logo drifts diagonally across a black screen at a constant
 * speed, bounces off every edge, and switches to a new color each time it
 * hits one. Hitting a corner exactly is rare, just like the original.
 *
 * Works as an xscreensaver hack (honours $XSCREENSAVER_WINDOW, -root and
 * -window-id) or as a standalone window.
 *
 * Build: cc -O2 -o midnight midnight.c -lX11 -lm
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

/* 5x7 block glyphs for the letters in "MIDNIGHT". */
static const char *glyph(char c)
{
	switch (c) {
	case 'M': return "10001" "11011" "10101" "10101" "10001" "10001" "10001";
	case 'I': return "11111" "00100" "00100" "00100" "00100" "00100" "11111";
	case 'D': return "11110" "10001" "10001" "10001" "10001" "10001" "11110";
	case 'N': return "10001" "11001" "10101" "10011" "10001" "10001" "10001";
	case 'G': return "01111" "10000" "10000" "10111" "10001" "10001" "01111";
	case 'H': return "10001" "10001" "10001" "11111" "10001" "10001" "10001";
	case 'T': return "11111" "00100" "00100" "00100" "00100" "00100" "00100";
	}
	return NULL;
}

static const char *WORD = "MIDNIGHT";

struct state {
	Display *dpy;
	Window win;
	int standalone;
	int width, height;
	GC gc, mask_gc;
	Pixmap mask;     /* 1-bit logo shape */
	Pixmap buf;      /* logo + margin, used to draw without flicker */
	int lw, lh;      /* logo size */
	int margin;
	double x, y, dx, dy;
	double speed_opt;
	unsigned long color;
	double hue;
};

static volatile sig_atomic_t quit;
static void on_signal(int sig) { (void)sig; quit = 1; }

static unsigned long alloc_hue(struct state *s, double h)
{
	/* HSV -> RGB with full saturation and value. */
	double r, g, b, f = h * 6.0 - floor(h * 6.0);
	switch ((int)floor(h * 6.0) % 6) {
	case 0: r = 1; g = f; b = 0; break;
	case 1: r = 1 - f; g = 1; b = 0; break;
	case 2: r = 0; g = 1; b = f; break;
	case 3: r = 0; g = 1 - f; b = 1; break;
	case 4: r = f; g = 0; b = 1; break;
	default: r = 1; g = 0; b = 1 - f; break;
	}
	XColor xc;
	xc.red = (unsigned short)(r * 65535);
	xc.green = (unsigned short)(g * 65535);
	xc.blue = (unsigned short)(b * 65535);
	xc.flags = DoRed | DoGreen | DoBlue;
	Colormap cmap = DefaultColormap(s->dpy, DefaultScreen(s->dpy));
	XWindowAttributes wa;
	if (XGetWindowAttributes(s->dpy, s->win, &wa) && wa.colormap)
		cmap = wa.colormap;
	if (!XAllocColor(s->dpy, cmap, &xc))
		return WhitePixel(s->dpy, DefaultScreen(s->dpy));
	return xc.pixel;
}

/* Pick a new color that is clearly different from the current one. */
static void next_color(struct state *s)
{
	double h;
	do {
		h = (double)rand() / ((double)RAND_MAX + 1.0);
	} while (s->hue >= 0 &&
	         (fabs(h - s->hue) < 0.15 || fabs(h - s->hue) > 0.85));
	s->hue = h;
	s->color = alloc_hue(s, h);
}

static void build_logo(struct state *s)
{
	int n = (int)strlen(WORD);
	int cols = n * 5 + (n - 1);           /* one blank column between letters */
	/* Logo width is roughly a fifth of the screen, like the DVD logo on a TV. */
	int cell = s->width / (cols * 5);
	if (cell < 2) cell = 2;
	int slant = cell * 6 / 3;             /* italic lean of the top row */
	int text_w = cols * cell + slant;
	int text_h = 7 * cell;
	int gap = cell * 2;
	int disc_h = cell * 6;

	s->lw = text_w;
	s->lh = text_h + gap + disc_h;

	if (s->mask) XFreePixmap(s->dpy, s->mask);
	if (s->mask_gc) XFreeGC(s->dpy, s->mask_gc);
	s->mask = XCreatePixmap(s->dpy, s->win, s->lw, s->lh, 1);
	s->mask_gc = XCreateGC(s->dpy, s->mask, 0, NULL);
	XSetForeground(s->dpy, s->mask_gc, 0);
	XFillRectangle(s->dpy, s->mask, s->mask_gc, 0, 0, s->lw, s->lh);
	XSetForeground(s->dpy, s->mask_gc, 1);

	/* Slanted block letters. */
	for (int i = 0; i < n; i++) {
		const char *g = glyph(WORD[i]);
		for (int r = 0; r < 7; r++) {
			int shift = (6 - r) * cell / 3;
			for (int c = 0; c < 5; c++) {
				if (g[r * 5 + c] != '1') continue;
				XFillRectangle(s->dpy, s->mask, s->mask_gc,
				               (i * 6 + c) * cell + shift, r * cell,
				               cell, cell);
			}
		}
	}

	/* Flattened disc underneath with a hole in the middle. */
	int dy = text_h + gap;
	XFillArc(s->dpy, s->mask, s->mask_gc, 0, dy, s->lw - 1, disc_h - 1,
	         0, 360 * 64);
	XSetForeground(s->dpy, s->mask_gc, 0);
	int hw = s->lw / 6, hh = disc_h / 3;
	XFillArc(s->dpy, s->mask, s->mask_gc, (s->lw - hw) / 2,
	         dy + (disc_h - hh) / 2, hw, hh, 0, 360 * 64);

	s->margin = (int)ceil(fabs(s->dx) > fabs(s->dy) ? fabs(s->dx) : fabs(s->dy)) + 1;
	if (s->buf) XFreePixmap(s->dpy, s->buf);
	XWindowAttributes wa;
	XGetWindowAttributes(s->dpy, s->win, &wa);
	s->buf = XCreatePixmap(s->dpy, s->win, s->lw + 2 * s->margin,
	                       s->lh + 2 * s->margin, wa.depth);
}

static void reset(struct state *s)
{
	XWindowAttributes wa;
	XGetWindowAttributes(s->dpy, s->win, &wa);
	s->width = wa.width;
	s->height = wa.height;

	/* Constant diagonal velocity, scaled to the screen size. */
	double v = s->speed_opt * (s->width / 640.0);
	if (v < 1) v = 1;
	s->dx = (s->dx < 0 ? -v : v);
	s->dy = (s->dy < 0 ? -v : v);

	build_logo(s);

	if (s->lw >= s->width || s->lh >= s->height) {
		s->x = s->y = 0;
	} else {
		if (s->x < 0 || s->x > s->width - s->lw)
			s->x = rand() % (s->width - s->lw);
		if (s->y < 0 || s->y > s->height - s->lh)
			s->y = rand() % (s->height - s->lh);
	}

	XSetForeground(s->dpy, s->gc, BlackPixel(s->dpy, DefaultScreen(s->dpy)));
	XFillRectangle(s->dpy, s->win, s->gc, 0, 0, s->width, s->height);
}

static void step(struct state *s)
{
	int bounced = 0;
	s->x += s->dx;
	s->y += s->dy;

	double maxx = s->width - s->lw, maxy = s->height - s->lh;
	if (s->x <= 0)       { s->x = 0;    s->dx =  fabs(s->dx); bounced = 1; }
	else if (s->x >= maxx) { s->x = maxx; s->dx = -fabs(s->dx); bounced = 1; }
	if (s->y <= 0)       { s->y = 0;    s->dy =  fabs(s->dy); bounced = 1; }
	else if (s->y >= maxy) { s->y = maxy; s->dy = -fabs(s->dy); bounced = 1; }

	if (bounced)
		next_color(s);
}

static void draw(struct state *s)
{
	int m = s->margin;
	int bw = s->lw + 2 * m, bh = s->lh + 2 * m;

	XSetClipMask(s->dpy, s->gc, None);
	XSetForeground(s->dpy, s->gc, BlackPixel(s->dpy, DefaultScreen(s->dpy)));
	XFillRectangle(s->dpy, s->buf, s->gc, 0, 0, bw, bh);

	XSetForeground(s->dpy, s->gc, s->color);
	XSetClipMask(s->dpy, s->gc, s->mask);
	XSetClipOrigin(s->dpy, s->gc, m, m);
	XFillRectangle(s->dpy, s->buf, s->gc, m, m, s->lw, s->lh);
	XSetClipMask(s->dpy, s->gc, None);

	/* The margin covers wherever the logo was last frame. */
	XCopyArea(s->dpy, s->buf, s->win, s->gc, 0, 0, bw, bh,
	          (int)lround(s->x) - m, (int)lround(s->y) - m);
}

static double now(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec + tv.tv_usec / 1e6;
}

static void usage(const char *prog)
{
	fprintf(stderr,
	        "usage: %s [-root] [-window-id ID] [-speed N] [-fps N]\n"
	        "  -speed N   pixels per frame on a 640px wide screen (default 1)\n"
	        "  -fps N     frames per second (default 60)\n", prog);
	exit(1);
}

int main(int argc, char **argv)
{
	struct state s;
	memset(&s, 0, sizeof s);
	s.speed_opt = 1.0;
	s.x = s.y = -1;
	s.hue = -1;
	double fps = 60;
	int use_root = 0;
	Window given = 0;

	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (a[0] == '-' && a[1] == '-') a++;
		if (!strcmp(a, "-root")) use_root = 1;
		else if (!strcmp(a, "-window")) ; /* xscreensaver passes this; default anyway */
		else if (!strcmp(a, "-window-id") && i + 1 < argc)
			given = (Window)strtoul(argv[++i], NULL, 0);
		else if (!strcmp(a, "-speed") && i + 1 < argc) s.speed_opt = atof(argv[++i]);
		else if (!strcmp(a, "-fps") && i + 1 < argc) fps = atof(argv[++i]);
		else usage(argv[0]);
	}
	if (fps <= 0) fps = 60;

	srand((unsigned)(time(NULL) ^ getpid()));
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	s.dpy = XOpenDisplay(NULL);
	if (!s.dpy) {
		fprintf(stderr, "midnight: cannot open display\n");
		return 1;
	}
	int scr = DefaultScreen(s.dpy);

	const char *env = getenv("XSCREENSAVER_WINDOW");
	Atom wm_delete = XInternAtom(s.dpy, "WM_DELETE_WINDOW", False);
	if (given) {
		s.win = given;
	} else if (env && *env) {
		s.win = (Window)strtoul(env, NULL, 0);
	} else if (use_root) {
		s.win = RootWindow(s.dpy, scr);
	} else {
		s.standalone = 1;
		s.win = XCreateSimpleWindow(s.dpy, RootWindow(s.dpy, scr), 0, 0,
		                            960, 540, 0, BlackPixel(s.dpy, scr),
		                            BlackPixel(s.dpy, scr));
		XStoreName(s.dpy, s.win, "Midnight");
		XSetWMProtocols(s.dpy, s.win, &wm_delete, 1);
		XSelectInput(s.dpy, s.win, StructureNotifyMask | KeyPressMask);
		XMapRaised(s.dpy, s.win);
	}

	s.gc = XCreateGC(s.dpy, s.win, 0, NULL);
	s.dx = s.dy = 1;
	if (rand() & 1) s.dx = -1;
	if (rand() & 1) s.dy = -1;
	reset(&s);
	next_color(&s);

	double frame = 1.0 / fps, next = now(), last_check = next;
	while (!quit) {
		while (XPending(s.dpy)) {
			XEvent ev;
			XNextEvent(s.dpy, &ev);
			if (ev.type == ClientMessage &&
			    (Atom)ev.xclient.data.l[0] == wm_delete)
				quit = 1;
			else if (ev.type == KeyPress && s.standalone) {
				KeySym ks = XLookupKeysym(&ev.xkey, 0);
				if (ks == XK_q || ks == XK_Escape) quit = 1;
			}
		}

		/* Embedded windows don't always send events; poll for resizes. */
		double t = now();
		if (t - last_check > 0.5) {
			last_check = t;
			XWindowAttributes wa;
			if (!XGetWindowAttributes(s.dpy, s.win, &wa)) break;
			if (wa.width != s.width || wa.height != s.height)
				reset(&s);
		}

		step(&s);
		draw(&s);
		XFlush(s.dpy);

		next += frame;
		double wait = next - now();
		if (wait > 0)
			usleep((useconds_t)(wait * 1e6));
		else
			next = now();
	}

	XCloseDisplay(s.dpy);
	return 0;
}
