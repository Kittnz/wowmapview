#include "liquid.h"
#include "world.h"

struct LiquidVertex {
	unsigned char c[4];
	float h;
};


void Liquid::initFromTerrain(MPQFile &f, int flags)
{
	texRepeats = 4.0f;
	/*
	flags:
	8 - ocean
	4 - river
	16 - magma
	*/
	ydir = 1.0f;
	if (flags & 16) {
		// magma:
		initTextures("XTextures\\lava\\lava", 1, 30);
		type = 0; // not colored
	}
	else if (flags & 4) {
		// river/lake
		initTextures("XTextures\\river\\lake_a", 1, 30); // TODO: rivers etc.?
		type = 2; // dynamic colors
	}
	else {
		// ocean
		initTextures("XTextures\\ocean\\ocean_h", 1, 30);
		/*
		type = 1; // static color
		col = Vec3D(0.0f, 0.1f, 0.4f); // TODO: figure out real ocean colors?
		*/
		type = 2;
	}
	initGeometry(f);
	trans = false;
}

void Liquid::initFromWMO(MPQFile &f, WMOMaterial &mat, bool indoor)
{
	texRepeats = 4.0f;
	ydir = -1.0f;

	initGeometry(f);

	trans = false;

	// tmpflag is the flags value for the last drawn tile
	if (tmpflag & 1) {
		initTextures("XTextures\\slime\\slime", 1, 30);
		type = 0;
		texRepeats = 2.0f;
	}
	else if (tmpflag & 2) {
		initTextures("XTextures\\lava\\lava", 1, 30);
		type = 0;
	}
	else {
		initTextures("XTextures\\river\\lake_a", 1, 30);
		if (indoor) {
			trans = true;
			type = 1;
			col = Vec3D( ((mat.col2&0xFF0000)>>16)/255.0f, ((mat.col2&0xFF00)>>8)/255.0f, (mat.col2&0xFF)/255.0f);
		} else {
			type = 2; // outdoor water (...?)
		}
	}

	/*
	// HACK: this is just...wrong
	// TODO: figure out proper way to identify liquid types
	const char *texname = video.textures.items[mat.tex]->name.c_str();
	char *pos = strstr(texname, "Slime");
	if (pos!=0) {
		// slime
		initTextures("XTextures\\slime\\slime", 1, 30);
		type = 0;
		texRepeats = 4.0f;
	} else {
		if (mat.transparent == 1) {
			// lava?
			initTextures("XTextures\\lava\\lava", 1, 30);
			type = 0;
		} else {
			// water?
			initTextures("XTextures\\river\\lake_a", 1, 30);
			type = 1;
			col = Vec3D( ((mat.col2&0xFF0000)>>16)/255.0f, ((mat.col2&0xFF00)>>8)/255.0f, (mat.col2&0xFF)/255.0f);
		}
	}
	*/
}


