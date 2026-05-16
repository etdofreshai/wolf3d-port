// WL_DRAW.C - Raycasting renderer
// Ported from original WOLFSRC/WL_DRAW.C to C11/SDL3
//
// Changes from original:
//   - All inline x86 assembly replaced with C equivalents
//   - VGA Mode X planar access replaced with flat 320x200 byte framebuffer
//   - Compiled scaling code (self-modifying x86) replaced with C pixel loops
//   - outportb/inportb for VGA registers replaced with VL_* calls
//   - All far/_seg/huge qualifiers removed
//   - VL_Present() called at end of ThreeDRefresh instead of VGA CRTC paging
//   - AsmRefresh (originally in WL_DR_A.ASM) rewritten as portable C DDA

#include "id_heads.h"
#include "wl_def.h"
#include <SDL3/SDL.h>

//#define DEBUGWALLS
//#define DEBUGTICS

/*
=============================================================================

                         LOCAL CONSTANTS

=============================================================================
*/

// the door is the last picture before the sprites
#define DOORWALL	(PMSpriteStart-8)

#define ACTORSIZE	0x4000

/*
=============================================================================

                         GLOBAL VARIABLES

=============================================================================
*/


#ifdef DEBUGWALLS
unsigned screenloc[3]= {0,0,0};
#else
unsigned screenloc[3]= {PAGE1START,PAGE2START,PAGE3START};
#endif
unsigned freelatch = FREESTART;

long 	lasttimecount;
long 	frameon;


unsigned	wallheight[MAXVIEWWIDTH];

fixed	tileglobal	= TILEGLOBAL;
fixed	mindist		= MINDIST;


//
// math tables
//
int			pixelangle[MAXVIEWWIDTH];
long		finetangent[FINEANGLES/4];
fixed 		sintable[ANGLES+ANGLES/4], *costable = sintable+(ANGLES/4);

//
// refresh variables
//
fixed	viewx,viewy;			// the focal point
int		viewangle;
fixed	viewsin,viewcos;



fixed	FixedByFrac (fixed a, fixed b);
void	TransformActor (objtype *ob);
void	BuildTables (void);
void	ClearScreen (void);
int		CalcRotate (objtype *ob);
void	DrawScaleds (void);
void	CalcTics (void);
void	FixOfs (void);
void	ThreeDRefresh (void);



//
// wall optimization variables
//
int		lastside;		// true for vertical
long	lastintercept;
int		lasttilehit;


//
// ray tracing variables
//
int			focaltx,focalty,viewtx,viewty;

int			midangle,angle;
unsigned	xpartial,ypartial;
unsigned	xpartialup,xpartialdown,ypartialup,ypartialdown;
unsigned	xinttile,yinttile;

unsigned	tilehit;
unsigned	pixx;

int		xtile,ytile;
int		xtilestep,ytilestep;
long	xintercept,yintercept;
long	xstep,ystep;

int		horizwall[MAXWALLTILES],vertwall[MAXWALLTILES];


/*
=============================================================================

                         LOCAL VARIABLES

=============================================================================
*/


/*
============================================================================

               3 - D  DEFINITIONS

============================================================================
*/


//==========================================================================


/*
========================
=
= FixedByFrac
=
= multiply a 16/16 bit, 2's complement fixed point number by a 16 bit
= fraction, passed as a signed magnitude 32 bit number
=
========================
*/

fixed FixedByFrac (fixed a, fixed b)
{
	int sign;
	long result;

//
// figure sign of result
//
// b is passed in signed-magnitude form: the magnitude is in the low 16
// bits and the sign is bit 31 (the sintable/costable builder ORs in
// 0x80000000 for negative entries).  It must NOT be negated as a two's
// complement number - that yields 0x80000000-mag instead of the magnitude.
//
	sign = 0;
	if ((unsigned long)b & 0x80000000UL)
		sign ^= 1;
	b &= 0xFFFF;

	if (a < 0)
	{
		a = -a;
		sign ^= 1;
	}

//
// multiply a by b
//
// a is 16.16 fixed point (high 16 = integer, low 16 = fraction)
// b is treated as a 16 bit fraction (low word)
// result is 16.16 fixed point
//
// Both a and b are now non-negative.  The product a*(b&0xFFFF) can reach
// ~2^47, and even the (a&0xFFFF)*(b&0xFFFF) cross term reaches ~2^32, which
// overflows a signed 32-bit int and corrupts the sign.  Compute the whole
// thing in 64 bits, then shift down by the 16-bit fraction of b.
//
	result = (long)(((int64_t)a * (b & 0xFFFF)) >> 16);

	if (sign)
		result = -result;

	return (fixed)result;
}

