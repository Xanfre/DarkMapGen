/* DarkMapGen is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * DarkMapGen is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with DarkMapGen.
 */

#ifdef _WIN32
#include <string.h>
#else
#include <string>
#endif
#include <stdio.h>
#include <math.h>
#ifdef _WIN32
#include <direct.h>
#elif defined(__unix__)
#include <unistd.h>
#define stricmp strcasecmp
#define _copysign copysign
#define _stat stat
#define _getcwd getcwd
#define _chdir chdir
#ifdef __linux__
#include <linux/limits.h>
#define MAX_PATH PATH_MAX
#endif
#endif
#ifdef _MSC_VER
#include <stddef.h>
#else
#include <cstdint>
#endif
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Scroll.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_File_Chooser.H> 
#include "Fl_Cursor_Shape.H"
#ifdef CUSTOM_FLTK
#include "Fl_Image_Surface.H"
#include "png.h"
#else
#include <FL/Fl_Image_Surface.H>
#include <png.h>
#endif
#ifdef __GNUC__
#define min fmin
#endif


/////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#define DIRSEP_STR "\\"
#else
#define DIRSEP_STR "/"
#endif

#ifndef TRUE
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int UINT;
typedef unsigned int BOOL;
#define FALSE 0
#define TRUE 1
#endif

/////////////////////////////////////////////////////////////////////

#define DARKMAPGEN_VERSION		"v1.01"

#define DARKMAPGEN_TITLE		"DarkMapGen"

#define PROJ_FILENAME			"DarkMapGen.proj"


// min and max zoom levels for image view (don't make max too large because scaled image drawing is crude,
// it creates a scaled copy of an image, dimmed location drawing also has scaled copies for each location)
#define MIN_ZOOM				1
#define MAX_ZOOM				6

// dark engine defined limits for max map pages and max locations per page
#define MAX_MAPS				40
#define MAX_LOCATIONS_PER_MAP	256

// max verts per shape
#define MAX_VERTS				128

#define VERT_HANDLE_RADIUS		3
#define VERT_RADIUS				1

#define VERT_COLOR				FL_WHITE
#define CUR_VERT_COLOR			FL_MAGENTA
#define VERT_FRAME_COLOR		FL_BLACK
#define CUR_SHAPE_LINE_COLOR	FL_GREEN
#define INPROGRESS_SHAPE_COLOR	FL_CYAN
#define SHAPE_LINE_COLOR		FL_YELLOW

#define SNAP_RADIUS				(VERT_HANDLE_RADIUS+3)


// transform a coordinate from original map coordinates to image view client space
#define MAP2CL(_coord) \
	((int)(_coord) * g_iZoom + g_iCTR)


// construct a tree id from a map page index and a location index/id
#define MAKE_TREE_ID(_mapidx, _locidx) \
	((((UINT)(_mapidx))<<16) | (UINT)(WORD)(_locidx))

// get map page index from a tree id (-1 if invalid)
#define MAP_FROM_TREE_ID(_treeid) \
	((int)(((UINT)(_treeid))>>16))

// get location index/id from a tree id (-1 if invalid)
#define LOCIDX_FROM_TREE_ID(_treeid) \
	((int)(short)((UINT)(_treeid)))

// get location array element index from tree id (-1 if invalid)
#define LOC_FROM_TREE_ID(_treeid) \
	(MAP_FROM_TREE_ID(_treeid) < 0) ? -1 : g_pProj->maps[MAP_FROM_TREE_ID(_treeid)].GetArrayIndexFromLocationIndex(LOCIDX_FROM_TREE_ID(_treeid))

#define SCALE(_x) _x * g_iScale / 4

#ifdef DEF_THEME
#define DEF_THEME_S(_s) DEF_THEME_S_(_s)
#define DEF_THEME_S_(_s) #_s
#endif

#define SETCLR(_x,_y) Fl::set_color((Fl_Color)_x,dark_cmap[_y++])

#ifdef __unix__
// work around bug in X11
#define EVENT_SHIFT() (g_iEventState&FL_SHIFT)
#define EVENT_CTRL() (g_iEventState&FL_CTRL)
#define EVENT_ALT() (g_iEventState&FL_ALT)
#else
#define EVENT_SHIFT() Fl::event_shift()
#define EVENT_CTRL() Fl::event_ctrl()
#define EVENT_ALT() Fl::event_alt()
#endif


enum DisplayMode
{
	DM_OUTLINES,		// shapes are drawn as outlines only
	DM_FILLSEL,			// the selected shape is filled with color, other shapes are drawn as outlines
	DM_FILLALL,			// all shapes are filled with color
	DM_DIMMED,			// all shapes are drawn with image data as darkened grayscale
	DM_FADE_NONSEL,		// alpha fade everything but the selected shape

	DM_NUM_MODES
};


/////////////////////////////////////////////////////////////////////

class cImageView;
struct sMap;
struct sLocation;

static void SetModifiedFlag();
static void ClearModifiedFlag();
static void GenerateLocationImageForView(const sMap &map, sLocation &loc, BOOL bDim = TRUE, const int AA = 4);
static void FakeTransparentImage(Fl_Image *img, Fl_Color bg, const UINT alpha = 48);
static void OnCmdDelete(Fl_Widget* = NULL, void* = NULL);
static void OnWindowResized(int w, int h);
static int GetScrollViewClientWidth();
static int GetScrollViewClientHeight();
static void ScrollImageTo(int Xzoomed, int Yzoomed);
static BOOL ChangeZoom(int n);


/////////////////////////////////////////////////////////////////////

#include "dmg_std.ptr"
#include "dmg_move.ptr"
#include "dmg_add.ptr"
#include "dmg_del.ptr"
#include "dmg_adddel.ptr"
#include "dmg_sel.ptr"
#include "dmg_lblpos.ptr"
#include "dmg_pan.ptr"

static Fl_Cursor_Shape g_cursStd;
static Fl_Cursor_Shape g_cursMove;
static Fl_Cursor_Shape g_cursAdd;
static Fl_Cursor_Shape g_cursDel;
static Fl_Cursor_Shape g_cursAddDel;
static Fl_Cursor_Shape g_cursLblPos;
static Fl_Cursor_Shape g_cursPan;

enum
{
	DMG_CURS_STD,
	DMG_CURS_MOVE,
	DMG_CURS_ADD,
	DMG_CURS_DEL,
	DMG_CURS_ADDDEL,
	DMG_CURS_LABELPOS,
	DMG_CURS_PAN,
};

static void InitCursors()
{
	g_cursStd.shape(dmg_std_hotX, dmg_std_hotY, dmg_std_and, dmg_std_xor);
	g_cursMove.shape(dmg_move_hotX, dmg_move_hotY, dmg_move_and, dmg_move_xor);
	g_cursAdd.shape(dmg_add_hotX, dmg_add_hotY, dmg_add_and, dmg_add_xor);
	g_cursDel.shape(dmg_del_hotX, dmg_del_hotY, dmg_del_and, dmg_del_xor);
	g_cursAddDel.shape(dmg_adddel_hotX, dmg_adddel_hotY, dmg_adddel_and, dmg_adddel_xor);
	g_cursLblPos.shape(dmg_lblpos_hotX, dmg_lblpos_hotY, dmg_lblpos_and, dmg_lblpos_xor);
	g_cursPan.shape(dmg_pan_hotX, dmg_pan_hotY, dmg_pan_and, dmg_pan_xor);
}


/////////////////////////////////////////////////////////////////////

struct sVertex
{
	short x;
	short y;
};

struct sShape
{
	sShape()
	{
		next = NULL;
		owner = NULL;
		bHole = FALSE;
		memset(iBoundRect, 0, sizeof(iBoundRect));
		iLabelPos[1] = iLabelPos[0] = 0;
		iVertCount = 0;
	}

	void CalcBoundingRect()
	{
		iBoundRect[2] = iBoundRect[0] = verts[0].x;
		iBoundRect[3] = iBoundRect[1] = verts[0].y;

		for (int i=1; i<iVertCount; i++)
		{
			if (verts[i].x < iBoundRect[0])
				iBoundRect[0] = verts[i].x;
			else if (verts[i].x > iBoundRect[2])
				iBoundRect[2] = verts[i].x;

			if (verts[i].y < iBoundRect[1])
				iBoundRect[1] = verts[i].y;
			else if (verts[i].y > iBoundRect[3])
				iBoundRect[3] = verts[i].y;
		}
	}

	// complex point in poly algorithm posted on the net by Ken McElvain

	/*
	 * Quadrants:
	 *    1 | 0
	 *    -----
	 *    2 | 3
	 */
	static inline int whichquad(const sVertex &pt, const sVertex &orig)
	{
		int quad;
		if (pt.x < orig.x)
		{
			if (pt.y < orig.y) quad = 2;
			else quad = 1;
		}
		else
		{
			if (pt.y < orig.y) quad = 3;
			else quad = 0;
		}
		return quad;
	}

	BOOL IsPosInShape(int x, int y) const
	{
		if (iVertCount < 3)
			return FALSE;

		// do a quick bound rect test
		if (x < iBoundRect[0] || x > iBoundRect[2] || y < iBoundRect[1] || y > iBoundRect[3])
			return FALSE;

		const sVertex* polypts = verts;
		const sVertex pt = { x, y };

		int wind = 0;
		sVertex lastpt = polypts[iVertCount-1];
		/* get starting angle */
		int oldquad = whichquad(lastpt, pt);

		for (int i=0; i<iVertCount; i++)
		{
			sVertex thispt = *polypts++;
			int newquad = whichquad(thispt, pt);

			if (oldquad != newquad)
			{
				/*
				* use mod 4 comparisons to see if we have
				* advanced or backed up one quadrant
				*/
				if (((oldquad + 1) & 3) == newquad)
					wind++;
				else if (((newquad + 1) & 3) == oldquad)
					wind--;
				else
				{
					/*
					* upper left to lower right, or
					* upper right to lower left. Determine
					* direction of winding  by intersection
					* with x==0.
					*/
					int a = (int)lastpt.y - (int)thispt.y;
					a *= ((int)pt.x - (int)lastpt.x);
					int b = (int)lastpt.x - (int)thispt.x;
					a += (int)lastpt.y * b;
					b *= (int)pt.y;

					if (a > b) wind += 2;
					else wind -= 2;
				}
				oldquad = newquad;
			}
			lastpt = thispt;
		}

		/* non-zero means point in poly */
		return wind;
	}

	// returns TRUE is subShape is this shape or a hole in it
	BOOL IsPartOf(const sShape &subShape) const
	{
		if (&subShape == this)
			return TRUE;

		if (!bHole)
			for (const sShape *p=next; p && p->bHole; p=p->next)
				if (p == &subShape)
					return TRUE;

		return FALSE;
	}

	void MovePos(int dx, int dy)
	{
		if (iVertCount <= 0)
			return;

		iBoundRect[0] += dx;
		iBoundRect[1] += dy;
		iBoundRect[2] += dx;
		iBoundRect[3] += dy;

		iLabelPos[0] += dx;
		iLabelPos[1] += dy;

		for (int i=0; i<iVertCount; i++)
		{
			verts[i].x += dx;
			verts[i].y += dy;
		}
	}

	sShape *next;
	sLocation *owner;

	BOOL bHole;

	int iBoundRect[4];
	int iLabelPos[2];

	int iVertCount;
	sVertex verts[MAX_VERTS];
};

struct sLocation
{
	sLocation()
	{
		img = NULL;
		iLocationIndex = 0;
		memset(iBoundRect, 0, sizeof(iBoundRect));
		shape = NULL;
	}
	~sLocation()
	{
		while (shape)
		{
			sShape *p = shape;
			shape = shape->next;
			p->next = NULL;
			delete p;
		}
	}

	void FlushScaledImages()
	{
		if (img)
		{
			delete img;
			img = NULL;
		}
	}

	void CalcBoundingRect()
	{
		iBoundRect[0] = INT_MAX;
		iBoundRect[1] = INT_MAX;
		iBoundRect[2] = INT_MIN;
		iBoundRect[3] = INT_MIN;

		for (sShape *p=shape; p; p=p->next)
		{
			p->CalcBoundingRect();

			if (p->iBoundRect[0] < iBoundRect[0])
				iBoundRect[0] = p->iBoundRect[0];
			if (p->iBoundRect[1] < iBoundRect[1])
				iBoundRect[1] = p->iBoundRect[1];
			if (p->iBoundRect[2] > iBoundRect[2])
				iBoundRect[2] = p->iBoundRect[2];
			if (p->iBoundRect[3] > iBoundRect[3])
				iBoundRect[3] = p->iBoundRect[3];
		}
	}

	// has more than one shape (a shape with holes counts as multiple)
	BOOL IsMultiShape() const { return shape && shape->next; }

	// has more than one solid shape (holes don't count)
	BOOL IsMultiSolidShape() const
	{
		if (shape && shape->next)
			for (const sShape *p=shape->next; p; p=p->next)
				if (!p->bHole)
					return TRUE;

		return FALSE;
	}

	BOOL IsPosInLocation(int x, int y) const
	{
		for (sShape *p=shape; p; p=p->next)
			if (!p->bHole && p->IsPosInShape(x, y))
			{
				// make sure it's not inside any hole of this shape
				for (p=p->next; p && p->bHole; p=p->next)
					if ( p->IsPosInShape(x, y) )
						goto next_solid;
				return TRUE;
next_solid:;
			}

		return FALSE;
	}

	sShape* GetShapeContainingPos(int x, int y) const
	{
		// return last shape containting pos since that will be on top in the drawing order

		sShape *ret = NULL;

		for (sShape *p=shape; p; p=p->next)
			if ( p->IsPosInShape(x, y) )
				ret = p;

		return ret;
	}

	BOOL IsVertPtrInLocation(const sVertex *pv) const
	{
		for (sShape *p=shape; p; p=p->next)
			if (pv >= p->verts && pv < p->verts+MAX_VERTS)
				return TRUE;

		return FALSE;
	}

	// determine is 'ps' is the only non-hole shape in this location
	BOOL IsOnlyNonHoleShape(const sShape *ps) const
	{
		if (!ps || ps->bHole || ps != shape)
			return FALSE;

		if (shape->next)
		{
			const sShape *p = shape->next;
			while (p && p->bHole)
				p = p->next;

			return !p;
		}

		return TRUE;
	}

	void MovePos(int dx, int dy)
	{
		if (!shape)
			return;

		FlushScaledImages();

		iBoundRect[0] += dx;
		iBoundRect[1] += dy;
		iBoundRect[2] += dx;
		iBoundRect[3] += dy;

		for (sShape *p=shape; p; p=p->next)
			p->MovePos(dx, dy);
	}

	void AddShape(const sShape &sh, BOOL bHole = FALSE)
	{
		sShape *p = new sShape(sh);

		p->owner = this;
		p->bHole = bHole;

		if (!shape)
			shape = p;
		else
		{
			// find last list element
			sShape *last;
			for (last=shape; last->next; last=last->next);

			last->next = p;
		}

		FlushScaledImages();
		CalcBoundingRect();
	}

	void AddShapeMaybeHole(const sShape &sh)
	{
		// determine if the added shape is a hole or not, by seeing if the first point is inside another shape
		sShape *p = GetShapeContainingPos(sh.verts[0].x, sh.verts[0].y);

		if (!p)
			AddShape(sh);
		else
		{
			// the new shape is a hole in 'p', insert the shape after 'p' in the list (as the last hole if it has several)

			sShape *hole = new sShape(sh);
			hole->owner = this;
			hole->bHole = TRUE;
			hole->iLabelPos[1] = hole->iLabelPos[0] = 0;

			for (; p->next && p->next->bHole; p=p->next);

			hole->next = p->next;
			p->next = hole;

			FlushScaledImages();
			CalcBoundingRect();
		}
	}

