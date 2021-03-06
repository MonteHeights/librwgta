#include "euryopa.h"

namespace Timecycle
{

struct Box
{
	CBox box;
	float farclp;
	float lodDistMult;
	int extraColor;
	float strength;
	float falloff;

	CBox getOuterBox(void){
		CBox outbox = box;
		outbox.min.x -= falloff;
		outbox.min.y -= falloff;
		outbox.min.z -= falloff/3.0;
		outbox.max.x += falloff;
		outbox.max.y += falloff;
		outbox.max.z += falloff/3.0f;
		return outbox;
	}
};

static Box boxes[NUMTCYCBOXES];
static int numBoxes;

Box*
FindBox(rw::V3d pos, float *amount, bool wantLod, bool wantFar, Box *exclude)
{
	int i;
	Box *b;
	rw::V3d dist;
	for(i = 0; i < numBoxes; i++){
		b = &boxes[i];
		if(b == exclude ||
		   wantLod && b->lodDistMult == 1.0f ||
		   wantFar && b->farclp == 0.0f)
			continue;

		CBox inbox = b->box;
		CBox outbox = b->getOuterBox();
		if(pos.x < outbox.min.x || pos.y < outbox.min.y || pos.z < outbox.min.z ||
		   pos.x > outbox.max.x || pos.y > outbox.max.y || pos.z > outbox.max.z)
			continue;

		if(pos.x < inbox.min.x)
			dist.x = inbox.min.x - pos.x;
		else if(pos.x > inbox.max.x)
			dist.x = pos.x - inbox.max.x;
		else
			dist.x = 0.0f;

		if(pos.y < inbox.min.y)
			dist.y = inbox.min.y - pos.y;
		else if(pos.y > inbox.max.y)
			dist.y = pos.y - inbox.max.y;
		else
			dist.y = 0.0f;

		if(pos.z < inbox.min.z)
			dist.z = inbox.min.z - pos.z;
		else if(pos.z > inbox.max.z)
			dist.z = pos.z - inbox.max.z;
		else
			dist.z = 0.0f;

		dist.z *= 3.0f;
		*amount = 1.0f - length(dist)/b->falloff;
		return b;
	}
	return nil;
}

void
AddBox(CBox box, int farClp, int extraCol, float extraStrength, float falloff, float lodDist)
{
	if(numBoxes >= NUMTCYCBOXES){
		log("warning: more than %d timecycle boxes\n", NUMTCYCBOXES);
		return;
	}
	Box *tbox = &boxes[numBoxes++];
	tbox->box = box;
	tbox->farclp = farClp;
	if(lodDist > 4.0f){
		log("timecycle: clamping box lodDist %f to 4.0\n", lodDist);
		lodDist = 4.0f;
	}
	tbox->lodDistMult = lodDist;
	tbox->extraColor = extraCol;
	tbox->strength = extraStrength/100.0f;
	tbox->falloff = falloff;
}

static void
Interpolate(rw::RGBAf *dst, rw::RGBAf *a, rw::RGBAf *b, float fa, float fb)
{
	dst->red = fa * a->red + fb * b->red;
	dst->green = fa * a->green + fb * b->green;
	dst->blue = fa * a->blue + fb * b->blue;
	dst->alpha = fa * a->alpha + fb * b->alpha;
}

static void
Interpolate(ColourSet *dst, ColourSet *a, ColourSet *b, float fa, float fb)
{
	Interpolate(&dst->amb, &a->amb, &b->amb, fa, fb);
	Interpolate(&dst->amb_obj, &a->amb_obj, &b->amb_obj, fa, fb);
	Interpolate(&dst->amb_bl, &a->amb_bl, &b->amb_bl, fa, fb);
	Interpolate(&dst->amb_obj_bl, &a->amb_obj_bl, &b->amb_obj_bl, fa, fb);
	Interpolate(&dst->dir, &a->dir, &b->dir, fa, fb);
	Interpolate(&dst->skyTop, &a->skyTop, &b->skyTop, fa, fb);
	Interpolate(&dst->skyBottom, &a->skyBottom, &b->skyBottom, fa, fb);
	Interpolate(&dst->sunCore, &a->sunCore, &b->sunCore, fa, fb);
	Interpolate(&dst->sunCorona, &a->sunCorona, &b->sunCorona, fa, fb);
	dst->sunSz = fa * a->sunSz + fb * b->sunSz;
	dst->sprSz = fa * a->sprSz + fb * b->sprSz;
	dst->sprBght = fa * a->sprBght + fb * b->sprBght;
	dst->shdw = fa * a->shdw + fb * b->shdw;
	dst->lightShd = fa * a->lightShd + fb * b->lightShd;
	dst->poleShd = fa * a->poleShd + fb * b->poleShd;
	dst->farClp = fa * a->farClp + fb * b->farClp;
	dst->fogSt = fa * a->fogSt + fb * b->fogSt;
	dst->lightOnGround = fa * a->lightOnGround + fb * b->lightOnGround;
	Interpolate(&dst->lowCloud, &a->lowCloud, &b->lowCloud, fa, fb);
	Interpolate(&dst->fluffyCloudTop, &a->fluffyCloudTop, &b->fluffyCloudTop, fa, fb);
	Interpolate(&dst->fluffyCloudBottom, &a->fluffyCloudBottom, &b->fluffyCloudBottom, fa, fb);
	Interpolate(&dst->water, &a->water, &b->water, fa, fb);
	Interpolate(&dst->postfx1, &a->postfx1, &b->postfx1, fa, fb);
	Interpolate(&dst->postfx2, &a->postfx2, &b->postfx2, fa, fb);
	dst->cloudAlpha = fa * a->cloudAlpha + fb * b->cloudAlpha;
	dst->intensityLimit = fa * a->intensityLimit + fb * b->intensityLimit;
	dst->waterFogAlpha = fa * a->waterFogAlpha + fb * b->waterFogAlpha;
	dst->dirMult = fa * a->dirMult + fb * b->dirMult;
	dst->lightMapIntensity = fa * a->lightMapIntensity + fb * b->lightMapIntensity;
}

static ColourSet *timecycleData;
ColourSet currentColours;
rw::RGBAf currentFogColour;
rw::RGBA belowHorizonColour;
#define GetColourSet(h, w) timecycleData[(h)*params.numWeathers + (w)]

static void
InitializeIII(void)
{
	int ambR, ambG, ambB;
	int dirR, dirG, dirB;
	int skyTopR, skyTopG, skyTopB;
	int skyBotR, skyBotG, skyBotB;
	int sunCoreR, sunCoreG, sunCoreB;
	int sunCoronaR, sunCoronaG, sunCoronaB;
	float sunSz, sprSz, sprBght;
	int shad, lightShad, treeShad;
	float farClp, fogSt, lightGnd;
	int cloudR, cloudG, cloudB;
	int fluffyTopR, fluffyTopG, fluffyTopB;
	int fluffyBotR, fluffyBotG, fluffyBotB;
	float blurR, blurG, blurB, blurA;

	ColourSet *cs;

	FILE *file;
	char *line;
	if(file = fopen_ci("data/timecyc.dat", "rb"), file == nil)
		return;
	int h = 0, w = 0;
	while(line = FileLoader::LoadLine(file)){
		if(line[0] == '/' && line[1] == '/')
			continue;
		sscanf(line, "%d %d %d  %d %d %d  %d %d %d  %d %d %d "
		             "%d %d %d  %d %d %d  %f %f %f %d %d %d %f %f %f "
		             "%d %d %d  %d %d %d  %d %d %d  %f %f %f %f",
			&ambR, &ambG, &ambB,
			 &dirR, &dirG, &dirB,
			 &skyTopR, &skyTopG, &skyTopB,
			 &skyBotR, &skyBotG, &skyBotB,
			&sunCoreR, &sunCoreG, &sunCoreB,
			 &sunCoronaR, &sunCoronaG, &sunCoronaB,
			 &sunSz, &sprSz, &sprBght,
			 &shad, &lightShad, &treeShad,
			 &farClp, &fogSt, &lightGnd,
			&cloudR, &cloudG, &cloudB,
			 &fluffyTopR, &fluffyTopG, &fluffyTopB,
			 &fluffyBotR, &fluffyBotG, &fluffyBotB,
			 &blurR, &blurG, &blurB, &blurA);

		cs = &GetColourSet(h, w);
		cs->amb = rw::makeRGBAf(ambR/255.0f, ambG/255.0f, ambB/255.0f, 0);
		cs->dir = rw::makeRGBAf(dirR/255.0f, dirG/255.0f, dirB/255.0f, 0);
		cs->skyTop = rw::makeRGBAf(skyTopR/255.0f, skyTopG/255.0f, skyTopB/255.0f, 0);
		cs->skyBottom = rw::makeRGBAf(skyBotR/255.0f, skyBotG/255.0f, skyBotB/255.0f, 0);
		cs->sunCore = rw::makeRGBAf(sunCoreR/255.0f, sunCoreG/255.0f, sunCoreB/255.0f, 0);
		cs->sunCorona = rw::makeRGBAf(sunCoronaR/255.0f, sunCoronaG/255.0f, sunCoronaB/255.0f, 0);
		cs->sunSz = sunSz;
		cs->sprSz = sprSz;
		cs->sprBght = sprBght;
		cs->shdw = shad;
		cs->lightShd = lightShad;
		cs->poleShd = treeShad;
		cs->farClp = farClp;
		cs->fogSt = fogSt;
		cs->lightOnGround = lightGnd;
		cs->lowCloud = rw::makeRGBAf(cloudR/255.0f, cloudG/255.0f, cloudB/255.0f, 0);
		cs->fluffyCloudTop = rw::makeRGBAf(fluffyTopR/255.0f, fluffyTopG/255.0f, fluffyTopB/255.0f, 0);
		cs->fluffyCloudBottom = rw::makeRGBAf(fluffyBotR/255.0f, fluffyBotG/255.0f, fluffyBotB/255.0f, 0);
		cs->postfx1 = rw::makeRGBAf(blurR/255.0f, blurG/255.0f, blurB/255.0f, blurA/255.0f);

		h++;
		if(h == params.numHours){
			h = 0;
			w++;
		}
		if(w == params.numWeathers)
			break;
	}
	fclose(file);
}

static void
InitializeVC(void)
{
	int ambR, ambG, ambB;
	int ambR_obj, ambG_obj, ambB_obj;
	int ambR_bl, ambG_bl, ambB_bl;
	int ambR_obj_bl, ambG_obj_bl, ambB_obj_bl;
	int dirR, dirG, dirB;
	int skyTopR, skyTopG, skyTopB;
	int skyBotR, skyBotG, skyBotB;
	int sunCoreR, sunCoreG, sunCoreB;
	int sunCoronaR, sunCoronaG, sunCoronaB;
	float sunSz, sprSz, sprBght;
	int shad, lightShad, poleShad;
	float farClp, fogSt, lightGnd;
	int cloudR, cloudG, cloudB;
	int fluffyTopR, fluffyTopG, fluffyTopB;
	int fluffyBotR, fluffyBotG, fluffyBotB;
	float blurR, blurG, blurB;
	float waterR, waterG, waterB, waterA;

	ColourSet *cs;

	FILE *file;
	char *line;
	if(file = fopen_ci("data/timecyc.dat", "rb"), file == nil)
		return;
	int h = 0, w = 0;
	while(line = FileLoader::LoadLine(file)){
		if(line[0] == '/' && line[1] == '/')
			continue;
		sscanf(line, "%d %d %d  %d %d %d  %d %d %d  %d %d %d "
		             "%d %d %d  %d %d %d  %d %d %d "
		             "%d %d %d  %d %d %d  %f %f %f %d %d %d %f %f %f "
		             "%d %d %d  %d %d %d  %d %d %d  %f %f %f  %f %f %f %f",
			&ambR, &ambG, &ambB,
			 &ambR_obj, &ambG_obj, &ambB_obj,
			 &ambR_bl, &ambG_bl, &ambB_bl,
			 &ambR_obj_bl, &ambG_obj_bl, &ambB_obj_bl,
			&dirR, &dirG, &dirB,
			 &skyTopR, &skyTopG, &skyTopB,
			 &skyBotR, &skyBotG, &skyBotB,
			&sunCoreR, &sunCoreG, &sunCoreB,
			 &sunCoronaR, &sunCoronaG, &sunCoronaB,
			 &sunSz, &sprSz, &sprBght,
			 &shad, &lightShad, &poleShad,
			 &farClp, &fogSt, &lightGnd,
			&cloudR, &cloudG, &cloudB,
			 &fluffyTopR, &fluffyTopG, &fluffyTopB,
			 &fluffyBotR, &fluffyBotG, &fluffyBotB,
			 &blurR, &blurG, &blurB,
			 &waterR, &waterG, &waterB, &waterA);

		cs = &GetColourSet(h, w);
		cs->amb = rw::makeRGBAf(ambR/255.0f, ambG/255.0f, ambB/255.0f, 0);
		cs->amb_obj = rw::makeRGBAf(ambR_obj/255.0f, ambG_obj/255.0f, ambB_obj/255.0f, 0);
		cs->amb_bl = rw::makeRGBAf(ambR_bl/255.0f, ambG_bl/255.0f, ambB_bl/255.0f, 0);
		cs->amb_obj_bl = rw::makeRGBAf(ambR_obj_bl/255.0f, ambG_obj_bl/255.0f, ambB_obj_bl/255.0f, 0);
		cs->dir = rw::makeRGBAf(dirR/255.0f, dirG/255.0f, dirB/255.0f, 0);
		cs->skyTop = rw::makeRGBAf(skyTopR/255.0f, skyTopG/255.0f, skyTopB/255.0f, 0);
		cs->skyBottom = rw::makeRGBAf(skyBotR/255.0f, skyBotG/255.0f, skyBotB/255.0f, 0);
		cs->sunCore = rw::makeRGBAf(sunCoreR/255.0f, sunCoreG/255.0f, sunCoreB/255.0f, 0);
		cs->sunCorona = rw::makeRGBAf(sunCoronaR/255.0f, sunCoronaG/255.0f, sunCoronaB/255.0f, 0);
		cs->sunSz = sunSz;
		cs->sprSz = sprSz;
		cs->sprBght = sprBght;
		cs->shdw = shad;
		cs->lightShd = lightShad;
		cs->poleShd = poleShad;
		cs->farClp = farClp;
		cs->fogSt = fogSt;
		cs->lightOnGround = lightGnd;
		cs->lowCloud = rw::makeRGBAf(cloudR/255.0f, cloudG/255.0f, cloudB/255.0f, 0);
		cs->fluffyCloudTop = rw::makeRGBAf(fluffyTopR/255.0f, fluffyTopG/255.0f, fluffyTopB/255.0f, 0);
		cs->fluffyCloudBottom = rw::makeRGBAf(fluffyBotR/255.0f, fluffyBotG/255.0f, fluffyBotB/255.0f, 0);
		cs->postfx1 = rw::makeRGBAf(blurR/255.0f, blurG/255.0f, blurB/255.0f, 0);
		cs->water = rw::makeRGBAf(waterR/255.0f, waterG/255.0f, waterB/255.0f, waterA/255.0f);

		h++;
		if(h == params.numHours){
			h = 0;
			w++;
		}
		if(w == params.numWeathers)
			break;
	}
	fclose(file);
}

static void
InitializeSA(void)
{
	int ambR, ambG, ambB;
	int ambR_obj, ambG_obj, ambB_obj;
	int dirR, dirG, dirB;
	int skyTopR, skyTopG, skyTopB;
	int skyBotR, skyBotG, skyBotB;
	int sunCoreR, sunCoreG, sunCoreB;
	int sunCoronaR, sunCoronaG, sunCoronaB;
	float sunSz, sprSz, sprBght;
	int shad, lightShad, poleShad;
	float farClp, fogSt, lightGnd;
	int cloudR, cloudG, cloudB;
	int fluffyBotR, fluffyBotG, fluffyBotB;
	float postfx1R, postfx1G, postfx1B, postfx1A;
	float postfx2R, postfx2G, postfx2B, postfx2A;
	float waterR, waterG, waterB, waterA;
	float cloudAlpha;
	int intensityLimit, waterFogAlpha;
	float dirMult;

	ColourSet *cs;

	FILE *file;
	char *line;
	if(file = fopen_ci("data/timecyc.dat", "rb"), file == nil)
		return;
	int h = 0, w = 0;
	while(line = FileLoader::LoadLine(file)){
		if(line[0] == '/' && line[1] == '/')
			continue;
		sscanf(line, "%d %d %d  %d %d %d "
		             "%d %d %d  %d %d %d  %d %d %d "
		             "%d %d %d  %d %d %d  %f %f %f %d %d %d %f %f %f "
		             "%d %d %d  %d %d %d  %f %f %f %f "
		             "%f %f %f %f  %f %f %f %f  %f %d %d %f",
			&ambR, &ambG, &ambB,
			 &ambR_obj, &ambG_obj, &ambB_obj,
			&dirR, &dirG, &dirB,
			 &skyTopR, &skyTopG, &skyTopB,
			 &skyBotR, &skyBotG, &skyBotB,
			&sunCoreR, &sunCoreG, &sunCoreB,
			 &sunCoronaR, &sunCoronaG, &sunCoronaB,
			 &sunSz, &sprSz, &sprBght,
			 &shad, &lightShad, &poleShad,
			 &farClp, &fogSt, &lightGnd,
			&cloudR, &cloudG, &cloudB,
			 &fluffyBotR, &fluffyBotG, &fluffyBotB,
			 &waterR, &waterG, &waterB, &waterA,
			&postfx1A, &postfx1R, &postfx1G, &postfx1B,
			 &postfx2A, &postfx2R, &postfx2G, &postfx2B,
			 &cloudAlpha, &intensityLimit, &waterFogAlpha, &dirMult);

		cs = &GetColourSet(h, w);
		cs->amb = rw::makeRGBAf(ambR/255.0f, ambG/255.0f, ambB/255.0f, 0);
		cs->amb_obj = rw::makeRGBAf(ambR_obj/255.0f, ambG_obj/255.0f, ambB_obj/255.0f, 0);
		cs->dir = rw::makeRGBAf(dirR/255.0f, dirG/255.0f, dirB/255.0f, 0);
		cs->skyTop = rw::makeRGBAf(skyTopR/255.0f, skyTopG/255.0f, skyTopB/255.0f, 0);
		cs->skyBottom = rw::makeRGBAf(skyBotR/255.0f, skyBotG/255.0f, skyBotB/255.0f, 0);
		cs->sunCore = rw::makeRGBAf(sunCoreR/255.0f, sunCoreG/255.0f, sunCoreB/255.0f, 0);
		cs->sunCorona = rw::makeRGBAf(sunCoronaR/255.0f, sunCoronaG/255.0f, sunCoronaB/255.0f, 0);
		cs->sunSz = sunSz;
		cs->sprSz = sprSz;
		cs->sprBght = sprBght;
		cs->shdw = shad;
		cs->lightShd = lightShad;
		cs->poleShd = poleShad;
		cs->farClp = farClp;
		cs->fogSt = fogSt;
		cs->lightOnGround = lightGnd;
		cs->lowCloud = rw::makeRGBAf(cloudR/255.0f, cloudG/255.0f, cloudB/255.0f, 0);
		cs->fluffyCloudBottom = rw::makeRGBAf(fluffyBotR/255.0f, fluffyBotG/255.0f, fluffyBotB/255.0f, 0);
		cs->water = rw::makeRGBAf(waterR/255.0f, waterG/255.0f, waterB/255.0f, waterA/255.0f);
		// Clamp values so PC timecycle works
		if(postfx1A > 128) postfx1A = 128;
		if(postfx2A > 128) postfx2A = 128;
		cs->postfx1 = rw::makeRGBAf(postfx1R/255.0f, postfx1G/255.0f, postfx1B/255.0f, postfx1A/255.0f);
		cs->postfx2 = rw::makeRGBAf(postfx2R/255.0f, postfx2G/255.0f, postfx2B/255.0f, postfx2A/255.0f);
		cs->cloudAlpha = cloudAlpha;
		cs->intensityLimit = intensityLimit;
		cs->waterFogAlpha = waterFogAlpha;
		cs->dirMult = dirMult;

		h++;
		if(h == params.numHours){
			h = 0;
			w++;
		}
		if(w == params.numWeathers)
			break;
	}
	fclose(file);
}

void
Initialize(void)
{
	timecycleData = rwNewT(ColourSet, params.numHours*params.numWeathers, 0);
	memset(timecycleData, 0, sizeof(ColourSet)*params.numHours*params.numWeathers);

	switch(params.timecycle){
	case GAME_III:
		InitializeIII();
		break;
	case GAME_VC:
		InitializeVC();
		break;
	case GAME_SA:
		InitializeSA();
		break;
	}
}

static void
readFloat(char *s, int line, int field, uint32 offset)
{
	ColourSet *cs = &GetColourSet(line, field);
	sscanf(s, "%f", (float*)((uint8*)cs + offset));
}

static void
neoReadWeatherTimeBlock(FILE *file, void (*f)(char*,int,int,uint32), uint32 offset)
{
	char buf[24], *p;
	int c;
	int line, field;

	line = 0;
	c = getc(file);
	while(c != EOF && line < params.numHours){
		field = 0;
		if(c != EOF && c != '#'){
			while(c != EOF && c != '\n' && field < params.numWeathers){
				p = buf;
				while(c != EOF && c == '\t')
					c = getc(file);
				*p++ = c;
				while(c = getc(file), c != EOF && c != '\t' && c != '\n')
					*p++ = c;
				*p++ = '\0';
				f(buf, line, field, offset);
				field++;
			}
			line++;
		}
		while(c != EOF && c != '\n')
			c = getc(file);
		c = getc(file);
	}
	ungetc(c, file);
}

void
InitNeoWorldTweak(void)
{
	FILE *file;
	if(file = fopen_ci("neo/worldTweakingTable.dat", "r"), file){
		neoReadWeatherTimeBlock(file, readFloat, offsetof(ColourSet, lightMapIntensity));
		fclose(file);
	}else
		log("warning: couldn't open neo/worldTweakingTable.dat");
}

void
UpdateSA(void)
{
	// TODO: 24h support!

	rw::V3d pos = TheCamera.m_position;

	// Find modifying boxes
	float lodAmt, farAmt1, farAmt2, weatherAmt;
	Box *lodBox = FindBox(pos, &lodAmt, true, false, nil);
	Box *farBox1 = FindBox(pos, &farAmt1, false, true, nil);
	Box *farBox2 = nil;
	// Try to find a smaller box for the far value
	if(farBox1){
		farBox2 = FindBox(pos, &farAmt2, false, true, farBox1);
		if(farBox2 &&
		   farBox2->box.max.x - farBox2->box.min.x > farBox2->box.max.y - farBox2->box.min.y){
			// swap if the new box was the bigger one
			Box *tb = farBox1; farBox1 = farBox2; farBox2 = tb;
			float tf = farAmt1; farAmt1 = farAmt2; farAmt2 = tf;
		}
	}
	Box *weatherBox = FindBox(pos, &weatherAmt, false, false, nil);


	static int hours[] = { 0, 5, 6, 7, 12, 19, 20, 22, 24 };
	static int belowHoriz[] = { 30, 30, 30, 50, 60, 60, 50, 35 };
	ColourSet curOld, curNew, nextOld, nextNew, oldInterp, newInterp;
	float time = currentHour + currentMinute/60.0f;
	int curHour, nextHour;
	int curHourSel, nextHourSel;

	for(curHourSel = 0; time >= hours[curHourSel+1]; curHourSel++);
	nextHourSel = (curHourSel + 1) % params.numHours;
	curHour = hours[curHourSel];
	nextHour = hours[curHourSel+1];
	float timeInterp = (time - curHour) / (float)(nextHour - curHour);

	curOld = GetColourSet(curHourSel, oldWeather);
	curNew = GetColourSet(curHourSel, newWeather);
	nextOld = GetColourSet(nextHourSel, oldWeather);
	nextNew = GetColourSet(nextHourSel, newWeather);

	int bh = belowHoriz[curHourSel]*(1.0f-timeInterp) + belowHoriz[nextHourSel]*timeInterp;
	belowHorizonColour = rw::makeRGBA(bh, bh, bh, 255);

	Interpolate(&oldInterp, &curOld, &nextOld, 1.0f-timeInterp, timeInterp);
	Interpolate(&newInterp, &curNew, &nextNew, 1.0f-timeInterp, timeInterp);
	Interpolate(&currentColours, &oldInterp, &newInterp, 1.0f-weatherInterpolation, weatherInterpolation);

	// TODO: lots of stuff that's not so important now

	// Apply box modifiers
	if(gEnableTimecycleBoxes){
		if(weatherBox && weatherBox->extraColor >= 0){
			int boxWeather = (weatherBox->extraColor / params.numHours) + params.extraColours;
			int boxHour = weatherBox->extraColor % params.numHours;
			float f = weatherBox->strength*weatherAmt;
			ColourSet boxcolours = GetColourSet(boxHour, boxWeather);
			ColourSet tmp;
			Interpolate(&tmp, &currentColours, &boxcolours, 1.0f - f, f);
			// We don't want to change everything
			currentColours.skyTop = tmp.skyTop;
			currentColours.skyBottom = tmp.skyBottom;
			currentColours.water = tmp.water;
			currentColours.amb = tmp.amb;
			currentColours.amb_obj = tmp.amb_obj;
			if(tmp.farClp < currentColours.farClp)
				currentColours.farClp = tmp.farClp;
			currentColours.fogSt = tmp.fogSt;
			currentColours.postfx1 = tmp.postfx1;
			currentColours.postfx2 = tmp.postfx2;
		}

		// TODO: lod box, and also use the value somewhere

		if(farBox1 && farBox1->farclp < currentColours.farClp)
			currentColours.farClp = currentColours.farClp*(1.0f-farAmt1) + farBox1->farclp*farAmt1;
		if(farBox2 && farBox2->farclp < currentColours.farClp)
			currentColours.farClp = currentColours.farClp*(1.0f-farAmt2) + farBox2->farclp*farAmt2;
	}

	if(extraColours >= 0)
		currentColours = GetColourSet(extraColours % params.numHours, params.extraColours + extraColours / params.numHours);

	currentFogColour.red = (currentColours.skyTop.red + 2.0f*currentColours.skyBottom.red)/3.0f;
	currentFogColour.green = (currentColours.skyTop.green + 2.0f*currentColours.skyBottom.green)/3.0f;
	currentFogColour.blue = (currentColours.skyTop.blue + 2.0f*currentColours.skyBottom.blue)/3.0f;

	gNeoLightMapStrength = currentColours.lightMapIntensity;
}

void
Update(void)
{
	if(params.timecycle == GAME_SA){
		UpdateSA();
		return;
	}

	ColourSet curOld, curNew, nextOld, nextNew, oldInterp, newInterp;
	int curHourSel = currentHour;
	int nextHourSel = (currentHour+1)%24;
	float timeInterp = currentMinute/60.0f;
	curOld = GetColourSet(curHourSel, oldWeather);
	curNew = GetColourSet(curHourSel, newWeather);
	nextOld = GetColourSet(nextHourSel, oldWeather);
	nextNew = GetColourSet(nextHourSel, newWeather);

	Interpolate(&oldInterp, &curOld, &nextOld, 1.0f-timeInterp, timeInterp);
	Interpolate(&newInterp, &curNew, &nextNew, 1.0f-timeInterp, timeInterp);
	Interpolate(&currentColours, &oldInterp, &newInterp, 1.0f-weatherInterpolation, weatherInterpolation);

	if(extraColours >= 0)
		currentColours = GetColourSet(extraColours % params.numHours, params.extraColours + extraColours / params.numHours);

	currentFogColour.red = (currentColours.skyTop.red + 2.0f*currentColours.skyBottom.red)/3.0f;
	currentFogColour.green = (currentColours.skyTop.green + 2.0f*currentColours.skyBottom.green)/3.0f;
	currentFogColour.blue = (currentColours.skyTop.blue + 2.0f*currentColours.skyBottom.blue)/3.0f;

	gNeoLightMapStrength = currentColours.lightMapIntensity;
}

void
SetLights(void)
{
	if(params.timecycle != GAME_SA)
		pDirect->setColor(currentColours.dir.red, currentColours.dir.green, currentColours.dir.blue);
	else
		pDirect->setColor(currentColours.dirMult, currentColours.dirMult, currentColours.dirMult);

	if(params.timecycle == GAME_VC && gUseBlurAmb)
		pAmbient->setColor(currentColours.amb_bl.red, currentColours.amb_bl.green, currentColours.amb_bl.blue);
	else
		pAmbient->setColor(currentColours.amb.red, currentColours.amb.green, currentColours.amb.blue);
}

static void
GetBoxVertices(CBox box, rw::V3d *verts)
{
	verts[0].x = box.min.x;
	verts[0].y = box.min.y;
	verts[0].z = box.min.z;
	verts[1].x = box.max.x;
	verts[1].y = box.min.y;
	verts[1].z = box.min.z;
	verts[2].x = box.min.x;
	verts[2].y = box.max.y;
	verts[2].z = box.min.z;
	verts[3].x = box.max.x;
	verts[3].y = box.max.y;
	verts[3].z = box.min.z;
	verts[4].x = box.min.x;
	verts[4].y = box.min.y;
	verts[4].z = box.max.z;
	verts[5].x = box.max.x;
	verts[5].y = box.min.y;
	verts[5].z = box.max.z;
	verts[6].x = box.min.x;
	verts[6].y = box.max.y;
	verts[6].z = box.max.z;
	verts[7].x = box.max.x;
	verts[7].y = box.max.y;
	verts[7].z = box.max.z;
}

void
RenderBoxes(void)
{
	rw::RGBA colin = { 255, 255, 0, 255 };
	rw::RGBA colout = { 0, 255, 255, 255 };
	rw::Matrix ident;
	ident.setIdentity();

	int i, j;
	rw::V3d corners[16];
	for(i = 0; i < numBoxes; i++){
		CBox outbox = boxes[i].getOuterBox();

		RenderWireBox(&boxes[i].box, colin, &ident);
		RenderWireBox(&outbox, colout, &ident);

		GetBoxVertices(boxes[i].box, corners);
		GetBoxVertices(outbox, corners+8);
		for(j = 0; j < 8; j++)
			RenderLine(corners[j], corners[j+8], colin, colout);
	}
}

}