//==========================================================================

/*
========================
=
= TransformActor
=
= Takes paramaters:
=   gx,gy		: globalx/globaly of point
=
= globals:
=   viewx,viewy		: point of view
=   viewcos,viewsin	: sin/cos of viewangle
=   scale		: conversion from global value to screen value
=
= sets:
=   screenx,transx,transy,screenheight: projected edge location and size
=
========================
*/

void TransformActor (objtype *ob)
{
	fixed gx,gy,gxt,gyt,nx,ny;
	long	temp;

//
// translate point to view centered coordinates
//
	gx = ob->x-viewx;
	gy = ob->y-viewy;

//
// calculate newx
//
	gxt = FixedByFrac(gx,viewcos);
	gyt = FixedByFrac(gy,viewsin);
	nx = gxt-gyt-ACTORSIZE;		// fudge the shape forward a bit, because
									// the midpoint could put parts of the shape
									// into an adjacent wall

//
// calculate newy
//
	gxt = FixedByFrac(gx,viewsin);
	gyt = FixedByFrac(gy,viewcos);
	ny = gyt+gxt;

//
// calculate perspective ratio
//
	ob->transx = nx;
	ob->transy = ny;

	if (nx<mindist)			// too close, don't overflow the divide
	{
	  ob->viewheight = 0;
	  return;
	}

	ob->viewx = centerx + (int)(ny*scale/nx);

//
// calculate height (heightnumerator/(nx>>8))
//
	temp = heightnumerator / (nx>>8);

	ob->viewheight = temp;
}

//==========================================================================

/*
========================
=
= TransformTile
=
= Takes paramaters:
=   tx,ty		: tile the object is centered in
=
= globals:
=   viewx,viewy		: point of view
=   viewcos,viewsin	: sin/cos of viewangle
=   scale		: conversion from global value to screen value
=
= sets:
=   screenx,transx,transy,screenheight: projected edge location and size
=
= Returns true if the tile is withing getting distance
=
========================
*/

boolean TransformTile (int tx, int ty, int *dispx, int *dispheight)
{
	fixed gx,gy,gxt,gyt,nx,ny;
	long	temp;

//
// translate point to view centered coordinates
//
	gx = ((long)tx<<TILESHIFT)+0x8000-viewx;
	gy = ((long)ty<<TILESHIFT)+0x8000-viewy;

//
// calculate newx
//
	gxt = FixedByFrac(gx,viewcos);
	gyt = FixedByFrac(gy,viewsin);
	nx = gxt-gyt-0x2000;		// 0x2000 is size of object

//
// calculate newy
//
	gxt = FixedByFrac(gx,viewsin);
	gyt = FixedByFrac(gy,viewcos);
	ny = gyt+gxt;


//
// calculate perspective ratio
//
	if (nx<mindist)			// too close, don't overflow the divide
	{
		*dispheight = 0;
		return false;
	}

	*dispx = centerx + (int)(ny*scale/nx);

//
// calculate height (heightnumerator/(nx>>8))
//
	temp = heightnumerator / (nx>>8);

	*dispheight = temp;

//
// see if it should be grabbed
//
	if (nx<TILEGLOBAL && ny>-TILEGLOBAL/2 && ny<TILEGLOBAL/2)
		return true;
	else
		return false;
}

//==========================================================================

/*
====================
=
= CalcHeight
=
= Calculates the height of xintercept,yintercept from viewx,viewy
=
====================
*/

int CalcHeight (void)
{
	fixed gxt,gyt,nx;
	long	gx,gy;

	gx = xintercept-viewx;
	gxt = FixedByFrac(gx,viewcos);

	gy = yintercept-viewy;
	gyt = FixedByFrac(gy,viewsin);

	nx = gxt-gyt;

  //
  // calculate perspective ratio (heightnumerator/(nx>>8))
  //
	if (nx<mindist)
		nx=mindist;			// don't let divide overflow

	return (int)(heightnumerator / (nx>>8));
}


//==========================================================================

