#pragma once

class Cell
{
private:
	int row, col;
	Cell* parent;
	double g, h, f;

public:
	Cell(int r, int c, Cell* p, double g_val, double h_val);
	int getRow() const { return row; }
	int getCol() const { return col; }
	Cell* getParent() const { return parent; }
	double getG() const { return g; }
	double getH() const { return h; }
	double getF() const { return f; }
};