void Liquid::initGeometry(MPQFile& f) {
	LiquidVertex* map = (LiquidVertex*)f.getPointer();

	// Validate pointer and check for obvious corruption
	if (!map || reinterpret_cast<uintptr_t>(map) & 0x3) {
		gLog("Error: Invalid or misaligned liquid vertex map pointer: %p\n", map);
		return;
	}

	size_t flagsOffset = (xtiles + 1) * (ytiles + 1) * sizeof(LiquidVertex);
	if (flagsOffset != 648) { // Expected size based on 8x8 tiles
		gLog("Error: Invalid flags offset %zu (expected 648)\n", flagsOffset);
		return;
	}

	// Validate tiles dimensions
	if (xtiles != 8 || ytiles != 8) {
		gLog("Error: Invalid tile dimensions: %dx%d\n", xtiles, ytiles);
		return;
	}

	unsigned char* flags = (unsigned char*)(f.getPointer() + flagsOffset);
	if (!flags) {
		gLog("Error: Invalid flags pointer\n");
		return;
	}

	Vec3D* verts = new Vec3D[(xtiles + 1) * (ytiles + 1)];

	for (int j = 0; j < ytiles + 1; j++) {
		for (int i = 0; i < xtiles + 1; i++) {
			size_t p = j * (xtiles + 1) + i;

			// Check array bounds
			if (p >= (xtiles + 1) * (ytiles + 1)) {
				gLog("Error: Vertex index out of bounds\n");
				delete[] verts;
				return;
			}

			float h = map[p].h;
			if (h > 100000) h = pos.y;
			verts[p] = Vec3D(pos.x + tilesize * i, h, pos.z + ydir * tilesize * j);
		}
	}

	dlist = glGenLists(1);
	glNewList(dlist, GL_COMPILE);

	// TODO: handle light/dark liquid colors
	glNormal3f(0, 1, 0);
	glBegin(GL_QUADS);
	// draw tiles
	for (int j=0; j<ytiles; j++) {
		for (int i=0; i<xtiles; i++) {
			unsigned char f = flags[j*xtiles+i];
			if ((f&8)==0) {
				tmpflag = f;
				// 15 seems to be "don't draw"
				size_t p = j*(xtiles+1)+i;
				
				glTexCoord2f(i / texRepeats, j / texRepeats);
				glVertex3fv(verts[p]);
				
				glTexCoord2f((i+1) / texRepeats, j / texRepeats);
				glVertex3fv(verts[p+1]);
				
				glTexCoord2f((i+1) / texRepeats, (j+1) / texRepeats);
				glVertex3fv(verts[p+xtiles+1+1]);
				
				glTexCoord2f(i / texRepeats, (j+1) / texRepeats);
				glVertex3fv(verts[p+xtiles+1]);

			}
		}
	}
	glEnd();

	/*
	// debug triangles:
	//glColor4f(1,1,1,1);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glBegin(GL_TRIANGLES);
	for (int j=0; j<ytiles+1; j++) {
		for (int i=0; i<xtiles+1; i++) {
			size_t p = j*(xtiles+1)+i;
			Vec3D v = verts[p];
			//short s = *( (short*) (f.getPointer() + p*8) );
			//float f = s / 255.0f;
			//glColor4f(f,(1.0f-f),0,1);
			unsigned char c[4];
			c[0] = 255-map[p].c[3];
			c[1] = 255-map[p].c[2];
			c[2] = 255-map[p].c[1];
			c[3] = map[p].c[0];
			glColor4ubv(c);

			glVertex3fv(v + Vec3D(-0.5f, 1.0f, 0));
			glVertex3fv(v + Vec3D(0.5f, 1.0f, 0));
			glVertex3fv(v + Vec3D(0.0f, 2.0f, 0));
		}
	}
	glEnd();
	glColor4f(1,1,1,1);
	glEnable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	*/
	

	/*
	// temp: draw outlines
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_LINE_LOOP);
	Vec3D wx = Vec3D(tilesize*xtiles,0,0);
	Vec3D wy = Vec3D(0,0,tilesize*ytiles*ydir);
	glColor4f(1,0,0,1);
	glVertex3fv(pos);
	glColor4f(1,1,1,1);
	glVertex3fv(pos+wx);
	glVertex3fv(pos+wx+wy);
	glVertex3fv(pos+wy);
	glEnd();
	glEnable(GL_TEXTURE_2D);
	*/

	glEndList();
	delete[] verts;

	/*
	// only log .wmo
	if (ydir > 0) return;
	// LOGGING: debug info
	std::string slq;
	char buf[32];
	for (int j=0; j<ytiles+1; j++) {
		slq = "";
		for (int i=0; i<xtiles+1; i++) {
			//short ival[2];
			unsigned int ival;
			float fval;
			f.read(&ival, 4);
			f.read(&fval, 4);
			//sprintf(buf, "%4d,%4d ", ival[0],ival[1]);slq.append(buf);
			sprintf(buf, "%08x ", ival);slq.append(buf);
		}
		gLog("%s\n", slq.c_str());
	}
	slq = "";
	for (int i=0; i<ytiles*xtiles; i++) {
		unsigned char bval;
		f.read(&bval,1);
		if (bval==15) {
            sprintf(buf,"    ");			
		} else sprintf(buf, "%3d ", bval);
		slq.append(buf);
		if ( ((i+1)%xtiles) == 0 ) slq.append("\n");
	}
	gLog("%s",slq.c_str());
	*/
}

void Liquid::draw()
{
	// First validate the this pointer - if the memory pattern shows 0xCD, 
	// this is likely uninitialized memory
	if (this == nullptr ||
		reinterpret_cast<uintptr_t>(this) == 0xCDCDCDCD ||
		reinterpret_cast<uintptr_t>(this) > 0xFFFFFFFF) {
		return;
	}

	// Now validate our member variables before accessing them
	try {
		if (textures.empty() || !dlist) {
			return;
		}

		glDisable(GL_CULL_FACE);
		glDepthFunc(GL_LESS);

		size_t texidx = 0;
		if (gWorld) {
			texidx = (size_t)(gWorld->animtime / 60.0f) % textures.size();
		}

		glBindTexture(GL_TEXTURE_2D, textures[texidx]);

		const float tcol = trans ? 0.9f : 1.0f;
		if (trans) {
			glEnable(GL_BLEND);
			glDepthMask(GL_FALSE);
		}

		if (type == 0) {
			glColor4f(1, 1, 1, tcol);
		}
		else {
			if (type == 2 && gWorld && gWorld->skies) {
				col = gWorld->skies->colorSet[WATER_COLOR_LIGHT];
			}
			glColor4f(col.x, col.y, col.z, tcol);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
		}

		glCallList(dlist);

		if (type != 0) {
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
		glDepthFunc(GL_LEQUAL);
		glColor4f(1, 1, 1, 1);
		if (trans) {
			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);
		}
	}
	catch (...) {
		// If anything goes wrong during drawing, restore GL state
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
		glColor4f(1, 1, 1, 1);
	}
}

void Liquid::initTextures(char *basename, int first, int last)
{
	char buf[256];
	for (int i=first; i<=last; i++) {
		sprintf(buf, "%s.%d.blp", basename, i);
		textures.push_back(video.textures.add(buf));
	}
}

Liquid::~Liquid()
{
	// Early return if textures is empty or invalid
	if (!textures.size()) {
		return;
	}

	// Delete all textures
	for (size_t i = 0; i < textures.size(); i++) {
		video.textures.del(textures[i]);
	}
}