/*
===================
=
= ScalePost
=
= Draws a vertical strip of wall texture, scaled to the proper height.
= Replaces the original compiled scaling code (self-modifying x86 assembly
= that used VGA Mode X planar access).
=
= Globals used:
=   postx      - screen column to draw at
=   postwidth  - number of columns wide (1-3)
=   postpage/posttex - texture data pointer and column byte offset
=   wallheight[] - per-column wall height (used for scaling)
=
= Wall textures are 64x64 pixels stored column-major: texture[col*64 + row].
= The framebuffer is a flat 320-pixel-wide byte array accessed via VL_Plot.
=
===================
*/

byte		*postpage;		// texture data pointer from PM_GetPage
unsigned	posttex;		// texture column byte offset (0, 64, 128, ..., 4032)
unsigned	postx;
unsigned	postwidth;

void ScalePost (void)
{
	int height;
	int top, bottom;
	int y;
	int src_row;
	unsigned col;
	byte *fb;

	if (postwidth == 0)
		return;

	if (!postpage)
		return;

	// wallheight[] holds the raw projection value; the on-screen pixel
	// height is that value >> 2 (matches the original compiled scalers,
	// where a sprite of raw height h draws (h>>3)*2 == h>>2 pixels).
	height = wallheight[postx] >> 2;

	if (height <= 0)
		return;

	top = (viewheight / 2) - (height / 2);
	bottom = top + height - 1;

	if (top < 0)
		top = 0;
	if (bottom >= viewheight)
		bottom = viewheight - 1;

	fb = VL_GetFramebuffer();

	for (col = 0; col < postwidth; col++)
	{
		int screenx = (int)(postx + col);
		if (screenx < 0 || screenx >= viewwidth)
			continue;

		unsigned base = bufferofs + ylookup[top] + screenx;

		for (y = top; y <= bottom; y++)
		{
			src_row = ((y - (viewheight / 2) + (height / 2)) * 64) / height;
			if (src_row > 63) src_row = 63;

			fb[base] = postpage[posttex + src_row];
			base += linewidth;
		}
	}
}

void FarScalePost (void)				// just so other files can call
{
	ScalePost ();
}


/*
====================
=
= HitVertWall
=
= tilehit bit 7 is 0, because it's not a door tile
= if bit 6 is 1 and the adjacent tile is a door tile, use door side pic
=
====================
*/

void HitVertWall (void)
{
	int			wallpic;
	unsigned	texture;

	texture = (yintercept>>4)&0xfc0;
	if (xtilestep == -1)
	{
		texture = 0xfc0-texture;
		xintercept += TILEGLOBAL;
	}
	wallheight[pixx] = CalcHeight();

	if (lastside==1 && lastintercept == xtile && lasttilehit == tilehit)
	{
		// in the same wall type as last time, so check for optimized draw
		if (texture == posttex)
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = pixx > 0 ? wallheight[pixx-1] : wallheight[pixx];
			return;
		}
		else
		{
			ScalePost ();
			posttex = texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
	// new wall
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();

		lastside = true;
		lastintercept = xtile;

		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		if (tilehit & 0x40)
		{								// check for adjacent doors
			ytile = yintercept>>TILESHIFT;
			if ( tilemap[xtile-xtilestep][ytile]&0x80 )
				wallpic = DOORWALL+3;
			else
				wallpic = vertwall[tilehit & ~0x40];
		}
		else
			wallpic = vertwall[tilehit];

		postpage = PM_GetPage(wallpic); posttex = texture;

	}
}


/*
====================
=
= HitHorizWall
=
= tilehit bit 7 is 0, because it's not a door tile
= if bit 6 is 1 and the adjacent tile is a door tile, use door side pic
=
====================
*/

void HitHorizWall (void)
{
	int			wallpic;
	unsigned	texture;

	texture = (xintercept>>4)&0xfc0;
	if (ytilestep == -1)
		yintercept += TILEGLOBAL;
	else
		texture = 0xfc0-texture;
	wallheight[pixx] = CalcHeight();

	if (lastside==0 && lastintercept == ytile && lasttilehit == tilehit)
	{
		// in the same wall type as last time, so check for optimized draw
		if (texture == posttex)
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = pixx > 0 ? wallheight[pixx-1] : wallheight[pixx];
			return;
		}
		else
		{
			ScalePost ();
			posttex = texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
	// new wall
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();

		lastside = 0;
		lastintercept = ytile;

		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		if (tilehit & 0x40)
		{								// check for adjacent doors
			xtile = xintercept>>TILESHIFT;
			if ( tilemap[xtile][ytile-ytilestep]&0x80 )
				wallpic = DOORWALL+2;
			else
				wallpic = horizwall[tilehit & ~0x40];
		}
		else
			wallpic = horizwall[tilehit];

		postpage = PM_GetPage(wallpic); posttex = texture;
	}

}

