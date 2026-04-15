#include "Cell.h"

Cell::Cell(int r, int c, Cell* p, double g_val, double h_val)
{
	row = r;
	col = c;
	parent = p;
	g = g_val;
	h = h_val;
	f = g + h;
}