	void DeleteShape(sShape *pShape)
	{
		if (!pShape)
			return;

		// delete shape and holes belonging to the shape, if applicable

		const BOOL bDelHoles = !pShape->bHole;

		if (pShape == shape)
		{
			shape = pShape->next;
			pShape->next = NULL;
			delete pShape;

			if (bDelHoles)
			{
				while (shape && shape->bHole)
				{
					pShape = shape;
					shape = pShape->next;
					pShape->next = NULL;
					delete pShape;
				}
			}

			// make sure the first shape isn't a whole (never should be)
			if (shape && shape->bHole)
				shape->bHole = FALSE;
		}
		else
		{
			for (sShape *p=shape; p; p=p->next)
			{
				if (p->next == pShape)
				{
					p->next = pShape->next;
					pShape->next = NULL;
					delete pShape;

					if (bDelHoles)
					{
						while (p->next && p->next->bHole)
						{
							pShape = p->next;
							p->next = pShape->next;
							pShape->next = NULL;
							delete pShape;
						}
					}

					break;
				}
			}
		}

		FlushScaledImages();
		CalcBoundingRect();
	}

	void UpdateShapeOwnerPtrs()
	{
		// 'this' pointer changed due to location array reshuffling, update the owner pointer in the shapes
		for (sShape *p=shape; p; p=p->next)
			p->owner = this;
	}

	int iLocationIndex;

	// optional cached image of the location (used in some drawing modes)
	Fl_Image *img;
	int iImgViewPos[2];

	int iBoundRect[4];

	sShape *shape;
};

struct sMap
{
	sMap()
	{
		img = NULL;
		imgscaled = NULL;
		imgHilightSS2 = NULL;
		iLocationCount = 0;
	}
	~sMap()
	{
		if (img)
			delete img;
		if (imgscaled)
			delete imgscaled;
		if (imgHilightSS2)
			delete imgHilightSS2;
	}

	void FlushScaledImages()
	{
		if (imgscaled)
		{
			delete imgscaled;
			imgscaled = NULL;
		}

		for (int i=0; i<iLocationCount; i++)
			locs[i].FlushScaledImages();
	}

	int GetFreeLocationIndex() const
	{
		char used[MAX_LOCATIONS_PER_MAP] = {0};

		for (int i=0; i<iLocationCount; i++)
			used[locs[i].iLocationIndex] = 1;

		for (int i=0; i<MAX_LOCATIONS_PER_MAP; i++)
			if (!used[i])
				return i;

		return -1;
	}

	const sLocation* GetByLocationIndex(int iLocIdx) const
	{
		if (iLocIdx < 0)
			return NULL;

		for (int i=0; i<iLocationCount; i++)
			if (locs[i].iLocationIndex == iLocIdx)
				return &locs[i];

		return NULL;
	}

	sLocation* GetByLocationIndex(int iLocIdx)
	{
		if (iLocIdx < 0)
			return NULL;

		for (int i=0; i<iLocationCount; i++)
			if (locs[i].iLocationIndex == iLocIdx)
				return &locs[i];

		return NULL;
	}

	int GetArrayIndexFromLocationIndex(int iLocIdx) const
	{
		if (iLocIdx < 0)
			return -1;

		for (int i=0; i<iLocationCount; i++)
			if (locs[i].iLocationIndex == iLocIdx)
				return i;

		return -1;
	}

	void DeleteLocation(int iArrayIndex)
	{
		if (iArrayIndex < 0)
			return;

		locs[iArrayIndex].FlushScaledImages();
		locs[iArrayIndex].~sLocation();

		if (iArrayIndex < iLocationCount-1)
			memmove(locs + iArrayIndex, locs + iArrayIndex + 1, sizeof(locs[0]) * (iLocationCount - iArrayIndex - 1));

		iLocationCount--;

		locs[iLocationCount] = sLocation();

		for (int i=iArrayIndex; i<iLocationCount; i++)
			locs[i].UpdateShapeOwnerPtrs();
	}

	Fl_Image *img;
	Fl_Image *imgscaled;

	// only available in SS2 mode, used for generating two sets of location images
	// one for visited areas and one for hilighted area the player is currently in
	Fl_Image *imgHilightSS2;

	int iLocationCount;
	sLocation locs[MAX_LOCATIONS_PER_MAP];
};

struct sProject
{
	sProject()
	{
		sDir = NULL;
		iCurMap = 0;
		iMapCount = 0;
		bModified = FALSE;
	}
	~sProject()
	{
		if (sDir)
			free(sDir);
	}

	void FlushScaledImages()
	{
		for (int i=0; i<iMapCount; i++)
			maps[i].FlushScaledImages();
	}

	BOOL Load()
	{
		char s[MAX_PATH+32];
		sprintf(s, "%s" DIRSEP_STR PROJ_FILENAME, sDir);

		FILE *f = fopen(s, "r");
		if (!f)
			// assume no project file, means we're starting a new project
			return TRUE;

		BOOL ret = TRUE;

		sShape shape;
		char sCmd[4];
		int iCurPage = 0;

		sMap *pMap = &maps[0];
		sLocation *pLoc = &pMap->locs[0];

		while ( !feof(f) )
		{
			sCmd[0] = 0;
			fscanf(f, "%*[ ]");
			fscanf(f, "%3s ", sCmd);

			if ( feof(f) )
				break;

			if ( !stricmp(sCmd, "PAG") )
			{
				// start loading new page

				iCurPage = -999999;
				fscanf(f, "%d", &iCurPage);

				if ((UINT)iCurPage >= (UINT)MAX_MAPS)
				{
					ret = FALSE;
					break;
				}

				pMap = &maps[iCurPage];
				pLoc = &pMap->locs[0];
			}
			else if ( !stricmp(sCmd, "LOC") )
			{
				if (pMap->iLocationCount >= MAX_LOCATIONS_PER_MAP)
				{
					ret = FALSE;
					break;
				}

				int iVertCount = 0;
				double dLocIdx = 0;

				fscanf(f, "%lf %d", &dLocIdx, &iVertCount);

				// some silliness to also detect -0 vals as negative (used for hole shapes in location 0)
				BOOL bHole = _copysign(1.0, dLocIdx) < 0;

				const int iLocIdx = (int)(fabs(dLocIdx) + 0.1);

				if (iLocIdx >= MAX_LOCATIONS_PER_MAP || iVertCount < 3)
				{
					ret = FALSE;
					break;
				}

				BOOL bNewLoc = TRUE;

				// if location already exists then just add another sub-shape
				pLoc = pMap->GetByLocationIndex(iLocIdx);
				if (pLoc)
					bNewLoc = FALSE;
				else
				{
					pLoc = &pMap->locs[pMap->iLocationCount];
					pLoc->iLocationIndex = iLocIdx;
					// the first shape in a location can never be a hole
					bHole = FALSE;
				}

				shape.iVertCount = iVertCount;

				for (int i=0; i<iVertCount; i++)
				{
					if ( feof(f) )
					{
						ret = FALSE;
						break;
					}

					int x = -999999, y = -999999;
					fscanf(f, "%*[ ]");
					fscanf(f, "(%d %d)", &x, &y);

					if (y == -999999)
					{
						ret = FALSE;
						break;
					}

					shape.verts[i].x = x;
					shape.verts[i].y = y;
				}

				int x = -999999, y = -999999;
				fscanf(f, "%*[ ]");
				fscanf(f, "<%d %d>", &x, &y);
				if (y != -999999)
				{
					shape.iLabelPos[0] = x;
					shape.iLabelPos[1] = y;
				}
				else if (bHole)
				{
					shape.iLabelPos[0] = 0;
					shape.iLabelPos[1] = 0;
				}
				else
				{
					// default label offset to bounds center (can be outside for concave shapes, but the user can move it if desired)
					shape.CalcBoundingRect();
					shape.iLabelPos[0] = shape.iBoundRect[0] + ((shape.iBoundRect[2] - shape.iBoundRect[0]) / 2);
					shape.iLabelPos[1] = shape.iBoundRect[1] + ((shape.iBoundRect[3] - shape.iBoundRect[1]) / 2);
				}

				pLoc->AddShape(shape, bHole);

				if (bNewLoc)
					pMap->iLocationCount++;
			}
		}

		fclose(f);

		ClearModifiedFlag();

		return ret;
	}

	BOOL Save()
	{
		char s[MAX_PATH+32];
		sprintf(s, "%s" DIRSEP_STR PROJ_FILENAME, sDir);

		FILE *f = fopen(s, "w");
		if (!f)
		{
			fl_alert("Failed to open project file \"%s\" for saving", s);
			return FALSE;
		}

		// save all pages that have locations, even if they have no image currently, it could be that an image failed to load
		// or temporarily got lost, but wouldn't want to lose existing location data in that case
		for (int j=0; j<MAX_MAPS; j++)
		{
			const sMap &map = maps[j];

			if (!map.iLocationCount)
				continue;

			fprintf(f, "PAG %d\n", j);

			for (int i=0; i<map.iLocationCount; i++)
			{
				const sLocation &loc = map.locs[i];

				if (!loc.shape)
					continue;

				for (sShape *p=loc.shape; p; p=p->next)
				{
					if (p->bHole)
						fprintf(f, "LOC -%d %d", loc.iLocationIndex, p->iVertCount);
					else
						fprintf(f, "LOC %d %d", loc.iLocationIndex, p->iVertCount);

					for (int k=0; k<p->iVertCount; k++)
						fprintf(f, " (%d %d)", p->verts[k].x, p->verts[k].y);

					if (!p->bHole)
						fprintf(f, " <%d %d>", p->iLabelPos[0], p->iLabelPos[1]);

					fprintf(f, "\n");
				}
			}
		}

		fclose(f);

		ClearModifiedFlag();

		return TRUE;
	}

	char *sDir;

	int iCurMap;
	BOOL bModified;

	int iMapCount;
	sMap maps[MAX_MAPS];
};


/////////////////////////////////////////////////////////////////////

static char g_sAppTitle[MAX_PATH+128];
static char g_sAppTitleModified[MAX_PATH+128];

// TRUE if running in System Shock 2 mode
static BOOL g_bShockMaps = FALSE;

static int g_iLocationImageExtraBorder = 0;
static int g_iAlphaExportAA = 4;
static int g_iScale = 4;

static sProject *g_pProj = NULL;
static int g_iZoom = 2;
static DisplayMode g_displayMode = DM_OUTLINES;
static BOOL g_bFillNewShape = FALSE;
static int g_iLineWidth = 0;
static BOOL g_bDrawLabels = FALSE;
static BOOL g_bDrawCursorGuides = FALSE;
static BOOL g_bHideSelectedOutline = FALSE;

static int g_iCurSelTreeId = -1;
static Fl_Tree_Item *g_pCurSelTreeItem = NULL;
// pixel center offset in zoomed image view coords
static int g_iCTR = 0;

static Fl_Double_Window *g_pMainWnd = NULL;
static Fl_Scroll *g_pScrollView = NULL;
static cImageView *g_pImageView = NULL;
static Fl_Tree *g_pTreeView = NULL;
static Fl_Menu_Bar *g_pMenuBar = NULL;
static int g_iWplus, g_iHplus;
static BOOL g_bUserDefinedWindowSize = FALSE;

static BOOL g_bShowingFlInputDialog = FALSE;

static unsigned int dark_cmap[42] = {
#include "dark_cmap.h"
};

#ifdef __unix__
// work around bug in X11
static int g_iEventState = 0;
#endif


/////////////////////////////////////////////////////////////////////

// WARNING: ugly mess ahead for all the edit mode/state stuff

class cImageView : public Fl_Widget
{
public:
	enum EditMode
	{
		EM_CREATE,
		EM_MOVE,
		EM_ADD_DEL,
		EM_LABELPOS,
	};

public:
	cImageView(int X, int Y, int W, int H)
		: Fl_Widget(X, Y, W, H)
	{
		m_mode = EM_CREATE;
		m_iDragging = 0;
		m_bCreatingShape = FALSE;
		m_iMousePos[1] = m_iMousePos[0] = 0;
		m_iMousePosSnapped[1] = m_iMousePosSnapped[0] = 0;
		m_iCurCursor = DMG_CURS_STD;

		m_pEditShape = NULL;
		m_pEditVert = NULL;
		m_pHilightShape = NULL;
		m_pHilightVert = NULL;
		m_iInsEdge = -1;
		m_pLabelShape = NULL;
		m_bMaybeDeleteShape = FALSE;

		m_bPanning = FALSE;
		m_iPanRestoreCursor = 0;

		m_newLoc.shape = &m_newShape;
		m_newShape.owner = &m_newLoc;
	}

	virtual ~cImageView()
	{
		m_newLoc.shape = NULL;
	}

	int m_iCurCursor;
	void set_cursor(int n, BOOL bForce = FALSE)
	{
		if (!bForce && m_iCurCursor == n)
			return;

		m_iCurCursor = n;

		switch (n)
		{
		case -1: fl_cursor(FL_CURSOR_DEFAULT); break;
		case DMG_CURS_STD: fl_cursor(&g_cursStd); break;
		case DMG_CURS_MOVE: fl_cursor(&g_cursMove); break;
		case DMG_CURS_ADD: fl_cursor(&g_cursAdd); break;
		case DMG_CURS_DEL: fl_cursor(&g_cursDel); break;
		case DMG_CURS_ADDDEL: fl_cursor(&g_cursAddDel); break;
		case DMG_CURS_LABELPOS: fl_cursor(&g_cursLblPos); break;
		case DMG_CURS_PAN: fl_cursor(&g_cursPan); break;
		}
	}

	void UpdateEditMode()
	{
		if (m_iDragging || m_bPanning)
			return;

		EditMode newMode = EM_CREATE;

		const BOOL bOldMaybeDeleteShape = m_bMaybeDeleteShape;
		m_bMaybeDeleteShape = FALSE;

		if ( EVENT_CTRL() )
		{
			if (EVENT_ALT() && !m_bCreatingShape && LOCIDX_FROM_TREE_ID(g_iCurSelTreeId) >= 0)
			{
				newMode = EM_LABELPOS;

				// if the current location only has one label, then make that the labelshape without requiring mouse over
				int iMap = g_pProj->iCurMap;
				int iLoc = LOC_FROM_TREE_ID(g_iCurSelTreeId);
				if (iMap >= 0 && iLoc >= 0 && !g_pProj->maps[iMap].locs[iLoc].IsMultiSolidShape())
					m_pLabelShape = g_pProj->maps[iMap].locs[iLoc].shape;
			}
			else
				newMode = EM_MOVE;
		}
		else if ( EVENT_ALT() )
		{
			if (!m_bCreatingShape)
				newMode = EM_ADD_DEL;

			m_bMaybeDeleteShape = (newMode == EM_ADD_DEL) && EVENT_SHIFT();
		}
		/*else if ( EVENT_SHIFT() )
		{
		}*/

		if (m_bMaybeDeleteShape != bOldMaybeDeleteShape && newMode == EM_ADD_DEL)
			redraw();

		if (newMode == m_mode)
			return;

		if (m_mode == EM_LABELPOS)
			// leaving EM_LABELPOS mode
			m_pLabelShape = NULL;

		m_mode = newMode;

		switch (m_mode)
		{
		case EM_CREATE: set_cursor(DMG_CURS_STD); break;
		case EM_MOVE: set_cursor(DMG_CURS_MOVE); break;
		case EM_ADD_DEL:
			if (m_pHilightVert)
				set_cursor(DMG_CURS_DEL);
			else if (m_iInsEdge >= 0)
				set_cursor(DMG_CURS_ADD);
			else
				set_cursor(DMG_CURS_ADDDEL);
			break;
		case EM_LABELPOS: set_cursor(DMG_CURS_LABELPOS); break;
		}

		UpdateMouseOverCues();

		redraw();
	}