//==========================================================================

/*
====================
=
= HitHorizDoor
=
====================
*/

void HitHorizDoor (void)
{
	unsigned	texture,doorpage,doornum;

	doornum = tilehit&0x7f;
	if (doornum >= MAXDOORS) return;
	texture = ( (xintercept-doorposition[doornum]) >> 4) &0xfc0;

	wallheight[pixx] = CalcHeight();

	if (lasttilehit == tilehit)
	{
	// in the same door as last time, so check for optimized draw
		if (texture == posttex)
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = pixx > 0 ? wallheight[pixx-1] : wallheight[pixx];
			return;
		}
		else
		{
			ScalePost ();
			posttex = texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();			// draw last post
	// first pixel in this door
		lastside = 2;
		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		switch (doorobjlist[doornum].lock)
		{
		case dr_normal:
			doorpage = DOORWALL;
			break;
		case dr_lock1:
		case dr_lock2:
		case dr_lock3:
		case dr_lock4:
			doorpage = DOORWALL+6;
			break;
		case dr_elevator:
			doorpage = DOORWALL+4;
			break;
		default:
			doorpage = DOORWALL;
			break;
		}

		postpage = PM_GetPage(doorpage); posttex = texture;
	}
}

//==========================================================================

/*
====================
=
= HitVertDoor
=
====================
*/

void HitVertDoor (void)
{
	unsigned	texture,doorpage,doornum;

	doornum = tilehit&0x7f;
	if (doornum >= MAXDOORS) return;
	texture = ( (yintercept-doorposition[doornum]) >> 4) &0xfc0;

	wallheight[pixx] = CalcHeight();

	if (lasttilehit == tilehit)
	{
	// in the same door as last time, so check for optimized draw
		if (texture == posttex)
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = pixx > 0 ? wallheight[pixx-1] : wallheight[pixx];
			return;
		}
		else
		{
			ScalePost ();
			posttex = texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();			// draw last post
	// first pixel in this door
		lastside = 2;
		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		switch (doorobjlist[doornum].lock)
		{
		case dr_normal:
			doorpage = DOORWALL;
			break;
		case dr_lock1:
		case dr_lock2:
		case dr_lock3:
		case dr_lock4:
			doorpage = DOORWALL+6;
			break;
		case dr_elevator:
			doorpage = DOORWALL+4;
			break;
		default:
			doorpage = DOORWALL;
			break;
		}

		postpage = PM_GetPage(doorpage+1); posttex = texture;
	}
}

//==========================================================================


/*
====================
=
= HitHorizPWall
=
= A pushable wall in action has been hit
=
====================
*/

void HitHorizPWall (void)
{
	int			wallpic;
	unsigned	texture,offset;

	texture = (xintercept>>4)&0xfc0;
	offset = pwallpos<<10;
	if (ytilestep == -1)
		yintercept += TILEGLOBAL-offset;
	else
	{
		texture = 0xfc0-texture;
		yintercept += offset;
	}

	wallheight[pixx] = CalcHeight();

	if (lasttilehit == tilehit)
	{
		// in the same wall type as last time, so check for optimized draw
		if (texture == posttex)
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = pixx > 0 ? wallheight[pixx-1] : wallheight[pixx];
			return;
		}
		else
		{
			ScalePost ();
			posttex = texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
	// new wall
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();

		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		wallpic = horizwall[tilehit&63];

		postpage = PM_GetPage(wallpic); posttex = texture;
	}

}


/*
====================
=
= HitVertPWall
=
= A pushable wall in action has been hit
=
====================
*/

