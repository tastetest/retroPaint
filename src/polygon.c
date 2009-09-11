/*	polygon.c
	Copyright (C) 2005-2008 Mark Tyler and Dmitry Groshev

	This file is part of mtPaint.

	mtPaint is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	mtPaint is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with mtPaint in the file COPYING.
*/

#include <stdlib.h>
#include <math.h>

#include <gtk/gtk.h>

#include "polygon.h"
#include "mygtk.h"
#include "memory.h"

/* !!! Currently, poly_points should be set to 0 when there's no polygonal
 * selection, because poly_lasso() depends on that - WJ */
int poly_points;
int poly_mem[MAX_POLY][2];
		// Coords in poly_mem are raw coords as plotted over image
int poly_xy[4];


void poly_draw(int filled, unsigned char *buf, int wbuf)
{
	linedata line;
	int poly_lines[MAX_POLY][2][2], poly_cuts[MAX_POLY];
	int i, i2, j, j2, j3, cuts, maxx = mem_width, maxy = mem_height;
	int oldmode = mem_undo_opacity;
	float ratio;

	mem_undo_opacity = TRUE;
	for ( i=0; i<poly_points; i++ )		// Populate poly_lines - smallest Y is first point
	{
		i2 = i+1;
		if ( i2 >= poly_points ) i2 = 0;	// Remember last point back to 1st point

		if ( poly_mem[i][1] < poly_mem[i2][1] )
			j = 0;
		else
			j = 1;

		poly_lines[i][j][0] = poly_mem[i][0];
		poly_lines[i][j][1] = poly_mem[i][1];
		poly_lines[i][1-j][0] = poly_mem[i2][0];
		poly_lines[i][1-j][1] = poly_mem[i2][1];

		if (!filled)
		{
			f_circle(poly_mem[i][0], poly_mem[i][1], tool_size);
			tline(poly_mem[i][0], poly_mem[i][1],
				poly_mem[i2][0], poly_mem[i2][1], tool_size);
		}
		else if (!buf) sline(poly_mem[i][0], poly_mem[i][1],
			poly_mem[i2][0], poly_mem[i2][1]);
		else
		{
			line_init(line, poly_mem[i][0] - poly_min_x, poly_mem[i][1] - poly_min_y,
				poly_mem[i2][0] - poly_min_x, poly_mem[i2][1] - poly_min_y);
			for (; line[2] >= 0; line_step(line))
			{
				buf[line[0] + line[1] * wbuf] = 255;
			}
		}
		// Outline is needed to properly edge the polygon
	}

	if (!filled)		// If drawing outline only, finish now
	{
		mem_undo_opacity = oldmode;
		return;
	}

	if ( poly_min_y < 0 ) poly_min_y = 0;		// Vertical clipping
	if ( poly_max_y >= maxy ) poly_max_y = maxy-1;

	for ( j=poly_min_y; j<=poly_max_y; j++ )	// Scanline
	{
		cuts = 0;			
		for ( i=0; i<poly_points; i++ )		// Count up line intersections - X value cuts
		{
			if ( j >= poly_lines[i][0][1] && j <= poly_lines[i][1][1] )
			{
				if ( poly_lines[i][0][1] == poly_lines[i][1][1] )
				{	// Line is horizontal so use each end point as a cut
					poly_cuts[cuts++] = poly_lines[i][0][0];
					poly_cuts[cuts++] = poly_lines[i][1][0];
				}
				else	// Calculate cut X value - intersection on y=j
				{
					ratio = ( (float) j - poly_lines[i][0][1] ) /
						( poly_lines[i][1][1] - poly_lines[i][0][1] );
					poly_cuts[cuts++] = poly_lines[i][0][0] +
						rint(ratio * ( poly_lines[i][1][0] -
						poly_lines[i][0][0]));
					if ( j == poly_lines[i][0][1] )	cuts--;
							// Don't count start point
				}
			}
		}
		for ( i=cuts-1; i>0; i-- )	// Sort cuts table - the venerable bubble sort
		{
			for ( i2=0; i2<i; i2++ )
			{
				if ( poly_cuts[i2] > poly_cuts[i2+1] )
				{
					j2 = poly_cuts[i2];
					poly_cuts[i2] = poly_cuts[i2+1];
					poly_cuts[i2+1] = j2;
				}
			}
		}

			// Paint from first X to 2nd, gap from 2-3, paint 3-4, gap 4-5 ...

		for ( i=0; i<(cuts-1); i=i+2 )
		{
			if ( poly_cuts[i] < maxx && poly_cuts[i+1] >= 0 )
			{
				if ( poly_cuts[i] < 0 ) poly_cuts[i] = 0;
				if ( poly_cuts[i] >= maxx ) poly_cuts[i] = maxx-1;
				if ( poly_cuts[i+1] < 0 ) poly_cuts[i+1] = 0;	// Horizontal Clipping
				if ( poly_cuts[i+1] >= maxx ) poly_cuts[i+1] = maxx-1;

				if (buf)
				{
					j3 = (j - poly_min_y) * wbuf + poly_cuts[i] - poly_min_x;
					memset(buf + j3, 255, poly_cuts[i + 1] - poly_cuts[i] + 1);
				}
				else
				{
					for ( i2=poly_cuts[i]; i2<=poly_cuts[i+1]; i2++ )
						put_pixel( i2, j );
				}
			}
		}
	}
	mem_undo_opacity = oldmode;
}