	void UpdateMousePos()
	{
		int X = Fl::event_x() - x();
		int Y = Fl::event_y() - y();

		m_iMousePos[0] = X;
		m_iMousePos[1] = Y;

		m_iMousePosSnapped[0] = X;
		m_iMousePosSnapped[1] = Y;

		// snapping if shift is pressed
		if (!EVENT_SHIFT() || g_pProj->iCurMap < 0 || m_mode == EM_LABELPOS)
			return;

		// "smart-snap (tm)", snap against any other shape vert if within snap radius to it
		// if no direct snappable vert is found then snap axially against any vertex in the currently edited shape

		sShape *pSnapShape = NULL;
		int iSnapVert = -1;

		if ( GetPosOverVert(X, Y, pSnapShape, iSnapVert) )
		{
			m_iMousePosSnapped[0] = MAP2CL(pSnapShape->verts[iSnapVert].x);
			m_iMousePosSnapped[1] = MAP2CL(pSnapShape->verts[iSnapVert].y);
			return;
		}

		if (!m_iDragging || (m_pEditVert && m_iDragging))
		{
			// no snap vert found, do axial alignment snapping to verts of current location

			const sMap &map = g_pProj->maps[g_pProj->iCurMap];

			const sLocation *pSnapLoc = m_bCreatingShape ? &m_newLoc : (m_pEditShape ? m_pEditShape->owner : NULL);
			if (!pSnapLoc)
			{
				int iLoc = LOC_FROM_TREE_ID(g_iCurSelTreeId);
				if (iLoc >= 0)
					pSnapLoc = &map.locs[iLoc];
			}

			if (pSnapLoc)
			{
				const sVertex *pHorzSnap = NULL;
				const sVertex *pVertSnap = NULL;

				const sLocation &loc = *pSnapLoc;

				for (sShape *p=loc.shape; p; p=p->next)
				{
					for (int i=0; i<p->iVertCount; i++)
					{
						const sVertex &vert = p->verts[i];

						if (&vert == m_pEditVert)
							continue;

						const int dx = abs(MAP2CL(vert.x) - X);
						if (dx < SNAP_RADIUS && (!pHorzSnap || dx < abs(MAP2CL(pHorzSnap->x) - X)))
							pHorzSnap = &vert;

						const int dy = abs(MAP2CL(vert.y) - Y);
						if (dy < SNAP_RADIUS && (!pVertSnap || dy < abs(MAP2CL(pVertSnap->y) - Y)))
							pVertSnap = &vert;
					}
				}

				if (pHorzSnap)
					m_iMousePosSnapped[0] = MAP2CL(pHorzSnap->x);

				if (pVertSnap)
					m_iMousePosSnapped[1] = MAP2CL(pVertSnap->y);
			}
		}
	}

	void UpdateMouseOverCues()
	{
		sVertex *prev = m_pHilightVert;
		m_pHilightVert = NULL;

		int iPrevEdge = m_iInsEdge;
		m_iInsEdge = -1;

		if (m_mode == EM_CREATE && m_bCreatingShape)
		{
			// if shape can be finalized by clicking on the first vertex then hilight it
			if (m_newShape.iVertCount > 2 && IsPosOverVert(m_iMousePos[0], m_iMousePos[1], m_newShape.verts[0]))
			{
				m_pHilightVert = &m_newShape.verts[0];
				m_pHilightShape = &m_newShape;
			}
		}
		else if (m_mode == EM_LABELPOS)
		{
			// if location has multiple labels then highlight the one the mouse is over
			int iMap = g_pProj->iCurMap;
			int iLoc = LOC_FROM_TREE_ID(g_iCurSelTreeId);
			if (iMap >= 0 && iLoc >= 0 && g_pProj->maps[iMap].locs[iLoc].IsMultiSolidShape())
			{
				const sShape *pPrevLabelShape = m_pLabelShape;
				m_pLabelShape = NULL;
				int iCurDist = INT_MAX, iDist;

				for (sShape *p=g_pProj->maps[iMap].locs[iLoc].shape; p; p=p->next)
					if (!p->bHole && IsPosOverLabel(m_iMousePos[0], m_iMousePos[1], *p, iDist) && iDist < iCurDist)
					{
						iCurDist = iDist;
						m_pLabelShape = p;
					}

				if (m_pLabelShape != pPrevLabelShape)
					redraw();
			}
		}
		else if (m_mode == EM_MOVE || (m_mode == EM_ADD_DEL && !m_bCreatingShape))
		{
			sLocation *pLoc = m_pEditShape ? m_pEditShape->owner : NULL;
			if (!pLoc)
			{
				int iMap = g_pProj->iCurMap;
				int iLoc = LOC_FROM_TREE_ID(g_iCurSelTreeId);
				if (iMap >= 0 && iLoc >= 0)
					pLoc = &g_pProj->maps[iMap].locs[iLoc];
			}

			if (pLoc)
			{
				sShape *p;
				int i;

				if ( GetPosOverVert(m_iMousePos[0], m_iMousePos[1], p, i, pLoc) )
				{
					m_pHilightVert = &p->verts[i];
					m_pHilightShape = p;
					pLoc = p->owner;
				}
			}

			if (m_mode == EM_ADD_DEL && pLoc && !m_pHilightVert)
			{
				// check if over an edge and display virtual insert point
				for (sShape *p=pLoc->shape; p; p=p->next)
				{
					for (int i=0; i<p->iVertCount; i++)
					{
						if ( IsPosOverEdge(m_iMousePos[0], m_iMousePos[1], p->verts[i], p->verts[(i + 1) % p->iVertCount], m_insVert) )
						{
							m_iInsEdge = i;
							m_pHilightShape = p;
							goto found_edge;
						}
					}
				}
found_edge:;
			}
		}

		if (!m_pHilightVert && m_iInsEdge < 0)
			m_pHilightShape = NULL;

		if (m_mode == EM_ADD_DEL)
		{
			if (m_pHilightVert)
				set_cursor(DMG_CURS_DEL);
			else if (m_iInsEdge >= 0)
				set_cursor(DMG_CURS_ADD);
			else
				set_cursor(DMG_CURS_ADDDEL);
		}

		if (m_pHilightVert != prev || m_iInsEdge != iPrevEdge || m_iInsEdge >= 0)
			redraw();
	}

	static BOOL IsPosOverVert(int Xclient, int Yclient, const sVertex &vtx)
	{
		const int D = SNAP_RADIUS;

		const int X = MAP2CL(vtx.x);
		const int Y = MAP2CL(vtx.y);

		return (Xclient >= X-D && Yclient >= Y-D && Xclient <= X+D && Yclient <= Y+D);
	}

	static BOOL IsPosOverEdge(int Xclient, int Yclient, const sVertex &v0, sVertex &v1, sVertex &pointOnEdge)
	{
		int dx = (v1.x - v0.x) * g_iZoom;
		int dy = (v1.y - v0.y) * g_iZoom;

		int lensq = dx*dx + dy*dy;

		if (lensq <= 1)
			return FALSE;

		int mdx = Xclient - MAP2CL(v0.x);
		int mdy = Yclient - MAP2CL(v0.y);

		float f = (float)(mdx * dx + mdy * dy) / (float)lensq;

		if (f <= 0.0f || f >= 1.0f)
			return FALSE;

		pointOnEdge.x = (MAP2CL(v0.x) + (int)((float)dx * f)) / g_iZoom;
		pointOnEdge.y = (MAP2CL(v0.y) + (int)((float)dy * f)) / g_iZoom;

		const int D = SNAP_RADIUS;

		dx = (Xclient / g_iZoom) - pointOnEdge.x;
		dy = (Yclient / g_iZoom) - pointOnEdge.y;

		return (dx*dx + dy*dy) <= D;
	}

	static BOOL IsPosOverLabel(int Xclient, int Yclient, const sShape &sh, int &iDistSq)
	{
		// a bit larger than actual label size (or half to be precisce, since these values are max distance from center)
		const int Dx = 16;
		const int Dy = 10;

		const int X = Xclient / g_iZoom;
		const int Y = Yclient / g_iZoom;

		iDistSq = (sh.iLabelPos[0]-X)*(sh.iLabelPos[0]-X) + (sh.iLabelPos[1]-Y)*(sh.iLabelPos[1]-Y);

		return (X >= sh.iLabelPos[0]-Dx && Y >= sh.iLabelPos[1]-Dy && X <= sh.iLabelPos[0]+Dx && Y <= sh.iLabelPos[1]+Dy);
	}

	static int DistSqToVert(const int Xclient, const int Yclient, const sVertex &vert)
	{
		const int dx = Xclient - MAP2CL(vert.x);
		const int dy = Yclient - MAP2CL(vert.y);
		return dx * dx + dy * dy;
	}

	BOOL GetPosOverVert(int Xclient, int Yclient, sShape *&pOverShape, int &iOverVert, const sLocation *pLoc = NULL) const
	{
		const int D = SNAP_RADIUS;

		pOverShape = NULL;
		iOverVert = -1;
		int iOverVertDist = INT_MAX;

		const int iLocs = pLoc ? 1 : g_pProj->maps[g_pProj->iCurMap].iLocationCount;

		for (int j=0; j<iLocs; j++)
		{
			sLocation &loc = pLoc ? *(sLocation*)pLoc : g_pProj->maps[g_pProj->iCurMap].locs[j];

			// do a quick bound rect test
			if ((Xclient < (loc.iBoundRect[0] * g_iZoom)-D || Xclient > (loc.iBoundRect[2] * g_iZoom + g_iZoom-1)+D
				|| Yclient < (loc.iBoundRect[1] * g_iZoom)-D || Yclient > (loc.iBoundRect[3] * g_iZoom + g_iZoom-1)+D)
				&& pLoc != &m_newLoc)
				continue;

			for (sShape *p=loc.shape; p; p=p->next)
				for (int i=0; i<p->iVertCount; i++)
					if ( IsPosOverVert(Xclient, Yclient, p->verts[i]) )
					{
						const int dist = DistSqToVert(Xclient, Yclient, p->verts[i]);
						if (dist < iOverVertDist)
						{
							iOverVert = i;
							pOverShape = p;
							iOverVertDist = dist;
						}
					}
		}

		return iOverVert >= 0;
	}

	virtual int handle(int ev)
	{
		switch (ev)
		{
		case FL_PUSH:
			UpdateMousePos();

			if (m_iDragging)
				return 1;

			if (m_bPanning)
			{
				if (Fl::event_button() == FL_LEFT_MOUSE)
				{
					m_iPanRefMousePos[0] = Fl::event_x();
					m_iPanRefMousePos[1] = Fl::event_y();
					m_iPanRefScrollPos[0] = g_pScrollView->xposition();
					m_iPanRefScrollPos[1] = g_pScrollView->yposition();
					m_iDragging = FL_LEFT_MOUSE;
				}
				return 1;
			}

			if (Fl::event_button() == FL_LEFT_MOUSE)
			{
				m_iDragging = FL_LEFT_MOUSE;

				if (m_mode == EM_CREATE)
				{
					if (!m_bCreatingShape)
					{
						take_focus();
						m_bCreatingShape = TRUE;
						m_newShape.iVertCount = 0;

						m_pEditShape = &m_newShape;

						AddShapePoint(m_iMousePosSnapped[0], m_iMousePosSnapped[1]);
					}
					else
					{
						if (m_pHilightVert == &m_newShape.verts[0] && m_newShape.iVertCount > 2)
						{
							// clicked on the first vertex to complete the shape loop
							CommitShapeAsNewLoc();
							return 1;
						}

						AddShapePoint(m_iMousePosSnapped[0], m_iMousePosSnapped[1]);
					}

					m_pHilightVert = m_pEditVert;
					m_pHilightShape = m_pEditShape;
				}
				else if (m_mode == EM_MOVE)
				{
					if (m_pHilightVert)
					{
						m_pEditVert = m_pHilightVert;
						m_pEditShape = m_pHilightShape;
					}
				}
				else if (m_mode == EM_ADD_DEL)
				{
					if (m_pHilightVert)
					{
						if ( DeleteShapePoint(m_pHilightShape, m_pHilightVert) )
						{
							m_pHilightShape = NULL;
							m_pHilightVert = NULL;
						}
						UpdateEditMode();
					}
					else if (!m_pHilightVert && m_iInsEdge >= 0 && m_pHilightShape)
					{
						InsertShapePoint();
						redraw();
					}
				}
				else if (m_mode == EM_LABELPOS)
				{
					UpdateLabelPos(m_iMousePos[0], m_iMousePos[1]);
				}
			}
			else if (Fl::event_button() == FL_RIGHT_MOUSE)
			{
				m_iDragging = FL_RIGHT_MOUSE;

				if (m_mode == EM_CREATE)
				{
					if (m_bCreatingShape)
						DeleteLastNewShapePoint();
					else
						SelectShapeFromPos(m_iMousePos[0], m_iMousePos[1]);
				}
			}
			else if (Fl::event_button() == FL_MIDDLE_MOUSE)
			{
				if (m_bCreatingShape)
				{
					if ( EVENT_ALT() )
						CommitSubShape();
					else
						CommitShapeAsNewLoc();
				}
			}
			return 1;

		case FL_RELEASE:
			if (Fl::event_button() != m_iDragging)
				return 1;

			m_iDragging = 0;
			m_pEditVert = NULL;
			if (!m_bCreatingShape)
				m_pEditShape = NULL;
			UpdateEditMode();
			UpdateMouseOverCues();
			redraw();
			return 1;

		case FL_DRAG:
			UpdateMousePos();

			if (m_bPanning)
			{
				int nx = m_iPanRefScrollPos[0] + (m_iPanRefMousePos[0] - Fl::event_x());
				int ny = m_iPanRefScrollPos[1] + (m_iPanRefMousePos[1] - Fl::event_y());

				ScrollImageTo(nx, ny);

				return 1;
			}

			if (m_pEditVert)
			{
				// drag-move the just created point until mouse button gets released
				UpdateCurShapePoint(m_iMousePosSnapped[0], m_iMousePosSnapped[1]);
				redraw();
			}
			else if (m_mode == EM_LABELPOS)
			{
				UpdateLabelPos(m_iMousePos[0], m_iMousePos[1]);
			}
			else if (m_bCreatingShape || g_bDrawCursorGuides)
				redraw();
			return 1;

		case FL_MOVE:
			UpdateMousePos();
			if (m_bPanning)
				return 1;
			if (m_bCreatingShape || g_bDrawCursorGuides)
				redraw();
			UpdateMouseOverCues();
			return 1;

		case FL_KEYDOWN:
			if (m_bPanning)
				return 1;

			switch ( Fl::event_key() )
			{
			case FL_Escape:
				if (m_bCreatingShape)
					AbortShape();
				return 1;

			case FL_Enter:
				if (m_bCreatingShape)
					CommitShapeAsNewLoc();
				return 1;

			case FL_Shift_L:
			case FL_Shift_R:
#ifdef __unix__
				g_iEventState |= FL_SHIFT;
#endif
				if (m_bCreatingShape)
				{
					UpdateMousePos();
					redraw();
					return 1;
				}
				UpdateEditMode();
				break;

			case FL_Control_L:
			case FL_Control_R:
#ifdef __unix__
				g_iEventState |= FL_CTRL;
#endif
				UpdateEditMode();
				return 1;

			case FL_Alt_L:
			case FL_Alt_R:
#ifdef __unix__
				g_iEventState |= FL_ALT;
#endif
				UpdateEditMode();
				return 1;

			case FL_Delete:
			case FL_BackSpace:
				if (m_bCreatingShape)
				{
					DeleteLastNewShapePoint();
					return 1;
				}
				break;

			case FL_Insert:
				if (m_bCreatingShape)
				{
					CommitSubShape();
					return 1;
				}
				break;

			case ' ':
				if (!m_iDragging && m_mode == EM_CREATE)
				{
					m_bPanning = TRUE;
					m_iPanRestoreCursor = m_iCurCursor;
					set_cursor(DMG_CURS_PAN);
					redraw();
				}
				break;
			}
			break;

		case FL_KEYUP:
			switch ( Fl::event_key() )
			{
			case FL_Shift_L:
			case FL_Shift_R:
#ifdef __unix__
				g_iEventState &= ~FL_SHIFT;
#endif
				if (m_bCreatingShape && !m_bPanning)
				{
					UpdateMousePos();
					redraw();
					return 1;
				}
				break;

#ifdef __unix__
			case FL_Control_L:
			case FL_Control_R:
				g_iEventState &= ~FL_CTRL;
				break;

			case FL_Alt_L:
			case FL_Alt_R:
				g_iEventState &= ~FL_ALT;
				break;
#endif

			case ' ':
				if (m_bPanning)
				{
					m_bPanning = FALSE;
					set_cursor(m_iPanRestoreCursor);
					UpdateEditMode();
					redraw();
				}
				break;
			}
			UpdateEditMode();
			break;

		case FL_MOUSEWHEEL:
			if (m_iDragging)
				return 1;
			if ( EVENT_ALT() )
			{
				// zoom around mouse cursor (maintain same orig map XY pos at cursor before and after zoom change)
				int Xclient = Fl::event_x() - g_pScrollView->x();
				int Yclient = Fl::event_y() - g_pScrollView->y();
				// if mouse is outside the client area then zoom around center
				if (Xclient < 0 || Yclient < 0 || Xclient >= g_pScrollView->w() || Yclient >= g_pScrollView->h())
				{
					Xclient = g_pScrollView->w() / 2;
					Yclient = g_pScrollView->h() / 2;
				}

				const int Xmap = (g_pScrollView->xposition() + Xclient) / g_iZoom;
				const int Ymap = (g_pScrollView->yposition() + Yclient) / g_iZoom;

				BOOL ret = FALSE;
				if (Fl::event_dy() < 0)
					ret = ChangeZoom(g_iZoom + 1);
				else if (Fl::event_dy() > 0)
					ret = ChangeZoom(g_iZoom - 1);

				if (ret)
					ScrollImageTo(Xmap * g_iZoom - Xclient, Ymap * g_iZoom - Yclient);

				return 1;
			}
			else if ( EVENT_CTRL() )
			{
				if ( g_pScrollView->hscrollbar.visible() )
				{
					const int e_dx = Fl::e_dx;
					Fl::e_dx = Fl::e_dy;

					g_pScrollView->hscrollbar.handle(FL_MOUSEWHEEL);

					Fl::e_dx = e_dx;
				}
				return 1;
			}
			break;

		case FL_FOCUS:
		case FL_UNFOCUS:
			return 1;

		case FL_ENTER:
			take_focus();
			set_cursor(m_iCurCursor, TRUE);
			return 1;

		case FL_LEAVE:
			fl_cursor(FL_CURSOR_DEFAULT);
			m_iDragging = 0;
			return 1;
		}

		return Fl_Widget::handle(ev);
	}