void HitVertPWall (void)
{
	int			wallpic;
	unsigned	texture,offset;

	texture = (yintercept>>4)&0xfc0;
	offset = pwallpos<<10;
	if (xtilestep == -1)
	{
		xintercept += TILEGLOBAL-offset;
		texture = 0xfc0-texture;
	}
	else
		xintercept += offset;

	wallheight[pixx] = CalcHeight();

	if (lasttilehit == tilehit)
	{
		// in the same wall type as last time, so check for optimized draw
		if (texture == posttex)
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = pixx > 0 ? wallheight[pixx-1] : wallheight[pixx];
			return;
		}
		else
		{
			ScalePost ();
			posttex = texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
	// new wall
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();

		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		wallpic = vertwall[tilehit&63];

		postpage = PM_GetPage(wallpic); posttex = texture;
	}

}

//==========================================================================


unsigned vgaCeiling[]=
{
#ifndef SPEAR
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0xbfbf,
 0x4e4e,0x4e4e,0x4e4e,0x1d1d,0x8d8d,0x4e4e,0x1d1d,0x2d2d,0x1d1d,0x8d8d,
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x2d2d,0xdddd,0x1d1d,0x1d1d,0x9898,

 0x1d1d,0x9d9d,0x2d2d,0xdddd,0xdddd,0x9d9d,0x2d2d,0x4d4d,0x1d1d,0xdddd,
 0x7d7d,0x1d1d,0x2d2d,0x2d2d,0xdddd,0xd7d7,0x1d1d,0x1d1d,0x1d1d,0x2d2d,
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0xdddd,0xdddd,0x7d7d,0xdddd,0xdddd,0xdddd
#else
 0x6f6f,0x4f4f,0x1d1d,0xdede,0xdfdf,0x2e2e,0x7f7f,0x9e9e,0xaeae,0x7f7f,
 0x1d1d,0xdede,0xdfdf,0xdede,0xdfdf,0xdede,0xe1e1,0xdcdc,0x2e2e,0x1d1d,0xdcdc
#endif
};

/*
=====================
=
= VGAClearScreen
=
= Clear ceiling and floor areas with the appropriate colors.
= Replaces VGA Mode X planar clear with flat framebuffer VL_Hlin calls.
=
=====================
*/

void VGAClearScreen (void)
{
	unsigned ceiling=vgaCeiling[gamestate.episode*10+mapon];
	byte ceilingColor = ceiling & 0xFF;
	byte floorColor = 0x19;

	int halfH = viewheight >> 1;

	//
	// draw ceiling (top half)
	//
	for (int y = 0; y < halfH; y++)
		VL_Hlin(0, y, viewwidth, ceilingColor);

	//
	// draw floor (bottom half)
	//
	for (int y = halfH; y < viewheight; y++)
		VL_Hlin(0, y, viewwidth, floorColor);
}

//==========================================================================

/*
=====================
=
= CalcRotate
=
=====================
*/

int	CalcRotate (objtype *ob)
{
	int	angle,viewangle;

	// this isn't exactly correct, as it should vary by a trig value,
	// but it is close enough with only eight rotations

	viewangle = player->angle + (centerx - ob->viewx)/8;

	if (ob->obclass == rocketobj || ob->obclass == hrocketobj)
		angle =  (viewangle-180)- ob->angle;
	else
		angle =  (viewangle-180)- dirangle[ob->dir];

	angle+=ANGLES/16;
	while (angle>=ANGLES)
		angle-=ANGLES;
	while (angle<0)
		angle+=ANGLES;

	if (ob->state->rotate == 2)             // 2 rotation pain frame
		return 4*(angle/(ANGLES/2));        // seperated by 3 (art layout...)

	return angle/(ANGLES/8);
}


/*
=====================
=
= DrawScaleds
=
= Draws all objects that are visable
=
=====================
*/

#define MAXVISABLE	50

typedef struct
{
	int	viewx,
		viewheight,
		shapenum;
} visobj_t;

visobj_t	vislist[MAXVISABLE],*visptr,*visstep,*farthest;