void poly_mask()	// Paint polygon onto clipboard mask
{
	mem_clip_mask_init(0);		/* Clear mask */
	if (!mem_clip_mask) return;	/* Failed to get memory */
	poly_draw(TRUE, mem_clip_mask, mem_clip_w);
}

void poly_paint()	// Paint polygon onto image
{
	poly_draw(TRUE, NULL, 0);
}

void poly_outline()	// Paint polygon outline onto image
{
	poly_draw(FALSE, NULL, 0);
}

void poly_add(int x, int y)	// Add point to list
{
	if (!poly_points)
	{
		poly_min_x = poly_max_x = x;
		poly_min_y = poly_max_y = y;
	}
	else
	{
		if (poly_points >= MAX_POLY) return;
		if (poly_min_x > x) poly_min_x = x;
		if (poly_max_x < x) poly_max_x = x;
		if (poly_min_y > y) poly_min_y = y;
		if (poly_max_y < y) poly_max_y = y;
	}

	poly_mem[poly_points][0] = x;
	poly_mem[poly_points][1] = y;
	poly_points++;
}


void flood_fill_poly( int x, int y, unsigned int target )
{
	int minx = x, maxx = x, ended = 0, newx = 0;

	mem_clip_mask[ x + y*mem_clip_w ] = 1;
	while ( ended == 0 )				// Search left for target pixels
	{
		minx--;
		if ( minx < 0 ) ended = 1;
		else
		{
			if ( mem_clipboard[ minx + y*mem_clip_w ] == target )
				mem_clip_mask[ minx + y*mem_clip_w ] = 1;
			else ended = 1;
		}
	}
	minx++;

	ended = 0;
	while ( ended == 0 )				// Search right for target pixels
	{
		maxx++;
		if ( maxx >= mem_clip_w ) ended = 1;
		else
		{
			if ( mem_clipboard[ maxx + y*mem_clip_w ] == target )
				mem_clip_mask[ maxx + y*mem_clip_w ] = 1;
			else ended = 1;
		}
	}
	maxx--;

	if ( (y-1) >= 0 )				// Recurse upwards
		for ( newx = minx; newx <= maxx; newx++ )
			if ( mem_clipboard[ newx + (y-1)*mem_clip_w ] == target &&
				mem_clip_mask[newx + mem_clip_w*(y-1)] != 1 )
					flood_fill_poly( newx, y-1, target );

	if ( (y+1) < mem_clip_h )			// Recurse downwards
		for ( newx = minx; newx <= maxx; newx++ )
			if ( mem_clipboard[ newx + (y+1)*mem_clip_w ] == target &&
				mem_clip_mask[newx + mem_clip_w*(y+1)] != 1 )
					flood_fill_poly( newx, y+1, target );
}

void flood_fill24_poly( int x, int y, int target )
{
	int minx = x, maxx = x, ended = 0, newx = 0;

	mem_clip_mask[ x + y*mem_clip_w ] = 1;
	while ( ended == 0 )				// Search left for target pixels
	{
		minx--;
		if ( minx < 0 ) ended = 1;
		else
		{
			if ( MEM_2_INT(mem_clipboard, 3*(minx + mem_clip_w*y) ) == target )
				mem_clip_mask[ minx + y*mem_clip_w ] = 1;
			else ended = 1;
		}
	}
	minx++;

	ended = 0;
	while ( ended == 0 )				// Search right for target pixels
	{
		maxx++;
		if ( maxx >= mem_clip_w ) ended = 1;
		else
		{
			if ( MEM_2_INT(mem_clipboard, 3*(maxx + mem_clip_w*y) ) == target )
				mem_clip_mask[ maxx + y*mem_clip_w ] = 1;
			else ended = 1;
		}
	}
	maxx--;

	if ( (y-1) >= 0 )				// Recurse upwards
		for ( newx = minx; newx <= maxx; newx++ )
			if ( MEM_2_INT(mem_clipboard, 3*(newx + mem_clip_w*(y-1)) ) == target
				&& mem_clip_mask[newx + mem_clip_w*(y-1)] != 1 )
					flood_fill24_poly( newx, y-1, target );

	if ( (y+1) < mem_clip_h )			// Recurse downwards
		for ( newx = minx; newx <= maxx; newx++ )
			if ( MEM_2_INT(mem_clipboard, 3*(newx + mem_clip_w*(y+1)) ) == target
				&& mem_clip_mask[newx + mem_clip_w*(y+1)] != 1 )
					flood_fill24_poly( newx, y+1, target );
}

void poly_lasso()		// Lasso around current clipboard
{
	int i, j, x = 0, y = 0;

	if (!mem_clip_mask) return;	/* Nothing to do */

	/* Fill seed is the first point of polygon, if any,
	 * or top left corner of the clipboard by default */
	if (poly_points)
	{
		x = poly_mem[0][0] - poly_min_x;
		y = poly_mem[0][1] - poly_min_y;
	}

	if ( mem_clip_bpp == 1 )
	{
		j = mem_clipboard[x + y*mem_clip_w];
		flood_fill_poly( x, y, j );
	}
	if ( mem_clip_bpp == 3 )
	{
		i = 3*(x + y*mem_clip_w);
		j = MEM_2_INT(mem_clipboard, i);
		flood_fill24_poly( x, y, j );
	}

	j = mem_clip_w*mem_clip_h;
	for ( i=0; i<j; i++ )
		if ( mem_clip_mask[i] == 1 ) mem_clip_mask[i] = 0;
			// Turn flood (1) into clear (0)
}