	virtual void draw()
	{
		if (g_pProj->iCurMap < 0)
		{
			fl_rectf(x(), y(), w(), h(), FL_DARK1);
			return;
		}

		fl_push_clip(x(), y(), w(), h());

		const int dx = x();
		const int dy = y();

		sMap &map = g_pProj->maps[g_pProj->iCurMap];

		// draw page image

		Fl_Image *img = map.img;

		if (map.img)
		{
			if (g_displayMode == DM_FADE_NONSEL)
			{
				if (!map.imgscaled)
				{
					map.imgscaled = img->copy(img->w() * g_iZoom, img->h() * g_iZoom);
					FakeTransparentImage(map.imgscaled, FL_DARK1);
				}

				img = map.imgscaled;
			}
			else if (g_iZoom != 1)
			{
				if (!map.imgscaled)
					map.imgscaled = img->copy(img->w() * g_iZoom, img->h() * g_iZoom);

				img = map.imgscaled;
			}

			img->draw(dx, dy);
		}
		else
			fl_rectf(dx, dy, w(), h(), FL_DARK1);

		// draw shapes (currently selected shape is drawn last to ensure it's not obstructed)

		const int AA = min(g_iAlphaExportAA, 4);

		int iCurSel = -1;

		// if shapes are filled then draw locations in two passes, first only the filling, then all outlines, handles and labels
		// to ensure that important visual cues are always visible and not hidden behind filled shapes
		const BOOL bFill = g_displayMode == DM_FILLALL;
		const BOOL bTwoPass = bFill || g_displayMode == DM_DIMMED;

		for (int pass = 0; pass < (bTwoPass ? 2 : 1); pass++)
		{
			const BOOL bFillThisPass = bFill ? (bTwoPass ? (pass ? FALSE : 2) : TRUE) : FALSE;

			for (int i=0; i<map.iLocationCount; i++)
			{
				const BOOL bCurSel = g_iCurSelTreeId == MAKE_TREE_ID(g_pProj->iCurMap, map.locs[i].iLocationIndex);
				if (bCurSel)
				{
					iCurSel = i;
					continue;
				}

				if (g_displayMode == DM_DIMMED && map.img)
				{
					if (!map.locs[i].img)
						GenerateLocationImageForView(map, map.locs[i], AA);

					if (map.locs[i].img)
						map.locs[i].img->draw(dx + map.locs[i].iImgViewPos[0], dy + map.locs[i].iImgViewPos[1]);
				}

				DrawShape(map.locs[i], SHAPE_LINE_COLOR, FALSE, TRUE, bFillThisPass, g_bDrawLabels);
			}
		}

		// currently selected shape
		if (iCurSel != -1)
		{
			const int i = iCurSel;
			const BOOL bFill = g_displayMode == DM_FILLALL || g_displayMode == DM_FILLSEL;

			BOOL bShowHandles = !m_bCreatingShape;

			if (bShowHandles && g_displayMode > DM_OUTLINES && g_bHideSelectedOutline)
				// show handles but no outlines
				bShowHandles = 2;

			if (g_displayMode >= DM_DIMMED && map.img)
			{
				if (!map.locs[i].img)
					GenerateLocationImageForView(map, map.locs[i], g_displayMode != DM_FADE_NONSEL, AA);

				if (map.locs[i].img)
					map.locs[i].img->draw(dx + map.locs[i].iImgViewPos[0], dy + map.locs[i].iImgViewPos[1]);
			}

			DrawShape(map.locs[i], CUR_SHAPE_LINE_COLOR, bShowHandles, TRUE, bFill, (m_mode == EM_LABELPOS) ? 2 : g_bDrawLabels);
		}

		// draw cross-hair guide lines
		if (g_bDrawCursorGuides && !m_bPanning && m_mode != EM_ADD_DEL)
		{
			// snap position to (center of) map pixels
			int X = MAP2CL(m_iMousePosSnapped[0] / g_iZoom);
			int Y = MAP2CL(m_iMousePosSnapped[1] / g_iZoom);

			fl_color( fl_color_average(FL_BLACK, INPROGRESS_SHAPE_COLOR, 0.25f) );
			fl_line_style(FL_DASH, 0);
			fl_line(dx, dy + Y, dx + w(), dy + Y);
			fl_line(dx + X, dy, dx + X, dy + h());
#ifndef _WIN32
			fl_line_style(0);
#endif
		}

		// draw in-progress shape
		if (m_bCreatingShape && m_newShape.iVertCount)
		{
			if (g_bFillNewShape && m_newShape.iVertCount > 1)
			{
				fl_color( fl_color_average(FL_BLACK, INPROGRESS_SHAPE_COLOR, 0.5f) );

				fl_begin_complex_polygon();
				for (int i=0; i<m_newShape.iVertCount; i++)
				{
					int X = dx + MAP2CL(m_newShape.verts[i].x);
					int Y = dy + MAP2CL(m_newShape.verts[i].y);

					fl_vertex((double)X, (double)Y);
				}

				fl_vertex((double)(dx + m_iMousePosSnapped[0]), (double)(dy + m_iMousePosSnapped[1]));

				fl_end_complex_polygon();
			}

			DrawShape(m_newShape, INPROGRESS_SHAPE_COLOR, TRUE, FALSE);

			if (m_mode == EM_CREATE)
			{
				// draw rubber-band from last vert to mouse
				fl_color(INPROGRESS_SHAPE_COLOR);
				fl_line_style(FL_DOT, g_iLineWidth);
				const int Xprev = dx + MAP2CL(m_newShape.verts[m_newShape.iVertCount-1].x);
				const int Yprev = dy + MAP2CL(m_newShape.verts[m_newShape.iVertCount-1].y);
				fl_line(Xprev, Yprev, dx+m_iMousePosSnapped[0], dy+m_iMousePosSnapped[1]);
				// draw rubber-band from mouse to first vert if there are at least 2 verts
				if (m_newShape.iVertCount >= 2)
				{
					const int Xfirst = dx + MAP2CL(m_newShape.verts[0].x);
					const int Yfirst = dy + MAP2CL(m_newShape.verts[0].y);
					fl_line(dx+m_iMousePosSnapped[0], dy+m_iMousePosSnapped[1], Xfirst, Yfirst);
				}
#ifndef _WIN32
				fl_line_style(0);
#endif
			}
		}

		// draw insert-point
		if (m_iInsEdge >= 0 && m_pHilightShape)
		{
			const int X = dx + MAP2CL(m_insVert.x);
			const int Y = dy + MAP2CL(m_insVert.y);

			const int rect[4] = { X-VERT_HANDLE_RADIUS, Y-VERT_HANDLE_RADIUS, VERT_HANDLE_RADIUS*2+1, VERT_HANDLE_RADIUS*2+1 };

			fl_rectf(rect[0]+1, rect[1]+1, rect[2]-2, rect[3]-2, INPROGRESS_SHAPE_COLOR);
			fl_rect(rect[0], rect[1], rect[2], rect[3], VERT_FRAME_COLOR);
		}

		fl_pop_clip();
	}

	void DrawShape(const sLocation &loc, Fl_Color linecolor, BOOL bShowHandles, BOOL bClosed = TRUE, BOOL bFilled = FALSE, BOOL bDrawLabel = FALSE)
	{
		const sShape *next = loc.shape;
		while (next)
			next = DrawShape(*next, linecolor, bShowHandles, bClosed, bFilled, bDrawLabel);
	}

	const sShape* DrawShape(const sShape &shape, Fl_Color linecolor, BOOL bShowHandles, BOOL bClosed = TRUE, BOOL bFilled = FALSE, BOOL bDrawLabel = FALSE)
	{
		const int dx = x();
		const int dy = y();

		if (bFilled && shape.iVertCount > 2)
		{
			fl_color( fl_color_average(FL_BLACK, linecolor, 0.5f) );

			fl_begin_complex_polygon();
			for (const sShape *p=&shape; p; p=p->next)
			{
				if (p->bHole && p != &shape)
					fl_gap();

				for (int i=0; i<p->iVertCount; i++)
				{
					const int X = dx + MAP2CL(p->verts[i].x);
					const int Y = dy + MAP2CL(p->verts[i].y);

					fl_vertex((double)X, (double)Y);
				}

				if (p->next && !p->next->bHole)
					break;
			}
			fl_end_complex_polygon();
		}

		if (bFilled == 2)
		{
			for (const sShape *p=&shape; p; p=p->next)
				if (p->next && !p->next->bHole)
					return p->next;

			return NULL;
		}

		if (bDrawLabel)
		{
			const int X = dx + MAP2CL(shape.iLabelPos[0]);
			const int Y = dy + MAP2CL(shape.iLabelPos[1]);

			char s[16];
			sprintf(s, "%03d", shape.owner->iLocationIndex);

			int tw, th;

			if (bDrawLabel == 2)
			{
				if (m_pLabelShape == &shape)
				{
					fl_font(FL_HELVETICA | FL_BOLD, FL_NORMAL_SIZE);
					fl_measure(s, tw, th);
					fl_rectf(X-(tw/2)-2, Y-(th/2)-2, tw+4, th+4, CUR_VERT_COLOR);
					fl_color(FL_BLACK);
				}
				else
				{
					fl_font(FL_HELVETICA | FL_BOLD, FL_NORMAL_SIZE);
					fl_measure(s, tw, th);
					fl_color(CUR_VERT_COLOR);
				}
			}
			else
			{
				fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
				fl_measure(s, tw, th);
				fl_color(linecolor);
			}
			fl_draw(s, X - (tw/2), Y - (th/2), tw, th, FL_ALIGN_LEFT|FL_ALIGN_TOP|FL_ALIGN_INSIDE);
		}

		int Xprev, Yprev;
		const sShape *ret = NULL;

		// drawing an open loop is only an option for the main shape, not holes (open loops are only drawn for the in-progress shape
		// which never is multi-shaped anyway)
		int n = bClosed ? shape.iVertCount + 1 : shape.iVertCount;

		for (const sShape *p=&shape; p; p=p->next)
		{
			// if vertex is hilighted for delete and the shape only has 3 verts, then also hilight all verts to indicate shape deletion
			// otherwise only hilight all if shape delete modifier key is pressed
			const BOOL bHilightAllVerts = (m_pHilightVert && m_mode == EM_ADD_DEL && m_pHilightShape && m_pHilightShape->IsPartOf(*p) &&
				(m_pHilightShape->iVertCount == 3 || m_bMaybeDeleteShape));

			if (p != &shape)
				n = p->iVertCount + 1;

			if (bShowHandles != 2)
			{
				for (int i=0; i<n; i++)
				{
					const int j = i % p->iVertCount;
					const int X = dx + MAP2CL(p->verts[j].x);
					const int Y = dy + MAP2CL(p->verts[j].y);

					if (!bShowHandles)
					{
						const int rect[4] = { X-VERT_RADIUS, Y-VERT_RADIUS, VERT_RADIUS*2+1, VERT_RADIUS*2+1 };

						fl_rectf(rect[0], rect[1], rect[2], rect[3], linecolor);
					}

					if (i)
					{
						fl_color(linecolor);
						fl_line_style(FL_DOT, g_iLineWidth);
						fl_line(Xprev, Yprev, X, Y);
#ifndef _WIN32
						fl_line_style(0);
#endif
					}

					Xprev = X;
					Yprev = Y;
				}
			}

			if (bShowHandles)
			{
				for (int i=0; i<n; i++)
				{
					const int j = i % p->iVertCount;
					const int X = dx + MAP2CL(p->verts[j].x);
					const int Y = dy + MAP2CL(p->verts[j].y);

					const int rect[4] = { X-VERT_HANDLE_RADIUS, Y-VERT_HANDLE_RADIUS, VERT_HANDLE_RADIUS*2+1, VERT_HANDLE_RADIUS*2+1 };

					const BOOL bHilightVert = m_pHilightVert == &p->verts[j] || bHilightAllVerts;

					fl_rectf(rect[0]+1, rect[1]+1, rect[2]-2, rect[3]-2, bHilightVert ? CUR_VERT_COLOR : VERT_COLOR);
					fl_rect(rect[0], rect[1], rect[2], rect[3], VERT_FRAME_COLOR);

					Xprev = X;
					Yprev = Y;
				}
			}

			if (p->next && !p->next->bHole)
			{
				ret = p->next;
				break;
			}
		}

		return ret;
	}

	void AddShapePoint(int mouse_x, int mouse_y)
	{
		if (m_newShape.iVertCount >= MAX_VERTS || !m_bCreatingShape)
			return;

		const int X = mouse_x / g_iZoom;
		const int Y = mouse_y / g_iZoom;

		m_newShape.verts[m_newShape.iVertCount].x = X;
		m_newShape.verts[m_newShape.iVertCount].y = Y;

		m_pEditVert = &m_newShape.verts[m_newShape.iVertCount];

		m_newShape.iVertCount++;
		m_newShape.CalcBoundingRect();

		redraw();
	}

	void UpdateCurShapePoint(int mouse_x, int mouse_y)
	{
		if (!m_pEditVert)
			return;

		const int X = mouse_x / g_iZoom;
		const int Y = mouse_y / g_iZoom;

		m_pEditVert->x = X;
		m_pEditVert->y = Y;

		m_pEditShape->owner->CalcBoundingRect();
		m_pEditShape->owner->FlushScaledImages();

		// if modifying an actual shape in the project then make sure modified flag is set
		if (m_pEditShape != &m_newShape)
			SetModifiedFlag();
	}

	void InsertShapePoint()
	{
		if (m_iInsEdge < 0 || !m_pHilightShape || m_pHilightShape->iVertCount >= MAX_VERTS)
			return;

		sShape &shape = *m_pHilightShape;

		if (m_iInsEdge < shape.iVertCount-1)
			memmove(shape.verts + m_iInsEdge + 2, shape.verts + m_iInsEdge + 1, sizeof(shape.verts[0]) * (shape.iVertCount - m_iInsEdge - 1));
		shape.verts[m_iInsEdge+1] = m_insVert;

		shape.iVertCount++;

		m_pHilightVert = &shape.verts[m_iInsEdge+1];
		m_pEditVert = m_pHilightVert;
		m_pEditShape = m_pHilightShape;
		m_iInsEdge = -1;

		shape.owner->CalcBoundingRect();
		shape.owner->FlushScaledImages();

		// if modifying an actual shape in the project then make sure modified flag is set
		if (m_pEditShape != &m_newShape)
			SetModifiedFlag();
	}