void DrawScaleds (void)
{
	int 		i,j,least,numvisable,height;
	byte		*tilespot,*visspot;
	int			shapenum;
	unsigned	spotloc;

	statobj_t	*statptr;
	objtype		*obj;

	visptr = &vislist[0];

//
// place static objects
//
	for (statptr = &statobjlist[0] ; statptr !=laststatobj ; statptr++)
	{
		if ((visptr->shapenum = statptr->shapenum) == -1)
			continue;						// object has been deleted

		if (!*statptr->visspot)
			continue;						// not visable

		if (TransformTile (statptr->tilex,statptr->tiley
			,&visptr->viewx,&visptr->viewheight) && statptr->flags & FL_BONUS)
		{
			GetBonus (statptr);
			continue;
		}

		if (!visptr->viewheight)
			continue;						// to close to the object

		if (visptr < &vislist[MAXVISABLE-1])	// don't let it overflow
			visptr++;
	}

//
// place active objects
//
	for (obj = player->next;obj;obj=obj->next)
	{
		if (!(visptr->shapenum = obj->state->shapenum))
			continue;						// no shape

		spotloc = (obj->tilex<<6)+obj->tiley;	// optimize: keep in struct?
		visspot = &spotvis[0][0]+spotloc;
		tilespot = &tilemap[0][0]+spotloc;

		//
		// could be in any of the nine surrounding tiles
		//
		if (*visspot
		|| ( *(visspot-1) && !*(tilespot-1) )
		|| ( *(visspot+1) && !*(tilespot+1) )
		|| ( *(visspot-65) && !*(tilespot-65) )
		|| ( *(visspot-64) && !*(tilespot-64) )
		|| ( *(visspot-63) && !*(tilespot-63) )
		|| ( *(visspot+65) && !*(tilespot+65) )
		|| ( *(visspot+64) && !*(tilespot+64) )
		|| ( *(visspot+63) && !*(tilespot+63) ) )
		{
			obj->active = true;
			TransformActor (obj);
			if (!obj->viewheight)
				continue;						// too close or far away

			visptr->viewx = obj->viewx;
			visptr->viewheight = obj->viewheight;
			if (visptr->shapenum == -1)
				visptr->shapenum = obj->temp1;	// special shape

			if (obj->state->rotate)
				visptr->shapenum += CalcRotate (obj);

			if (visptr < &vislist[MAXVISABLE-1])	// don't let it overflow
				visptr++;
			obj->flags |= FL_VISABLE;
		}
		else
			obj->flags &= ~FL_VISABLE;
	}

//
// draw from back to front
//
	numvisable = (int)(visptr-&vislist[0]);

	if (!numvisable)
		return;									// no visable objects

	for (i = 0; i<numvisable; i++)
	{
		least = 32000;
		for (visstep=&vislist[0] ; visstep<visptr ; visstep++)
		{
			height = visstep->viewheight;
			if (height < least)
			{
				least = height;
				farthest = visstep;
			}
		}
		//
		// draw farthest
		//
		ScaleShape(farthest->viewx,farthest->shapenum,farthest->viewheight);

		farthest->viewheight = 32000;
	}

}

//==========================================================================

/*
==============
=
= DrawPlayerWeapon
=
= Draw the player's hands
=
==============
*/

int	weaponscale[NUMWEAPONS] = {SPR_KNIFEREADY,SPR_PISTOLREADY
	,SPR_MACHINEGUNREADY,SPR_CHAINREADY};

void DrawPlayerWeapon (void)
{
	int	shapenum;

#ifndef SPEAR
	if (gamestate.victoryflag)
	{
		if (player->state == &s_deathcam && (TimeCount&32) )
			SimpleScaleShape(viewwidth/2,SPR_DEATHCAM,viewheight+1);
		return;
	}
#endif

	if (gamestate.weapon != -1)
	{
		shapenum = weaponscale[gamestate.weapon]+gamestate.weaponframe;
		SimpleScaleShape(viewwidth/2,shapenum,viewheight+1);
	}

	if (demorecord || demoplayback)
		SimpleScaleShape(viewwidth/2,SPR_DEMO,viewheight+1);
}


//==========================================================================


/*
=====================
=
= CalcTics
=
=====================
*/

void CalcTics (void)
{
	static Uint32 last_sdl_ticks = 0;
	Uint32 now, elapsed;

//
// Use SDL_GetTicks for timing (original used hardware timer interrupt)
// 70 Hz tick rate in original Wolf3D
//
	now = SDL_GetTicks();
	if (last_sdl_ticks == 0) last_sdl_ticks = now;

	elapsed = now - last_sdl_ticks;

// Cap at ~70 fps (14ms per frame)
	if (elapsed < 14)
	{
		SDL_Delay(14 - elapsed);
		now = SDL_GetTicks();
		elapsed = now - last_sdl_ticks;
	}

	tics = (int)(elapsed * 70 / 1000);
	if (tics == 0)
		tics = 1;
	if (tics > MAXTICS)
		tics = MAXTICS;

	last_sdl_ticks = now;
	TimeCount += tics;
	lasttimecount = TimeCount;
}


