// Released under the MIT License; see LICENSE
// Copyright (c) 2021 José Cordeiro

// Some naive implementations of linear algebra functions
// not typically available as primitives in APL.

#include <math.h>
#include <string.h>

#include "apl.h"

#define	MAT(r_,c_)	mat[(r_)*nc+(c_)]
#define	MATL(r_,c_)	matl[(r_)*nc+(c_)]
#define	ROW(r_)		&mat[(r_)*nc]
#define	ROW_SIZE	(nc*sizeof(double))
#define	SWAP_ROWS(i_,j_)	{			\
	memcpy(tmp, ROW(i_), ROW_SIZE);		\
	memcpy(ROW(i_), ROW(j_), ROW_SIZE);	\
	memcpy(ROW(j_), tmp, ROW_SIZE);		\
}

// Transform matrix 'mat' into its Reduced Row Echelon Form
// Return the rank of the square sub-matrix (non-zero pivots)
int MatRref(double *mat, int nr, int nc)
{
	int rank = 0;
	int maxc = min(nr, nc);	// Restrict rank to this size

	// Temp row used for swapping rows
	// Allocated only when needed
	double *tmp = 0;
	double pivot;
	double mult;

//#define	RREF_DEBUG
#ifdef	RREF_DEBUG
	print_line("\nMatRref() initial matrix\n");
	DescPrintln(poprTop);
#endif

	int c = 0;	// Current lead column

	// Scan all rows
	for (int r = 0; r < nr; ++r) {
		if (c >= nc) goto end;
		int i = r;
#if	1	// Partial pivoting
		// Find largest pivot in this column
		pivot = 0.0;
		do {
			for (int ii = r; ii < nr; ++ii) {
				double temp = fabs(MAT(ii,c));
				if (temp > pivot) {
					i = ii;
					pivot = temp;
				}
			}
			if (pivot == 0.0) {	// No pivot in this column
				// Advance to next one
				if (++c == nc) goto end;	// Done
			}
		} while (pivot == 0.0);
#else	// Just get the first non-zero pivot
		while ((pivot = MAT(i,c)) == 0.0) {
			++i;
			if (i == nr) {
				i = r;
				++c;
				if (c == nc) goto end;
			}
		}
#endif
		if (i != r) {
			if (!tmp)
				tmp = TempAlloc(sizeof(double), nc);
			SWAP_ROWS(i,r);
#ifdef	RREF_DEBUG
			print_line("\nSwapped rows %d and %d\n", i, r);
			DescPrintln(poprTop);
#endif
		}
		// Found one pivot
		if (c < maxc)
			++rank;
		double *row_r;
#ifdef	RREF_DEBUG
		print_line("\nPivot = %g\n", mult);
#endif
		if (pivot != 1.0) {
			// Multiply row r by 1/M[r,c]
			mult = 1.0 / pivot;
			row_r = ROW(r);
			for (int j = 0; j < nc; ++j)
				*row_r++ *= mult;
			MAT(r,c) = 1.0;	// Force 1, avoid precision errors
#ifdef	RREF_DEBUG
			print_line("\nMultiplied row %d by %g\n", r, mult);
			DescPrintln(poprTop);
#endif
		}
		for (i = 0; i < nr; ++i) {
			mult = MAT(i,c);
			if (i != r && mult != 0.0) {
				row_r = ROW(r);
				double *row_i = ROW(i);
				for (int k = 0; k < nc; ++k) {
					*row_i++ -= *row_r++ * mult;
				}
				MAT(i,c) = 0.0; // Force 0, avoid precision errors
#ifdef	RREF_DEBUG
				print_line("\nSubtracted row %d x %g from row %d\n", r, mult, i);
				DescPrintln(poprTop);
#endif
			}
		}
		++c;
	}

end:
#ifdef	RREF_DEBUG
	print_line("\nRank=%d\n", rank);
#endif
	return rank;
}

int MatLU(double *matl, int nr, int nc)
{
	double *mat = matl + nr * nc;
	int rank = 0;
	int maxc = min(nr, nc);	// Restrict rank to this size

	// Temp row used for swapping rows
	// Allocated only when needed
	double *tmp = 0;
	double pivot;
	double mult;

//#define	RREF_DEBUG
#ifdef	RREF_DEBUG
	print_line("\nMatLU() initial matrix\n");
	DescPrintln(poprTop);
#endif

	int c = 0;	// Current lead column

	// Scan all rows
	for (int r = 0; r < nr; ++r) {
		if (c >= nc) goto end;
		int i = r;
#if	1	// Partial pivoting
		// Find largest pivot in this column
		pivot = 0.0;
		do {
			for (int ii = r; ii < nr; ++ii) {
				double temp = fabs(MAT(ii,c));
				if (temp > pivot) {
					i = ii;
					pivot = temp;
				}
			}
			if (pivot == 0.0) {	// No pivot in this column
				// Advance to next one
				if (++c == nc) goto end;	// Done
			}
		} while (pivot == 0.0);
#else	// Just get the first non-zero pivot
		while ((pivot = MAT(i,c)) == 0.0) {
			++i;
			if (i == nr) {
				i = r;
				++c;
				if (c == nc) goto end;
			}
		}
#endif
		if (i != r) {
			if (!tmp)
				tmp = TempAlloc(sizeof(double), nc);
			SWAP_ROWS(i,r);
#ifdef	RREF_DEBUG
			print_line("\nSwapped rows %d and %d\n", i, r);
			DescPrintln(poprTop);
#endif
		}
		// Found one pivot
		if (c < maxc)
			++rank;
#ifdef	RREF_DEBUG
		print_line("\nPivot = %g\n", pivot);
#endif
		MATL(r,c) = 1.0;
		for (i = r + 1; i < nr; ++i) {
			mult = MAT(i,c) / pivot;
			MATL(i,c) = mult;
			if (mult != 0.0) {
				double *row_r = ROW(r);
				double *row_i = ROW(i);
				for (int k = 0; k < nc; ++k) {
					*row_i++ -= *row_r++ * mult;
				}
				MAT(i,c) = 0.0; // Force 0, avoid precision errors
#ifdef	RREF_DEBUG
				print_line("\nSubtracted row %d x %g from row %d\n", r, mult, i);
				DescPrintln(poprTop);
#endif
			}
		}
		++c;
	}

end:
#ifdef	RREF_DEBUG
	print_line("\nRank=%d\n", rank);
#endif
	return rank;
}