	void DeleteLastNewShapePoint()
	{
		if (!m_bCreatingShape)
			return;

		if (m_newShape.iVertCount > 1)
		{
			m_newShape.iVertCount--;
			m_newShape.CalcBoundingRect();
			redraw();
		}
		else
			AbortShape();
	}

	BOOL DeleteShapePoint(sShape *pShape, sVertex *pVert)
	{
		if (pShape->iVertCount <= 3 || m_bMaybeDeleteShape)
		{
			if (pShape == &m_newShape)
				return FALSE;

			// if deleting the last shape of the location then ask if location really should be deleted
			if ( pShape->owner->IsOnlyNonHoleShape(pShape) )
			{
				const int n = g_pProj->maps[g_pProj->iCurMap].iLocationCount;

				m_iDragging = 0;

				OnCmdDelete();

				if (g_pProj->maps[g_pProj->iCurMap].iLocationCount != n)
				{
					m_pEditVert = NULL;
					return TRUE;
				}

				return FALSE;
			}

			if ( pShape->owner->IsMultiShape() )
			{
				sLocation &loc = *pShape->owner;

				loc.DeleteShape(pShape);

				if (m_pEditVert && !loc.IsVertPtrInLocation(m_pEditVert))
					m_pEditVert = NULL;

				SetModifiedFlag();

				return TRUE;
			}

			return FALSE;
		}

		int iVert = -1;
		for (int i=0; i<pShape->iVertCount; i++)
			if (&pShape->verts[i] == pVert)
			{
				iVert = i;
				break;
			}

		if (iVert < 0)
			return FALSE;

		if (iVert < pShape->iVertCount-1)
			memmove(pShape->verts + iVert, pShape->verts + iVert + 1, sizeof(pShape->verts[0]) * (pShape->iVertCount - iVert - 1));

		pShape->iVertCount--;
		pShape->owner->CalcBoundingRect();
		pShape->owner->FlushScaledImages();

		if (m_pEditVert == pVert)
			m_pEditVert = NULL;

		// if modifying an actual shape in the project then make sure modified flag is set
		if (pShape != &m_newShape)
			SetModifiedFlag();

		return TRUE;
	}

	void CommitShapeAsNewLoc()
	{
		if (!m_bCreatingShape || m_newShape.iVertCount < 3)
			return;

		const int iNewLocIndex = g_pProj->maps[g_pProj->iCurMap].GetFreeLocationIndex();
		if (iNewLocIndex < 0)
			return;

		sMap &map = g_pProj->maps[g_pProj->iCurMap];

		sLocation &newLoc = map.locs[map.iLocationCount];
		map.iLocationCount++;

		// default label offset to bounds center (can be outside for concave shapes, but the user can move it if desired)
		m_newShape.CalcBoundingRect();
		m_newShape.iLabelPos[0] = m_newShape.iBoundRect[0] + ((m_newShape.iBoundRect[2] - m_newShape.iBoundRect[0]) / 2);
		m_newShape.iLabelPos[1] = m_newShape.iBoundRect[1] + ((m_newShape.iBoundRect[3] - m_newShape.iBoundRect[1]) / 2);

		newLoc.iLocationIndex = iNewLocIndex;
		newLoc.AddShape(m_newShape);

		g_pTreeView->begin();
		char s[64];
		sprintf(s, "PAGE%03d/%03d", g_pProj->iCurMap, iNewLocIndex);
		Fl_Tree_Item *p = g_pTreeView->add(s);
		p->user_data( (void*)MAKE_TREE_ID(g_pProj->iCurMap, iNewLocIndex) );
		g_pTreeView->end();
		g_pTreeView->select_only(p);
		g_pTreeView->set_item_focus(p);

		m_newShape.iVertCount = 0;
		m_pEditShape = NULL;
		m_pEditVert = NULL;
		m_pHilightVert = NULL;
		m_pHilightShape = NULL;
		m_bCreatingShape = FALSE;

		SetModifiedFlag();

		redraw();
	}

	void CommitSubShape()
	{
		if (!m_bCreatingShape || m_newShape.iVertCount < 3)
			return;

		const int iMap = g_pProj->iCurMap;
		const int iLoc = LOC_FROM_TREE_ID(g_iCurSelTreeId);

		if (iMap < 0 || iLoc < 0)
			return;

		sMap &map = g_pProj->maps[g_pProj->iCurMap];
		sLocation &loc = map.locs[iLoc];

		// default label offset to bounds center (can be outside for concave shapes, but the user can move it if desired)
		m_newShape.CalcBoundingRect();
		m_newShape.iLabelPos[0] = m_newShape.iBoundRect[0] + ((m_newShape.iBoundRect[2] - m_newShape.iBoundRect[0]) / 2);
		m_newShape.iLabelPos[1] = m_newShape.iBoundRect[1] + ((m_newShape.iBoundRect[3] - m_newShape.iBoundRect[1]) / 2);

		// auto-detect if shape is a hole, determined if the first shape point is inside another non-hole shape of the location
		loc.AddShapeMaybeHole(m_newShape);

		m_newShape.iVertCount = 0;
		m_pEditShape = NULL;
		m_pEditVert = NULL;
		m_pHilightVert = NULL;
		m_pHilightShape = NULL;
		m_bCreatingShape = FALSE;

		SetModifiedFlag();

		redraw();
	}

	void AbortShape()
	{
		m_newShape.iVertCount = 0;
		m_pEditShape = NULL;
		m_pEditVert = NULL;
		m_bCreatingShape = FALSE;

		redraw();
	}

	void UpdateLabelPos(int mouse_x, int mouse_y)
	{
		if (!m_pLabelShape)
			return;

		const int X = mouse_x / g_iZoom;
		const int Y = mouse_y / g_iZoom;

		m_pLabelShape->iLabelPos[0] = X;
		m_pLabelShape->iLabelPos[1] = Y;

		SetModifiedFlag();

		redraw();
	}

	void SelectShapeFromPos(int mouse_x, int mouse_y)
	{
		char s[64];

		const int X = mouse_x / g_iZoom;
		const int Y = mouse_y / g_iZoom;

		// support selection cycling if there are overlapping locations
		int iCurSelLoc = LOC_FROM_TREE_ID(g_iCurSelTreeId); 
		if (iCurSelLoc < 0)
			iCurSelLoc = 0;
		else
			iCurSelLoc++;

		for (int k=0, j=iCurSelLoc; k<g_pProj->maps[g_pProj->iCurMap].iLocationCount; k++, j++)
		{
			const int i = j % g_pProj->maps[g_pProj->iCurMap].iLocationCount;

			if ( g_pProj->maps[g_pProj->iCurMap].locs[i].IsPosInLocation(X, Y) )
			{
				sprintf(s, "PAGE%03d/%03d", g_pProj->iCurMap, g_pProj->maps[g_pProj->iCurMap].locs[i].iLocationIndex);
				Fl_Tree_Item *pItem = g_pTreeView->find_item(s);
				if (pItem)
				{
					g_pTreeView->select_only(pItem);
					g_pTreeView->set_item_focus(pItem);
					redraw();
				}

				return;
			}
		}

		// no shape found, select page (/ deselect current selection)
		sprintf(s, "PAGE%03d", g_pProj->iCurMap);
		Fl_Tree_Item *pItem = g_pTreeView->find_item(s);
		if (pItem)
		{
			g_pTreeView->select_only(pItem);
			g_pTreeView->set_item_focus(pItem);
			redraw();
		}
	}

	EditMode m_mode;

	int m_iDragging;

	// current mouse pos (in local widget coords)
	int m_iMousePos[2];
	int m_iMousePosSnapped[2];

	// in-progress drawn shape
	BOOL m_bCreatingShape;
	sShape m_newShape;
	sLocation m_newLoc;

	// current shape and vert being edited
	sShape *m_pEditShape;
	sVertex *m_pEditVert;

	sShape *m_pHilightShape;
	sVertex *m_pHilightVert;
	int m_iInsEdge;
	sVertex m_insVert;
	// current label that can be moved (only valid during EM_LABELPOS)
	sShape *m_pLabelShape;

	// TRUE when hovering over a vertex and holding down shift in EM_ADD_DEL
	// used to indicate that a vertex delete operation in this state will delete the entire shape
	// (this var should never be used if m_pHilightVert is NULL)
	BOOL m_bMaybeDeleteShape;

	BOOL m_bPanning;
	int m_iPanRestoreCursor;
	int m_iPanRefMousePos[2];
	int m_iPanRefScrollPos[2];
};


/////////////////////////////////////////////////////////////////////

class cMainWindow : public Fl_Double_Window
{
public:
	cMainWindow(int W, int H, const char *l = 0) : Fl_Double_Window(W, H, l) {}

	virtual void resize(int X, int Y, int W, int H)
	{
		Fl_Double_Window::resize(X, Y, W, H);

		OnWindowResized(W, H);
	}
};

class Fl_Tree_Ex : public Fl_Tree
{
public:
	Fl_Tree_Ex(int X, int Y, int W, int H) : Fl_Tree(X,Y,W,H) {}

	virtual int handle(int ev)
	{
		if (ev == FL_ENTER)
		{
			take_focus();
			return 1;
		}

		return Fl_Tree::handle(ev);
	}
};


/////////////////////////////////////////////////////////////////////

static BOOL LoadProject(const char *sDir)
{
	char s[MAX_PATH*2];
	char s2[MAX_PATH*2];

	if (g_pProj)
		delete g_pProj;

	if (g_bShockMaps == -1)
	{
		// auto-detect shock/thief mode
		g_bShockMaps = FALSE;

		struct _stat st;
		sprintf(s, "%s"DIRSEP_STR"page001a.png", sDir);
		if ( !_stat(s, &st) )
		{
			sprintf(s, "%s"DIRSEP_STR"page001a-hi.png", sDir);
			if ( !_stat(s, &st) )
				g_bShockMaps = TRUE;
		}
	}

	g_pProj = new sProject;

	g_pProj->sDir = strdup(sDir);

	// loads any map images found, without requiring a gap-free sequence
	for (int i=0; i<MAX_MAPS; i++)
	{
		if (g_bShockMaps)
			sprintf(s, "%s"DIRSEP_STR"page%03da.png", sDir, i);
		else
			sprintf(s, "%s"DIRSEP_STR"page%03d.png", sDir, i);

		Fl_PNG_Image *img = new Fl_PNG_Image(s);
		if ( !img->w() )
		{
			delete img;
			continue;
		}
		if (img->d() != 3 && img->d() != 4)
		{
			// will probably never happen, Fl_PNG_Image should load-convert to true-color
			fl_alert("Failed to load image \"%s\", not a 24- or 32-bit image", s);
			delete img;
			continue;
		}

		if (g_bShockMaps)
		{
			sprintf(s2, "%s"DIRSEP_STR"page%03da-hi.png", sDir, i);

			Fl_PNG_Image *imgHi = new Fl_PNG_Image(s2);
			if ( !imgHi->w() )
			{
				delete img;
				delete imgHi;
				continue;
			}
			if (imgHi->d() != 3 && imgHi->d() != 4)
			{
				// will probably never happen, Fl_PNG_Image should load-convert to true-color
				fl_alert("Failed to load image \"%s\", not a 24- or 32-bit image", s2);
				delete img;
				delete imgHi;
				continue;
			}
			if (imgHi->w() != img->w() || imgHi->h() != img->h())
			{
				// will probably never happen, Fl_PNG_Image should load-convert to true-color
				fl_alert("Failed to load map, image \"%s\" and \"%s\" are not the same size", s, s2);
				delete img;
				delete imgHi;
				continue;
			}

			g_pProj->maps[i].imgHilightSS2 = imgHi;
		}

		g_pProj->maps[i].img = img;
		g_pProj->iMapCount = i+1;
	}

	if ( !g_pProj->Load() )
		return FALSE;

	return TRUE;
}

static Fl_RGB_Image* GenerateLocationImage(const sMap &map, const sLocation &loc, int *pOutPos, const int iExtraBorder = 0, const int AA = 4)
{
	int brect[4] = { loc.iBoundRect[0], loc.iBoundRect[1], loc.iBoundRect[2], loc.iBoundRect[3] };

	if (iExtraBorder > 0)
	{
		brect[0] -= iExtraBorder;
		brect[1] -= iExtraBorder;
		brect[2] += iExtraBorder;
		brect[3] += iExtraBorder;

		if (brect[0] < 0) brect[0] = 0;
		if (brect[1] < 0) brect[1] = 0;
		if (brect[2] >= map.img->w()) brect[2] = map.img->w() - 1;
		if (brect[3] >= map.img->h()) brect[3] = map.img->h() - 1;
	}

	const int w = brect[2] - brect[0] + 1;
	const int h = brect[3] - brect[1] + 1;

	if (w <= 0 || h <= 0)
		return NULL;

	// xy offset for shape upper left corner
	const int xoffs = brect[0];
	const int yoffs = brect[1];

	if (pOutPos)
	{
		pOutPos[0] = xoffs;
		pOutPos[1] = yoffs;
	}

	// draw shape polygon into an off-screen surface to generate an alpha mask
	// (surface is AA times larger and then downscaled with boxfilter to get an anti-aliased alpha mask)

	Fl_Image_Surface *surf = new Fl_Image_Surface(w*AA, h*AA);

	surf->set_current();

	fl_push_clip(0, 0, w*AA, h*AA);

	fl_rectf(0, 0, w*AA, h*AA, FL_BLACK);

	fl_color(FL_WHITE);

	const double ctr = (double)AA / 2.0;

	const sShape *p = loc.shape;
	while (p)
	{
		fl_begin_complex_polygon();
		for (; p; p=p->next)
		{
			if (p->bHole && p != loc.shape)
				fl_gap();

			for (int i=0; i<p->iVertCount; i++)
			{
				int X = (p->verts[i].x - xoffs) * AA;
				int Y = (p->verts[i].y - yoffs) * AA;

				fl_vertex((double)X + ctr, (double)Y + ctr);
			}

			if (p->next && !p->next->bHole)
				break;
		}
		fl_end_complex_polygon();

		if (p)
			p = p->next;
	}

	fl_pop_clip();

	Fl_RGB_Image *alphamask = surf->image();

	delete surf;
	Fl_Display_Device::display_device()->set_current();

	// downscale alphamask to original size

	const BYTE *srcdata = (const BYTE *) alphamask->data()[0];
	const int Dsrc = alphamask->d();
	int iPitchSrc = w * AA * Dsrc + alphamask->ld();

	BYTE *data = new BYTE[w * h * 4];

	for (int y=0, ysrc=0; y<h; y++, ysrc+=AA)
	{
		for (int x=0, xsrc=0; x<w; x++, xsrc+=AA)
		{
			// get average alpha val for the current AAxAA sized box
			int iAlpha = 0;
			for (int yy=0; yy<AA; yy++)
				for (int xx=0; xx<AA; xx++)
					iAlpha += (int)(UINT)srcdata[(ysrc + yy) * iPitchSrc + (xsrc + xx) * Dsrc];
			iAlpha /= AA * AA;

			data[(y * w + x) * 4 + 3] = iAlpha;
		}
	}

	delete alphamask;

	// copy RGB components for brect from unscaled original map image

	srcdata = (const BYTE*) map.img->data()[0];
	iPitchSrc = map.img->w() * map.img->d() + map.img->ld();

	if (map.img->d() == 3)
	{
		for (int y=0; y<h; y++)
		{
			for (int x=0; x<w; x++)
			{
				const int i = (y * w + x) * 4;
				const int j = (y + yoffs) * iPitchSrc + (x + xoffs) * 3;

				data[i+0] = srcdata[j+0];
				data[i+1] = srcdata[j+1];
				data[i+2] = srcdata[j+2];
			}
		}
	}
	else if (map.img->d() == 4)
	{
		for (int y=0; y<h; y++)
		{
			for (int x=0; x<w; x++)
			{
				const int i = (y * w + x) * 4;
				const int j = (y + yoffs) * iPitchSrc + (x + xoffs) * 4;

				data[i+0] = srcdata[j+0];
				data[i+1] = srcdata[j+1];
				data[i+2] = srcdata[j+2];
			}
		}
	}

	// create image object

	Fl_RGB_Image *img = new Fl_RGB_Image(data, w, h, 4);

	return img;
}