//==========================================================================


/*
========================
=
= FixOfs
=
========================
*/

void	FixOfs (void)
{
	VW_ScreenToScreen (displayofs,bufferofs,viewwidth,viewheight);
}


//==========================================================================


/*
====================
=
= AsmRefresh
=
= Raycasting loop - originally in WL_DR_A.ASM, now portable C.
= Casts one ray per screen column using a DDA (Digital Differential Analyzer)
= algorithm, identical to the original game's logic.
=
= For each screen column (pixx = 0..viewwidth-1):
=   1. Compute the ray angle from the player's view angle + pixel offset
=   2. Determine tile step direction (xtilestep, ytilestep) from the angle
=   3. Calculate initial intercepts (xintercept, yintercept) on tile boundaries
=   4. Calculate step sizes (xstep, ystep) for the DDA
=   5. Step through tiles until a wall/door/pushwall is hit
=   6. Dispatch to HitVertWall/HitHorizWall/Hit*Door/Hit*PWall handlers
=   7. Mark visible tiles in spotvis for sprite rendering
=
====================
*/

static void AsmRefresh (void)
{
	long		xstep, ystep;
	long		xintbuf, yintbuf;
	unsigned	texdelta;
	int			count;

	for (pixx = 0; pixx < viewwidth; pixx++)
	{
		//
		// calculate fine angle for this screen column
		//
		angle = midangle + pixelangle[pixx];
		if (angle < 0)
			angle += FINEANGLES;
		else if (angle >= FINEANGLES)
			angle -= FINEANGLES;

		//
		// set up tile stepping and step values per quadrant
		//
		xtile = focaltx;
		ytile = focalty;

		if (angle < ANG90)
		{
			xtilestep = 1;
			ytilestep = -1;
			xstep =  finetangent[ANG90 - 1 - angle];
			ystep = -finetangent[angle];
			xpartial = xpartialup;
			ypartial = ypartialdown;
		}
		else if (angle < ANG180)
		{
			xtilestep = -1;
			ytilestep = -1;
			xstep = -finetangent[angle - ANG90];
			ystep = -finetangent[ANG180 - 1 - angle];
			xpartial = xpartialdown;
			ypartial = ypartialdown;
		}
		else if (angle < ANG270)
		{
			xtilestep = -1;
			ytilestep = 1;
			xstep = -finetangent[ANG270 - 1 - angle];
			ystep =  finetangent[angle - ANG180];
			xpartial = xpartialdown;
			ypartial = ypartialup;
		}
		else
		{
			xtilestep = 1;
			ytilestep = 1;
			xstep =  finetangent[angle - ANG270];
			ystep =  finetangent[FINEANGLES - 1 - angle];
			xpartial = xpartialup;
			ypartial = ypartialup;
		}

		//
		// calculate initial intercepts
		//
		yintercept = FixedByFrac(ystep, xpartial) + viewy;
		xtile += xtilestep;

		xintercept = FixedByFrac(xstep, ypartial) + viewx;
		ytile += ytilestep;

		//
		// DDA ray tracing loop
		//
		do
		{
			//
			// check vertical wall boundaries (stepping along x)
			//
			// Compare yintercept (y-position at x boundary) against ytile
			// to see if we've crossed a y tile boundary
			//
			if (ytilestep == -1 && (yintercept >> 16) <= ytile)
				goto horizentry;
			if (ytilestep == 1 && (yintercept >> 16) >= ytile)
				goto horizentry;

		vertcheck:
			//
			// vertical wall boundary hit
			//
			if ((unsigned long)yintercept > (unsigned long)mapheight * 65536 - 1
				|| (unsigned)xtile >= mapwidth)
			{
				// out of map bounds
				wallheight[pixx] = 0;
				break;
			}

			tilehit = tilemap[xtile][(unsigned)yintercept >> 16];
			if (tilehit)
			{
				if (tilehit & 0x80)
				{
					// door tile - check half step for door position
					yintbuf = yintercept + (ystep >> 1);
					if ((yintbuf >> 16) != (yintercept >> 16))
						goto passvert;
					if ((word)yintbuf < doorposition[tilehit & 0x7f])
						goto passvert;
					yintercept = yintbuf;
					xintercept = 0x8000 + ((long)xtile << TILESHIFT);
					ytile = (int)(yintercept >> TILESHIFT);
					HitVertDoor();
				}
				else
				{
					xintercept = (long)xtile << TILESHIFT;
					ytile = (int)(yintercept >> TILESHIFT);
					HitVertWall();
				}
				break;
			}

		passvert:
			spotvis[xtile][(unsigned)yintercept >> 16] = 1;
			xtile += xtilestep;
			yintercept += ystep;
			continue;

		horizcheck:
			//
			// check horizontal wall boundaries (stepping along y)
			//
			// Compare xintercept (x-position at y boundary) against xtile
			//
			if (xtilestep == -1 && (xintercept >> 16) <= xtile)
				goto vertcheck;
			if (xtilestep == 1 && (xintercept >> 16) >= xtile)
				goto vertcheck;

		horizentry:
			//
			// horizontal wall boundary hit
			//
			if ((unsigned long)xintercept > (unsigned long)mapwidth * 65536 - 1
				|| (unsigned)ytile >= mapheight)
			{
				// out of map bounds
				wallheight[pixx] = 0;
				break;
			}

			tilehit = tilemap[(unsigned)xintercept >> 16][ytile];
			if (tilehit)
			{
				if (tilehit & 0x80)
				{
					// door tile
					xintbuf = xintercept + (xstep >> 1);
					if ((xintbuf >> 16) != (xintercept >> 16))
						goto passhoriz;
					if ((word)xintbuf < doorposition[tilehit & 0x7f])
						goto passhoriz;
					xintercept = xintbuf;
					yintercept = 0x8000 + ((long)ytile << TILESHIFT);
					xtile = (int)(xintercept >> TILESHIFT);
					HitHorizDoor();
				}
				else
				{
					yintercept = (long)ytile << TILESHIFT;
					xtile = (int)(xintercept >> TILESHIFT);
					HitHorizWall();
				}
				break;
			}

		passhoriz:
			spotvis[(unsigned)xintercept >> 16][ytile] = 1;
			ytile += ytilestep;
			xintercept += xstep;
			goto horizcheck;
		} while (1);
	}
}

