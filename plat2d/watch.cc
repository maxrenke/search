// © 2013 the Search Authors under the MIT license. See AUTHORS for the list of authors.

#include "../graphics/ui.hpp"
#include "../utils/utils.hpp"
#include "plat2d.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <vector>
#include <limits>
#include <SDL/SDL_opengl.h>

static unsigned long frametime = 20;	// in milliseconds
static long startdelay;	// in milliseconds
static long enddelay;	// in milliseconds
static long mintime;	// in milliseconds
static long maxtime = std::numeric_limits<long>::max();	// in milliseconds
static bool echo;
static bool save;
static std::string level;
static std::vector<unsigned int> controls;

static void parseargs(int, const char*[]);
static void helpmsg(int);
static void dfline(std::vector<std::string>&, void*);

struct Lvl;

class Anim {
public:
	Anim() { }
	Anim(Scene::Img&, int row, int w, int h, int len, int delay);
	void update();
	void reset();
	void draw(Scene&, const geom2d::Pt&);
private:
	Scene::Img sheet;
	int len, delay;
	int f, d;
};

class WatchUi : public Ui {
public:
	WatchUi(unsigned int, unsigned int, bool, Lvl*, std::vector<unsigned int>);
	virtual bool frame();
	virtual void key(int, bool) { /* ignore */ }
	virtual void motion(int, int, int, int) { /* ignore */ }
	virtual void click(int, int, int, bool) { /* ignore */ }

private:
	void move();
	void scroll(const geom2d::Pt&, const geom2d::Pt&);
	void draw();

	Lvl *lvl;
	Player p;
	geom2d::Pt tr;
	unsigned int width, height;
	std::vector<unsigned int> controls;
	std::vector<unsigned int>::iterator iter;

	// level images.
	Scene::Img bkgnd, block, door;

	// player animation.
	enum { Stand, Walk, Jump, Nacts };
	Scene::Img knight;
	int act;
	Anim leftas[Nacts], rightas[Nacts];
	Anim *anim;
};

int main(int argc, const char *argv[]) {
	parseargs(argc, argv);
	dfread(stdin, dfline, NULL, echo ? stdout : NULL);
	if (level == "")
		fatal("No level key in the datafile");
	if (controls.size() == 0)
		fatal("No controls");

	FILE *f = fopen(level.c_str(), "r");
	if (!f)
		fatalx(errno, "Failed to open %s", level.c_str());
	Lvl lvl(f);
	fclose(f);

	WatchUi ui(800, 600, save, &lvl, controls);
	ui.run(frametime);	
}

static void parseargs(int argc, const char *argv[]) {
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			helpmsg(0);
		} else if (strcmp(argv[i], "-f") == 0) {
			frametime = strtol(argv[++i], NULL, 10);
		} else if (strcmp(argv[i], "-d") == 0) {
			startdelay = strtol(argv[++i], NULL, 10);
		} else if (strcmp(argv[i], "-min") == 0) {
			mintime = strtol(argv[++i], NULL, 10);
		} else if (strcmp(argv[i], "-max") == 0) {
			maxtime = strtol(argv[++i], NULL, 10);
		} else if (strcmp(argv[i], "-e") == 0) {
			echo = true;
		} else if (strcmp(argv[i], "-s") == 0) {
			save = true;
		} else {
			printf("Unknown option %s", argv[i]);
			helpmsg(1);
		}
	}
}

static void helpmsg(int status) {
	puts("Usage: watch [options]");
	puts("Options:");
	puts("	-h	print this help message");
	puts("	-d	delay in milliseconds before playing");
	puts("	-min	minimum play time in milliseconds");
	puts("	-max	maximum play time in milliseconds");
	puts("	-f	frame rate in milliseconds (default 20)");
	puts("	-e	echo the input to standard output");
	puts("	-s	saves an mpeg");
	exit(status);
}

static void dfline(std::vector<std::string> &toks, void*) {
	if (toks[1] == "level")
		level = toks[2];
	else if (toks[1] == "controls")
		controls = controlvec(toks[2]);
}