// to be called directly after GenerateLocationImage to get a second variant of the image based on sMap::imgHilightSS2 instead
static Fl_RGB_Image* GenerateLocationImageHilightSS2(Fl_RGB_Image *pImgNonHi, const sMap &map, int xoffs, int yoffs)
{
	// copy RGB components for brect from unscaled original map image

	const int w = pImgNonHi->w();
	const int h = pImgNonHi->h();

	const BYTE *srcdata = (const BYTE*) map.imgHilightSS2->data()[0];
	const int iPitchSrc = map.imgHilightSS2->w() * map.imgHilightSS2->d() + map.imgHilightSS2->ld();

	Fl_RGB_Image *img = (Fl_RGB_Image*) pImgNonHi->copy();
	const int iPitchDst = img->w() * img->d() + img->ld();
	BYTE *data = (BYTE*) img->data()[0];

	if (map.imgHilightSS2->d() == 3)
	{
		for (int y=0; y<h; y++)
		{
			for (int x=0; x<w; x++)
			{
				const int i = y * iPitchDst + x * 4;
				const int j = (y + yoffs) * iPitchSrc + (x + xoffs) * 3;

				data[i+0] = srcdata[j+0];
				data[i+1] = srcdata[j+1];
				data[i+2] = srcdata[j+2];
			}
		}
	}
	else if (map.imgHilightSS2->d() == 4)
	{
		for (int y=0; y<h; y++)
		{
			for (int x=0; x<w; x++)
			{
				const int i = y * iPitchDst + x * 4;
				const int j = (y + yoffs) * iPitchSrc + (x + xoffs) * 4;

				data[i+0] = srcdata[j+0];
				data[i+1] = srcdata[j+1];
				data[i+2] = srcdata[j+2];
			}
		}
	}

	return img;
}

static void GenerateLocationImageForView(const sMap &map, sLocation &loc, BOOL bDim, const int AA)
{
	if (loc.img)
		loc.FlushScaledImages();

	Fl_RGB_Image *img = GenerateLocationImage(map, loc, loc.iImgViewPos, 0, AA);
	if (!img)
		return;

	if (bDim)
	{
		// desaturate and darken the image

		BYTE *data = (BYTE*) img->data()[0];
		const int iPitch = img->w() * 4 + img->ld();

		for (int y=0; y<img->h(); y++)
		{
			for (int x=0; x<img->w(); x++)
			{
				const int i = y * iPitch + x * 4;

				UINT clr = ((UINT)data[i] * 31 + (UINT)data[i+1] * 61 + (UINT)data[i+2] * 8) / 100;
				clr >>= 1;

				data[i+0] = (BYTE)clr;
				data[i+1] = (BYTE)clr;
				data[i+2] = (BYTE)clr;
			}
		}
	}

	// scale to view size if necessary
	if (g_iZoom != 1)
	{
		Fl_Image *scaled = img->copy(img->w() * g_iZoom, img->h() * g_iZoom);
		delete img;
		loc.img = scaled;
	}
	else
		loc.img = img;

	loc.iImgViewPos[0] *= g_iZoom;
	loc.iImgViewPos[1] *= g_iZoom;
}


static void FakeTransparentImage(Fl_Image *img, Fl_Color bg, const UINT alpha)
{
	if (!img || (img->d() != 4 && img->d() != 3))
		return;

	const UINT alpha_inv = 255 - alpha;

	BYTE rgb[3];
	Fl::get_color(bg, rgb[0], rgb[1], rgb[2]);

	const UINT r = (UINT)rgb[0] * alpha_inv;
	const UINT g = (UINT)rgb[1] * alpha_inv;
	const UINT b = (UINT)rgb[2] * alpha_inv;

	BYTE *data = (BYTE*) img->data()[0];
	const int D = img->d();
	const int iPitch = img->w() * D + img->ld();

	for (int y=0; y<img->h(); y++)
	{
		for (int x=0; x<img->w(); x++)
		{
			const int i = y * iPitch + x * D;

			data[i] = (BYTE)(((UINT)data[i] * alpha + r) / 255);
			data[i+1] = (BYTE)(((UINT)data[i+1] * alpha + g) / 255);
			data[i+2] = (BYTE)(((UINT)data[i+2] * alpha + b) / 255);
		}
	}
}


static void fwrite_png(png_structp pPng, png_bytep data, png_size_t length) { fwrite(data, 1, length, (FILE*)png_get_io_ptr(pPng)); }
static void fflush_png(png_structp pPng) { fflush( (FILE*)png_get_io_ptr(pPng) ); }
static png_voidp malloc_png(png_structp pPng, png_alloc_size_t size) { return malloc(size); }
static void free_png(png_structp pPng, png_voidp p) { free(p); }

#ifndef png_jmpbuf
#  define png_jmpbuf(pPng) ((pPng)->png_jmpbuf)
#endif

static BOOL SavePNG32(Fl_RGB_Image *img, char *sFileName)
{
	if ( !strchr(sFileName, '.') )
		strcat(sFileName, ".png");

	if (!img || img->d() != 4)
		return FALSE;

	FILE *f = fopen(sFileName, "wb");
	if (!f)
		return FALSE;

	png_structp pPng = png_create_write_struct_2(PNG_LIBPNG_VER_STRING,
		NULL, NULL, NULL,
		NULL, malloc_png, free_png);
	if (!pPng)
	{
		fclose(f);
		return FALSE;
	}

	png_infop pPngInfo = png_create_info_struct(pPng);
	if (!pPngInfo)
	{
		png_destroy_write_struct(&pPng, NULL);
		fclose(f);
		return FALSE;
	}

	if ( setjmp( png_jmpbuf(pPng) ) )
	{
		// exception thrown during read
		png_destroy_write_struct(&pPng, &pPngInfo);
		fclose(f);
		return NULL;
	}

	png_set_write_fn(pPng, (void*)f, fwrite_png, fflush_png);

	png_set_IHDR(pPng, pPngInfo, img->w(), img->h(), 8,
		PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
		PNG_FILTER_TYPE_BASE);

	png_bytep *pImgLines = new png_bytep[img->h()];

	const BYTE *srcdata = (const BYTE*) img->data()[0];
	const int iPitchSrc = img->w() * 4 + img->ld();
	for (int y=0; y<img->h(); y++)
		pImgLines[y] = (png_bytep)(srcdata + y * iPitchSrc);

	png_write_info(pPng, pPngInfo);
	png_write_image(pPng, pImgLines);
	png_write_end(pPng, pPngInfo);
	png_destroy_write_struct(&pPng, NULL);

	fclose(f);

	delete[] pImgLines;

	return TRUE;
}

static BOOL SaveTGA32(Fl_RGB_Image *img, char *sFileName)
{
	if ( !strchr(sFileName, '.') )
		strcat(sFileName, ".tga");

	if (!img || img->d() != 4)
		return FALSE;

	FILE *f = fopen(sFileName, "wb");
	if (!f)
		return FALSE;

	#pragma pack(1)
	struct sTgaHeader
	{
		BYTE id_len;
		BYTE col_map_type;
		BYTE img_type;
		WORD col_map_orig;
		WORD col_map_len;
		BYTE col_mat_bpp;
		WORD x_orig;
		WORD y_orig;
		WORD width;
		WORD height;
		BYTE bpp;
		BYTE img_descr;
	};
	#pragma pack()

	sTgaHeader hdr = {0};

	hdr.img_type = 2;
	hdr.bpp = 32;
	hdr.width = img->w();
	hdr.height = img->h();
	hdr.img_descr |= 0x20;

	fwrite(&hdr, sizeof(hdr), 1, f);

	const BYTE *srcdata = (const BYTE*) img->data()[0];
	const int iPitchSrc = img->w() * 4 + img->ld();

	for (int y=0; y<img->h(); y++)
	{
		const BYTE *line = srcdata + y * iPitchSrc;
		for (int x=0; x<img->w(); x++)
		{
			const BYTE pixel[4] = { line[x*4+2], line[x*4+1], line[x*4], line[x*4+3] };
			fwrite(pixel, 4, 1, f);
		}
	}

	fclose(f);

	return TRUE;
}

// save PNG or TGA 32-bit image
static BOOL SaveImg32(Fl_RGB_Image *img, char *sFileName, BOOL bTGA)
{
	return bTGA ? SaveTGA32(img, sFileName) : SavePNG32(img, sFileName);
}

static void GenerateFiles(BOOL bSaveTGA, int iGenerateMap = -1, int iGenerateLocIdx = -1)
{
	fl_cursor(FL_CURSOR_WAIT);

	int iGeneratedImages = 0;
	int iGeneratedPages = 0;
	BOOL bErrors = FALSE;
	char s[MAX_PATH+32];

	for (int i=0; i<g_pProj->iMapCount; i++)
	{
		const sMap &map = g_pProj->maps[i];

		if (!map.img || !map.iLocationCount || (iGenerateMap >= 0 && iGenerateMap != i))
			continue;

		iGeneratedPages++;

		// dark rects file containing the location positions (in index order)
		sprintf(s, "p%03dra.bin", i);
		FILE *f = fopen(s, "wb");
		if (!f)
		{
			fl_cursor(FL_CURSOR_DEFAULT);
			fl_alert("Failed to save rects file \"%s\"", s);
			fl_cursor(FL_CURSOR_WAIT);
			bErrors = TRUE;
		}

		FILE *f2 = NULL;
		if (g_bShockMaps)
		{
			sprintf(s, "p%03dxa.bin", i);
			FILE *f2 = fopen(s, "wb");
			if (!f2)
			{
				fl_cursor(FL_CURSOR_DEFAULT);
				fl_alert("Failed to save rects file \"%s\"", s);
				fl_cursor(FL_CURSOR_WAIT);
				bErrors = TRUE;
			}
		}

		for (int j=0; j<map.iLocationCount; j++)
		{
			const sLocation *pLoc = map.GetByLocationIndex(j);
			if (!pLoc)
			{
				// no defined location for this index, add dummy entry in rects file
				short darkRect[4] = {0};
				if (f)
					fwrite(darkRect, sizeof(darkRect), 1, f);
				if (f2)
					fwrite(darkRect, sizeof(darkRect), 1, f2);

				continue;
			}

			const sLocation &loc = *pLoc;

			int iImgPos[2];
			short darkRect[4];

			Fl_RGB_Image *img = GenerateLocationImage(map, loc, iImgPos, g_iLocationImageExtraBorder, g_iAlphaExportAA);
			if (img)
			{
				if (iGenerateLocIdx < 0 || iGenerateLocIdx == j)
				{
					if (g_bShockMaps)
						sprintf(s, "%s"DIRSEP_STR"p%03dx%03d", g_pProj->sDir, i, loc.iLocationIndex);
					else
						sprintf(s, "%s"DIRSEP_STR"p%03dr%03d", g_pProj->sDir, i, loc.iLocationIndex);

					if ( !SaveImg32(img, s, bSaveTGA) )
					{
						fl_cursor(FL_CURSOR_DEFAULT);
						fl_alert("Failed to save location image \"%s\"", s);
						fl_cursor(FL_CURSOR_WAIT);
						delete img;
						bErrors = TRUE;
						break;
					}

					if (g_bShockMaps)
					{
						Fl_RGB_Image *imgHi = GenerateLocationImageHilightSS2(img, map, iImgPos[0], iImgPos[1]);
						if (!imgHi)
						{
							fl_cursor(FL_CURSOR_DEFAULT);
							fl_alert("Failed to generate location hilight image %03d on PAG%03d", loc.iLocationIndex, i);
							fl_cursor(FL_CURSOR_WAIT);
							bErrors = TRUE;
							delete img;
							break;
						}

						sprintf(s, "%s"DIRSEP_STR"p%03dr%03d", g_pProj->sDir, i, loc.iLocationIndex);

						if ( !SaveImg32(imgHi, s, bSaveTGA) )
						{
							fl_cursor(FL_CURSOR_DEFAULT);
							fl_alert("Failed to save location image \"%s\"", s);
							fl_cursor(FL_CURSOR_WAIT);
							delete img;
							delete imgHi;
							bErrors = TRUE;
							break;
						}
					}
				}

				// save rect
				darkRect[0] = (short)iImgPos[0];
				darkRect[1] = (short)iImgPos[1];
				darkRect[2] = (short)(iImgPos[0] + img->w());
				darkRect[3] = (short)(iImgPos[1] + img->h());
				if (f)
					fwrite(darkRect, sizeof(darkRect), 1, f);
				if (f2)
					fwrite(darkRect, sizeof(darkRect), 1, f2);

				delete img;

				iGeneratedImages++;
			}
			else
			{
				fl_cursor(FL_CURSOR_DEFAULT);
				fl_alert("Failed to generate location image %03d on PAG%03d", loc.iLocationIndex, i);
				fl_cursor(FL_CURSOR_WAIT);
				bErrors = TRUE;
				break;
			}
		}

		if (f)
			fclose(f);
		if (f2)
			fclose(f2);
	}

	fl_cursor(FL_CURSOR_DEFAULT);

	if (bErrors)
		fl_alert("Errors occurred, generated files are icomplete");
	else
	{
		if (iGenerateLocIdx >= 0)
			fl_message("Generated files for location %03d on PAGE%03d", iGenerateLocIdx, iGenerateMap);
		else if (iGenerateMap >= 0)
			fl_message("Generated files for %d location(s) on PAGE%03d", iGeneratedImages, iGenerateMap);
		else
			fl_message("Generated files for %d locations on %d page(s)", iGeneratedImages, iGeneratedPages);
	}
}


/////////////////////////////////////////////////////////////////////

static void SetModifiedFlag()
{
	if (!g_pProj->bModified)
	{
		g_pProj->bModified = TRUE;

		if (g_pMainWnd)
			g_pMainWnd->label(g_sAppTitleModified);
	}
}

static void ClearModifiedFlag()
{
	if (g_pProj->bModified)
	{
		g_pProj->bModified = FALSE;

		if (g_pMainWnd)
			g_pMainWnd->label(g_sAppTitle);
	}
}

static void OnWindowResized(int w, int h)
{
	const int W = w - g_iWplus;
	const int H = h - g_iHplus;

	g_pMenuBar->size(W + g_iWplus, g_pMenuBar->h());

	g_pScrollView->size(W, H);

	if (g_pProj->maps[g_pProj->iCurMap].img)
		g_pImageView->size(g_pProj->maps[g_pProj->iCurMap].img->w() * g_iZoom, g_pProj->maps[g_pProj->iCurMap].img->h() * g_iZoom);
	else
		g_pImageView->size(640 * g_iZoom, 480 * g_iZoom);

	g_pTreeView->resize(W, g_pTreeView->y(), g_pTreeView->w(), H);

	// if scroll pos is out of range (can happen when zooming out) then clamp scroll pos
	int x = g_pScrollView->xposition();
	int y = g_pScrollView->yposition();
	if (x > g_pImageView->w() - g_pScrollView->w())
		x = g_pImageView->w() - g_pScrollView->w();
	if (y > g_pImageView->h() - g_pScrollView->h())
		y = g_pImageView->h() - g_pScrollView->h();
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	g_pScrollView->scroll_to(x, y);

	g_pTreeView->redraw();
	g_pImageView->redraw();
	g_pScrollView->redraw();
}

static void ChangeMap(int n, BOOL bZoomChangeOnly = FALSE)
{
	if (n >= g_pProj->iMapCount)
	{
		// should never happen
		g_pProj->iCurMap = -1;
		return;
	}

	g_pProj->FlushScaledImages();

	g_pProj->iCurMap = n;

	g_iCTR = (g_iZoom - 1) / 2;

	if (!bZoomChangeOnly)
		g_pScrollView->scroll_to(0, 0);

	OnWindowResized(g_pMainWnd->w(), g_pMainWnd->h());
}

static BOOL ChangeZoom(int n)
{
	if (n < MIN_ZOOM)
		n = MIN_ZOOM;
	else if (n > MAX_ZOOM)
		n = MAX_ZOOM;

	if (g_iZoom == n)
		return FALSE;

	g_iZoom = n;

	g_pProj->FlushScaledImages();

	ChangeMap(g_pProj->iCurMap, TRUE);

	return TRUE;
}

static const char* fl_input_ex(const char *label, char *deflt)
{
	g_bShowingFlInputDialog = TRUE;

	const char *ret = fl_input(label, deflt);

	g_bShowingFlInputDialog = FALSE;

	return ret;
}