void WallRefresh (void)
{
//
// set up variables for this view
//
	viewangle = player->angle;
	midangle = viewangle*(FINEANGLES/ANGLES);
	viewsin = sintable[viewangle];
	viewcos = costable[viewangle];
	viewx = player->x - FixedByFrac(focallength,viewcos);
	viewy = player->y + FixedByFrac(focallength,viewsin);

	focaltx = viewx>>TILESHIFT;
	focalty = viewy>>TILESHIFT;

	viewtx = player->x >> TILESHIFT;
	viewty = player->y >> TILESHIFT;

	xpartialdown = viewx&(TILEGLOBAL-1);
	xpartialup = TILEGLOBAL-xpartialdown;
	ypartialdown = viewy&(TILEGLOBAL-1);
	ypartialup = TILEGLOBAL-ypartialdown;

	lastside = -1;			// the first pixel is on a new wall
	AsmRefresh ();
	ScalePost ();			// no more optimization on last post
}

//==========================================================================

/*
========================
=
= ThreeDRefresh
=
========================
*/

void	ThreeDRefresh (void)
{
//
// clear out the traced array
//
	memset (spotvis, 0, sizeof(spotvis));

	bufferofs += screenofs;

//
// follow the walls from there to the right, drawing as we go
//
	VGAClearScreen ();

	WallRefresh ();

//
// draw all the scaled images
//
	DrawScaleds();			// draw scaled stuff
	DrawPlayerWeapon ();	// draw player's hands

//
// show screen and time last cycle
//
	if (fizzlein)
	{
		FizzleFade(bufferofs,displayofs+screenofs,viewwidth,viewheight,20,false);
		fizzlein = false;

		lasttimecount = TimeCount = 0;		// don't make a big tic count
	}

	bufferofs -= screenofs;
	displayofs = bufferofs;

	// Present the framebuffer to the screen via SDL3
	VL_Present();

	bufferofs += SCREENSIZE;
	if (bufferofs > PAGE3START)
		bufferofs = PAGE1START;

	frameon++;
	PM_NextFrame();
}


//===========================================================================