WatchUi::WatchUi(unsigned int w, unsigned int h, bool save,
		Lvl *l, std::vector<unsigned int> cs) :
		Ui(w, h, save), lvl(l), tr(0, 0), width(w),
		height(h), controls(cs) {

	// flip everything over
	glTranslated(0.0, height, 0.0);
	glScaled(1.0, -1.0, 1.0);

	unsigned int x0 = lvl->width(), y0 = lvl->height();
	for (unsigned int x = 0; x < lvl->width(); x++) {
	for (unsigned int y = 0; y < lvl->height(); y++) {
		if (lvl->at(x, y).tile.flags & Tile::Up) {
			x0 = x;
			y0 = y;
		}
	}
	}
	if (x0 == lvl->width() || y0 == lvl->height())
		fatal("No start location");
	p = Player(x0 * Tile::Width + Player::Offx, y0 * Tile::Height + Player::Offy,
		Player::Width, Player::Height);
	geom2d::Pt startloc(x0 * Tile::Width + Player::Offx, y0 * Tile::Height + Player::Offy);
	scroll(geom2d::Pt(width/2, height/2), startloc);

	iter = controls.begin();

	bkgnd = Scene::Img("plat2d/img/tiles.png");
	bkgnd.w = Tile::Width;
	bkgnd.h = Tile::Height;
	bkgnd.smin = 0.0;
	bkgnd.smax = (double)Tile::Width/bkgnd.texw;
	bkgnd.tmin = 0.0;
	bkgnd.tmax = (double)Tile::Height/bkgnd.texh;

	block = Scene::Img(bkgnd);
	block.smax = (double)Tile::Width/block.texw;
	block.tmin = (double)Tile::Height/block.texh;
	block.tmax = (double)(Tile::Height*2)/block.texh;

	door = Scene::Img(bkgnd);
	door.smax = (double)Tile::Width/door.texw;
	door.tmin = (double)(Tile::Height*3)/door.texh;
	door.tmax = (double)(Tile::Height*4)/door.texh;

	knight = Scene::Img("plat2d/img/knight.png");
	leftas[Stand] = Anim(knight, 0, Tile::Width, Tile::Height, 1, 1);
	leftas[Walk] = Anim(knight, 1, Tile::Width, Tile::Height, 4, 10);
	leftas[Jump] = Anim(knight, 2, Tile::Width, Tile::Height, 1, 1);
	rightas[Stand] = Anim(knight, 3, Tile::Width, Tile::Height, 1, 1);
	rightas[Walk] = Anim(knight, 4, Tile::Width, Tile::Height, 4, 10);
	rightas[Jump] = Anim(knight, 5, Tile::Width, Tile::Height, 1, 1);
	act = Stand;
	anim = rightas;
}

bool WatchUi::frame() {
	mintime -= frametime;
	maxtime -= frametime;
	if (maxtime <= 0)
		return false;
	if (iter == controls.end()) {
		if (enddelay == 0)
			enddelay = 1000;
		else
			enddelay -= frametime;
		if (enddelay <= 0 && mintime <= 0)
			return false;
		return true;
	}
	if (startdelay > 0)
		startdelay -= frametime;
	else
		move();
	draw();
	return true;
}

void WatchUi::move() {
	unsigned int keys = *iter++;
	geom2d::Pt p0(p.loc());
	p.act(*lvl, keys);

	Anim *prevanim = anim;
	anim = rightas;
	if (keys & Player::Left)
		anim = leftas;

	act = Stand;
	if (p.body.fall)
		act = Jump;
	else if (keys & (Player::Left | Player::Right))
		act = Walk;

	if (anim != prevanim)
		anim[act].reset();
	else
		anim[act].update();

	scroll(p0, p.loc());
}

void WatchUi::scroll(const geom2d::Pt &p0, const geom2d::Pt &p1) {
	geom2d::Pt delta(p1.x - p0.x, p1.y - p0.y);
	if ((delta.x > 0 && p1.x + tr.x > width * 0.75) ||
		(delta.x < 0 && p1.x + tr.x < width * 0.25))
		tr.x -= delta.x;
	if ((delta.y > 0 && p1.y + tr.y > height * 0.75) ||
		(delta.y < 0 && p1.y + tr.y < height * 0.25))
		tr.y -= delta.y;
}

void WatchUi::draw() {
	scene.clear();

	geom2d::Poly rect(4,
		tr.x, tr.y,
		tr.x + lvl->width()*Tile::Width, tr.y,
		tr.x + lvl->width()*Tile::Width, tr.y + lvl->height()*Tile::Height,
		tr.x, tr.y + lvl->height()*Tile::Height);
	scene.add(new Scene::Poly(rect, Image::black, -1));

	for (unsigned int i = 0; i < lvl->width(); i++) {
	for (unsigned int j = 0; j < lvl->height(); j++) {
		Lvl::Blkinfo bi(lvl->at(i, j));

		double x = i*Tile::Width + tr.x;
		double y = j*Tile::Height + tr.y;


		Scene::Img *img;
		if (bi.tile.flags & Tile::Collide) {
			img = new Scene::Img(block, x, y);
			scene.add(img);
			continue;
		}

		img = new Scene::Img(bkgnd, x, y);
		scene.add(img);

		if (bi.tile.flags & Tile::Down) {
			img = new Scene::Img(door, x, y);
			scene.add(img);
		}

	}
	}

	geom2d::Pt loc(p.body.bbox.min);
	loc.translate(tr.x, tr.y);
	anim[act].draw(scene, loc);
}

Anim::Anim(Scene::Img &img, int r, int w, int h, int l, int d) :
		sheet(img), len(l), delay(d), f(0), d(delay) {
	sheet.w = w;
	sheet.h = h;
	sheet.tmin = r*sheet.h / sheet.texh;
	sheet.tmax = (r+1)*sheet.h / sheet.texh;
}

void Anim::update() {
	d--;
	if (d > 0)
		return;
	d = delay;
	f++;
	if (f == len)
		f = 0;
}

void Anim::reset() {
	f = 0;
	d = delay;
}

void Anim::draw(Scene &s, const geom2d::Pt &pt) {
	Scene::Img *img = new Scene::Img(sheet, pt.x, pt.y);
	img->smin = f*sheet.w / sheet.texw;
	img->smax = (f+1)*sheet.w / sheet.texw;
	s.add(img);
}