static void PopulateTree()
{
	char s[128];

	g_pTreeView->clear();
	g_iCurSelTreeId = -1;
	g_pCurSelTreeItem = NULL;

	if (!g_pProj->iMapCount)
		return;

	g_pTreeView->begin();

	for (int i=0; i<g_pProj->iMapCount; i++)
	{
		if (!g_pProj->maps[i].img)
			continue;

		sprintf(s, "PAGE%03d", i);
		Fl_Tree_Item *p = g_pTreeView->add(s);
		p->user_data((void*)MAKE_TREE_ID(i, -1));
		p->labelfont(FL_HELVETICA | FL_BOLD);

		for (int j=0; j<g_pProj->maps[i].iLocationCount; j++)
		{
			sLocation &loc = g_pProj->maps[i].locs[j];

			sprintf(s, "PAGE%03d/%03d", i, loc.iLocationIndex);
			p = g_pTreeView->add(s);
			p->user_data((void*)MAKE_TREE_ID(i, loc.iLocationIndex));
		}
	}

	g_pTreeView->end();

	Fl_Tree_Item *first = g_pTreeView->next( g_pTreeView->first() );
	g_pTreeView->select_only(first);
	g_pTreeView->set_item_focus(first);
}

static void OnTreeSelChange(Fl_Widget*, void*)
{
	Fl_Tree_Item *p = g_pTreeView->first_selected_item();
	if (!p)
	{
		g_iCurSelTreeId = -1;
		g_pCurSelTreeItem = NULL;
		g_pImageView->redraw();
		return;
	}

	g_iCurSelTreeId = (intptr_t)p->user_data();
	g_pCurSelTreeItem = p;

	int iMap = MAP_FROM_TREE_ID(g_iCurSelTreeId);

	if (iMap != g_pProj->iCurMap)
		ChangeMap(iMap);
	else
		g_pImageView->redraw();
}

static void OnMainWndClose(Fl_Widget*, void*)
{
	// prevent escape from closing app
	if (Fl::event() == FL_SHORTCUT && Fl::event_key() == FL_Escape)
		return;

	if (!g_pProj->bModified || !fl_choice("Any unsaved changes will be lost. Exit?", "Yes", "No", NULL))
		g_pMainWnd->hide();
}

static void OnCmdExit(Fl_Widget*, void*)
{
	OnMainWndClose(NULL, NULL);
}

static void OnCmdSave(Fl_Widget*, void*)
{
	g_pProj->Save();
}

static void OnCmdGenerateFiles(Fl_Widget*, void*)
{
	for (int i=0; i<g_pProj->iMapCount; i++)
		if (g_pProj->maps[i].iLocationCount)
			goto has_locations;

	fl_message("No locations have been defined, nothing to generate.");
	return;

has_locations:

	int res = fl_choice("Generate files for all map locations.\nExisting files will be overwritten. Proceed?", "Cancel", "OK", "Generate as TGA");
	if (!res)
		return;

	const BOOL bSaveTGA = (res == 2);

	GenerateFiles(bSaveTGA);
}

static void OnCmdGenerateSelected(Fl_Widget*, void*)
{
	int iMap = g_pProj->iCurMap;
	int iLoc = LOC_FROM_TREE_ID(g_iCurSelTreeId);
	int iLocIdx = LOCIDX_FROM_TREE_ID(g_iCurSelTreeId);

	if (iMap < 0 && iLoc < 0)
		return;

	int res;

	if (iLoc < 0)
	{
		if (!g_pProj->maps[iMap].iLocationCount)
		{
			fl_message("No locations have been defined on PAGE%03d, nothing to generate.", iMap);
			return;
		}

		res = fl_choice("Generate files for PAGE%03d.\nExisting files will be overwritten. Proceed?", "Cancel", "OK", "Generate as TGA", iMap);
	}
	else
		res = fl_choice("Generate files for location %03d on PAGE%03d.\nExisting files will be overwritten. Proceed?", "Cancel", "OK", "Generate as TGA", iLocIdx, iMap);

	if (!res)
		return;

	const BOOL bSaveTGA = (res == 2);

	GenerateFiles(bSaveTGA, iMap, iLocIdx);
}

static void OnCmdDelete(Fl_Widget*, void*)
{
	if (!g_pCurSelTreeItem || g_pImageView->m_iDragging || g_pImageView->m_bPanning)
		return;

	int iMap = g_pProj->iCurMap;
	int iLoc = LOC_FROM_TREE_ID(g_iCurSelTreeId);
	int iLocIndex = LOCIDX_FROM_TREE_ID(g_iCurSelTreeId);

	if (iMap < 0)
		return;

	if (iLocIndex < 0)
	{
		// page item selected in tree, ask if all shapes should be deleted

		if (!g_pProj->maps[iMap].iLocationCount)
			return;

		if ( !fl_choice("Delete all locations on PAGE%03d?", "Cancel", "OK", NULL, iMap) )
			return;

		if ( !fl_choice("Are you really sure that you want to delete all locations on PAGE%03d?", "Cancel", "OK", NULL, iMap) )
			return;

		g_pProj->maps[iMap].FlushScaledImages();
		g_pProj->maps[iMap].iLocationCount = 0;
		SetModifiedFlag();

		// delete all child tree items for page
		while ( g_pCurSelTreeItem->children() )
			g_pTreeView->remove( g_pCurSelTreeItem->child(0) );
	}
	else
	{
		if ( !fl_choice("Delete location %03d on PAGE%03d?", "Cancel", "OK", NULL, iLocIndex, iMap) )
			return;

		g_pProj->maps[iMap].DeleteLocation(iLoc);
		SetModifiedFlag();

		// delete tree item
		{
			// get next item on same page to be selected after deletion
			Fl_Tree_Item *pNewSel = g_pCurSelTreeItem->next_sibling();
			// if the last item was deleted then move selection to prev instead
			if (!pNewSel)
			{
				pNewSel = g_pCurSelTreeItem->prev_sibling();
				// if there's no prev either then select the page
				if (!pNewSel)
					pNewSel = g_pCurSelTreeItem->parent();
			}

			g_pTreeView->remove(g_pCurSelTreeItem);
			g_iCurSelTreeId = -1;
			g_pCurSelTreeItem = NULL;

			// select new tree item
			if (pNewSel)
			{
				g_pTreeView->select_only(pNewSel);
				g_pTreeView->set_item_focus(pNewSel);
			}
		}
	}

	g_pTreeView->redraw();
	g_pImageView->redraw();
}

static void OnCmdEditIndex(Fl_Widget*, void*)
{
	if (g_pImageView->m_iDragging || g_pImageView->m_bPanning)
		return;

	int iMap = g_pProj->iCurMap;
	int iLoc = LOC_FROM_TREE_ID(g_iCurSelTreeId);

	if (iLoc < 0)
		return;

	char s[64];
	sprintf(s, "%d", g_pProj->maps[iMap].locs[iLoc].iLocationIndex);

retry:

	const char *t = fl_input_ex("New Index", s);
	if (!t)
		return;

	const int iNewIndex = atoi(t);

	if (iNewIndex < 0 || iNewIndex > MAX_LOCATIONS_PER_MAP-1)
	{
		fl_alert("Invalid index, must be a value in the range 0 to %d", MAX_LOCATIONS_PER_MAP-1);
		goto retry;
	}

	if (iNewIndex == g_pProj->maps[iMap].locs[iLoc].iLocationIndex)
		return;

	sLocation *pPrevLoc = g_pProj->maps[iMap].GetByLocationIndex(iNewIndex);
	if (pPrevLoc)
	{
		if ( !fl_choice("Another location with index %03d already exists. Swap indices?", "Cancel", "OK", NULL, iNewIndex) )
			return;

		pPrevLoc->iLocationIndex = g_pProj->maps[iMap].locs[iLoc].iLocationIndex;
	}

	// change tree item

	g_pProj->maps[iMap].locs[iLoc].iLocationIndex = iNewIndex;
	SetModifiedFlag();

	if (pPrevLoc)
	{
		// indices were swapped, ju re-select the new name

		g_iCurSelTreeId = -1;
		g_pCurSelTreeItem = NULL;

		sprintf(s, "PAGE%03d/%03d", iMap, iNewIndex);
		Fl_Tree_Item *pItem = g_pTreeView->find_item(s);
		if (pItem)
		{
			g_pTreeView->select_only(pItem);
			g_pTreeView->set_item_focus(pItem);
		}
	}
	else
	{
		// rename tree item

		g_pTreeView->remove(g_pCurSelTreeItem);

		g_iCurSelTreeId = -1;
		g_pCurSelTreeItem = NULL;

		sprintf(s, "PAGE%03d/%03d", iMap, iNewIndex);
		g_pTreeView->begin();
		Fl_Tree_Item *pItem = g_pTreeView->add(s);
		pItem->user_data( (void*)MAKE_TREE_ID(iMap, iNewIndex) );
		g_pTreeView->end();
		g_pTreeView->select_only(pItem);
		g_pTreeView->set_item_focus(pItem);
	}

	g_pImageView->redraw();
}

static void OnCmdMove(Fl_Widget*, void*)
{
	if (g_pImageView->m_iDragging || g_pImageView->m_bPanning)
		return;

	int iMap = g_pProj->iCurMap;
	int iLocIndex = LOCIDX_FROM_TREE_ID(g_iCurSelTreeId);
	int iLoc = LOC_FROM_TREE_ID(g_iCurSelTreeId);

	if (iMap < 0)
		return;

	if (iLocIndex < 0)
	{
		if (!g_pProj->maps[iMap].iLocationCount)
		{
			fl_message("No locations on this page");
			return;
		}

		if ( !fl_choice("Move all locations on PAGE%03d?", "Cancel", "OK", NULL, iMap) )
			return;

		if ( !fl_choice("Are you really sure that you want to move all locations on PAGE%03d?", "Cancel", "OK", NULL, iMap) )
			return;
	}
	// should never happen
	else if (iLoc < 0)
		return;

	// static so dialog defaults to previously used vals
	static char s[256] = "0 0";

	const char *t = fl_input_ex("Move Delta (dX dY)", s);
	if (!t)
		return;

	int dx = 0, dy = 0;
	sscanf(t, "%d %d", &dx, &dy);

	sprintf(s, "%d %d", dx, dy);

	if (!dx && !dy)
		return;

	if (abs(dx) > 4096 || abs(dy) > 4096)
		return;

	if (iLocIndex < 0)
	{
		for (int i=0; i<g_pProj->maps[iMap].iLocationCount; i++)
			g_pProj->maps[iMap].locs[i].MovePos(dx, dy);
	}
	else
		g_pProj->maps[iMap].locs[iLoc].MovePos(dx, dy);

	SetModifiedFlag();
	g_pImageView->redraw();
}

static void OnCmdInfo(Fl_Widget*, void *)
{
	int iMap = g_pProj->iCurMap;
	int iLoc = LOC_FROM_TREE_ID(g_iCurSelTreeId);

	if (iMap < 0 || iLoc < 0)
		return;

	const sLocation &loc = g_pProj->maps[iMap].locs[iLoc];

	int brect[4] = { loc.iBoundRect[0], loc.iBoundRect[1], loc.iBoundRect[2], loc.iBoundRect[3] };
	if (g_iLocationImageExtraBorder > 0)
	{
		brect[0] -= g_iLocationImageExtraBorder;
		brect[1] -= g_iLocationImageExtraBorder;
		brect[2] += g_iLocationImageExtraBorder;
		brect[3] += g_iLocationImageExtraBorder;

		if (brect[0] < 0) brect[0] = 0;
		if (brect[1] < 0) brect[1] = 0;
		if (brect[2] >= g_pProj->maps[iMap].img->w()) brect[2] = g_pProj->maps[iMap].img->w() - 1;
		if (brect[3] >= g_pProj->maps[iMap].img->h()) brect[3] = g_pProj->maps[iMap].img->h() - 1;
	}

	fl_message(
		"Location %03d on PAGE%03d\n"
		"\n"
		"Origin: %d, %d\n"
		"Size: %d x %d\n"
		,
		loc.iLocationIndex, iMap,
		brect[0], brect[1],
		brect[2]-brect[0]+1, brect[3]-brect[1]+1
		);
}

static void OnCmdDisplayMode(Fl_Widget*, void *p)
{
	if (g_displayMode == (DisplayMode)(intptr_t)p)
		return;

	if ((g_displayMode == DM_FADE_NONSEL || (intptr_t)p == DM_FADE_NONSEL) && g_pProj)
		g_pProj->FlushScaledImages();

	g_displayMode = (DisplayMode)(intptr_t)p;
	g_pImageView->redraw();
}

static void OnCmdZoom(Fl_Widget*, void *p)
{
	if (g_pImageView->m_iDragging || g_pImageView->m_bPanning)
		return;

	// zoom around view center (maintain same orig map XY pos at center before and after zoom change)
	const int Xclient = g_pScrollView->w() / 2;
	const int Yclient = g_pScrollView->h() / 2;
	const int Xmap = (g_pScrollView->xposition() + Xclient) / g_iZoom;
	const int Ymap = (g_pScrollView->yposition() + Yclient) / g_iZoom;

	if ( ChangeZoom(g_iZoom + (intptr_t)p) )
		ScrollImageTo(Xmap * g_iZoom - Xclient, Ymap * g_iZoom - Yclient);
}

static void OnCmdToggleThickLines(Fl_Widget*, void*)
{
	g_iLineWidth = g_iLineWidth ? 0 : 2;
	g_pImageView->redraw();
}

static void OnCmdToggleLabels(Fl_Widget*, void*)
{
	g_bDrawLabels = !g_bDrawLabels;
	g_pImageView->redraw();
}

static void OnCmdToggleFillNewShape(Fl_Widget*, void*)
{
	g_bFillNewShape = !g_bFillNewShape;
	g_pImageView->redraw();
}

static void OnCmdToggleCursorGuides(Fl_Widget*, void*)
{
	g_bDrawCursorGuides = !g_bDrawCursorGuides;
	g_pImageView->redraw();
}

static void OnCmdToggleHideSelOutlines(Fl_Widget*, void*)
{
	g_bHideSelectedOutline = !g_bHideSelectedOutline;
	g_pImageView->redraw();
}

static void OnCmdChangePage(Fl_Widget*, void *p)
{
	if (!g_pCurSelTreeItem || g_pImageView->m_bCreatingShape || g_pImageView->m_iDragging || g_pImageView->m_bPanning)
		return;

	char s[64];
	sprintf(s, "PAGE%03d", g_pProj->iCurMap + (intptr_t)p);
	Fl_Tree_Item *pItem = g_pTreeView->find_item(s);
	if (pItem)
	{
		g_pTreeView->select_only(pItem);
		g_pTreeView->set_item_focus(pItem);
	}
}

static void OnCmdAbout(Fl_Widget*, void*)
{
	fl_message(
		"DarkMapGen "DARKMAPGEN_VERSION"\n"
		"\n"
		"A tool for defining and generating auto-map locations for the Dark engine.\n"
		"\n"
		"\n"
		"This program uses:\n"
		"\n"
		"    FLTK (fltk.org)\n"
		"    Fl_Cursor_Shape for FLTK by Matthias Melcher\n"
		);
}


/////////////////////////////////////////////////////////////////////

// NOTE: don't forget to update the command-line InvokeShortcutFLTK calls if any shortcuts here change or display modes are added

// main menu
static Fl_Menu_Item g_menu[] =
{
	{"&File", 0, 0, 0, FL_SUBMENU},
		{"&Save Project", FL_COMMAND+'s', OnCmdSave, NULL, FL_MENU_DIVIDER},
		{"&Generate Map Files ", FL_F+7, OnCmdGenerateFiles},
		{"G&enerate Selected Only ", FL_COMMAND+(FL_F+7), OnCmdGenerateSelected, NULL, FL_MENU_DIVIDER},
		{"E&xit", FL_ALT+'x', OnCmdExit},
		{ 0 },

	{"&Edit", 0, 0, 0, FL_SUBMENU},
		{"&Delete Selected Location ", FL_Delete, OnCmdDelete},
		{"Edit Location &Index ", FL_F+9, OnCmdEditIndex},
		{"&Move Selected Location ", 'm', OnCmdMove},
		{"&View Location Info", 'i', OnCmdInfo},
		{ 0 },

	{"&View", 0, 0, 0, FL_SUBMENU},
		{"&Outlines Only", FL_COMMAND+'1', OnCmdDisplayMode, (void*)DM_OUTLINES, FL_MENU_RADIO|FL_MENU_VALUE},
		{"Fill &Selection", FL_COMMAND+'2', OnCmdDisplayMode, (void*)DM_FILLSEL, FL_MENU_RADIO},
		{"Fill &All", FL_COMMAND+'3', OnCmdDisplayMode, (void*)DM_FILLALL, FL_MENU_RADIO},
		{"&Dim All", FL_COMMAND+'4', OnCmdDisplayMode, (void*)DM_DIMMED, FL_MENU_RADIO},
		{"Fade &Unselected", FL_COMMAND+'5', OnCmdDisplayMode, (void*)DM_FADE_NONSEL, FL_MENU_RADIO|FL_MENU_DIVIDER},
		{"&Thick Lines", FL_COMMAND+'t', OnCmdToggleThickLines, NULL, FL_MENU_TOGGLE},
		{"Draw &Labels", FL_COMMAND+'l', OnCmdToggleLabels, NULL, FL_MENU_TOGGLE},
		{"&Fill Create Shape", FL_COMMAND+'f', OnCmdToggleFillNewShape, NULL, FL_MENU_TOGGLE},
		{"Draw Cursor &Guides", FL_COMMAND+'g', OnCmdToggleCursorGuides, NULL, FL_MENU_TOGGLE},
		{"&Hide Selection Outline ", FL_COMMAND+'h', OnCmdToggleHideSelOutlines, NULL, FL_MENU_TOGGLE|FL_MENU_DIVIDER},
		{"Zoom O&ut", FL_KP+'-', OnCmdZoom, (void*)-1},
		{"Zoom &In", FL_KP+'+', OnCmdZoom, (void*)1, FL_MENU_DIVIDER},
		{"About...", 0, OnCmdAbout},
		{ 0 },

	// hidden entries, only provide app-wide shortcuts
	{"Prev Page", FL_Page_Up, OnCmdChangePage, (void*)-1, FL_MENU_INVISIBLE},
	{"Next Page", FL_Page_Down, OnCmdChangePage, (void*)1, FL_MENU_INVISIBLE},

	{ 0 }
};


/////////////////////////////////////////////////////////////////////

static void InitControls()
{
	sprintf(g_sAppTitle, DARKMAPGEN_TITLE " " DARKMAPGEN_VERSION " %s - %s", g_bShockMaps?"[SS2]":"[Thief]", g_pProj->sDir);
	sprintf(g_sAppTitleModified, "%s *", g_sAppTitle);

	g_pMainWnd->label(g_sAppTitle);

	// center on desktop
	g_pMainWnd->position(
		Fl::x() + ((Fl::w() - g_pMainWnd->w()) / 2),
		Fl::y() + ((Fl::h() - g_pMainWnd->h()) / 2)
		);

	g_pMenuBar->menu(g_menu);

	PopulateTree();

	for (int i=0; i<g_pProj->iMapCount; i++)
		if (g_pProj->maps[i].img)
		{
			ChangeMap(i);
			if (!g_bUserDefinedWindowSize)
			{
				int cx = g_pMainWnd->x() + (g_pMainWnd->w() / 2);
				int cy = g_pMainWnd->y() + (g_pMainWnd->h() / 2);

				int iZoom = g_iZoom > 2 ? 2 : g_iZoom;
				int w = g_pProj->maps[i].img->w() * iZoom + g_iWplus;
				int h = g_pProj->maps[i].img->h() * iZoom + g_iHplus;

				g_pMainWnd->resize(cx - (w / 2), cy - (h / 2), w, h);
			}
			break;
		}
}


static void MakeWindow(int W, int H)
{
	if (W <= 0 || H <= 0)
	{
		W = SCALE(800);
		H = SCALE(507);
	}

	g_pMainWnd = new cMainWindow(W, H, DARKMAPGEN_TITLE);
	g_pMainWnd->end();

    g_pMenuBar = new Fl_Menu_Bar(0, 0, W, SCALE(25));
	g_pMainWnd->add(g_pMenuBar);

	g_pScrollView = new Fl_Scroll(0, SCALE(25), W-SCALE(160), H-SCALE(27));
	g_pScrollView->end();
	g_pMainWnd->add(g_pScrollView);
	g_pScrollView->box(FL_FLAT_BOX);

	g_pImageView = new cImageView(0, SCALE(25), W-SCALE(160), H-SCALE(27));
	g_pScrollView->add(g_pImageView);

    g_pTreeView = new Fl_Tree_Ex(W-SCALE(160), SCALE(25), SCALE(160), H-SCALE(27));
	g_pMainWnd->add(g_pTreeView);
	g_pTreeView->color(FL_LIGHT3);
	g_pTreeView->selectmode(FL_TREE_SELECT_SINGLE);
	g_pTreeView->sortorder(FL_TREE_SORT_ASCENDING);
	g_pTreeView->showroot(0);
	g_pTreeView->when(FL_WHEN_CHANGED);
	g_pTreeView->callback(OnTreeSelChange);
	g_pTreeView->marginleft(0);

#ifdef _WIN32
    g_pMainWnd->resizable(NULL);
#else
    g_pMainWnd->resizable(g_pScrollView);
#endif
	g_pMainWnd->callback(OnMainWndClose);

	// get number of pixels to add to g_pScrollView to get the window size
	g_iWplus = g_pMainWnd->w() - g_pScrollView->w();
	g_iHplus = g_pMainWnd->h() - g_pScrollView->h();

	g_pMainWnd->size_range(SCALE(320) + g_iWplus, SCALE(240) + g_iHplus, 0, 0);

	InitCursors();
}


static int GetScrollViewClientWidth()
{
	if (g_pScrollView)
	{
		if ( g_pScrollView->scrollbar.visible() )
			return g_pScrollView->scrollbar_size() ? g_pScrollView->w() - g_pScrollView->scrollbar_size() : g_pScrollView->w() - Fl::scrollbar_size();

		return g_pScrollView->w();
	}

	return 0;
}

static int GetScrollViewClientHeight()
{
	if (g_pScrollView)
	{
		if ( g_pScrollView->hscrollbar.visible() )
			return g_pScrollView->scrollbar_size() ? g_pScrollView->h() - g_pScrollView->scrollbar_size() : g_pScrollView->h() - Fl::scrollbar_size();

		return g_pScrollView->h();
	}

	return 0;
}

static void ScrollImageTo(int Xzoomed, int Yzoomed)
{
	if (Xzoomed < 0)
		Xzoomed = 0;
	else
	{
		const int xmax = g_pImageView->w() - GetScrollViewClientWidth();
		if (Xzoomed > xmax)
			Xzoomed = xmax;
		if (Xzoomed < 0)
			Xzoomed = 0;
	}

	if (Yzoomed < 0)
		Yzoomed = 0;
	else
	{
		const int ymax = g_pImageView->h() - GetScrollViewClientHeight();
		if (Yzoomed > ymax)
			Yzoomed = ymax;
		if (Yzoomed < 0)
			Yzoomed = 0;
	}

	g_pScrollView->scroll_to(Xzoomed, Yzoomed);
}


#ifdef CUSTOM_FLTK
extern Fl_Callback *fl_message_preshow_cb_;
#endif

static void OnPrepareFlMessageBox(Fl_Window *w, void*)
{
	Fl_Window *parent = Fl::modal();
	if (!parent)
		parent = g_pMainWnd;
	if (!parent)
		return;

	const int cx = parent->x() + (parent->w()/2);
	const int cy = parent->y() + (parent->h()/2);

	int x = cx - (w->w()/2);
	int y = cy - (w->h()/2);

	// neutralize offset caused by mouse in hotspot() (we still use hotspot() so we don't have to deal with the
	// preventing the window from being off-screen junk)
	int mx,my;
	Fl::get_mouse(mx,my);
	x -= mx;
	y -= my;

	w->hotspot(-x, -y);

	// UGLY HACK: make the text initally selected in the input box of fl_input dialogs, because FLTK doesn't
	if (g_bShowingFlInputDialog)
	{
		// the second child is an Fl_Input (at least for the currently used FLTK version)
		Fl_Input *o = (Fl_Input*) w->child(1);
		o->mark(0);
	}
}

static void InitFLTK(const char *lpszFlTheme, int iFontSize)
{
#ifdef CUSTOM_FLTK
	// set custom pre-show callback for fl message boxes so that we can center them to the dialog instead of mouse
	fl_message_preshow_cb_ = (Fl_Callback*)OnPrepareFlMessageBox;
#endif

	Fl::visual(FL_RGB);

	if (*lpszFlTheme == 'd')
	{
		unsigned int c, i = 0;
		for (c = FL_FOREGROUND_COLOR; c < FL_FOREGROUND_COLOR+16; c++) SETCLR(c,i); // 3-bit colormap
		for (c = FL_GRAY0; c <= FL_BLACK; c++) SETCLR(c,i); // grayscale ramp and FL_BLACK
		SETCLR(FL_WHITE,i); // FL_WHITE
		Fl::scheme(lpszFlTheme+1);
	}
	else
		Fl::scheme(lpszFlTheme);

	if (iFontSize < 8)
		iFontSize = 8;
	else if (iFontSize > 18)
		iFontSize = (SCALE(14) > 18) ? SCALE(14) : 18;

	FL_NORMAL_SIZE = iFontSize;
	fl_message_font(FL_HELVETICA, FL_NORMAL_SIZE);
	fl_message_icon()->box(FL_FLAT_BOX);
	fl_message_icon()->color(FL_GRAY0+20);

	Fl_Tooltip::size(FL_NORMAL_SIZE);
}

static BOOL HasCommandLineOption(int argc, char **argv, const char *lpszOption)
{
	for (int i=1; i<argc; i++)
		if ( !stricmp(argv[i], lpszOption) )
			return TRUE;

	return FALSE;
}

static BOOL GetCommandLineInt(int argc, char **argv, const char *lpszOption, int &iVal)
{
	for (int i=1; i<argc; i++)
		if (!stricmp(argv[i], lpszOption) && argc > i+1 && isdigit((int)(UINT)(BYTE)argv[i+1][0]))
		{
			iVal = atoi(argv[i+1]);
			return TRUE;
		}

	return FALSE;
}

static void InvokeShortcutFLTK(int key)
{
	int e_state = Fl::e_state;
	int e_keysym = Fl::e_keysym;
	char *e_text = Fl::e_text;
	int e_length = Fl::e_length;

	char s[1] = {0};
	Fl::e_text = s;
	Fl::e_length = 0;

	Fl::e_state = (e_state & FL_NUM_LOCK) | (key & ~FL_KEY_MASK);
	Fl::e_keysym = key & FL_KEY_MASK;

	g_pMenuBar->handle(FL_SHORTCUT);

	Fl::e_state = e_state;
	Fl::e_keysym = e_keysym;
	Fl::e_text = e_text;
	Fl::e_length = e_length;
}


/////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
	BOOL bUseCurrentDir = FALSE;
#ifdef DEF_THEME
	const char *lpszFlTheme = DEF_THEME_S(DEF_THEME);
#else
	const char *lpszFlTheme = "gtk+";
#endif
	int iFontSize = 14;
	int w = -1, h = -1;

	g_bShockMaps = -1;

	if (argc > 1)
	{
		if ( HasCommandLineOption(argc, argv, "-thief") )
			g_bShockMaps = FALSE;
		else if ( HasCommandLineOption(argc, argv, "-shock") )
			g_bShockMaps = TRUE;
		bUseCurrentDir = HasCommandLineOption(argc, argv, "-cwd");
		GetCommandLineInt(argc, argv, "-fontsize", iFontSize);
		g_bHideSelectedOutline = HasCommandLineOption(argc, argv, "-hidelines");

		if ( GetCommandLineInt(argc, argv, "-zoom", g_iZoom) )
		{
				if (g_iZoom < MIN_ZOOM)
					g_iZoom = MIN_ZOOM;
				else if (g_iZoom > MAX_ZOOM)
					g_iZoom = MAX_ZOOM;
		}

		if ( GetCommandLineInt(argc, argv, "-margin", g_iLocationImageExtraBorder) )
			if (g_iLocationImageExtraBorder < 0)
				g_iLocationImageExtraBorder = 0;

		if ( GetCommandLineInt(argc, argv, "-aa", g_iAlphaExportAA) )
		{
			if (g_iAlphaExportAA < 1)
				g_iAlphaExportAA = 1;
			else if (g_iAlphaExportAA > 8)
				g_iAlphaExportAA = 8;
		}

		if ( GetCommandLineInt(argc, argv, "-scale", g_iScale) )
		{
			g_iScale += 4;
			if (g_iScale < 4)
				g_iScale = 4;
			else if (g_iScale > 8)
				g_iScale = 8;
		}

#ifdef DEF_THEME
		if ( HasCommandLineOption(argc, argv, "-theme_gtk") )
			lpszFlTheme = "gtk+";
		else
#endif
		if ( HasCommandLineOption(argc, argv, "-theme_dark") )
			lpszFlTheme = "dgtk+";
		else if ( HasCommandLineOption(argc, argv, "-theme_plastic") )
			lpszFlTheme = "plastic";
		else if ( HasCommandLineOption(argc, argv, "-theme_base") )
			lpszFlTheme = "base";

		for (int i=1; i<argc; i++)
			if (!stricmp(argv[i], "-winsize") && argc > i+1)
			{
				sscanf(argv[i+1], "%dx%d", &w, &h);
				if (h > 0)
				{
					if (w < SCALE(320) + g_iWplus)
						w = SCALE(320) + g_iWplus;
					if (h < SCALE(240) + g_iHplus)
						h = SCALE(240) + g_iHplus;

					g_bUserDefinedWindowSize = TRUE;
				}
			}
	}

	InitFLTK(lpszFlTheme, iFontSize);

    MakeWindow(w, h);

	if (*lpszFlTheme == 'd' && g_pTreeView)
	{
		g_pTreeView->selection_color(FL_FOREGROUND_COLOR);
		g_pTreeView->item_labelfgcolor(FL_FOREGROUND_COLOR);
		g_pTreeView->connectorcolor(FL_FOREGROUND_COLOR);
	}

	if (g_pMainWnd)
	{
		if (!bUseCurrentDir)
		{
			if (iFontSize > 18)
				FL_NORMAL_SIZE = 18;
			char *sDir = fl_dir_chooser("Select Map Directory", NULL, 0);
			if (!sDir)
			{
				delete g_pMainWnd;
				return 0;
			}
			if (iFontSize > 18)
				FL_NORMAL_SIZE = iFontSize;

			_chdir(sDir);
		}

		char s[MAX_PATH];
		LoadProject( _getcwd(s, sizeof(s)) );

		if (!g_pProj->iMapCount)
		{
			fl_alert("No map pages found. Make sure that the working directory contains properly named map page images as PNG.");
			return 0;
		}

		Fl::scrollbar_size(SCALE(Fl::scrollbar_size()));

		InitControls();

		g_pMainWnd->show();

		// apply any command-line settings for UI configurable settings
		if (argc > 1)
		{
			int i;

			if (GetCommandLineInt(argc, argv, "-viewmode", i) && i >= 1 && i < DM_NUM_MODES)
				InvokeShortcutFLTK(FL_COMMAND+'1'+(i-1));
			if ( HasCommandLineOption(argc, argv, "-thicklines") )
				InvokeShortcutFLTK(FL_COMMAND+'t');
			if ( HasCommandLineOption(argc, argv, "-labels") )
				InvokeShortcutFLTK(FL_COMMAND+'l');
			if ( HasCommandLineOption(argc, argv, "-cguides") )
				InvokeShortcutFLTK(FL_COMMAND+'g');
			if ( HasCommandLineOption(argc, argv, "-fillnew") )
				InvokeShortcutFLTK(FL_COMMAND+'f');
		}

		Fl::run();

		delete g_pMainWnd;

		if (g_pProj)
		{
			delete g_pProj;
			g_pProj = NULL;
		}
	}

	return 0;
}
