/*--------------------------------------------------------------------
 *	$Id$
 *
 *	Copyright (c) 2004-2013 by J. Luis and J. M. Miranda
 *
 * 	This program is part of Mirone and is free software; you can redistribute
 * 	it and/or modify it under the terms of the GNU Lesser General Public
 * 	License as published by the Free Software Foundation; either
 * 	version 2.1 of the License, or any later version.
 * 
 * 	This program is distributed in the hope that it will be useful,
 * 	but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * 	Lesser General Public License for more details.
 *
 *	Contact info: w3.ualg.pt/~jluis/mirone
 *--------------------------------------------------------------------*/


/*
 *	Original Fortran version of core hydrodynamic code by J.M. Miranda
 *
 *	Translated to C & mexified By
 *	Joaquim Luis - 2013

 */

#include "mex.h"
#include "mwsize.h"
#include <netcdf.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <time.h>

#if HAVE_OPENMP
#include <omp.h>
#endif

#define	FALSE	0
#define	TRUE	1
#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif
#define D2R		M_PI / 180.

#define	NORMAL_GRAV	9.806199203	/* Moritz's 1980 IGF value for gravity at 45 degrees latitude */
#define EPS12 1e-12
#define EPS10 1e-10
#define EPS6 1e-6
#define EPS5 1e-5
#define EPS3 1e-3
#define EPS2 1e-2

#define MAXRUNUP -50 	/* Do not waste time computing flood above this altitude */

#define CNULL	((char *)NULL)
#define Loc_copysign(x,y) ((y) < 0.0 ? -fabs(x) : fabs(x))

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))	/* min and max value macros */
#endif
#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef rint
#define rint(x) (floor((x)+0.5))
#endif
#ifndef irint
#define irint(x) ((int)rint(x))
#endif

#define ijs(i,j,n) ((i) + (j)*n)
#define ijc(i,j) ((i) + (j)*n_ptmar)
#define ij_grd(col,row,hdr) ((col) + (row)*hdr.nx)

struct srf_header {		/* Surfer file hdr structure */
	char id[4];		/* ASCII Binary identifier (DSAA/DSBB) */
	short int nx;		/* Number of columns */
	short int ny;		/* Number of rows */
	double x_min;		/* Minimum x coordinate */
	double x_max;		/* Maximum x coordinate */
	double y_min;		/* Minimum y coordinate */
	double y_max;		/* Maximum y coordinate */
	double z_min;		/* Minimum z value */
	double z_max;		/* Maximum z value */
};

struct grd_header {        /* Generic grid hdr structure */
	int nx;            /* Number of columns */
	int ny;            /* Number of rows */
	double x_inc;
	double y_inc;
	double x_min;		/* Minimum x coordinate */
	double x_max;		/* Maximum x coordinate */
	double y_min;		/* Minimum y coordinate */
	double y_max;		/* Maximum y coordinate */
	double z_min;		/* Minimum z value */
	double z_max;		/* Maximum z value */

	int doCoriolis;		/* Apply Coriolis if this != 0 */
	double lat_min4Coriolis;	/* PRECISA SOLUÇÃO. POR AGORA SERÁ Cte = 0 */
};

struct nestContainer {		/* Container for the nestings */
	int LLrow[10], LLcol[10], ULrow[10], ULcol[10], URrow[10], URcol[10], LRrow[10], LRcol[10];
	int incRatio[10];
	double manning2[10];	/* Square of Manning coefficient. Set to zero if no friction */
	double LLx[10], LLy[10], ULx[10], ULy[10], URx[10], URy[10], LRx[10], LRy[10];
	double dt_P[10];                           /* Time step of Parent grid                 */
	double dt[10];                             /* Time step at current level               */
	double *bat[10];                           /* Bathymetry of current level              */
	double *fluxm_a[10],  *fluxm_d[10];        /* t-1/2 & t+1/2 fluxes arrays along X      */
	double *fluxn_a[10],  *fluxn_d[10];        /* t-1/2 & t+1/2 fluxes arrays along Y      */
	double *htotal_a[10], *htotal_d[10];       /* t-1/2 & t+1/2 total whater depth         */
	double *vex[10], *vey[10];                 /* X,Y velocity components                  */
	double *etaa[10], *etad[10];               /* t-1/2 & t+1/2 whater height (eta) arrays */
	double *edge_col[10], *edge_colTmp[10];
	double *edge_row[10], *edge_rowTmp[10];
	double *edge_col_P[10], *edge_col_Ptmp[10];
	double *edge_row_P[10], *edge_row_Ptmp[10];
	double *r0[10], *r1m[10], *r1n[10], *r2m[10], *r2n[10], *r3m[10], *r3n[10], *r4m[10], *r4n[10];
	double *etaa_L0, *etad_L0, *bat_L0;        /* To hold copies of the pointers of the     */
	double *fluxm_a_L0, *fluxn_a_L0;           /* variables of the base level (less the L0) */
	double *fluxm_d_L0, *fluxn_d_L0;           /*              ""                   */
	double *htotal_a_L0, *htotal_d_L0;
	struct grd_header hdr[10];
	struct grd_header hdr_P[10];
	double time_h;
	double *zz;
	int 	nn;
};

void no_sys_mem (char *where, mwSize n);
int count_col (char *line);
int read_grd_info_ascii (char *file, struct srf_header *hdr);
int read_header_bin (FILE *fp, struct srf_header *hdr);
int write_grd_bin(char *name, double x_min, double y_min, double x_inc, double y_inc, mwSize i_start, 
		mwSize j_start, mwSize i_end, mwSize j_end, mwSize nX, float *work);
int read_grd_ascii (char *file, struct srf_header *hdr, double *work);
int read_grd_bin (char *file, struct srf_header *hdr, double *work);
int read_maregs(struct grd_header hdr, char *file, int *lcum, int nx);
int count_n_maregs(char *file);
int decode_R (char *item, double *w, double *e, double *s, double *n);
int check_region (double w, double e, double s, double n);
double ddmmss_to_degree (char *text);
void err_trap(mwSize status);
void write_most_slice(mwSize *ncid_most, mwSize *ids_most, mwSize i_start, mwSize j_start, mwSize i_end, mwSize j_end, mwSize nX, float *work, double *h, double *dep, double *u, double *v, float *tmp, size_t *start, size_t *count);
int open_most_nc (char *basename, char *name_var, mwSize *ids, mwSize nx, mwSize ny, double dx, double dy, double xMinOut, double yMinOut);
int open_anuga_sww (char *fname_sww, mwSize *ids, mwSize i_start, mwSize j_start, mwSize i_end, mwSize j_end, mwSize nX, double dx, double dy, double *dep, double xMinOut, double yMinOut, float z_min, float z_max);
void write_anuga_slice(mwSize ncid, mwSize z_id, mwSize i_start, mwSize j_start, mwSize i_end, mwSize j_end, mwSize nX, float *work, double *h, double *dep, double *u, double *v, float *tmp, size_t *start, size_t *count, float *slice_range, mwSize idx, mwSize with_land);
void mass(struct grd_header hdr, double dt, double *bat, double *etaa, double *htotal_d, double *fluxm_a, double *fluxn_a, double *etad);
void openb(struct grd_header hdr, double *bat, double *fluxm_d, double *fluxn_d, double *etad);
void update(struct grd_header hdr, double *etaa, double *etad, double *fluxm_a, double *fluxm_d,
	double *fluxn_a, double *fluxn_d, double *htotal_a, double *htotal_d);
void moment(struct grd_header hdr, double dt, double manning2, double *htotal_a, double *htotal_d, double *bat, double *etad, double *fluxm_a, double *fluxn_a, double *fluxm_d, double * fluxn_d, double *vex, double *vey, int isNested);
void inisp(struct grd_header hdr, double dt, double *r0, double *r1m, double *r1n, double *r2m, double *r2n, double *r3m, double *r3n, double *r4m, double *r4n);
void mass_sp(struct grd_header hdr, double *bat, double *etaa, double *htotal_d, double *fluxm_a,
	double *fluxn_a, double *etad, double *r0, double * r1m, double *r1n, double *r2m, double *r2n);
void moment_sp(struct grd_header hdr, double dt, double manning2, double *htotal_a, double *htotal_d, double *bat, double *etad, double *fluxm_a, double *fluxn_a, double *fluxm_d, double * fluxn_d, double *vex, double *vey, double *r0, double *r1m, double *r1n, double *r2m, double *r2n, double *r3m, double *r3n, double *r4m, double *r4n, int isNested);
int initialize_nestum(struct nestContainer *nest, struct grd_header hdr, int isGeog, int lev);
void interp_edges(struct nestContainer *nest, double *flux_L1, double *flux_L2, char *what, int lev);
int intp_lin (double *x, double *y, int n, int m, double *u, double *v);
void upscale(struct nestContainer *nest, double *out, int lev, int i_tsr);
void sanitize_nestContainer(struct nestContainer *nest);
void nestify(struct nestContainer *nest, int nNg, int recursionLevel, int isGeog);
void edge_communication(struct nestContainer *nest, int m);
void mass_conservation(struct nestContainer *nest, int isGeog, int m);
void moment_conservation(struct nestContainer *nest, int isGeog, int m);
void upscale_(struct nestContainer *nest, double *etad, int lev, int i_tsr);

/* --------------------------------------------------------------------------- */
/* Matlab Gateway routine */

void mexFunction(mwSize nlhs, mxArray *plhs[], mwSize nrhs, const mxArray *prhs[]) {

	float	*work, dz, *ptr_mov_32, *mov_32, *time_p;
	float	work_min = FLT_MAX, work_max = -FLT_MAX;
	double	*h, *h_bak, *zm;
	double	*xpp, x, y, small = 1e-6, m_per_deg = 111317.1;
	double	x_min, y_min, *head, *tmp;
	double	*bat, *dep1, *dep2 = NULL, *cum_p, cang, angltt, dumb;
	double	x_inc, y_inc, x_tmp, y_tmp;		/* Used in the maregs positiojn test */
	double	*ptr_wb; 		/* Pointer to be used in the aguentabar */
	double	dfXmin = 0.0, dfYmin = 0.0, dfXmax = 0.0, dfYmax = 0.0, xMinOut, yMinOut;
	double	time_jump = 0, time0, time_for_anuga, prc;
	double	dt;			/* Time step for Base level grid */

	double *etaa, *etad, *fluxm_a, *fluxm_d, *fluxn_a, *fluxn_d, *htotal_a, *htotal_d;
	double	*vmax, *wmax, *vex, *vey, *r0, *r1m, *r1n, *r2m, *r2n, *r3m, *r3n, *r4m, *r4n;
	double	dx, dy, etam, vel2, one_100;
	double	manning2 = 0.;
	double	time_h = 0;
	int	IamBechmark1 = TRUE;
	int	row, col;
	int	writeLevel = 0;		/* If save grids, will hold the saving level (when nesting) */

	mwSize	i, j, k, m, n, i2, ip2, ij, ix, jy, lcum, n_mareg, n_ptmar, cycle;
	mwSize	i_start, j_start, i_end, j_end;
	int	grn = 0, cumint = 0, *lcum_p = NULL, iprc;
	int	w_bin = TRUE, cumpt = FALSE, error = FALSE;
	int	first_anuga_time = TRUE, out_sww = FALSE, out_most = FALSE;
	mwSize	r_bin_b, r_bin_f, surf_level = TRUE, max_level = FALSE, water_depth = FALSE;
	mwSize	argc, n_arg_no_char = 0, nx, ny, dims[3], n_frames = 0;
	mwSize	ncid, ncid_most[3], z_id = -1, ids[13], ids_ha[6], ids_ua[6], ids_va[6], ids_most[3];
	int	n_of_cycles = 1010;	/* Numero de ciclos a calcular */
	int	num_of_nestGrids = 0;	/* Number of nesting grids */
	char	*bathy = NULL;		/* Name pointer for bathymetry file */
	char 	*params = NULL;		/* Name pointer for parameters file */
	char 	*hcum = NULL;		/* Name pointer for cumulative hight file */
	char 	*maregs = NULL;		/* Name pointer for maregraph positions file */
	char 	*fname_sww = NULL;	/* Name pointer for Anuga's .sww file */
	char 	*basename_most = NULL;	/* Name pointer for basename of MOST .nc files */
	char	*fonte = NULL;		/* Name pointer for tsunami source file */
	char	stem[256] = "", prenome[128] = "", cmd[16];
	char	**argv, *pch;
	unsigned char	*ptr_mov_8, *mov_8, *mov_8_tmp;
	unsigned int nm, ncl;
	int	bat_in_input = FALSE, source_in_input = FALSE, write_grids = FALSE, isGeog = FALSE;
	int	maregs_in_input = FALSE, out_momentum = FALSE, got_R = FALSE;
	int	with_land = FALSE, IamCompiled = FALSE, do_nestum = FALSE, saveNested = FALSE, verbose = FALSE;
	int out_velocity = FALSE, out_velocity_x = FALSE, out_velocity_y = FALSE, out_velocity_r = FALSE;

	size_t	start0 = 0, count0 = 1, start1_A[2] = {0,0}, count1_A[2], start1_M[3] = {0,0,0}, count1_M[3];
	float	stage_range[2], xmom_range[2], ymom_range[2], *tmp_slice;
	struct	srf_header hdr_b, hdr_f;
	struct	grd_header hdr;
	struct  nestContainer nest;
	mxArray *rhs[3];
	FILE	*fp;
#ifdef MIR_TIMEIT
	clock_t toc, tic;
#endif

	bathy = "bathy.grd";
	params = "swan.par";
	hcum = "maregs.dat";
	fonte = "fonte.grd";
	maregs = "mareg.xy";

	sanitize_nestContainer(&nest);

	argc = nrhs;
	for (i = 0; i < nrhs; i++) {		/* Check input to find how many arguments are of type char */
		if (!mxIsChar(prhs[i])) {
			argc--;
			n_arg_no_char++;	/* Number of arguments that have a type other than char */
		}
	}

	if (n_arg_no_char >= 5) {		/* Not all combinations are tested */
		/* Bathymetry */
		dep1 = mxGetPr(prhs[0]);
		nx = mxGetN (prhs[0]);
		ny = mxGetM (prhs[0]);
		head  = mxGetPr(prhs[1]);	/* Get bathymetry header info */
		hdr_b.x_min = head[0];		hdr_b.x_max = head[1];
		hdr_b.y_min = head[2];		hdr_b.y_max = head[3];
		hdr_b.z_min = head[4];		hdr_b.z_max = head[5];
		hdr_b.nx = nx;			hdr_b.ny = ny;
		bat_in_input = TRUE;
		/* Tsunami source */
		h = mxGetPr(prhs[2]);
		nx = mxGetN (prhs[2]);
		ny = mxGetM (prhs[2]);
		head  = mxGetPr(prhs[3]);	/* Get bathymetry header info */
		hdr_f.x_min = head[0];		hdr_f.x_max = head[1];
		hdr_f.y_min = head[2];		hdr_f.y_max = head[3];
		hdr_f.z_min = head[4];		hdr_f.z_max = head[5];
		hdr_f.nx = nx;				hdr_f.ny = ny;
		source_in_input = TRUE;
		/* Now test if bat & source are compatible. If they are not, we go out right here. */
		if (hdr_f.nx != hdr_b.nx || hdr_f.ny != hdr_b.ny) {
			mexPrintf ("Bathymetry & Source grids have different nx and/or ny\n"); 
			mexPrintf ("%d %d %d %d\n", hdr_f.nx, hdr_b.nx, hdr_f.ny, hdr_b.ny); 
			return;
		}
		if ( fabs(hdr_f.x_min - hdr_b.x_min) > small || fabs(hdr_f.x_max - hdr_b.x_max) > small ||
			fabs(hdr_f.y_min - hdr_b.y_min) > small || fabs(hdr_f.y_max - hdr_b.y_max) > small ) {
			mexPrintf ("Bathymetry & Source grids do not cover the same region\n"); 
			mexPrintf ("%lf %lf %lf %lf\n", hdr_f.x_min, hdr_b.x_min, hdr_f.x_max, hdr_b.x_max); 
			mexPrintf ("%lf %lf %lf %lf\n", hdr_f.y_min, hdr_b.y_min, hdr_f.y_max, hdr_b.y_max); 
			return;
		}

		/* Now "read" the parameters */
		tmp = mxGetPr(prhs[4]);
		i = mxGetN (prhs[4]);
		if (i < 7) {
			mexPrintf("Params input argument has a wrong (=%d) number of arguments (should be 7)\n", i);
			return;
		}
		dt = tmp[0];		grn = (int)tmp[1];
		isGeog = (int)tmp[6];
	}
	if (n_arg_no_char == 6 && !mxIsCell(prhs[5])) {		/* A maregraph vector was given as the sixth argument*/
		tmp = mxGetPr(prhs[5]);
		n_mareg = mxGetM(prhs[5]);
		dx = head[7];		dy = head[8];
		lcum_p = (mwSize *) mxCalloc ((size_t)(n_mareg), sizeof(mwSize));
		for (i = 0; i < n_mareg; i++) {
			x = tmp[i];		y = tmp[i+n_mareg];	/* Matlab vectors are stored by columns */
			lcum_p[i] = (irint((y - hdr_b.y_min) / dy) ) * hdr_b.nx + irint((x - hdr_b.x_min) / dx);
		}
		maregs_in_input = TRUE;
		cumpt = TRUE;
	}

	if (n_arg_no_char == 6 && mxIsCell(prhs[5])) {
		const mxArray *mx_ptr;

		num_of_nestGrids = mxGetNumberOfElements(prhs[5]);
		if (num_of_nestGrids % 2 != 0)
			mexErrMsgTxt("BARNABEU: Cell array with nested grids is not a even number of elements");
		num_of_nestGrids /= 2;

		for (k = 0; k < num_of_nestGrids; k++) {
			mx_ptr = mxGetCell(prhs[5], k);
			if (mx_ptr == NULL)
				mexErrMsgTxt("BARNABEU: bloody cell element is empty!!!!!!!");
			dep2 = mxGetPr(mx_ptr);		/* Get the grid for this level */
			nest.hdr[k].nx = mxGetN (mx_ptr);
			nest.hdr[k].ny = mxGetM (mx_ptr);
			mx_ptr = mxGetCell(prhs[5], k + num_of_nestGrids);
			head  = mxGetData(mx_ptr);	/* Get header info */
			nest.hdr[k].x_min = head[0];		nest.hdr[k].x_max = head[1];
			nest.hdr[k].y_min = head[2];		nest.hdr[k].y_max = head[3];
			nest.hdr[k].z_min = head[4];		nest.hdr[k].z_max = head[5];
			nest.hdr[k].x_inc = head[7];		nest.hdr[k].y_inc = head[8];

			if (k == 0)	nest.dt_P[k] = dt;	/* The others are initialized latter by initialize_nestum() */

			nm = nest.hdr[k].nx * nest.hdr[k].ny;
			if ((nest.bat[k] = (double *)mxCalloc ((size_t)nm, sizeof(double)) ) == NULL) 
				{no_sys_mem("(bat)", nm);}
			for (i = 0; i < nest.hdr[k].ny; i++) {
				for (j = 0; j < nest.hdr[k].nx; j++)
					nest.bat[k][i*nest.hdr[k].nx+j] = -dep2[j*nest.hdr[k].ny+i];
			}
		}
		do_nestum = TRUE;
	}

	if (n_arg_no_char == 7) {
		dep2 = mxGetPr(prhs[5]);
		nest.hdr[0].nx = mxGetN (prhs[5]);
		nest.hdr[0].ny = mxGetM (prhs[5]);
		head  = mxGetPr(prhs[6]);	/* Get bathymetry header info */
		nest.hdr[0].x_min = head[0];		nest.hdr[0].x_max = head[1];
		nest.hdr[0].y_min = head[2];		nest.hdr[0].y_max = head[3];
		nest.hdr[0].z_min = head[4];		nest.hdr[0].z_max = head[5];
		nest.hdr[0].x_inc = head[7];		nest.hdr[0].y_inc = head[8];
		nest.dt_P[0] = dt;
		nm = nest.hdr[0].nx * nest.hdr[0].ny;
		if ((nest.bat[0] = (double *)mxCalloc ((size_t)(nm), sizeof(double)) ) == NULL) 
			{no_sys_mem("(bat)", nm);}
		for (i = 0; i < nest.hdr[0].ny; i++) {
			for (j = 0; j < nest.hdr[0].nx; j++)
				nest.bat[0][i*nest.hdr[0].nx+j] = -dep2[j*nest.hdr[0].ny+i];
		}
		do_nestum = TRUE;
	}

	/* get the length of the input string */
	argv = (char **)mxCalloc(argc, sizeof(char *));
	for (i = 0; i < argc; i++)
		argv[i] = (char *)mxArrayToString(prhs[i+n_arg_no_char]);

	for (i = 0; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				case 'e':
					IamCompiled = TRUE;
					break;
				case 'f':	/* */
					isGeog = TRUE;
					break;
				case 'n':	/* Write MOST files (*.nc) */
					basename_most  = &argv[i][2];
					out_most = TRUE;
					break;
				case 'A':	/* Name for the Anuga .sww netCDF file */
					fname_sww  = &argv[i][2];
					out_sww = TRUE;
					break;
				case 'B':	/* File with batymetry */
					bathy  = &argv[i][2];
					break;
				case 'D':
					water_depth = TRUE;
					surf_level = FALSE;
					break;
				case 'F':	/* File with the source grid */
					fonte  = &argv[i][2];
					break;
				case 'G':	/* Write grids at grn (see params) intervals */
					sscanf (&argv[i][2], "%s/%d", &stem, &grn);
					if ( (pch = strstr(stem,"/")) != NULL) {
						pch[0] = '\0';		/* Strip the "/num" part that was, WTF??, read too */
					}
					if ( (pch = strstr(stem,"|")) != NULL) {
						grn = atoi(&pch[1]);
						pch[0] = '\0';		/* Strip the "|num" part */
					}
					if ( (pch = strstr(stem,"+")) != NULL) {
						writeLevel = atoi(pch++) - 1;		/* -1 because first nested grid has level == 0 */
						if (writeLevel < 0) writeLevel = 0;
						pch--;
						pch[0] = '\0';		/* Hide the +lev from the stem string */
						saveNested = TRUE;
					}
					write_grids = TRUE;
					if (!stem || grn == 0) {
						mexPrintf("NSWING: Error, -G option, must provide base name and saving interval\n");
						error++;
					}
					break;
				case 'J':
					sscanf (&argv[i][2], "%f", &time_jump);
					break;
				case 'L':	/* Output land nodes in SWW file */ 
					with_land = TRUE;
					break;
				case 'M':
					max_level = TRUE;
					surf_level = FALSE;
					break;
				case 'N':	/* Number of cycles to compute */
					n_of_cycles = atoi(&argv[i][2]);
					break;
				case 'O':	/* Time interval and File name for maregraph data */
					sscanf (&argv[i][2], "%d/%s", &cumint, &hcum);
					break;
				case 'R':
					error += decode_R (argv[i], &dfXmin, &dfXmax, &dfYmin, &dfYmax);
					got_R = TRUE;
					break;
				case 's':	/* Output velocity grids */ 
					if (&argv[i][2] == "x")        /* Only X component */
						out_velocity_x = TRUE;
					else if (&argv[i][2] == "y")    /* Only Y component */
						out_velocity_y = TRUE;
					else if (&argv[i][2] == "r")    /* Speed (velocity module) -- NOT YET -- */
						out_velocity_r = TRUE;
					else {                          /* Both X & Y*/
						out_velocity = TRUE;
						out_velocity_x = TRUE;
						out_velocity_y = TRUE;
					}
					break;
				case 'S':	/* Output momentum grids */ 
					out_momentum = TRUE;
					break;
				case 't':	/* Time step of simulation */ 
					dt = atof(&argv[i][2]);
					if (num_of_nestGrids)
						nest.dt_P[0] = dt;	/* Others are initialized latter by initialize_nestum() */
					break;
				case 'T':	/* File with maregraph positions */
					maregs  = &argv[i][2];
					cumpt = TRUE;
					maregs_in_input = FALSE;
					break;
				case 'V':
					verbose = TRUE;
					break;
				default:
					error = TRUE;
					break;
			}
		}
	}

	if (argc == 0 || error) {
		mexPrintf ("SWAN - Um gerador de tsunamis\n\n");
		mexPrintf ( "usage: swan(bat,hdr_bat,deform,hdr_deform,params, [maregs], [-G<name>[+lev]], [-B<bathy>] [-F<fonte>] [-M] [-N<n_cycles>] [-Rw/e/s/n] [-S] [-s] [-T<mareg>] [-D]\n");
		mexPrintf ("\t-A name of ANUGA file\n");
		mexPrintf ("\t-n basename for MOST triplet files (no extension)\n");
		mexPrintf ("\t-B name of bathymetry file. In case it was not transmited in input.\n");
		mexPrintf ("\t-D write grids with the total water depth\n");
		mexPrintf ("\t-F name of source file (default fonte.grd)\n");
		mexPrintf ("\t-G<stem> write grids at the grn intervals. Append file prefix. Files will be called <stem>#.grd\n");
		mexPrintf ("\t-M write grids of max water level [Default wave surface level]\n");
		mexPrintf ("\t-N number of cycles [Default 1010].\n");
		mexPrintf ("\t-R output grids only in the sub-region enclosed by <west/east/south/north>\n");
		mexPrintf ("\t-s write grids with the velocity. Grid names are appended with _U and _V sufixes.\n");
		mexPrintf ("\t-S write grids with the momentum. i.e velocity times water depth.\n");
		mexPrintf ("\t-T name of maregraph file (default mareg.xy)\n");
		mexPrintf ("\t-e To be used from the Mirone stand-alone version.\n");
		return;
	}

	if (error) return;

	if (!bat_in_input) {			/* If bathymetry & source where not given as arguments, load them */
		r_bin_b = read_grd_info_ascii (bathy, &hdr_b);	/* Para saber como alocar a memoria */
		r_bin_f = read_grd_info_ascii (fonte, &hdr_f);	/* e verificar se as grelhas sao compativeis */
		if (r_bin_b < 0 || r_bin_f < 0) {
			mexPrintf ("Invalid grid. Possibly it is in the Surfer 7 format\n"); 
			error++;
		}

		if (hdr_f.nx != hdr_b.nx || hdr_f.ny != hdr_b.ny) {
			mexPrintf ("Bathymetry and source grids have different rows/columns\n"); 
			mexPrintf ("%d %d %d %d\n", hdr_f.nx, hdr_b.nx, hdr_f.ny, hdr_b.ny); 
			error++;
		}
		if (hdr_f.x_min != hdr_b.x_min || hdr_f.x_max != hdr_b.x_max ||
			hdr_f.y_min != hdr_b.y_min || hdr_f.y_max != hdr_b.y_max) {
			mexPrintf ("Bathymetry and source grids do not cover the same region\n"); 
			mexPrintf ("%lf %lf %lf %lf\n", hdr_f.x_min, hdr_b.x_min, hdr_f.x_max, hdr_b.x_max); 
			mexPrintf ("%lf %lf %lf %lf\n", hdr_f.y_min, hdr_b.y_min, hdr_f.y_max, hdr_b.y_max); 
			error++;
		}
		if (error) return;
	}

	if ( !(write_grids || out_sww || out_most || cumpt) ) {
		mexPrintf("Nothing selected for output (grids, or mregraphs), exiting\n");
		return;
	}
	if ( water_depth && (out_sww || out_most) ) {
		water_depth = FALSE;
		mexPrintf("WARNING: Total water option is not compatible with ANUGA|MOST outputs. Ignoring\n");
	}
	if (out_momentum && (out_sww || out_most)) out_momentum = FALSE;
	if (out_velocity && (out_sww || out_most)) out_velocity = FALSE;

	if (writeLevel > num_of_nestGrids) {
		mexPrintf("Requested save grid level is higher that actual number of nested grids. Using last\n");
		writeLevel = num_of_nestGrids - 1;		/* -1 because first nested grid is level 0 */
	}

	dx = (hdr_b.x_max - hdr_b.x_min) / (hdr_b.nx - 1);
	dy = (hdr_b.y_max - hdr_b.y_min) / (hdr_b.ny - 1);
	x_min = hdr_b.x_min;		y_min = hdr_b.y_min;
	ip2 = hdr_b.nx;
	ncl = hdr_b.nx * hdr_b.ny;
	if (cumpt && !maregs_in_input)
		n_mareg = count_n_maregs(maregs);	/* Count maragraphs number */

	if (cumpt) {
		n_ptmar = n_of_cycles / cumint + 1;
		if ((fp = fopen (hcum, "w")) == NULL) {
			mexPrintf ("%s: Unable to create file %s - exiting\n", "swan", hcum);
			return;
		}
	}

	/* Allocate memory	*/
	if ((bat = (double *) mxCalloc ((size_t)(ncl),	sizeof(double)) ) == NULL) 
		{no_sys_mem("(bat)", ncl);}
	if ((work = (float *) mxCalloc ((size_t)(ncl),	sizeof(float)) ) == NULL) 
		{no_sys_mem("(work)", ncl);}

	if ((etaa = (double *) mxCalloc ((size_t)(ncl),	sizeof(double)) ) == NULL) 
		{no_sys_mem("(etaa)", ncl);}
	if ((etad = (double *) mxCalloc ((size_t)(ncl),	sizeof(double)) ) == NULL) 
		{no_sys_mem("(etad)", ncl);}
	if ((fluxm_a = (double *) mxCalloc ((size_t)(ncl), sizeof(double)) ) == NULL) 
		{no_sys_mem("(fluxm_a)", ncl);}
	if ((fluxm_d = (double *) mxCalloc ((size_t)(ncl), sizeof(double)) ) == NULL) 
		{no_sys_mem("(fluxm_d)", ncl);}
	if ((fluxn_a = (double *) mxCalloc ((size_t)(ncl), sizeof(double)) ) == NULL) 
		{no_sys_mem("(fluxn_a)", ncl);}
	if ((fluxn_d = (double *) mxCalloc ((size_t)(ncl), sizeof(double)) ) == NULL) 
		{no_sys_mem("(fluxn_d)", ncl);}
	if ((htotal_a = (double *) mxCalloc ((size_t)((hdr_b.nx+1) * (hdr_b.ny+1)), sizeof(double)) ) == NULL) 
		{no_sys_mem("(htotal_a)", ncl);}
	if ((htotal_d = (double *) mxCalloc ((size_t)((hdr_b.nx+1) * (hdr_b.ny+1)), sizeof(double)) ) == NULL) 
		{no_sys_mem("(htotal_d)", ncl);}
	if ((vmax = (double *) mxCalloc ((size_t)(ncl),	sizeof(double)) ) == NULL) 
		{no_sys_mem("(vmax)", ncl);}
	if ((wmax = (double *) mxCalloc ((size_t)(ncl),	sizeof(double)) ) == NULL) 
		{no_sys_mem("(wmax)", ncl);}
	if ((vex = (double *) mxCalloc ((size_t)(ncl),	sizeof(double)) ) == NULL) 
		{no_sys_mem("(vex)", ncl);}
	if ((vey = (double *) mxCalloc ((size_t)(ncl),	sizeof(double)) ) == NULL) 
		{no_sys_mem("(vey)", ncl);}

	if (isGeog == 1) {		/* case spherical coordinates  */
		if ((r0 = (double *) mxCalloc ((size_t)(hdr_b.ny),	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r0)", hdr_b.ny);	return;}
		if ((r1m = (double *) mxCalloc ((size_t)(hdr_b.ny),	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r1m)", hdr_b.ny);	return;}
		if ((r1n = (double *) mxCalloc ((size_t)(hdr_b.ny),	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r1n)", hdr_b.ny);	return;}
		if ((r2m = (double *) mxCalloc ((size_t)(hdr_b.ny),	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r2m)", hdr_b.ny);	return;}
		if ((r2n = (double *) mxCalloc ((size_t)(hdr_b.ny),	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r2n)", hdr_b.ny);	return;}
		if ((r3m = (double *) mxCalloc ((size_t)(hdr_b.ny),	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r3m)", hdr_b.ny);	return;}
		if ((r3n = (double *) mxCalloc ((size_t)(hdr_b.ny),	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r3n)", hdr_b.ny);	return;}
		if ((r4m = (double *) mxCalloc ((size_t)(hdr_b.ny),	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r4m)", hdr_b.ny);	return;}
		if ((r4n = (double *) mxCalloc ((size_t)(hdr_b.ny),	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r4n)", hdr_b.ny);	return;}
	}

	if (cumpt && !maregs_in_input) {
		if ((lcum_p = (mwSize *) mxCalloc ((size_t)(n_mareg), sizeof(mwSize)) ) == NULL) 
			{no_sys_mem("(lcum_p)", n_mareg);	return;}
		if ((cum_p = (double *) mxCalloc ((size_t)(n_mareg*n_ptmar), sizeof(double)) ) == NULL) 
			{no_sys_mem("(cum_p)", n_mareg*n_ptmar);	return;}
		if ((time_p = (float *) mxCalloc ((size_t)(n_ptmar), sizeof(float)) ) == NULL) 
			{no_sys_mem("(time_p)", n_ptmar);	return;}
	}
	if (cumpt && maregs_in_input) {		/* lcum_p is already allocated and filled */
		if ((cum_p = (double *) mxCalloc ((size_t)(n_mareg*n_ptmar), sizeof(double)) ) == NULL) 
			{no_sys_mem("(cum_p)", n_mareg*n_ptmar);	return;}
		if ((time_p = (float *) mxCalloc ((size_t)(n_ptmar), sizeof(float)) ) == NULL) 
			{no_sys_mem("(time_p)", n_ptmar);	return;}
	}

	if (!bat_in_input) {			/* If bathymetry & source where not given as arguments, load them */
		if (!r_bin_b)					/* Read bathymetry */
			read_grd_ascii (bathy, &hdr_b, bat);
		else
			read_grd_bin (bathy, &hdr_b, bat);
		if (r_bin_b)					/* Read source */
			read_grd_bin (fonte, &hdr_f, etaa);
		else
			read_grd_ascii (fonte, &hdr_f, etaa);
	}

	hdr.nx = hdr_b.nx;          hdr.ny = hdr_b.ny;
	hdr.x_inc = dx;             hdr.y_inc = dy;
	hdr.x_min = hdr_b.x_min;    hdr.x_max = hdr_b.x_max;
	hdr.y_min = hdr_b.y_min;    hdr.y_max = hdr_b.y_max;
	hdr.z_min = hdr_b.z_min;    hdr.z_max = hdr_b.z_max;
	hdr.lat_min4Coriolis = 0;	/* PRECISA ACABAR ISTO */
	hdr.doCoriolis = FALSE;

	if (cumpt && !maregs_in_input)
		read_maregs(hdr, maregs, lcum_p, ip2);	/* Read maregraph locations */

	nx = hdr.nx;			/* nx has been redeclared before (for counting params) */
	if (bat_in_input) {		/* If bathymetry & source where given as arguments */
		/* Transpose from Matlab orientation to scanline orientation */
		for (i = 0; i < hdr.ny; i++)
			for (j = 0; j < hdr.nx; j++)
				bat[i*hdr.nx+j] = -dep1[j*hdr.ny+i];

		for (i = 0; i < hdr.ny; i++) {
			for (j = 0; j < hdr.nx; j++)
				etaa[i*hdr.nx+j] = h[j*hdr.ny+i];
		}
	}

	lcum = 0;	cycle = 1;;

	if (!IamCompiled) {
		rhs[0] = mxCreateDoubleScalar(0.0);
		ptr_wb = mxGetPr(rhs[0]);
		rhs[1] = mxCreateString("title");
		rhs[2] = mxCreateString("Sit and Wait ...");
		mexCallMATLAB(0,NULL,3,rhs,"aguentabar");
	}
	else
		mexEvalString("aguentabar(0,\"title\",\"Sit and Wait...\")");

	/* ----------------- Compute vars to use if write grids --------------------- */
	if (!got_R && (write_grids || out_velocity || out_momentum || out_sww || out_most) ) {	
		/* Write grids over the whole region */
		i_start = 0;            i_end = hdr_b.nx;
		j_start = 0;            j_end = hdr_b.ny;
		xMinOut = hdr.x_min;	yMinOut = hdr.y_min;
	}
	else if (got_R && (write_grids || out_velocity || out_momentum || out_sww || out_most) ) {	
		/* Write grids in sub-region */
		i_start = irint((dfXmin - hdr_b.x_min) / dx);
		j_start = irint((dfYmin - hdr_b.y_min) / dy); 
		i_end   = irint((dfXmax - hdr_b.x_min) / dx) + 1;
		j_end   = irint((dfYmax - hdr_b.y_min) / dy) + 1;
		xMinOut = hdr_b.x_min + dx * i_start;	/* Adjustes xMin|yMin to lay on the closest grid node */
		yMinOut = hdr_b.y_min + dy * j_start;
	}
	/* ---------------------------------------------------------------------- */

	if (out_sww) {
		/* ----------------- Open a ANUGA netCDF file for writing --------------- */
		ncid = open_anuga_sww (fname_sww, ids, i_start, j_start, i_end, j_end, ip2,
		hdr.x_inc, hdr.y_inc, bat, xMinOut, yMinOut, (float)hdr.z_min, (float)hdr.z_max);
		if (ncid == -1)
			mexErrMsgTxt ("SWAN: failure to create ANUGA SWW file.\n");

		/* To be used when writing the data slices */
		count1_A[0] = 1;	count1_A[1] = (i_end - i_start) * (j_end - j_start);

		stage_range[0] = xmom_range[0] = ymom_range[0] = FLT_MAX;
		stage_range[1] = xmom_range[1] = ymom_range[1] = -FLT_MIN;

		tmp_slice = (float *) mxMalloc (sizeof(float) * (hdr.nx * hdr.ny));	/* To use inside slice writing */
	}

	if (out_most) {
		/* ----------------- Open a 3 MOST netCDF files for writing ------------- */
		mwSize nx, ny;
		nx = i_end - i_start;		ny = j_end - j_start;
		ncid_most[0] = open_most_nc (basename_most, "HA", ids_ha, nx, ny, dx, dy, xMinOut, yMinOut);
		ncid_most[1] = open_most_nc (basename_most, "UA", ids_ua, nx, ny, dx, dy, xMinOut, yMinOut);
		ncid_most[2] = open_most_nc (basename_most, "VA", ids_va, nx, ny, dx, dy, xMinOut, yMinOut);

		if (ncid_most[0] == -1 || ncid_most[1] == -1 || ncid_most[2] == -1)
			mexErrMsgTxt ("SWAN: failure to create one or more of the MOST files\n");

		ids_most[0] = ids_ha[5];	/* IDs of the Amp, Xmom & Ymom vriables */
		ids_most[1] = ids_ua[5];
		ids_most[2] = ids_va[5];

		/* To be used when writing the data slices */
		count1_M[0] = 1;	count1_M[1] = ny;	count1_M[2] = nx;

		if (!out_sww)
			tmp_slice = (float *)mxMalloc (sizeof(float) * (nx * ny));	/* To use inside slice writing */ 
	}

	if (do_nestum) {			/* If ...  it */
		for (k = 0; k < num_of_nestGrids; k++) {
			if (k == 0)
				initialize_nestum(&nest, hdr, isGeog, 0);
			else
				initialize_nestum(&nest, nest.hdr[k-1], isGeog, k);
		}
		nest.bat_L0  = bat;
		nest.etaa_L0 = etaa;            nest.etad_L0 = etad;
		nest.fluxm_a_L0 = fluxm_a;      nest.fluxn_a_L0 = fluxn_a;
		nest.fluxm_d_L0 = fluxm_d;      nest.fluxn_d_L0 = fluxn_d;
		nest.htotal_a_L0 = htotal_a;    nest.htotal_d_L0 = htotal_d;
		nest.time_h = time_h;

		plhs[0] = mxCreateDoubleMatrix(100,20,mxREAL);
		nest.zz = mxGetData(plhs[0]);
		nest.nn = 0;

		if (saveNested) {
			xMinOut = nest.hdr[writeLevel].x_min;
			yMinOut = nest.hdr[writeLevel].y_min;
			dx = nest.hdr[writeLevel].x_inc;
			dy = nest.hdr[writeLevel].y_inc;
			i_start = 0; j_start = 0;
			i_end = nest.hdr[writeLevel].nx;
			j_end = nest.hdr[writeLevel].ny;
			ip2 = nest.hdr[writeLevel].nx;
			ncl = nest.hdr[writeLevel].nx * nest.hdr[writeLevel].ny;
		}
	}

	if (verbose) {
		mexPrintf("Layer 0 time step = %f\n", dt);
		if (do_nestum) {
			for (k = 0; k < num_of_nestGrids; k++) {
				mexPrintf("Layer %d x_min = %g\tx_max = %g\ty_min = %g\ty_max = %g\n",
				k+1, nest.LLx[k], nest.LRx[k], nest.LLy[k], nest.URy[k]);
				mexPrintf("Layer %d inserting index LL (row,col) = %d\t%d\t\tUR (row,col) = %d\t%d\n",
				k+1, nest.LLrow[k], nest.LLcol[k], nest.URrow[k], nest.URcol[k]);
				if (k == 0)
					mexPrintf("Time step ratio to parent grid = %d\n", (int)(nest.dt_P[0] / nest.dt[0])); 
				else {
					mexPrintf("Time step ratio to parent grid = %d\n", (int)(nest.dt_P[k] / nest.dt[k]));
					mexPrintf("\t\tdts = %f\t%f\n", nest.dt_P[k], nest.dt[k]);
				}
			}
		}
	}

	/* case spherical coordinates initializes parameters */
	if (isGeog == 1)
		inisp(hdr, dt, r0, r1m, r1n, r2m, r2n, r3m, r3n, r4m, r4n);

#ifdef MIR_TIMEIT
	tic = clock();
#endif

	/*
	LOOP_OVER_ALL_CYCLES {
		mass_L0
		openbd
		LOOP_OVER_N_NESTED {
			LOOP_TIME_REFINE_1 {
	 			interp_edges_L1
 				mass_L1
				LOOP_TIME_REFINE_2 {
					interp_edges_L2
					mass_L2
					LOOP_TIME_REFINE_3 {
						...
					}
					moment_L2
					upscale_L2
					update_L2
				}
				moment_L1
				upscale_L1
				update_L1
			}
		}
		moment_L0
		update_L0
	}
	*/

	/* ---------------------------------------------------------------------- */
	if (time_jump == 0) time_jump = -1;	/* Trick to allow writing zero time grids when jump was not demanded */

	one_100 = (double)(n_of_cycles) / 100.0;

	/* Begin main iteration */
	for (k = iprc = 0; k < n_of_cycles; k++) {

		//if (k > iprc * one_100) {
			//prc = (double)iprc / 100.;
			//iprc++;
		if (cycle % 10 == 0) {		/* Waitbars stuff */
			prc = (double)cycle/(double)n_of_cycles;
			if (!IamCompiled) {
				ptr_wb[0] = prc;
				mexCallMATLAB(0,NULL,1,rhs,"aguentabar");
			}
			else {
				sprintf(cmd, "aguentabar(%f)",prc);
				mexEvalString(cmd);
			}
		}

		/* --------------------------------------------------------------------- */
		/* mass conservation */
		/* --------------------------------------------------------------------- */
		if (isGeog == 0)
			mass(hdr, dt, bat, etaa, htotal_d, fluxm_a, fluxn_a, etad);
		else
			mass_sp(hdr, bat, etaa, htotal_d, fluxm_a, fluxn_a, etad, r0, r1m, r1n, r2m, r2n);

		/* --------------------------------------------------------------------- */
		/* updates wmax - maximum water depth */
		/* --------------------------------------------------------------------- */
		if (max_level) {
			for (i = 0; i < ncl; i++) {
				etam = (etaa[i] + etad[i]) * 0.5;
				if (wmax[i] < etam)
					wmax[i] = etam;
			}
		}

		if ( surf_level && time_h > time_jump && ( (k % grn) == 0 || k == n_of_cycles - 1 || k == 1) ) {
			/* --------------------------------------------------------------------- */
			/* outputs eta file averaging eta(t-dt/2) e eta(t+dt/2)
			/* --------------------------------------------------------------------- */
			if (do_nestum && saveNested)
				for (i = 0; i < nest.hdr[writeLevel].nx * nest.hdr[writeLevel].ny; i++)
					work[i] = (nest.etaa[writeLevel][i] + nest.etad[writeLevel][i]) * 0.5;
			else
				for (i = 0; i < hdr.nx * hdr.ny; i++)
					work[i] = (etaa[i] + etad[i]) * 0.5;
		}
		if ( max_level && time_h > time_jump && ( (k % grn) == 0 || k == n_of_cycles - 1) ) {
			/* Outputs max surface level */
			for (i = 0; i < ncl; i++)
				work[i] = (float) wmax[i];
		}

		/* --------------------------------------------------------------------- */
		/* open boundary condition */
		/* --------------------------------------------------------------------- */
		openb(hdr, bat, fluxm_d, fluxn_d, etad);

		/* --------------------------------------------------------------------- */
		/* If Nested grids we have to do the nesting work */
		/* --------------------------------------------------------------------- */
		if (do_nestum && 0) {
			nestify(&nest, num_of_nestGrids, 0, isGeog);
		}
		if (do_nestum && 1) {
			int last_iter = (int)(nest.dt_P[0] / nest.dt[0]);  /* No truncations should occur here */
			int	nhalf = (int)((float)last_iter / 2);           /* */
			for (j = 0; j < last_iter; j++) {                  /* Loop over the time steps ratio */
				interp_edges(&nest, fluxm_a, nest.fluxm_a[0], "M", 0);
				interp_edges(&nest, fluxn_a, nest.fluxn_a[0], "N", 0);

				if (isGeog == 0)
					mass(nest.hdr[0], nest.dt[0], nest.bat[0], nest.etaa[0], nest.htotal_d[0],
						nest.fluxm_a[0], nest.fluxn_a[0], nest.etad[0]);
				else
					mass_sp(nest.hdr[0], nest.bat[0], nest.etaa[0], nest.htotal_d[0],
						nest.fluxm_a[0], nest.fluxn_a[0], nest.etad[0], nest.r0[0],
						nest.r1m[0], nest.r1n[0], nest.r2m[0], nest.r2n[0]);

				if (num_of_nestGrids == 2) {
					int last_iter2 = (int)(nest.dt_P[1] / nest.dt[1]), jj;
					for (jj = 0; jj < last_iter2; jj++) {                 /* Loop over the time steps ratio */
						interp_edges(&nest, nest.fluxm_a[0], nest.fluxm_a[1], "M", 1);
						interp_edges(&nest, nest.fluxn_a[0], nest.fluxn_a[1], "N", 1);

						mass(nest.hdr[1], nest.dt[1], nest.bat[1], nest.etaa[1], nest.htotal_d[1],
							nest.fluxm_a[1], nest.fluxn_a[1], nest.etad[1]);

						moment(nest.hdr[1], nest.dt[1], nest.manning2[1], nest.htotal_a[1],
							nest.htotal_d[1], nest.bat[1], nest.etad[1], nest.fluxm_a[1],
							nest.fluxn_a[1], nest.fluxm_d[1], nest.fluxn_d[1], nest.vex[1],
							nest.vey[1], TRUE);

						if (jj == last_iter2 - 1)
							upscale(&nest, nest.etad[0], 1, last_iter2);
							//upscale_(&nest, nest.etad[0], 1, last_iter2);

						update(nest.hdr[1], nest.etaa[1], nest.etad[1], nest.fluxm_a[1], nest.fluxm_d[1],
							nest.fluxn_a[1], nest.fluxn_d[1], nest.htotal_a[1], nest.htotal_d[1]);
					}
				}

				if (isGeog == 0)
					moment(nest.hdr[0], nest.dt[0], nest.manning2[0], nest.htotal_a[0],
						nest.htotal_d[0], nest.bat[0], nest.etad[0], nest.fluxm_a[0],
						nest.fluxn_a[0], nest.fluxm_d[0], nest.fluxn_d[0], nest.vex[0],
						nest.vey[0], TRUE);
				else
					moment_sp(nest.hdr[0], nest.dt[0], nest.manning2[0], nest.htotal_a[0],
						nest.htotal_d[0], nest.bat[0], nest.etad[0], nest.fluxm_a[0],
						nest.fluxn_a[0], nest.fluxm_d[0], nest.fluxn_d[0], nest.vex[0],
						nest.vey[0], nest.r0[0], nest.r1m[0], nest.r1n[0], nest.r2m[0],
						nest.r2n[0], nest.r3m[0], nest.r3n[0], nest.r4m[0], nest.r4n[0], TRUE);

				//if (j == last_iter - 1) 	/* Do the upscale only at last iteration of this cycle */
				if (j == nhalf) 	/* Do the upscale at ... iteration of this cycle */
					upscale(&nest, etad, 0, last_iter);
					//upscale_(&nest, etad, 0, last_iter);

				update(nest.hdr[0], nest.etaa[0], nest.etad[0], nest.fluxm_a[0], nest.fluxm_d[0],
					nest.fluxn_a[0], nest.fluxn_d[0], nest.htotal_a[0], nest.htotal_d[0]);
			}
		}

		/* --------------------------------------------------------------------- */
		/* momentum conservation */
		/* --------------------------------------------------------------------- */
		if (isGeog == 0)
			moment(hdr, dt, manning2, htotal_a, htotal_d, bat, etad, fluxm_a, fluxn_a, fluxm_d,
				fluxn_d, vex, vey, FALSE);
		else
			moment_sp(hdr, dt, manning2, htotal_a, htotal_d, bat, etad, fluxm_a, fluxn_a, fluxm_d,
				fluxn_d, vex, vey, r0, r1m, r1n, r2m, r2n, r3m, r3n, r4m, r4n, FALSE);

		/* --------------------------------------------------------------------- */
		/* updates vmax - maximum water velocity using upwind */
		/* --------------------------------------------------------------------- */
#if 0
		for (i = 0; i < ncl; i++) {
			vel2 = sqrt(vex[i] * vex[i] + vey[i] * vey[i]);
			if (vmax[i] < vel2)
				vmax[i] = vel2;
		}
#endif

		/* --------------------------------------------------------------------- */
		/* update eta and fluxes */
		/* --------------------------------------------------------------------- */
		update(hdr, etaa, etad, fluxm_a, fluxm_d, fluxn_a, fluxn_d, htotal_a, htotal_d);

		if (cumpt) {			/* Want time series at maregraph positions */
			if (cycle % cumint == 0) {	/* Save heights at cumint intervals */
				for (i = 0; i < n_mareg; i++) {
					cum_p[ijc(lcum,i)] = etaa[lcum_p[i]-1];
					time_p[lcum] = (float)time_h;
				}
				lcum++;
			}
		}

		if ( time_h > time_jump && ( (k % grn) == 0 || k == n_of_cycles - 1 || k == 1) ) {
			if (water_depth) {
				for (i = 0; i < ncl; i++)
					if ((work[i] = (float) (etaa[i] + bat[i])) < 0) work[i] = 0;
			}

			if (write_grids) {
				sprintf (prenome, "%s%05d.grd", stem, irint(time_h) );
				write_grd_bin(prenome, xMinOut, yMinOut, dx, dy, i_start, j_start, i_end, j_end, ip2, work);
			}
			if (out_velocity) {
				sprintf (prenome, "%s%05d", stem, irint(time_h) );

				if (do_nestum && saveNested) {
					if (out_velocity_x) {
						for (i = 0; i < nest.hdr[writeLevel].nx * nest.hdr[writeLevel].ny; i++)
							work[i] = (float) nest.vex[writeLevel][i];
						write_grd_bin(strcat(prenome,"_U.grd"), xMinOut, yMinOut, dx, dy, i_start, j_start,
									  i_end, j_end, ip2, work);
						prenome[strlen(prenome) - 6] = '\0';	/* Remove the _U.grd' so that we can add '_V.grd' */
					}
					if (out_velocity_y) {
						for (i = 0; i < nest.hdr[writeLevel].nx * nest.hdr[writeLevel].ny; i++)
							work[i] = (float) nest.vey[writeLevel][i];
						write_grd_bin(strcat(prenome,"_V.grd"), xMinOut, yMinOut, dx, dy, i_start, j_start,
									  i_end, j_end, ip2, work);
					}
				}
				else {
					if (out_velocity_x) {
						for (i = 0; i < hdr.nx * hdr.ny; i++) work[i] = (float) vex[i];
						write_grd_bin(strcat(prenome,"_U.grd"), xMinOut, yMinOut, dx, dy, i_start, j_start,
									  i_end, j_end, ip2, work);
						prenome[strlen(prenome) - 6] = '\0';	/* Remove the _U.grd' so that we can add '_V.grd' */
					}
					if (out_velocity_y) {
						for (i = 0; i < hdr.nx * hdr.ny; i++) work[i] = (float) vey[i];
						write_grd_bin(strcat(prenome,"_V.grd"), xMinOut, yMinOut, dx, dy, i_start, j_start,
									  i_end, j_end, ip2, work);
					}
				}
			}
			if (out_momentum) {
				if (stem[0] == 0)
					sprintf (prenome,"%.5d\0", irint(time_h) );
				else
					sprintf (prenome, "%s%.5d", stem, irint(time_h) );

				if (water_depth)	/* "work" is already the water depth */ 
					for (i = 0; i < ncl; i++) work[i] = (float) (vex[i] * work[i]);
				else
					for (i = 0; i < ncl; i++) {
						if (( work[i] = (float)(etaa[i] + bat[i]) ) < 0.) work[i] = 0.;
						work[i] *= (float)vex[i];
					}

				write_grd_bin( strcat(prenome,"_Uh.grd"), xMinOut, yMinOut, dx, dy, 
						i_start, j_start, i_end, j_end, ip2, work);

				if (water_depth)
					for (i = 0; i < ncl; i++) work[i] = (float) (vey[i] * work[i]);
				else
					for (i = 0; i < ncl; i++) {
						if (( work[i] = (float)(etaa[i] + bat[i]) ) < 0.) work[i] = 0.;
						work[i] *= (float)vey[i];
					}

				prenome[strlen(prenome) - 7] = '\0';	/* Remove the _Uh.grd' so that we can add '_Vh.grd' */
				write_grd_bin( strcat(prenome,"_Vh.grd"), xMinOut, yMinOut, dx, dy, 
						i_start, j_start, i_end, j_end, ip2, work);
			}

			if (out_sww) {
				if (first_anuga_time) {
					time0 = time_h;
					first_anuga_time = FALSE;
				}
				time_for_anuga = time_h - time0;	/* I think ANUGA wants time starting at zero */
				err_trap (nc_put_vara_double (ncid, ids[6], &start0, &count0, &time_for_anuga));

				write_anuga_slice(ncid, ids[7], i_start, j_start, i_end, j_end, ip2, work,
						etaa, bat, vex, vey, tmp_slice, start1_A, count1_A, stage_range, 1, with_land);
				write_anuga_slice(ncid, ids[9], i_start, j_start, i_end, j_end, ip2, work,
						etaa, bat, vex, vey, tmp_slice, start1_A, count1_A, xmom_range, 2, with_land);
				write_anuga_slice(ncid, ids[11],i_start, j_start, i_end, j_end, ip2, work,
						etaa, bat, vex, vey, tmp_slice, start1_A, count1_A, ymom_range, 3, with_land);

				start1_A[0]++;		/* Increment for the next slice */
			}

			if (out_most) {
				/* Here we'll use the start0 computed above */
				err_trap (nc_put_vara_double (ncid_most[0], ids_ha[4], &start0, &count0, &time_h));
				err_trap (nc_put_vara_double (ncid_most[1], ids_ua[4], &start0, &count0, &time_h));
				err_trap (nc_put_vara_double (ncid_most[2], ids_va[4], &start0, &count0, &time_h));

				write_most_slice(ncid_most, ids_most, i_start, j_start, i_end, j_end, ip2, 
					work, etaa, bat, vex, vey, tmp_slice, start1_M, count1_M);
				start1_M[0]++;		/* Increment for the next slice */
			}

			start0++;			/* Only used with netCDF formats */
		}
		time_h += dt;
		nest.time_h = time_h;
		cycle++;
	}
#ifdef MIR_TIMEIT
	toc = clock();
	mexPrintf("BARNABEU: CPU secs/ticks = %.3f\n", (double)(toc - tic));
#endif

	if (out_sww) {		/* Uppdate range values and close SWW file */
		err_trap (nc_put_var_float (ncid, ids[8], stage_range));
		err_trap (nc_put_var_float (ncid, ids[10], xmom_range));
		err_trap (nc_put_var_float (ncid, ids[12], ymom_range));
		err_trap(nc_close (ncid)); 
	}

	if (out_most) {		/* Close MOST files */
		err_trap(nc_close (ncid_most[0])); 
		err_trap(nc_close (ncid_most[1])); 
		err_trap(nc_close (ncid_most[2])); 
	}
	if (out_sww || out_most) mxFree ((void *)tmp_slice);

	if (!IamCompiled) {
		*ptr_wb = 1.0;
		mexCallMATLAB(0,NULL,1,rhs,"aguentabar");
	}
	else
		mexEvalString("aguentabar(1.0)");

	/*     WRITE THE OUTPUT ON FILE CUMHT */
	if (cumpt) {
		for (j = 0; j < lcum; j++) {
			fprintf (fp, "%.3f", time_p[j]);
			for (i = 0; i < n_mareg; i++)
				fprintf (fp, "\t%.4f", cum_p[ijc(j,i)]);
			fprintf (fp, "\n");
		}
	}
	/* Clean up allocated memory. */
	mxDestroyArray(rhs[0]);		mxDestroyArray(rhs[1]);		mxDestroyArray(rhs[2]);

	mxFree (etad);		mxFree (fluxm_a);
	mxFree (fluxm_d);	mxFree (fluxn_a);	mxFree (fluxn_d);
	mxFree (htotal_a);	mxFree (htotal_d);	mxFree (vmax);
	mxFree (wmax);		mxFree (vex);		mxFree (vey);

	if (cumpt) fclose (fp);

	mxFree (etaa);	mxFree (work);	mxFree (bat);
	if (cumpt) {
		mxFree ((void *) lcum_p);	mxFree((void *) cum_p);	mxFree ((void *) time_p);	 
	}
}

/* --------------------------------------------------------------------------- */
void err_trap(mwSize status) {
	if (status != NC_NOERR)	
		mexPrintf ("swan: error and errorcode = %d\n", status);
}

/* --------------------------------------------------------------------------- */
int write_grd_bin(char *name, double x_min, double y_min, double x_inc, double y_inc, mwSize i_start,
		mwSize j_start, mwSize i_end, mwSize j_end, mwSize nX, float *work) {

	/* Writes a grid in the Surfer binary format */
	mwSize i, j;
	double x_max, y_max;
	float work_min = FLT_MAX, work_max = -FLT_MAX, tmp;
	struct srf_header h;
	FILE *fp;

	if ((fp = fopen (name, "wb")) == NULL) {
		mexPrintf("Fatal Error: Could not create file %s!\n", name);
		return (-1);
	}

	x_max = x_min + (i_end - i_start - 1) * x_inc;
	y_max = y_min + (j_end - j_start - 1) * y_inc;

	/* Find zmin/zmax */
	for (j = j_start; j < j_end; j++) {
		for (i = i_start; i < i_end; i++) {
			tmp = work[ijs(i,j,nX)];
			work_max = MAX(tmp, work_max);
			work_min = MIN(tmp, work_min);
		}
	}

	/* store header information and array */
	strcpy (h.id,"DSBB");
	h.nx = (i_end - i_start);	h.ny = (j_end - j_start);
	h.x_min = x_min;		h.x_max = x_max;
	h.y_min = y_min;		h.y_max = y_max;
	h.z_min = (double)work_min;	h.z_max = (double)work_max;

	if (fwrite ((void *)&h, sizeof (struct srf_header), (size_t)1, fp) != 1) {
		fprintf (stderr, "Fatal Error: Error writing file %s!\n", name);
		return (-1);
	}

	for (j = j_start; j < j_end; j++)
		for (i = i_start; i < i_end; i++)
			fwrite ((void *)&work[ijs(i,j,nX)], sizeof(float), (size_t)1, fp);

	fclose(fp);
	return (0);
}

/* ------------------------------------------------------------------------------ */
int read_grd_info_ascii (char *file, struct srf_header *hdr) {

	char line[512], id[5];
	FILE *fp;

	if ((fp = fopen (file, "r")) == NULL) {
		mexPrintf ("%s: Unable to read file %s - exiting\n", "swan", file);
		return (-1);
	}

	fgets (line, 512, fp);
	sscanf (line, "%s", &hdr->id);
	fgets (line, 512, fp);
	sscanf (line, "%d %d", &hdr->nx, &hdr->ny);
	fgets (line, 512, fp);
	sscanf (line, "%lf %lf", &hdr->x_min, &hdr->x_max);
	fgets (line, 512, fp);
	sscanf (line, "%lf %lf", &hdr->y_min, &hdr->y_max);
	fgets (line, 512, fp);
	sscanf (line, "%lf %lf", &hdr->z_min, &hdr->z_max);
	fclose(fp);
	sprintf (id, "%.4s", hdr->id);
	if (strcmp (id, "DSAA") == 0)
		return (0);
	else if (strcmp (id, "DSBB") == 0) {
		fp = fopen (file, "rb");
		read_header_bin (fp, hdr);
		fclose(fp);
		return (1);
	}
	else
		return (-1);
}

/* ------------------------------------------------------------------------------ */
int read_grd_ascii (char *file, struct srf_header *hdr, double *work) {

	/* Reads a grid in the Surfer ascii format */
	mwSize i = 0, j, n_field;
	char *p, buffer[512], line[512];
	FILE *fp;

	if ((fp = fopen (file, "r")) == NULL) {
		mexPrintf ("%s: Unable to read file %s - exiting\n", "swan", file);
		return (-1);
	}

	fgets (line, 512, fp);
	sscanf (line, "%s", &hdr->id);
	fgets (line, 512, fp);
	sscanf (line, "%d %d", &hdr->nx, &hdr->ny);
	fgets (line, 512, fp);
	sscanf (line, "%lf %lf", &hdr->x_min, &hdr->x_max);
	fgets (line, 512, fp);
	sscanf (line, "%lf %lf", &hdr->y_min, &hdr->y_max);
	fgets (line, 512, fp);
	sscanf (line, "%lf %lf", &hdr->z_min, &hdr->z_max);

	while (fgets (line, 512, fp) != NULL) {
		strcpy (buffer, line);
		n_field = count_col (buffer);	/* Count # of fields in line */
		if (n_field == 0) continue;
		p = (char *)strtok (line, " \t\n\015\032");
		j = 0;
		while (p && j < n_field) {
			sscanf (p, "%lf", &work[i]);
			j++;	i++;
			p = (char *)strtok ((char *)NULL, " \t\n\015\032");
		}
	}
	fclose(fp);
	return (0);
} 

/* -------------------------------------------------------------------- */
int count_col (char *line) {
	/* Count # of fields contained in line */
	mwSize n_col = 0;
	char *p;

	p = (char *)strtok (line, " \t\n\015\032");
	while (p) {	/* Count # of fields */
		n_col++;
		p = (char *)strtok ((char *)NULL, " \t\n\015\032");
	}
	return (n_col);
}

/* -------------------------------------------------------------------- */
int read_header_bin (FILE *fp, struct srf_header *hdr) {
	/* Reads the header of a binary Surfer gridfile */
	fread ((void *)hdr, sizeof (struct srf_header), (size_t)1, fp); 
	return (0);
}

/* -------------------------------------------------------------------- */
int read_grd_bin (char *file, struct srf_header *hdr, double *work) {
	unsigned int i, j, ij, kk;
	float	*tmp;			/* Array pointer for reading in rows of data */
	FILE	*fp;

	if ((fp = fopen (file, "rb")) == NULL) {
		mexPrintf ("%s: Unable to read file %s - exiting\n", "swan", file);
		return (-1);
	}
	fread ((void *)hdr, sizeof (struct srf_header), (size_t)1, fp); 

	/* Allocate memory for one row of data (for reading purposes) */
	tmp = (float *) mxMalloc ((size_t)hdr->nx * sizeof (float));
	for (j = 0; j < (hdr->ny - 1); j++) {
		fread (tmp, sizeof (float), (size_t)hdr->nx, fp);	/* Get one row */
		ij = j * hdr->nx;
		for (i = 0; i < hdr->nx; i++) {
			kk = ij + i;
			work[kk] = tmp[i];
		}
	}
	fclose(fp);
	mxFree ((void *)tmp);
	return (0);
}

/* -------------------------------------------------------------------- */
int count_n_maregs(char *file) {
	mwSize	i = 0;
	char	line[512];
	FILE	*fp;

	if ((fp = fopen (file, "r")) == NULL) {
		mexPrintf ("%s: Unable to open file %s - exiting\n", "swan", file);
		return (-1);
	}
	while (fgets (line, 512, fp) != NULL) {
		if (line[0] == '#') continue;	/* Jump comment lines */
		i++;
	}
	return(i);
	fclose (fp);
	return (0);
}

/* -------------------------------------------------------------------- */
int read_maregs(struct grd_header hdr, char *file, int *lcum_p, int nx) {
	/* Read maregraph positions and convert them to vector indices */
	mwSize	i = 0, ix, jy;
	double	x, y;
	char	line[512];
	FILE	*fp;

	if ((fp = fopen (file, "r")) == NULL) {
		mexPrintf ("%s: Unable to open file %s - exiting\n", "swan", file);
		return (-1);
	}

	while (fgets (line, 512, fp) != NULL) {
		if (line[0] == '#') continue;	/* Jump comment lines */
		sscanf (line, "%f %f", &x, &y);
		ix = irint((x - hdr.x_min) / hdr.x_inc);
		jy = irint((y - hdr.y_min) / hdr.y_inc); 
		lcum_p[i] = jy * nx + ix; 
		i++;
	}
	fclose (fp);
	return (0);
}

/* -------------------------------------------------------------------- */
void no_sys_mem (char *where, mwSize n) {	
		mexPrintf ("Fatal Error: %s could not allocate memory, n = %d\n", where, n);
		mexErrMsgTxt("\n");
}

/* -------------------------------------------------------------------- */
int decode_R (char *item, double *w, double *e, double *s, double *n) {
	char *text, string[BUFSIZ];
	
	/* Minimalist code to decode option -R extracted from GMT_get_common_args */
	
	mwSize i, error = 0;
	double *p[4];
	
	p[0] = w;	p[1] = e;	p[2] = s;	p[3] = n;
			
	i = 0;
	strcpy (string, &item[2]);
	text = strtok (string, "/");
	while (text) {
		*p[i] = ddmmss_to_degree (text);
		i++;
		text = strtok (CNULL, "/");
	}
	if (item[strlen(item)-1] == 'r')	/* Rectangular box given, but valid here */
		error++;
	if (i != 4 || check_region (*p[0], *p[1], *p[2], *p[3]))
		error++;
	w = p[0];	e = p[1];
	s = p[2];	n = p[3];
	return (error);
}

/* -------------------------------------------------------------------- */
int check_region (double w, double e, double s, double n) {
	/* If region is given then we must have w < e and s < n */
	return ((w >= e || s >= n));
}

/* -------------------------------------------------------------------- */
double ddmmss_to_degree (char *text) {
	mwSize i, colons = 0, suffix;
	double degree, minute, degfrac, second;

	for (i = 0; text[i]; i++) if (text[i] == ':') colons++;
	suffix = (mwSize)text[i-1];	/* Last character in string */
	if (colons == 2) {	/* dd:mm:ss format */
		sscanf (text, "%lf:%lf:%lf", &degree, &minute, &second);
		degfrac = degree + Loc_copysign (minute / 60.0 + second / 3600.0, degree);
	}
	else if (colons == 1) {	/* dd:mm format */
		sscanf (text, "%lf:%lf", &degree, &minute);
		degfrac = degree + Loc_copysign (minute / 60.0, degree);
	}
	else
		degfrac = atof (text);
	if (suffix == 'W' || suffix == 'w' || suffix == 'S' || suffix == 's') degfrac = -degfrac;	/* Sign was given implicitly */
	return (degfrac);
}

/* -------------------------------------------------------------------- */
int open_most_nc (char *base, char *name_var, mwSize *ids, mwSize nx, mwSize ny, double dtx, double dty,
				  double xMinOut, double yMinOut) {
	/* Open and initialize a MOST netCDF file for writing ---------------- */
	mwSize ncid, m, n, status, dim0[3], dim3[3];
	float dummy = -1e34f;
	double *x, *y;
	char	*long_name = NULL, *units = NULL, *basename = NULL;

	basename = (char *) mxMalloc (strlen(base) * sizeof (char));
	strcpy(basename, base);
	if (!strcmp(name_var,"HA")) {
		strcat(basename,"_ha.nc");
		long_name = "Wave Amplitude";
		units = "CENTIMETERS";
	}
	else if (!strcmp(name_var,"VA")) {
		strcat(basename,"_va.nc");
		long_name = "Velocity Component along Latitude";
		units = "CENTIMETERS/SECOND";
	}
	else if (!strcmp(name_var,"UA")) {
		strcat(basename,"_ua.nc");
		long_name = "Velocity Component along Longitude";
		units = "CENTIMETERS/SECOND";
	}

	if ( (status = nc_create (basename, NC_CLOBBER, &ncid)) != NC_NOERR) {
		mexPrintf ("swan: Unable to create file %s - exiting\n", basename);
		return(-1);
	}
	/* ---- Define dimensions ------------ */
	err_trap (nc_def_dim (ncid, "LON", (size_t) nx, &dim0[0]));
	err_trap (nc_def_dim (ncid, "LAT", (size_t) ny, &dim0[1]));
	err_trap (nc_def_dim (ncid, "TIME", NC_UNLIMITED, &dim0[2]));

	/* ---- Define variables ------------- */
	dim3[0] = dim0[2];	dim3[1] = dim0[1];	dim3[2] = dim0[0];
	err_trap (nc_def_var (ncid, "LON",	NC_DOUBLE,1, &dim0[0], &ids[0]));
	err_trap (nc_def_var (ncid, "LAT",	NC_DOUBLE,1, &dim0[1], &ids[1]));
	err_trap (nc_def_var (ncid, "SLON",	NC_FLOAT,0,  &dim0[0], &ids[2]));
	err_trap (nc_def_var (ncid, "SLAT",	NC_FLOAT,0,  &dim0[1], &ids[3]));
	err_trap (nc_def_var (ncid, "TIME",	NC_DOUBLE,1, &dim0[2], &ids[4]));
	err_trap (nc_def_var (ncid, name_var,	NC_FLOAT,3,  dim3,     &ids[5]));

	/* ---- Variables Attributes --------- */
	err_trap (nc_put_att_text (ncid, ids[0], "units", 12, "degrees_east"));
	err_trap (nc_put_att_text (ncid, ids[0], "point_spacing", 4, "even"));
	err_trap (nc_put_att_text (ncid, ids[1], "units", 13, "degrees_north"));
	err_trap (nc_put_att_text (ncid, ids[1], "point_spacing", 4, "even"));
	err_trap (nc_put_att_text (ncid, ids[2], "units", 12, "degrees_east"));
	err_trap (nc_put_att_text (ncid, ids[2], "long_name", 16, "Source Longitude"));
	err_trap (nc_put_att_text (ncid, ids[3], "units", 13, "degrees_north"));
	err_trap (nc_put_att_text (ncid, ids[3], "long_name", 16, "Source Latitude"));
	err_trap (nc_put_att_text (ncid, ids[4], "units", 7, "SECONDS"));
	err_trap (nc_put_att_text (ncid, ids[5], "long_name", strlen(long_name), long_name));
	err_trap (nc_put_att_text (ncid, ids[5], "units", strlen(units), units));
	err_trap (nc_put_att_float (ncid,ids[5], "missing_value", NC_FLOAT, 1, &dummy));
	err_trap (nc_put_att_float (ncid,ids[5], "_FillValue", NC_FLOAT, 1, &dummy));
	err_trap (nc_put_att_text (ncid, ids[5], "history", 6, "Nikles"));

	/* ---- Global Attributes ------------ */
	err_trap (nc_put_att_text (ncid, NC_GLOBAL, "history", 10, "Mirone Tec"));
	err_trap (nc_put_att_text (ncid, NC_GLOBAL, "title", 22, "Created by Mirone-NSWING"));

	err_trap (nc_enddef (ncid));

	/* ---- Write the vector coords ------ */
	x = (double *) mxMalloc (sizeof (double) * nx);
	y = (double *) mxMalloc (sizeof (double) * ny);

	for (n = 0; n < nx; n++) x[n] = xMinOut + n * dtx;
	for (m = 0; m < ny; m++) y[m] = yMinOut + m * dty;
	err_trap (nc_put_var_double (ncid, ids[0], x));
	err_trap (nc_put_var_double (ncid, ids[1], y));
	mxFree ((void *)x); 
	mxFree ((void *)y); 
	mxFree ((void *)basename); 

	return (ncid);
}

/* --------------------------------------------------------------------------- */
void write_most_slice(mwSize *ncid_most, mwSize *ids_most, mwSize i_start, mwSize j_start, mwSize i_end, mwSize j_end,
		mwSize nX, float *work, double *h, double *dep, double *u, double *v, float *tmp, size_t *start, size_t *count) {
	/* Write a slice of _ha.nc, _va.nc & _ua.nc MOST netCDF files */
	mwSize i, j, n, ij, k;

	for (n = 0; n < 3; n++) {	/* Loop over, Amplitude, Xmomentum & Ymomentum */
		if (n == 0) {		/* Amplitude */
			for (j = j_start, k = 0; j < j_end; j++)
				for (i = i_start; i < i_end; i++)
					tmp[k++] = work[ijs(i,j,nX)] * 100;
			err_trap (nc_put_vara_float (ncid_most[0], ids_most[0], start, count, tmp));
		}
		else if (n == 1) {		/* X velocity */ 
			for (j = j_start, k = 0; j < j_end; j++)
				for (i = i_start; i < i_end; i++) {
					ij = ijs(i,j,nX);
					if (( tmp[k] = (float)(h[ij] + dep[ij]) ) < 0.) tmp[k] = 0.;
					tmp[k++] *= (float)u[ij] * 100;
				}
			err_trap (nc_put_vara_float (ncid_most[1], ids_most[1], start, count, tmp));
		}
		else {				/* Y velocity */ 
			for (j = j_start, k = 0; j < j_end; j++)
				for (i = i_start; i < i_end; i++) {
					ij = ijs(i,j,nX);
					if (( tmp[k] = (float)(h[ij] + dep[ij]) ) < 0.) tmp[k] = 0.;
					tmp[k++] *= (float)v[ij] * 100;
				}
			err_trap (nc_put_vara_float (ncid_most[2], ids_most[2], start, count, tmp));
		}
	}
}

/* --------------------------------------------------------------------------- */
void write_anuga_slice(mwSize ncid, mwSize z_id, mwSize i_start, mwSize j_start, mwSize i_end, mwSize j_end,
		mwSize nX, float *work, double *h, double *dep, double *u, double *v, float *tmp, size_t *start,
		size_t *count, float *slice_range, mwSize idx, mwSize with_land) {
	/* Write a slice of either STAGE, XMOMENTUM or YMOMENTUM of a Anuga's .sww netCDF file */
	mwSize i, j, ij, k, ncl;

	ncl = (i_end - i_start)*(j_end - j_start);
	k = 0;
	for (j = j_start; j < j_end; j++) {
		if (idx == 1) 			/* Anuga calls this -> stage */
			if (!with_land)		/* Land nodes are kept = 0 */
				for (i = i_start; i < i_end; i++)
					tmp[k++] = work[ijs(i,j,nX)];
			else {
				for (i = i_start; i < i_end; i++) {
					ij = ijs(i,j,nX);
					if (work[ij] == 0 && dep[ij] < 0) tmp[k++] = (float)-dep[ij];
					else 	tmp[k++] = work[ij];
				}
			}

		else if (idx == 2) {		/* X momentum */
			for (i = i_start; i < i_end; i++) {
				ij = ijs(i,j,nX);
				if (( tmp[k] = (float)(h[ij] + dep[ij]) ) < 0.) tmp[k] = 0.;
				tmp[k++] *= (float)u[ij];
			}
		}
		else {				/* Y momentum */
			for (i = i_start; i < i_end; i++) {
				ij = ijs(i,j,nX);
				if (( tmp[k] = (float)(h[ij] + dep[ij]) ) < 0.) tmp[k] = 0.;
				tmp[k++] *= (float)v[ij];
			}
		}
	}

	/* ----------- Find the min/max of this slice --------- */
	for (k = 0; k < ncl; k++) {
		slice_range[1] = MAX(tmp[k], slice_range[1]);
		slice_range[0] = MIN(tmp[k], slice_range[0]);
	}

	err_trap (nc_put_vara_float (ncid, z_id, start, count, tmp));
}

/* -------------------------------------------------------------------- */
int open_anuga_sww (char *fname_sww, mwSize *ids, mwSize i_start, mwSize j_start, mwSize i_end, mwSize j_end,
		mwSize nX, double dtx, double dty, double *dep, double xMinOut, double yMinOut, float z_min, float z_max) {

	/* Open and initialize a ANUGA netCDF file for writing ---------------- */
	mwSize ncid, m, n, nx, ny, status, nVolumes, nPoints, dim0[5], dim2[2], dim3[2];
	mwSize i, j, k, m_nx, m1_nx, *volumes, *vertices, v1, v2, v3, v4;
	float dummy2[2], *x, *y, yr, *tmp;
	double dummy, nan, faultPolyX[11], faultPolyY[11], faultSlip[10], faultStrike[10], 
		faultDip[10], faultRake[10], faultWidth[10], faultDepth[10];

	if ( (status = nc_create (fname_sww, NC_CLOBBER, &ncid)) != NC_NOERR) {
		mexPrintf ("swan: Unable to create file %s - exiting\n", fname_sww);
		return(-1);
	}
	/* ---- Define dimensions ------------ */
	nx = i_end - i_start;		ny = j_end - j_start;
	nVolumes = (nx - 1) * (ny - 1) * 2;
	nPoints = nx * ny;
	err_trap (nc_def_dim (ncid, "number_of_volumes", (size_t) nVolumes, &dim0[0]));
	err_trap (nc_def_dim (ncid, "number_of_vertices", (size_t) 3, &dim0[1]));
	err_trap (nc_def_dim (ncid, "numbers_in_range", (size_t) 2, &dim0[2]));
	err_trap (nc_def_dim (ncid, "number_of_points", (size_t) nPoints, &dim0[3]));
	err_trap (nc_def_dim (ncid, "number_of_timesteps", NC_UNLIMITED, &dim0[4]));

	/* ---- Define variables ------------- */
	dim2[0] = dim0[4];		dim2[1] = dim0[3];
	dim3[0] = dim0[0];		dim3[1] = dim0[1];
	err_trap (nc_def_var (ncid, "x",		NC_FLOAT,1, &dim0[3], &ids[0]));
	err_trap (nc_def_var (ncid, "y",		NC_FLOAT,1, &dim0[3], &ids[1]));
	err_trap (nc_def_var (ncid, "z",		NC_FLOAT,1, &dim0[3], &ids[2]));
	err_trap (nc_def_var (ncid, "elevation",	NC_FLOAT,1, &dim0[3], &ids[3]));
	err_trap (nc_def_var (ncid, "elevation_range", 	NC_FLOAT,1, &dim0[2], &ids[4]));
	err_trap (nc_def_var (ncid, "volumes",		NC_INT,  2, dim3, &ids[5]));
	err_trap (nc_def_var (ncid, "time",		NC_DOUBLE,1,&dim0[4], &ids[6]));
	err_trap (nc_def_var (ncid, "stage",		NC_FLOAT,2, dim2, &ids[7]));
	err_trap (nc_def_var (ncid, "stage_range",	NC_FLOAT,1, &dim0[2], &ids[8]));
	err_trap (nc_def_var (ncid, "xmomentum",	NC_FLOAT,2, dim2, &ids[9]));
	err_trap (nc_def_var (ncid, "xmomentum_range", 	NC_FLOAT,1, &dim0[2], &ids[10]));
	err_trap (nc_def_var (ncid, "ymomentum",	NC_FLOAT,2, dim2, &ids[11]));
	err_trap (nc_def_var (ncid, "ymomentum_range", 	NC_FLOAT,1, &dim0[2], &ids[12]));

	/* ---- Global Attributes ------------ */
	err_trap (nc_put_att_text (ncid, NC_GLOBAL, "institution", 10, "Mirone Tec"));
	err_trap (nc_put_att_text (ncid, NC_GLOBAL, "description", 22, "Created by Mirone-NSWING"));
	err_trap (nc_put_att_double (ncid, NC_GLOBAL, "xllcorner", NC_DOUBLE, 1, &xMinOut));
	err_trap (nc_put_att_double (ncid, NC_GLOBAL, "yllcorner", NC_DOUBLE, 1, &yMinOut));
	dummy = 29;	err_trap (nc_put_att_double (ncid, NC_GLOBAL, "zone", NC_DOUBLE, 1, &dummy));
	dummy = 0;	err_trap (nc_put_att_double (ncid, NC_GLOBAL, "starttime", NC_DOUBLE, 1, &dummy));
	dummy = 500000;	err_trap (nc_put_att_double (ncid, NC_GLOBAL, "false_easting", NC_DOUBLE, 1, &dummy));
	dummy = 0;	err_trap (nc_put_att_double (ncid, NC_GLOBAL, "false_northing", NC_DOUBLE, 1, &dummy));
	err_trap (nc_put_att_text (ncid, NC_GLOBAL, "datum", 5, "wgs84"));
	err_trap (nc_put_att_text (ncid, NC_GLOBAL, "projection", 3, "UTM"));
	err_trap (nc_put_att_text (ncid, NC_GLOBAL, "units", 1, "m"));
	/* Initialize the following attribs with NaNs. A posterior call will eventualy fill them with the right values */
	nan = mxGetNaN();
	for (i = 0; i < 10; i++) {
		faultPolyX[i] = faultPolyY[i] = faultSlip[i] = faultDip[i] = faultStrike[i] = 
				faultRake[i] = faultWidth[i] = faultDepth[i] = nan;
	}
	faultPolyX[10] = faultPolyY[10] = nan;		/* Those have an extra element */
	err_trap (nc_put_att_double (ncid, NC_GLOBAL, "faultPolyX", NC_DOUBLE, 11, faultPolyX));
	err_trap (nc_put_att_double (ncid, NC_GLOBAL, "faultPolyY", NC_DOUBLE, 11, faultPolyY));
	err_trap (nc_put_att_double (ncid, NC_GLOBAL, "faultStrike", NC_DOUBLE, 10, faultStrike));
	err_trap (nc_put_att_double (ncid, NC_GLOBAL, "faultSlip", NC_DOUBLE, 10, faultSlip));
	err_trap (nc_put_att_double (ncid, NC_GLOBAL, "faultDip", NC_DOUBLE, 10, faultDip));
	err_trap (nc_put_att_double (ncid, NC_GLOBAL, "faultRake", NC_DOUBLE, 10, faultRake));
	err_trap (nc_put_att_double (ncid, NC_GLOBAL, "faultWidth", NC_DOUBLE, 10, faultWidth));
	err_trap (nc_put_att_double (ncid, NC_GLOBAL, "faultDepth", NC_DOUBLE, 10, faultDepth));

	/* ---- Write the vector coords ------ */
	x = (float *) mxMalloc (sizeof (float) * (nx * ny));
	y = (float *) mxMalloc (sizeof (float) * (nx * ny));
	vertices = (mwSize *) mxMalloc (sizeof (mwSize) * (nx * ny));
	volumes = (mwSize *) mxMalloc (sizeof (mwSize) * (nVolumes * 3));

	/* Construct 2 triangles per 'rectangular' element */
	i = 0;
	for (m = 0; m < ny; m++) {		/* By rows    - Y */
		yr = (float)(m * dty);
		for (n = 0; n < nx; n++) {	/* By columns - X */
			x[i] = (float)(n * dtx);
			y[i] = yr;
			vertices[i] = i;
			i++;
		}
	}

	for (n = i = 0; n < nx - 1; n++) {			/* X */
		for (m = 0; m < ny - 1; m++) {			/* Y */
			m_nx = m * nx;		m1_nx = (m + 1) * nx;
			v1 = vertices[n + m_nx];	v2 = vertices[n + 1 + m_nx];
			v3 = vertices[n + 1 + m1_nx];	v4 = vertices[n + m1_nx];
			volumes[i] = v1;	volumes[i+1] = v2;	volumes[i+2] = v3;
			i += 3;
			volumes[i] = v1;	volumes[i+1] = v3;	volumes[i+2] = v4;
			i += 3;
		}
	}

	mxFree ((void *)vertices);

	err_trap (nc_enddef (ncid));
	err_trap (nc_put_var_float (ncid, ids[0], x));
	err_trap (nc_put_var_float (ncid, ids[1], y));
	mxFree ((void *)x); 
	mxFree ((void *)y); 

	tmp = (float *) mxMalloc (sizeof (float) * (nx * ny));
	for (j = j_start, k = 0; j < j_end; j++) {
		for (i = i_start; i < i_end; i++)
			tmp[k++] = (float)-dep[ijs(i,j,nX)];
	}

	err_trap (nc_put_var_float (ncid, ids[2], tmp));	/* z */

	err_trap (nc_put_var_float (ncid, ids[3], tmp));	/* elevation */
	dummy2[0] = z_min;		dummy2[1] = z_max;
	err_trap (nc_put_var_float (ncid, ids[4], dummy2));	/* elevation_range */
	err_trap (nc_put_var_int (ncid, ids[5], volumes));

	mxFree ((void *)tmp);
	mxFree ((void *)volumes);

	return (ncid);
}

/* --------------------------------------------------------------------
 * solves non linear continuity equation w/ cartesian coordinates
 * Computes water depth (htotal) needed for friction and
 * moving boundary algorithm
 * htotal < 0 - dry cell htotal (m) above still water
 * htotal > 0 - wet cell with htotal (m) of water depth
 *
 *		Updates only etad and htotal_d
 * -------------------------------------------------------------------- */
void mass(struct grd_header hdr, double dt, double *bat, double *etaa, double *htotal_d, double *fluxm_a, double *fluxn_a, double *etad) {

	int im1, jm1, row, col;
	int cm1, rm1;			/* previous column (cm1 = col -1) and row (rm1 = row - 1) */
	unsigned int ij;
	double dtdx, dtdy, dd, zzz;

	/* Function Body */
	dtdx = dt / hdr.x_inc;
	dtdy = dt / hdr.y_inc;

	for (row = 0; row < hdr.ny; row++) {
		ij = row * hdr.nx;
		rm1 = (row == 0) ? 0 : hdr.nx;
		for (col = 0; col < hdr.nx; col++) {
			cm1 = (col == 0) ? 0 : 1;
			/* case ocean and non permanent dry area */
			if (bat[ij] > MAXRUNUP) {
				zzz = etaa[ij] - dtdx * (fluxm_a[ij] - fluxm_a[ij-cm1]) - dtdy * (fluxn_a[ij] - fluxn_a[ij-rm1]);
				/* troquei EPS6 por EPS12 */
				if (fabs(zzz) < EPS6) zzz = 0;
				dd = zzz + bat[ij];
				/* wetable zone */
				/* troquei EPS6 por EPS12 */
				if (dd >= EPS6) {
					htotal_d[ij] = dd;
					etad[ij] = zzz;
				}
				else {
					htotal_d[ij] = 0;
					etad[ij] = -bat[ij];
				}
			}
			else {			/* over dry areas htotal is null and eta follows bat */
				htotal_d[ij] = 0;
				etad[ij] = -bat[ij];
			}
			ij++;
		}
	}
}

/* ---------------------------------------------------------------------- */
/* open boundary condition */
/* --------------------------------------------------------------------- */
void openb(struct grd_header hdr, double *bat, double *fluxm_d, double *fluxn_d, double *etad) {

	int i, j;
	double uh, zz, d__1, d__2;

	/* ----- first column */
	j = 0;
	for (i = 1; i < hdr.nx - 1; i++) {
		if (bat[ij_grd(i,j,hdr)] > EPS5) {
			uh = (fluxm_d[ij_grd(i,j,hdr)] + fluxm_d[ij_grd(i-1,j,hdr)]) * 0.5;
			d__2 = fluxn_d[ij_grd(i,j,hdr)];
			zz = sqrt(uh * uh + d__2 * d__2) / sqrt(NORMAL_GRAV * bat[ij_grd(i,j,hdr)]);
			if (fluxn_d[ij_grd(i,j,hdr)] > 0) zz *= -1;
			etad[ij_grd(i,j,hdr)] = zz;
		} 
		else
			etad[ij_grd(i,j,hdr)] = -bat[ij_grd(i,j,hdr)];
	}

	/* ------ last column */
	j = hdr.ny - 1;
	for (i = 1; i < hdr.nx - 1; i++) {
		if (bat[ij_grd(i,j,hdr)] > EPS5) {
			uh = (fluxm_d[ij_grd(i,j,hdr)] + fluxm_d[ij_grd(i-1,j,hdr)]) * 0.5;
			d__2 = fluxn_d[ij_grd(i,j-1,hdr)];
			zz = sqrt(uh * uh + d__2 * d__2) / sqrt(NORMAL_GRAV * bat[ij_grd(i,j,hdr)]);
			if (fluxn_d[ij_grd(i,j-1,hdr)] < 0) zz *= -1;
			if (fabs(zz) <= EPS5) zz = 0;
			etad[ij_grd(i,j,hdr)] = zz;
		} 
		else
			etad[ij_grd(i,j,hdr)] = -bat[ij_grd(i,j,hdr)];
	}

	/* ------ first row */
	i = 0;
	for (j = 1; j < hdr.ny - 1; j++) {
		if (bat[ij_grd(i,j,hdr)] > EPS5) {
			if (bat[ij_grd(i,j-1,hdr)] > EPS5)
				uh = (fluxn_d[ij_grd(i,j,hdr)] + fluxn_d[ij_grd(i,j-1,hdr)]) * 0.5;
			else
				uh = fluxn_d[ij_grd(i,j,hdr)];
	
			d__2 = fluxm_d[ij_grd(i,j,hdr)];
			zz = sqrt(uh * uh + d__2 * d__2) / sqrt(NORMAL_GRAV * bat[ij_grd(i,j,hdr)]);
			if (fluxm_d[ij_grd(i,j,hdr)] > 0) zz *= -1;
			if (fabs(zz) <= EPS5) zz = 0;
			etad[ij_grd(i,j,hdr)] = zz;
		} 
		else
			etad[ij_grd(i,j,hdr)] = -bat[ij_grd(i,j,hdr)];
	}
	
	/* ------- last row */
	i = hdr.nx - 1;
	for (j = 1; j < hdr.ny - 1; j++) {
		if (bat[ij_grd(i,j,hdr)] > EPS5) {
			uh = (fluxn_d[ij_grd(i,j,hdr)] + fluxn_d[ij_grd(i,j-1,hdr)]) * 0.5;
			d__2 = fluxm_d[ij_grd(i-1,j,hdr)];
			zz = sqrt(uh * uh + d__2 * d__2) / sqrt(NORMAL_GRAV * bat[ij_grd(i,j,hdr)]);
			if (fluxm_d[ij_grd(i-1,j,hdr)] < 0) zz *= -1;
			etad[ij_grd(i,j,hdr)] = zz;
		} 
		else
			etad[ij_grd(i,j,hdr)] = -bat[ij_grd(i,j,hdr)];
	}
	
	/* -------- first row & first column */
	if (bat[0] > EPS5) {
		zz = sqrt(fluxm_d[0] * fluxm_d[0] + fluxn_d[0] * fluxn_d[0]) / sqrt(NORMAL_GRAV * bat[0]);
		if (fluxm_d[0] > 0 || fluxn_d[0] > 0) zz *= -1;
		if (fabs(zz) <= EPS5) zz = 0;
		etad[0] = zz;
	} 
	else
		etad[0] = -bat[0];

	/* -------- last row & first column */
	if (bat[ij_grd(hdr.nx-1,0,hdr)] > EPS5) {
		d__1 = fluxm_d[ij_grd(hdr.nx-2,0,hdr)];
		d__2 = fluxn_d[ij_grd(hdr.nx-1,0,hdr)];
		zz = sqrt(d__1 * d__1 + d__2 * d__2) / sqrt(NORMAL_GRAV * bat[ij_grd(hdr.nx-1,0,hdr)]);
		if (fluxm_d[ij_grd(hdr.nx-2,0,hdr)] < 0 || fluxn_d[ij_grd(hdr.nx-1,0,hdr)] > 0) zz *= -1;
		if (fabs(zz) <= EPS5) zz = 0;
		etad[ij_grd(0,hdr.ny-1,hdr)] = zz;
	} 
	else
		etad[ij_grd(0,hdr.ny-1,hdr)] = -bat[ij_grd(0,hdr.ny-1,hdr)];

	/* -------- first row & last column */
	if (bat[ij_grd(0,hdr.ny-1,hdr)] > EPS5) {
		d__1 = fluxm_d[ij_grd(0,hdr.ny-1,hdr)];
		d__2 = fluxn_d[ij_grd(0,hdr.ny-2,hdr)];
		zz = sqrt(d__1 * d__1 + d__2 * d__2) / sqrt(NORMAL_GRAV * bat[ij_grd(0,hdr.ny-1,hdr)]);
		if (fluxm_d[ij_grd(0,hdr.ny-1,hdr)] > 0 || fluxn_d[ij_grd(0,hdr.ny-2,hdr)] < 0) zz = -zz;
		if (fabs(zz) <= EPS5) zz = 0;
		etad[ij_grd(0,hdr.ny-1,hdr)] = zz;
	} 
	else
		etad[ij_grd(0,hdr.ny-1,hdr)] = -bat[ij_grd(0,hdr.ny-1,hdr)];

	/* ---------- last row & last column */
	if (bat[ij_grd(hdr.nx-1,hdr.ny-1,hdr)] > EPS5) {
		d__1 = fluxm_d[ij_grd(hdr.nx-2,hdr.ny-1,hdr)];
		d__2 = fluxn_d[ij_grd(hdr.nx-1,hdr.ny-2,hdr)];
		zz = sqrt(d__1 * d__1 + d__2 * d__2) / sqrt(NORMAL_GRAV * bat[ij_grd(hdr.nx-1,hdr.ny-1,hdr)]);
		if (fluxm_d[ij_grd(hdr.nx-2,hdr.ny-1,hdr)] < 0 || fluxn_d[ij_grd(hdr.nx-1,hdr.ny-2,hdr)] < 0) zz *= -1;
		etad[ij_grd(hdr.nx-1,hdr.ny-1,hdr)] = zz;
	} 
	else
		etad[ij_grd(hdr.nx-1,hdr.ny-1,hdr)] = -bat[ij_grd(hdr.nx-1,hdr.ny-1,hdr)];
}

/* --------------------------------------------------------------------- */
/* update eta and fluxes */
/* --------------------------------------------------------------------- */
void update(struct grd_header hdr, double *etaa, double *etad, double *fluxm_a, double *fluxm_d,
            double *fluxn_a, double *fluxn_d, double *htotal_a, double *htotal_d) {

	unsigned int i;

	for (i = 0; i < hdr.nx * hdr.ny; i++) {
		etaa[i] = etad[i];
		fluxm_a[i] = fluxm_d[i];
		fluxn_a[i] = fluxn_d[i];
		htotal_a[i] = htotal_d[i];
	}
}



/* -------------------------------------------------------------------------
 * Solve nonlinear momentum equation, cartesian coordinates 
 * with moving boundary

 *    dx,dy,dt: grid spacing and time step
 *    ifriction: 0/1 to use manning coefficient manning_coef
 *    icor: 0/1 to consider Coriolis
 *    htotal_a / htotal_d: total water thickness previous/next time step
 *    bat: bathymetry
 *    etad: water heigth, in dry areas equals topography (positive)
 *    fluxm_a,fluxn_a: fluxes M and N previous time step

 *    fluxm_d,fluxn_d: fluxes M and N next time step (output)
 *    vex,vey: particle velocities next time step (output)
 *
 *		Updates fluxm_d and fluxn_d
 * ---------------------------------------------------------------------- */
void moment(struct grd_header hdr, double dt, double manning2, double *htotal_a, double *htotal_d,
	double *bat, double *etad, double *fluxm_a, double *fluxn_a, double *fluxm_d, double *fluxn_d,
	double *vex, double *vey, int isNested) {

	unsigned int ij;
	int first, last, jupe, row, col, ilin = 1;
	int cm1, rm1;			/* previous column (cm1 = col - 1) and row (rm1 = row - 1) */
	int cp1, rp1;			/* next column (cp1 = col + 1) and row (rp1 = row + 1) */
	int cm2, rm2;
	double xp, xq, xpe, xqe, xpp, xqq, ff, dd, df, cte;
	double advx, dtdx, dtdy, advy, rlat, r4mcart;
	double dpa_ij, dpa_ij_rp1, dpa_ij_rm1, dpa_ij_cm1, dpa_ij_cp1;
	double dqa_ij, dqa_ij_rp1, dqa_ij_rm1, dqa_ij_cm1, dqa_ij_cp1;

	dtdx = dt / hdr.x_inc;
	dtdy = dt / hdr.y_inc;

	/* fixes the width of the lateral buffer for linear aproximation */
	/* if jupe>nnx/2 and jupe>nny/2 linear model will be applied */
	if (isNested) {
		jupe  = 0;
		first = 0;
		last  = 1;
	}
	else {
		jupe  = 5;
		first = 0;
		last  = 1;
	}

	/* fixes friction parameter */
	cte = (manning2) ? dt * 4.9 : 0;

# if 0
	/* Calculate total water depth at discharge point */
	for (row = 0; row < hdr.ny; row++) {
		ij = row * hdr.nx;
		rp1 = (row < hdr.ny-1) ? hdr.nx : 0;
		for (col = 0; col < hdr.nx; col++) {
			cp1 = (col < hdr.nx-1) ? 1 : 0;
			dpa[ij] = (htotal_d[ij] + htotal_a[ij] + htotal_d[ij+cp1] + htotal_a[ij+cp1]) * 0.25;
			dqa[ij] = (htotal_d[ij] + htotal_a[ij] + htotal_d[ij+rp1] + htotal_a[ij+rp1]) * 0.25;
			if (dpa[ij] < EPS6) dpa[ij] = 0;
			if (dqa[ij] < EPS6) dqa[ij] = 0;
			ij++;
		}
	}
#endif
	/* main computation cycle fluxm_d */
//#if HAVE_OPENMP
//#pragma omp parallel for
//#endif
	for (row = 0 + first; row < hdr.ny - last; row++) {
		rp1 = hdr.nx;
		rm1 = (row == 0) ? 0 : hdr.nx;
		ij = row * hdr.nx - 1;
		for (col = 0 + first; col < hdr.nx - last; col++) {
			cp1 = 1;
			cm1 = (col == 0) ? 0 : 1;
			ij++;
			/* no flux to permanent dry areas */
			if (bat[ij] <= MAXRUNUP) {
				fluxm_d[ij] = vex[ij] = 0;
				continue;
			}

			dpa_ij = (htotal_d[ij] + htotal_a[ij] + htotal_d[ij+cp1] + htotal_a[ij+cp1]) * 0.25;
			if (dpa_ij < EPS6) dpa_ij = 0;

			/* case wet-wet */
			if (htotal_d[ij] > EPS6 && htotal_d[ij+cp1] > EPS6) {
				/* case b2 */
				if (-bat[ij+cp1] >= etad[ij]) {
					dd = htotal_d[ij+cp1];
					df = dd;
				}
				else if (-bat[ij] >= etad[ij+cp1]) {
					/* case d2 */
					dd = htotal_d[ij];
					df = dd;
				}
				else {
					/* case b3/d3 */
					dd = (htotal_d[ij] + htotal_d[ij+cp1]) * 0.5;
					if (dd < EPS6) dd = 0;
					df = dpa_ij;
				}
				/* case a3/d1 wet-dry */
			}
			else if (htotal_d[ij] >= EPS6 && htotal_d[ij+cp1] < EPS6 && etad[ij] >= etad[ij+cp1]) {
				if (bat[ij] > bat[ij+cp1])
					dd = etad[ij] - etad[ij+cp1];
				else
					dd = htotal_d[ij];
				df = dd;
			/* case b1 and c3 dry-wet */
			}
			else if (htotal_d[ij] < EPS6 && htotal_d[ij+cp1] >= EPS6 && etad[ij] <= etad[ij+cp1]) {
				if (bat[ij] > bat[ij+cp1])
					dd = htotal_d[ij+cp1];
				else
					dd = etad[ij+cp1] - etad[ij];
				df = dd;
			}
			else {			/* other cases no moving boundary a1,a2,c1,c2 */
				fluxm_d[ij] = vex[ij] = 0;
				continue;
			}
			/* disregards fluxes when dd is very small - pode ser EPS6 */
			if (dd < EPS3) {
				fluxm_d[ij] = vex[ij] = 0;
				continue;
			}
			if (df < EPS3) df = EPS3;
			xqq = (fluxn_a[ij] + fluxn_a[ij+cp1] + fluxn_a[ij-rm1] + fluxn_a[ij+cp1-rm1]) * 0.25;
			if (manning2)
				ff = cte * manning2 * sqrt(fluxm_a[ij] * fluxm_a[ij] + xqq * xqq) / pow(df, 2.333333);
			else
				ff = 0;

			/* computes linear terms in cartesian coordinates */
			xp = (1 - ff) * fluxm_a[ij] - dtdx * NORMAL_GRAV * dd * (etad[ij+cp1] - etad[ij]);
			/* - if requested computes coriolis term */
			if (hdr.doCoriolis) {
				rlat = hdr.lat_min4Coriolis + row * hdr.y_inc * M_PI / 2e9;
				r4mcart = dt * 7.2722e-5 * sin(rlat);
				xp += r4mcart * 2 * xqq;
			}
			/* - computes convection terms */
			advx = advy = 0;
			/* - lateral buffer >> linear */
			if (col < jupe || col > (hdr.nx - jupe - 1) || row < jupe || row > (hdr.ny - jupe - 1))
				goto L120;
			/* - total water depth is smaller than EPS3 >> linear */
			if (dpa_ij < EPS3)
				goto L120;

#if 0
             dpa(ip1,j)=0.25*(htotal_d(ip1,j)+htotal_a(ip1,j)+htotal_d(ip2,j) +htotal_a(ip2,j))
             dpa(im1,j)=0.25*(htotal_d(im1,j)+htotal_a(im1,j)+htotal_d(i,j) +htotal_a(i,j))
             dpa(i,jp1)=0.25*(htotal_d(i,jp1)+htotal_a(i,jp1)+htotal_d(ip1,jp1) +htotal_a(ip1,jp1))
             dpa(i,jm1)=0.25*(htotal_d(i,jm1)+htotal_a(i,jm1)+htotal_d(ip1,jm1) +htotal_a(ip1,jm1))

             dpa(i,j)=0.25*(htotal_d(i,j)+htotal_a(i,j)+htotal_d(ip1,j) +htotal_a(ip1,j))
             dpd(i,j)=0.50*(htotal_d(i,j)+htotal_d(ip1,j))
#endif


			/* - upwind scheme for x-direction volume flux */
			if (fluxm_a[ij] < 0) {
				dpa_ij_cp1 = (htotal_d[ij+cp1] + htotal_a[ij+cp1] + htotal_d[ij+2*cp1] + htotal_a[ij+2*cp1]) * 0.25;
				if (dpa_ij_cp1 < EPS3 || htotal_d[ij+cp1] < EPS6)
					advx = dtdx * (-(fluxm_a[ij] * fluxm_a[ij]) / dpa_ij);
				else
					advx = dtdx * (fluxm_a[ij+cp1]*fluxm_a[ij+cp1] / dpa_ij_cp1 - fluxm_a[ij]*fluxm_a[ij] / dpa_ij);
			}
			else {
				dpa_ij_cm1 = (htotal_d[ij-cm1] + htotal_a[ij-cm1] + htotal_d[ij] + htotal_a[ij]) * 0.25;
				if (dpa_ij_cm1 < EPS3 || htotal_d[ij] < EPS6)
					advx = dtdx * (fluxm_a[ij] * fluxm_a[ij] / dpa_ij);
				else
					advx = dtdx * (fluxm_a[ij]*fluxm_a[ij] / dpa_ij - fluxm_a[ij-cm1]*fluxm_a[ij-cm1] / dpa_ij_cm1);
			}
			/* - upwind scheme for y-direction volume flux */
			if (xqq < 0) {
				dpa_ij_rp1 = (htotal_d[ij+rp1] + htotal_a[ij+rp1] + htotal_d[ij+cp1+rp1] + htotal_a[ij+cp1+rp1]) * 0.25;
				if (htotal_d[ij+rp1] < EPS6 || htotal_d[ij+rp1+cp1] < EPS6)
					advy = dtdy * (-fluxm_a[ij] * xqq / dpa_ij);

				else if (dpa_ij_rp1 < EPS6)
					advy = dtdy * (-fluxm_a[ij] * xqq / dpa_ij);

				else {
					xqe = (fluxn_a[ij+rp1] + fluxn_a[ij+rp1+cp1] + fluxn_a[ij] + fluxn_a[ij+cp1]) * 0.25;
					advy = dtdy * (fluxm_a[ij+rp1] * xqe / dpa_ij_rp1 - fluxm_a[ij] * xqq / dpa_ij);
				}
			}
			else {
				dpa_ij_rm1 = (htotal_d[ij-rm1] + htotal_a[ij-rm1] + htotal_d[ij+cp1-rm1] + htotal_a[ij+cp1-rm1]) * 0.25;
				if (htotal_d[ij-rm1] < EPS6 || htotal_d[ij+cp1-rm1] < EPS6)
					advy = dtdy * fluxm_a[ij] * xqq / dpa_ij;

				else if (dpa_ij_rm1 < EPS6)
					advy = dtdy * fluxm_a[ij] * xqq / dpa_ij;

				else {
					rm2 = (row < 2) ? 0 : 2 * hdr.nx;
					xqe = (fluxn_a[ij-rm1] + fluxn_a[ij+cp1-rm1] + fluxn_a[ij-rm2] + fluxn_a[ij+cp1-rm2]) * 0.25;
					advy = dtdy * (fluxm_a[ij] * xqq / dpa_ij - fluxm_a[ij-rm1] * xqe / dpa_ij_rm1);
				}
			}
			/* disregards very small advection terms */
			if (fabs(advx) <= EPS12) advx = 0;
			if (fabs(advy) <= EPS12) advy = 0;
			/* adds linear+convection terms */
			xp = xp - advx - advy;
L120:
			xp /= (ff + 1);
			if (fabs(xp) < EPS12) xp = 0;

			fluxm_d[ij] = xp;
			/* elimina velocidades maiores que 10m/s para dd < 1 m */
			if (dd > EPS3)
				vex[ij] = xp / df;
			else
				vex[ij] = 0;
			if (df < 1 && fabs(vex[ij]) > 10)
				vex[ij] = 0;

		}
	}

	if (isNested) {
		fluxm_d[ij_grd(hdr.nx - 1, hdr.ny - 1, hdr)] = fluxm_d[ij_grd(hdr.nx - 1, hdr.ny - 2, hdr)];	/* UR */
		fluxm_d[ij_grd(hdr.nx - 1, 0, hdr)] = fluxm_d[ij_grd(hdr.nx - 1, 1, hdr)];	/* LR */
	}

	/* main computation cycle fluxn_d */
	for (row = 0 + first; row < hdr.ny - last; row++) {
		rp1 = hdr.nx;
		rm1 = (row == 0) ? 0 : hdr.nx;
		ij = row * hdr.nx - 1;
		for (col = 0 + first; col < hdr.nx - last; col++) {
			cp1 = 1;
			cm1 = (col == 0) ? 0 : 1;
			ij++;
			/* no flux to permanent dry areas */
			if (bat[ij] <= MAXRUNUP) {
				fluxn_d[ij] = vey[ij] = 0;
				continue;
			}

			dqa_ij = (htotal_d[ij] + htotal_a[ij] + htotal_d[ij+rp1] + htotal_a[ij+rp1]) * 0.25;
			if (dqa_ij < EPS6) dqa_ij = 0;

			/* moving boundary - Imamura algorithm following cho 2009 */
			if (htotal_d[ij] > EPS6 && htotal_d[ij+rp1] > EPS6) {
				/* case b2 */
				if (-bat[ij+rp1] >= etad[ij]) {
					dd = htotal_d[ij+rp1];
					df = dd;
					/* case d2 */
				} 
				else if (-bat[ij] >= etad[ij+rp1]) {
					dd = htotal_d[ij];
					df = dd;
				} 
				else {
					/* case b3/d3 */
					dd = (htotal_d[ij] + htotal_d[ij+rp1]) * 0.5;
					if (dd < EPS6) dd = 0;
					df = dqa_ij;
				}
				/* case a3 and d1 wet dry */
			}		 
			else if (htotal_d[ij] > EPS6 && htotal_d[ij+rp1] < EPS6 && etad[ij] > etad[ij+rp1]) {
				if (bat[ij] > bat[ij+rp1])
					dd = etad[ij] - etad[ij+rp1];
				else
					dd = htotal_d[ij];
	
				df = dd;
				/* case b1 and c3 dry-wet */
			}
			else if (htotal_d[ij] <= EPS6 && htotal_d[ij+rp1] > EPS6 && etad[ij+rp1] > etad[ij]) {
				if (bat[ij] > bat[ij+rp1])
					dd = htotal_d[ij+rp1];
				else
					dd = etad[ij+rp1] - etad[ij];
				df = dd;
			}
			else {				/* other cases no moving boundary */
				fluxn_d[ij] = vey[ij] = 0;
				continue;
			}
			/* disregards fluxes when dd is very small */
			if (dd < EPS3) {
				fluxn_d[ij] = vey[ij] = 0;
				continue;
			}
			if (df < EPS3) df = EPS3;
			xpp = (fluxm_a[ij] + fluxm_a[ij+rp1] + fluxm_a[ij-cm1] + fluxm_a[ij-cm1+rp1]) * 0.25;
			if (manning2)
				ff = cte * manning2 * sqrt(fluxn_a[ij] * fluxn_a[ij] + xpp * xpp) / pow(df, 2.333333);
			else
				ff = 0;

			/* computes linear terms of N in cartesian coordinates */
			xq = (1 - ff) * fluxn_a[ij] - dtdy * NORMAL_GRAV * dd * (etad[ij+rp1] - etad[ij]);

			/* - if requested computes coriolis term */
			if (hdr.doCoriolis) {
				rlat = hdr.lat_min4Coriolis + (row * hdr.y_inc + hdr.y_inc / 2.) * M_PI / 2e9;
				r4mcart = dt * 7.2722e-5 * sin(rlat);
				xq -= r4mcart * 2 * xqq;
			}

			/* - computes convection terms */
			advx = advy = 0;
			/* - lateral buffer >> linear */
			if (col < jupe || col > (hdr.nx - jupe - 1) || row < jupe || row > (hdr.ny - jupe - 1))
				goto L200;
			/* - total water depth is smaller than EPS3 >> linear */
			if (dqa_ij < EPS3)
					goto L200;

#if 0
             dqa(i,jp1)=0.25*(htotal_d(i,jp1)+htotal_a(i,jp1)+htotal_d(i,jp1) +htotal_a(i,jp1))
             dqa(i,jm1)=0.25*(htotal_d(i,jm1)+htotal_a(i,jp1)+htotal_d(i,j) +htotal_a(i,j))

             dqa(i,j)=0.25*(htotal_d(i,j)+htotal_a(i,j)+htotal_d(i,jp1) +htotal_a(i,jp1))
             dqd(i,j)=0.50*(htotal_d(i,j)+htotal_d(i,jp1))
#endif


			/* - upwind scheme for y-direction volume flux */
			/* - total water depth is smaller than EPS6 >> linear */
			if (fluxn_a[ij] < 0) {
				dqa_ij_rp1 = (htotal_d[ij+rp1] + htotal_a[ij+rp1] + htotal_d[ij+2*rp1] + htotal_a[ij+2*rp1]) * 0.25;
				if (dqa_ij_rp1 < EPS6 || htotal_d[ij+rp1] < EPS6)
					advy = dtdy * (-(fluxn_a[ij] * fluxn_a[ij]) / dqa_ij );
				else
					advy = dtdy * (fluxn_a[ij+rp1]*fluxn_a[ij+rp1] / dqa_ij_rp1 - fluxn_a[ij]*fluxn_a[ij] / dqa_ij);
			}
			else {
				dqa_ij_rm1 = (htotal_d[ij-rm1] + htotal_a[ij-rm1] + htotal_d[ij] + htotal_a[ij]) * 0.25;
				if (dqa_ij_rm1 < EPS6 || htotal_d[ij] < EPS6)
					advy = dtdy * (fluxn_a[ij] * fluxn_a[ij]) / dqa_ij;
				else
					advy = dtdy * (fluxn_a[ij] * fluxn_a[ij] / dqa_ij - fluxn_a[ij-rm1]*fluxn_a[ij-rm1] / dqa_ij_rm1);
			}
			/* - upwind scheme for x-direction volume flux */
			if (xpp < 0) {
				dqa_ij_cp1 = (htotal_d[ij+cp1] + htotal_a[ij+cp1] + htotal_d[ij+rp1+cp1] + htotal_a[ij+rp1+cp1]) * 0.25;
				if (htotal_d[ij+cp1] < EPS6 || htotal_d[ij+cp1+rp1] < EPS6)
					advx = dtdx * (-fluxn_a[ij] * xpp / dqa_ij);

				else if (dqa_ij_cp1 < EPS6)
					advx = dtdx * (-fluxn_a[ij] * xpp / dqa_ij);

				else {
					xpe = (fluxm_a[ij+cp1] + fluxm_a[ij+cp1+rp1] + fluxm_a[ij] + fluxm_a[ij+rp1]) * 0.25;
					advx = dtdx * (fluxn_a[ij+cp1] * xpe / dqa_ij_cp1 - fluxn_a[ij] * xpp / dqa_ij);
				}
			}
			else {
				dqa_ij_cm1 = (htotal_d[ij-cm1] + htotal_a[ij-cm1] + htotal_d[ij+rp1-cm1] + htotal_a[ij+rp1-cm1]) * 0.25;
				if (htotal_d[ij-cm1] < EPS6 || htotal_d[ij-cm1+rp1] < EPS6)
					advx = dtdx * (fluxn_a[ij] * xpp / dqa_ij);

				else if (dqa_ij_cm1 < EPS6)
					advx = dtdx * fluxn_a[ij] * xpp / dqa_ij;

				else {
					cm2 = (col < 2) ? 0 : 2;
					xpe = (fluxm_a[ij-cm1] + fluxm_a[ij-cm1+rp1] + fluxm_a[ij-cm2] + fluxm_a[ij-cm2+rp1]) * 0.25;
					advx = dtdx * (fluxn_a[ij] * xpp / dqa_ij - fluxn_a[ij-cm1] * xpe / dqa_ij_cm1);
				}
			}
			/* disregards very small advection terms */
			if (fabs(advx) <= EPS12) advx = 0;
			if (fabs(advy) <= EPS12) advy = 0;
			/* adds linear+convection terms */
			xq = xq - advx - advy;
L200:
			xq /= (ff + 1);
			if (fabs(xq) < EPS12) xq = 0;

			fluxn_d[ij] = xq;
			/* elimina velocidades maiores que 10m/s para dd < 1m */
			if (dd > EPS3)
				vey[ij] = xq / df;
			else
				vey[ij] = 0.;
			if (df < 1 && fabs(vey[ij]) > 10)
				vey[ij] = 0;

//fluxn_d[ij] = vey[ij] = 0;

		}
	}

	if (isNested) {
		fluxn_d[ij_grd(hdr.nx - 1, hdr.ny - 1, hdr)] = fluxn_d[ij_grd(hdr.nx - 2, hdr.ny - 1, hdr)];
		fluxn_d[ij_grd(hdr.nx - 1, 0, hdr)] = fluxn_d[ij_grd(hdr.nx - 2, 0, hdr)];
	}

}

/* -------------------------------------------------------------------- */
/* initializes parameters needed for spherical computations */
/* -------------------------------------------------------------------- */
void inisp(struct grd_header hdr, double dt, double *r0, double *r1m, double *r1n, double *r2m,
	double *r2n, double *r3m, double *r3n, double *r4m, double *r4n) {

	int row;
	double phim_rad, phin_rad, omega, raio_t, dxtemp, dytemp;

	raio_t = 6.371e6;
	omega = 7.2722e-5;
	dxtemp = raio_t * hdr.x_inc * D2R;
	dytemp = raio_t * hdr.y_inc * D2R;
	for (row = 0; row < hdr.ny; row++) {
		phim_rad = (hdr.y_min + row * hdr.y_inc) * D2R;
		phin_rad = (hdr.y_min + (row + 0.5) * hdr.y_inc) * D2R;
		r0[row] = dt / dytemp;
		r1m[row] = sin(phim_rad);
		r1n[row] = cos(phin_rad);
		r2m[row] = dt / dxtemp / cos(phim_rad);
		r2n[row] = dt / dytemp / cos(phin_rad);
		r3m[row] = NORMAL_GRAV * (dt / dxtemp) / cos(phim_rad);
		r3n[row] = NORMAL_GRAV * (dt / dytemp);
		r4m[row] = dt * omega * sin(phim_rad);
		r4n[row] = dt * omega * sin(phin_rad);
	}
}

/* -------------------------------------------------------------------- */
/* solves non linear continuity equation w/ spherical coordinates */
/* Computes water depth (htotal) needed for friction and */
/* moving boundary algorithm */
/* htotal < 0 - dry cell htotal (m) above still water */
/* htotal > 0 - wet cell with htotal (m) of water depth */
/* -------------------------------------------------------------------- */
void mass_sp(struct grd_header hdr, double *bat, double *etaa, double *htotal_d, double *fluxm_a,
	double *fluxn_a, double *etad, double *r0, double *r1m, double *r1n, double *r2m, double *r2n) {

	unsigned int ij;
	int row, col;
	int cm1, rm1, rowm1;			/* previous column (cm1 = col -1) and row (rm1 = row - 1) */
	double etan, dd;

	for (row = 0; row < hdr.ny; row++) {
		ij = row * hdr.nx;
		rm1 = ((row == 0) ? 0 : 1) * hdr.nx;
		rowm1 = MAX(row - 1, 0);
		for (col = 0; col < hdr.nx; col++) {
			cm1 = (col == 0) ? 0 : 1;
			/* case ocean and non permanent dry area */
			if (bat[ij] > MAXRUNUP) {
				etan = etaa[ij] - r2m[row] * (fluxm_a[ij] - fluxm_a[ij-cm1]) - r2n[row] *
					(fluxn_a[ij] * r1n[row] - fluxn_a[ij-rm1] * r1n[rowm1]);
				if (fabs(etan) < EPS10) etan = 0;
				dd = etan + bat[ij];
				/* caso da zona molhavel */
				if (dd >= EPS10) {
					htotal_d[ij] = dd;
					etad[ij] = etan;
				}
				else {
					htotal_d[ij] = 0;
					etad[ij] = -bat[ij];
				}
			}
			else {			/* nas regioes dry poe o h a 0 e o eta a -bat */
				htotal_d[ij] = 0;
				etad[ij] = -bat[ij];
			}
			ij++;
		}
	}
}

/* ---------------------------------------------------------------------- */
/* Solve nonlinear momentum equation, in spherical coordinates */
/* with moving boundary */
/* ---------------------------------------------------------------------- */
void moment_sp(struct grd_header hdr, double dt, double manning2, double *htotal_a, double *htotal_d,
	double *bat, double *etad, double *fluxm_a, double *fluxn_a, double *fluxm_d, double *fluxn_d,
	double *vex, double *vey, double *r0, double *r1m, double *r1n, double *r2m, double *r2n,
	double *r3m, double *r3n, double *r4m, double *r4n, int isNested) {

	unsigned int ij;
	int first, last, jupe, row, col, ilin = 1;
	int cm1, rm1;			/* previous column (cm1 = col - 1) and row (rm1 = row - 1) */
	int cp1, rp1;			/* next column (cp1 = col + 1) and row (rp1 = row + 1) */
	int cm2, rm2;
	double ff, hp, hq, cte;
	double dd, df, xp, xq, xpe, xqe, xpp, xqq, advx, advy;
	double dpa_ij, dpa_ij_rp1, dpa_ij_rm1, dpa_ij_cm1, dpa_ij_cp1;
	double dqa_ij, dqa_ij_rp1, dqa_ij_rm1, dqa_ij_cm1, dqa_ij_cp1;

	/* - fixes the width of the lateral buffer for linear aproximation */
	/* - if jupe>nnx/2 and jupe>nny/2 linear model will be applied */
	if (isNested) {
		jupe  = 0;
		first = 0;
		last  = 1;
	}
	else {
		jupe  = 10;
		first = 0;
		last  = 1;
	}

	/* - fixes friction parameter */
	cte = (manning2) ? dt * 4.9 : 0;

	for (row = 0 + first; row < hdr.ny - last; row++) {		/* - main computation cycle fluxm_d */
		rp1 = (row < hdr.ny-1) ? hdr.nx : 0;
		rm1 = ((row == 0) ? 0 : 1) * hdr.nx;
		ij = row * hdr.nx - 1;
		for (col = 0 + first; col < hdr.nx - last; col++) {
			cp1 = (col < hdr.nx-1) ? 1 : 0;
			cm1 = (col == 0) ? 0 : 1;
			ij++;
			/* no flux to permanent dry areas */
			if (bat[ij] <= MAXRUNUP) {
				fluxm_d[ij] = vex[ij] = 0;
				continue;
			}

			dpa_ij = (htotal_d[ij] + htotal_a[ij] + htotal_d[ij+cp1] + htotal_a[ij+cp1]) * 0.25;

			/* - moving boundary - Imamura algorithm following cho 2009 */
			if (htotal_d[ij] > EPS5 && htotal_d[ij+cp1] > EPS5) {
				/* - case b2 */
				if (-bat[ij+cp1] >= etad[ij]) {
					dd = htotal_d[ij+cp1];
					df = dd;
					/* case d2 */
				}
				else if (-bat[ij] >= etad[ij+cp1]) {
					dd = htotal_d[ij];
					df = dd;
				}
				else {
					/* case b3/d3 */
					dd = (htotal_d[ij] + htotal_d[ij+cp1]) * 0.5;
					if (dd < EPS5) dd = 0;
					df = dpa_ij;
				}
				/* - case a3/d1 wet dry */
			}
			else if (htotal_d[ij] >= EPS5 && htotal_d[ij+cp1] < EPS5 && etad[ij] > etad[ij+cp1]) {
				if (bat[ij] > bat[ij+cp1])
					dd = etad[ij] - etad[ij+cp1];
				else
					dd = htotal_d[ij];

				df = dd;
				/* - case b1 and c3 dry-wet */
			}
			else if (htotal_d[ij] < EPS5 && htotal_d[ij+cp1] >= EPS5 && etad[ij] < etad[ij+cp1]) {
				if (bat[ij] > bat[ij+cp1])
					dd = htotal_d[ij+cp1];
				else
					dd = etad[ij+cp1] - etad[ij];
				df = dd;
			}
			else {		/* - other cases no moving boundary a1,a2,c1,c2 */
				fluxm_d[ij] = 0;
				vex[ij] = 0;
				continue;
			}
			/* - no flux if dd too smal */
			if (dd < EPS5) {
				fluxm_d[ij] = 0;
				vex[ij] = 0;
				continue;
			}
			if (df < EPS3) df = EPS3;
			xqq = (fluxn_a[ij] + fluxn_a[ij+cp1] + fluxn_a[ij-rm1] + fluxn_a[ij+cp1-rm1]) * 0.25;
			if (manning2) {
				ff = cte * manning2 * sqrt(fluxm_a[ij] * fluxm_a[ij] + xqq * xqq) / pow(df, 2.333333);
			} 
			else
				ff = 0;

			/* - computes linear terms in spherical coordinates */
			xp = (1 - ff) * fluxm_a[ij] - r3m[row] * dd * (etad[ij+cp1] - etad[ij]); /* - includes coriolis */
			if (hdr.doCoriolis)
				xp += r4m[row] * 2 * xqq;
			/* - computes convection terms */
			advx = advy = 0;
			/* - total water depth is smaller than EPS3 >> linear */
			if (dpa_ij < EPS3)
				goto L120;
			/* - lateral buffer >> linear */
			if (col < jupe || col > (hdr.nx - jupe - 1) || row < jupe || row > (hdr.ny - jupe -1 ))
				goto L120;
			/* - upwind scheme for x-direction volume flux */
			if (fluxm_a[ij] < 0) {
				dpa_ij_cp1 = (htotal_d[ij+cp1] + htotal_a[ij+cp1] + htotal_d[ij+2*cp1] + htotal_a[ij+2*cp1]) * 0.25;
				if (dpa_ij_cp1 < EPS3 || htotal_d[ij+cp1] < EPS5) {
					advx = r2m[row] * (-(fluxm_a[ij] * fluxm_a[ij]) / dpa_ij);
				}
				else {
					advx = r2m[row] * (fluxm_a[ij+cp1]*fluxm_a[ij+cp1] / dpa_ij_cp1 - fluxm_a[ij]*fluxm_a[ij] / dpa_ij);
				}
			}
			else {
				dpa_ij_cm1 = (htotal_d[ij-cm1] + htotal_a[ij-cm1] + htotal_d[ij] + htotal_a[ij]) * 0.25;
				if (dpa_ij_cm1 < EPS3 || htotal_d[ij] < EPS5)
					advx = r2m[row] * (fluxm_a[ij] * fluxm_a[ij] / dpa_ij);

				else
					advx = r2m[row] * (fluxm_a[ij]*fluxm_a[ij] / dpa_ij) - r2m[row] *
						(fluxm_a[ij-cm1]*fluxm_a[ij-cm1] / dpa_ij_cm1);
			}
			/* - upwind scheme for y-direction volume flux */
			if (xqq < 0) {
				dpa_ij_rp1 = (htotal_d[ij+rp1] + htotal_a[ij+rp1] + htotal_d[ij+cp1+rp1] + htotal_a[ij+cp1+rp1]) * 0.25;
				if (htotal_d[ij+rp1] < EPS5 || htotal_d[ij+cp1+rp1] < EPS5)
					advy = r0[row] * (-fluxm_a[ij] * xqq / dpa_ij);

				else if (dpa_ij_rp1 < EPS5)
					advy = r0[row] * (-fluxm_a[ij] * xqq / dpa_ij);

				else {
					xqe = (fluxn_a[ij+rp1] + fluxn_a[ij+cp1+rp1] + fluxn_a[ij] + fluxn_a[ij+cp1]) * 0.25;
					advy = r0[row] * (-fluxm_a[ij] * xqq / dpa_ij) + r0[row] * (fluxm_a[ij+rp1] * xqe / dpa_ij_rp1);
				}
			} 
			else {
				dpa_ij_rm1 = (htotal_d[ij-rm1] + htotal_a[ij-rm1] + htotal_d[ij+cp1-rm1] + htotal_a[ij+cp1-rm1]) * 0.25;
				if (htotal_d[ij-rm1] < EPS5 || htotal_d[ij+cp1-rm1] < EPS5)
					advy = r0[row] * fluxm_a[ij] * xqq / dpa_ij;

				else if (dpa_ij_rm1 < EPS5)
					advy = r0[row] * fluxm_a[ij] * xqq / dpa_ij;

				else {
					rm2 = ((row < 2) ? 0 : 2) * hdr.nx;
					xqe = (fluxn_a[ij-rm1] + fluxn_a[ij+cp1-rm1] + fluxn_a[ij-rm2] + fluxn_a[ij+cp1-rm2]) * 0.25;
					advy = r0[row] * (fluxm_a[ij] * xqq / dpa_ij) - r0[row] * (fluxm_a[ij-rm1] * xqe / dpa_ij_rm1);
				}
			}
			/* - disregards very small advection terms */
			if (fabs(advx) <= EPS10) advx = 0;
			if (fabs(advy) <= EPS10) advy = 0;
			/* adds linear+convection terms */
			xp = xp - advx - advy;
L120:
			xp /= (ff + 1);
			if (fabs(xp) < EPS10) xp = 0;

			fluxm_d[ij] = xp;
			/* elimina velocidades maiores que 10m/s para dd<1m */
			if (dd > EPS3)
				vex[ij] = xp / df;
			else
				vex[ij] = 0;
			if (df < 1 && fabs(vex[ij]) > 10)
				vex[ij] = 0;
		}
	}

	/* - main computation cycle fluxn_d */
	for (row = 0 + first; row < hdr.ny - last; row++) {
		rp1 = (row < hdr.ny-1) ? hdr.nx : 0;
		rm1 = ((row == 0) ? 0 : 1) * hdr.nx;
		ij = row * hdr.nx - 1;
		for (col = 0 + first; col < hdr.nx - last; col++) {
			cp1 = (col < hdr.nx-1) ? 1 : 0;
			cm1 = (col == 0) ? 0 : 1;
			ij++;
			/* no flux to permanent dry areas */
			if (bat[ij] <= MAXRUNUP) {
				fluxn_d[ij] = vey[ij] = 0;
				continue;
			}

			dqa_ij = (htotal_d[ij] + htotal_a[ij] + htotal_d[ij+rp1] + htotal_a[ij+rp1]) * 0.25;

			/* - moving boundary - Imamura algorithm following cho 2009 */
			if (htotal_d[ij] > EPS5 && htotal_d[ij+rp1] > EPS5) {
				/* - case b2 */
				if (-bat[ij+rp1] >= etad[ij]) {
					dd = htotal_d[ij+rp1];
					df = dd;
				}
				else if (-bat[ij] >= etad[ij+rp1]) {	/* casse d2 */
					dd = htotal_d[ij];
					df = dd;
				} 
				else {	/* case b3/d3 */
					dd = (htotal_d[ij] + htotal_d[ij+rp1]) * 0.5;
					if (dd < EPS5) dd = 0;
					df = dqa_ij;
				}
			/* - case a3 and d1 wet dry */
			}
			else if (htotal_d[ij] > EPS5 && htotal_d[ij+rp1] <= EPS5 && etad[ij] > etad[ij+rp1]) {
				if (bat[ij] > bat[ij+rp1])
					dd = etad[ij] - etad[ij+rp1];
				else
					dd = htotal_d[ij];
				df = dd;
				/* - case b1 and c3 dry-wet */
			}
			else if (htotal_d[ij] <= EPS5 && htotal_d[ij+rp1] > EPS5 && etad[ij] < etad[ij+rp1]) {
				if (bat[ij] > bat[ij+rp1])
					dd = htotal_d[ij+rp1];
				else
					dd = etad[ij+rp1] - etad[ij];
				df = dd;
			} 
			else {				/* - other cases no moving boundary */
				fluxn_d[ij] = 0;
				vey[ij] = 0;
				continue;
			}
			/* - no flux if dd too small */
			if (dd < EPS5) {
				fluxn_d[ij] = 0;
				vey[ij] = 0;
				continue;
			}
			if (df < EPS3) df = EPS3;
			xpp = (fluxm_a[ij] + fluxm_a[ij+rp1] + fluxm_a[ij-cm1] + fluxm_a[ij-cm1+rp1]) * 0.25;
			if (manning2) 
				ff = cte * manning2 * sqrt(fluxn_a[ij] * fluxn_a[ij] + xpp * xpp) / pow(df, 2.333333);
			else
				ff = 0;

			/* - computes linear terms of N in cartesian coordinates */
			xq = (1 - ff) * fluxn_a[ij] - r3n[row] * dd * (etad[ij+rp1] - etad[ij]);
			/* - includes coriolis effect */
			if (hdr.doCoriolis)
				xq -= r4n[row] * 2 * xpp;

			/* - computes convection terms */
			advx = advy = 0;
			/* - lateral buffer >> linear */
			if (col < jupe || col > (hdr.nx - jupe - 1) || row < jupe || row > (hdr.ny - jupe - 1))
				goto L200;
			/* - total water depth is smaller than EPS3 >> linear */
			if (dqa_ij < EPS3)
				goto L200;

			/* - upwind scheme for y-direction volume flux */
			/* - total water depth is smaller than EPS5 >> linear */
			if (fluxn_a[ij] < 0) {
				dqa_ij_rp1 = (htotal_d[ij+rp1] + htotal_a[ij+rp1] + htotal_d[ij+2*rp1] + htotal_a[ij+2*rp1]) * 0.25;
				if (dqa_ij_rp1 < EPS5 || htotal_d[ij+rp1] < EPS5)
					advy = r0[row] * (-(fluxn_a[ij] * fluxn_a[ij]) / dqa_ij);
				else
					advy = r0[row] * (fluxn_a[ij+rp1]*fluxn_a[ij+rp1] / dqa_ij_rp1 -
						fluxn_a[ij] * fluxn_a[ij] / dqa_ij);
			} 
			else {
				dqa_ij_rm1 = (htotal_d[ij-rm1] + htotal_a[ij-rm1] + htotal_d[ij] + htotal_a[ij]) * 0.25;
				if (dqa_ij_rm1 < EPS3 || htotal_d[ij] < EPS5)
					advy = r0[row] * (fluxn_a[ij] * fluxn_a[ij]) / dqa_ij;
				else
					advy = r0[row] * (fluxn_a[ij]*fluxn_a[ij] / dqa_ij) - r0[row] *
						(fluxn_a[ij-rm1]*fluxn_a[ij-rm1] / dqa_ij_rm1);
			}
			/* - upwind scheme for x-direction volume flux */
			if (xpp < 0) {
				dqa_ij_cp1 = (htotal_d[ij+cp1] + htotal_a[ij+cp1] + htotal_d[ij+rp1+cp1] + htotal_a[ij+rp1+cp1]) * 0.25;
				if (htotal_d[ij+cp1] < EPS5 || htotal_d[ij+cp1+rp1] < EPS5)
					advx = r2n[row] * (-fluxn_a[ij] * xpp / dqa_ij);

				else if (dqa_ij_cp1 < EPS3)
					advx = r2n[row] * (-fluxn_a[ij] * xpp / dqa_ij);
 
				else {
					xpe = (fluxm_a[ij+cp1] + fluxm_a[ij+cp1+rp1] + fluxm_a[ij] + fluxm_a[ij+rp1]) * 0.25;
					advx = r2n[row] * (-fluxn_a[ij] * xpp / dqa_ij) + r2n[row] * (fluxn_a[ij+cp1] * xpe / dqa_ij_cp1);
				}
			} 
			else {
				dqa_ij_cm1 = (htotal_d[ij-cm1] + htotal_a[ij-cm1] + htotal_d[ij+rp1-cm1] + htotal_a[ij+rp1-cm1]) * 0.25;
				if (htotal_d[ij-cm1] < EPS5 || htotal_d[ij-cm1+rp1] < EPS5)
					advx = r2n[row] * (fluxn_a[ij] * xpp / dqa_ij);

				else if (dqa_ij_cm1 < EPS3)
					advx = r2n[row] * fluxn_a[ij] * xpp / dqa_ij;

				else {
					cm2 = (col < 2) ? 0 : 2;
					xpe = (fluxm_a[ij-cm1] + fluxm_a[ij-cm1+rp1] + fluxm_a[ij-cm2] + fluxm_a[ij-cm2+rp1]) * 0.25;
					advx = r2n[row] * (fluxn_a[ij] * xpp / dqa_ij) - r2n[row] * (fluxn_a[ij-cm1] * xpe / dqa_ij_cm1);
				}
			}
			/* disregards very small advection terms */
			if (fabs(advx) <= EPS10) advx = 0;
			if (fabs(advy) <= EPS10) advy = 0;
			/* adds linear+convection terms */
			xq = xq - advx - advy;
L200:
			xq /= (ff + 1);
			if (fabs(xq) < EPS10) xq = 0;

			fluxn_d[ij] = xq;
			/* elimina velocidades maiores que 10m/s para dd<1m */
			if (dd > EPS3)
				vey[ij] = xq / df;
			else
				vey[ij] = 0;
 
			if (df < 1 && fabs(vey[ij]) > 10)
				vey[ij] = 0;
		}
	}
}

/* -------------------------------------------------------------------------- */
int initialize_nestum(struct nestContainer *nest, struct grd_header hdr, int isGeog, int lev) {
	/* Read inner grid and initialize the nest struct.
	   The 'hdr' struct must contain the header of the parent grid. */

	unsigned int nm, nm2;
	int row, col, i, nSizeIncX, nSizeIncY, n;
	double dt;
	double xoff, yoff, xoff_P, yoff_P;		/* Offsets to move from grid to pixel registration (zero if grid in pix reg) */
	struct srf_header hdr_s;
	FILE *fp;

	if (nest->bat[lev] == NULL) {		/* ... */
		fp = fopen ("bat_L1.grd", "rb");
		read_header_bin (fp, &hdr_s);
		fclose(fp);

		nm  = hdr_s.nx * hdr_s.ny;
		nm2 = (hdr_s.nx + 1) * (hdr_s.ny + 1);
		if ((nest->bat[lev] = (double *)mxCalloc ((size_t)(nm), sizeof(double)) ) == NULL) 
			{no_sys_mem("(bat)", nm);}

		read_grd_bin ("bat_L1.grd", &hdr_s, nest->bat[lev]);
		nest->hdr[lev].nx    = hdr_s.nx;       nest->hdr[lev].ny    = hdr_s.ny;
		nest->hdr[lev].x_min = hdr_s.x_min;    nest->hdr[lev].x_max = hdr_s.x_max;
		nest->hdr[lev].y_min = hdr_s.y_min;    nest->hdr[lev].y_max = hdr_s.y_max;
		nest->hdr[lev].z_min = hdr_s.z_min;    nest->hdr[lev].z_max = hdr_s.z_max;
		nest->hdr[lev].x_inc = (hdr_s.x_max - hdr_s.x_min) / (hdr_s.nx - 1);
		nest->hdr[lev].y_inc = (hdr_s.y_max - hdr_s.y_min) / (hdr_s.ny - 1);
	}
	else {
		nm  = nest->hdr[lev].nx * nest->hdr[lev].ny;
		nm2 = (nest->hdr[lev].nx + 1) * (nest->hdr[lev].ny + 1);
	}

	/* -------------------- Check that this grid is nestifiable -------------------- */
	nSizeIncX = irint(hdr.x_inc / nest->hdr[lev].x_inc);
	if ((hdr.x_inc / nest->hdr[lev].x_inc) - nSizeIncX > 1e-5)
		mexErrMsgTxt("BARNABEU: X increments of inner and outer grids are incompatible.");

	nSizeIncY = irint(hdr.y_inc / nest->hdr[lev].y_inc);
	if ((hdr.y_inc / nest->hdr[lev].y_inc) - nSizeIncY > 1e-5)
		mexErrMsgTxt("BARNABEU: Y increments of inner and outer grids are incompatible.");

	if (nSizeIncX != nSizeIncY)
		mexErrMsgTxt("BARNABEU: X/Y increments of inner and outer grid do not divide equaly.");

	//if (nSizeIncX % 2 == 0)
		//mexErrMsgTxt("BARNABEU: Outer/Inner increment must be an odd number.");

	nest->incRatio[lev] = nSizeIncX;
	/* ----------------------------------------------------------------------------- */

	if (lev > 0)
		nest->dt_P[lev] = nest->dt[lev-1];		/* Only now we have info to initialize these */

	/* Compute the run time step interval for this level */
	dt = 0.5 * MIN(nest->hdr[lev].x_inc, nest->hdr[lev].y_inc) / sqrt(NORMAL_GRAV * fabs(nest->hdr[lev].z_min));
	nest->dt[lev] = nest->dt_P[lev] / ceil(nest->dt_P[lev] / dt);

	/* ------- Make a copy of parent grid header into this level hdr_parent[lev] member -------- */
	nest->hdr_P[lev].nx    = hdr.nx;
	nest->hdr_P[lev].ny    = hdr.ny;
	nest->hdr_P[lev].x_min = hdr.x_min;
	nest->hdr_P[lev].x_max = hdr.x_max;
	nest->hdr_P[lev].y_min = hdr.y_min;
	nest->hdr_P[lev].y_max = hdr.y_max;
	nest->hdr_P[lev].z_min = hdr.z_min;
	nest->hdr_P[lev].z_max = hdr.z_max;
	nest->hdr_P[lev].x_inc = hdr.x_inc;
	nest->hdr_P[lev].y_inc = hdr.y_inc;
	nest->hdr_P[lev].lat_min4Coriolis = hdr.lat_min4Coriolis;
	nest->hdr_P[lev].doCoriolis = hdr.doCoriolis;
	/* ------------------------------------------------------------------------------------------ */

	/* Allocate the working arrays */
	if ((nest->etaa[lev] = (double *)     mxCalloc ((size_t)nm, sizeof(double)) ) == NULL)
		{no_sys_mem("(etaa)", nm);}
	if ((nest->etad[lev] = (double *)     mxCalloc ((size_t)nm, sizeof(double)) ) == NULL)
		{no_sys_mem("(etad)", nm);}
	if ((nest->fluxm_a[lev] = (double *)  mxCalloc ((size_t)nm, sizeof(double)) ) == NULL)
		{no_sys_mem("(fluxm_a)", nm);}
	if ((nest->fluxm_d[lev] = (double *)  mxCalloc ((size_t)nm, sizeof(double)) ) == NULL)
		{no_sys_mem("(fluxm_d)", nm);}
	if ((nest->fluxn_a[lev] = (double *)  mxCalloc ((size_t)nm, sizeof(double)) ) == NULL)
		{no_sys_mem("(fluxn_a)", nm);}
	if ((nest->fluxn_d[lev] = (double *)  mxCalloc ((size_t)nm, sizeof(double)) ) == NULL)
		{no_sys_mem("(fluxn_d)", nm);}
	if ((nest->htotal_a[lev] = (double *) mxCalloc ((size_t)nm2,sizeof(double)) ) == NULL)
		{no_sys_mem("(htotal_a)", nm);}
	if ((nest->htotal_d[lev] = (double *) mxCalloc ((size_t)nm2,sizeof(double)) ) == NULL)
		{no_sys_mem("(htotal_d)", nm);}
	if ((nest->vex[lev] = (double *)      mxCalloc ((size_t)nm, sizeof(double)) ) == NULL)
		{no_sys_mem("(vex)", nm);}
	if ((nest->vey[lev] = (double *)      mxCalloc ((size_t)nm, sizeof(double)) ) == NULL)
		{no_sys_mem("(vey)", nm);}

	if (isGeog == 1) {		/* case spherical coordinates  */
		n = nest->hdr[lev].ny;
		if ((nest->r0[lev] = (double *)  mxCalloc ((size_t)n,	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r0)", n);}
		if ((nest->r1m[lev] = (double *) mxCalloc ((size_t)n,	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r1m)", n);}
		if ((nest->r1n[lev] = (double *) mxCalloc ((size_t)n,	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r1n)", n);}
		if ((nest->r2m[lev] = (double *) mxCalloc ((size_t)n,	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r2m)", n);}
		if ((nest->r2n[lev] = (double *) mxCalloc ((size_t)n,	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r2n)", n);}
		if ((nest->r3m[lev] = (double *) mxCalloc ((size_t)n,	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r3m)", n);}
		if ((nest->r3n[lev] = (double *) mxCalloc ((size_t)n,	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r3n)", n);}
		if ((nest->r4m[lev] = (double *) mxCalloc ((size_t)n,	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r4m)", n);}
		if ((nest->r4n[lev] = (double *) mxCalloc ((size_t)n,	sizeof(double)) ) == NULL) 
			{no_sys_mem("(r4n)", n);}
	}

	/* These two must be set to zero if inner grid was already pixel registrated */
	xoff = nest->hdr[lev].x_inc / 2;
	yoff = nest->hdr[lev].y_inc / 2;
	xoff_P = nest->hdr_P[lev].x_inc / 2;
	yoff_P = nest->hdr_P[lev].y_inc / 2;

	/* Compute the 4 coorners coordinates of the nodes on the parent grid embracing the nested grid */
	nest->LLx[lev] = (nest->hdr[lev].x_min - xoff) - hdr.x_inc / 2;
	nest->LLy[lev] = (nest->hdr[lev].y_min - yoff) - hdr.y_inc / 2;
	nest->ULx[lev] = (nest->hdr[lev].x_min - xoff) - hdr.x_inc / 2;
	nest->ULy[lev] = (nest->hdr[lev].y_max + yoff) + hdr.y_inc / 2;
	nest->URx[lev] = (nest->hdr[lev].x_max + xoff) + hdr.x_inc / 2;
	nest->URy[lev] = (nest->hdr[lev].y_max + yoff) + hdr.y_inc / 2;
	nest->LRx[lev] = (nest->hdr[lev].x_max + xoff) + hdr.x_inc / 2;
	nest->LRy[lev] = (nest->hdr[lev].y_min - yoff) - hdr.y_inc / 2;

	/* The row e column indices corresponding to above computed coordinates */
	nest->LLrow[lev] = irint((nest->LLy[lev] - hdr.y_min) / hdr.y_inc);
	nest->LLcol[lev] = irint((nest->LLx[lev] - hdr.x_min) / hdr.x_inc);
	nest->ULrow[lev] = irint((nest->ULy[lev] - hdr.y_min) / hdr.y_inc);
	nest->ULcol[lev] = irint((nest->ULx[lev] - hdr.x_min) / hdr.x_inc);
	nest->URrow[lev] = irint((nest->URy[lev] - hdr.y_min) / hdr.y_inc);
	nest->URcol[lev] = irint((nest->URx[lev] - hdr.x_min) / hdr.x_inc);
	nest->LRrow[lev] = irint((nest->LRy[lev] - hdr.y_min) / hdr.y_inc);
	nest->LRcol[lev] = irint((nest->LRx[lev] - hdr.x_min) / hdr.x_inc);

	/* Allocate vectors of the size of side inner grid to hold the BC */
	n = nest->hdr[lev].nx;
	nest->edge_rowTmp[lev] = (double *)mxCalloc((size_t)n, sizeof(double));	/* To be filled by interp */
	nest->edge_row[lev]    = (double *)mxCalloc((size_t)n, sizeof(double));
	/* Compute XXs of nested grid along N/S edge. We'll use only the south border coordinates */
	for (col = 0; col < n; col++)
		nest->edge_row[lev][col] = nest->hdr[lev].x_min + xoff + col * nest->hdr[lev].x_inc;

	n = nest->hdr[lev].ny;
	nest->edge_colTmp[lev] = (double *)mxCalloc((size_t)n, sizeof(double)); /* To be filled by interp */
	nest->edge_col[lev]    = (double *)mxCalloc ((size_t)n, sizeof(double));
	for (row = 0; row < n; row++)		/* Compute YYs of inner grid along W/E edge */
		nest->edge_col[lev][row] = nest->hdr[lev].y_min + yoff + row * nest->hdr[lev].y_inc;

	/* These two will be used to make copies of data around the connected boundary on parent grid */
	n = nest->LRcol[lev] - nest->LLcol[lev] + 1;
	nest->edge_row_Ptmp[lev] = (double *)mxCalloc((size_t)n, sizeof(double));
	nest->edge_row_P[lev]    = (double *)mxCalloc((size_t)n, sizeof(double));
	for (i = 0; i < n; i++)
		nest->edge_row_P[lev][i] = nest->LLx[lev] + xoff_P + i * hdr.x_inc;      /* XX coords of parent grid along N/S edge */

	n = nest->ULrow[lev] - nest->LLrow[lev] + 1;
	nest->edge_col_Ptmp[lev] = (double *)mxCalloc((size_t)n, sizeof(double));
	nest->edge_col_P[lev]    = (double *)mxCalloc((size_t)n, sizeof(double));
	for (i = 0; i < n; i++)
		nest->edge_col_P[lev][i] = nest->LLy[lev] + xoff_P + i * hdr.y_inc;      /* YY coords of parent grid along W/E edge */

	/* ------------------- PRECISA REVISÃO MAS TAMBÉM PRECISA INICIALIZAR --------------- */
	nest->hdr[lev].lat_min4Coriolis = 0;
	nest->hdr[lev].doCoriolis = hdr.doCoriolis;

	return(0);	
}

/* ----------------------------------------------------------------------------------------- */
void interp_edges(struct nestContainer *nest, double *flux_L1, double *flux_L2, char *what, int lev) {
	/* Interpolate outer Fluxes on boundary edges with the resolution of the nested grid
	   and assign them to inner grid, at its boundaries. */
	int i, ij, n, col, row;
	double s, t1, t2;		/* Three temp vars */

	if (what[0] == 'N') {			/* Only FLUXN uses this branch */
		n = (nest->LRcol[lev] - nest->LLcol[lev] + 1);
		/* SOUTH boundary */
		s = nest->hdr[lev].y_inc / nest->hdr_P[lev].y_inc;
		for (i = 0, col = nest->LLcol[lev]; col <= nest->LRcol[lev]; col++, i++) {
			t1 = flux_L1[ij_grd(col, nest->LLrow[lev],   nest->hdr_P[lev])];
			t2 = flux_L1[ij_grd(col, nest->LLrow[lev]+1, nest->hdr_P[lev])];
			nest->edge_row_Ptmp[lev][i] = (t1 + t2) * 0.5;
		}
		intp_lin (nest->edge_row_P[lev], nest->edge_row_Ptmp[lev], n, nest->hdr[lev].nx,
			nest->edge_row[lev], nest->edge_rowTmp[lev]);
		for (col = 0; col < nest->hdr[lev].nx; col++) {		/* Put interp val in nested grid */
			if (nest->bat[lev][ij_grd(col, 0, nest->hdr[lev])] + nest->etaa[lev][ij_grd(col, 0, nest->hdr[lev])] > EPS5)
				flux_L2[ij_grd(col, 0, nest->hdr[lev])] = nest->edge_rowTmp[lev][col];
			else
				flux_L2[ij_grd(col,0,nest->hdr[lev])] = 0;
		}

		/* NORTH boundary */
		for (i = 0, col = nest->LLcol[lev]; col <= nest->LRcol[lev]; col++, i++) {
			t1 = flux_L1[ij_grd(col, nest->ULrow[lev]-1, nest->hdr_P[lev])];
			t2 = flux_L1[ij_grd(col, nest->ULrow[lev],   nest->hdr_P[lev])];
			nest->edge_row_Ptmp[lev][i] = (t1 + t2) * 0.5;
		}
		intp_lin (nest->edge_row_P[lev], nest->edge_row_Ptmp[lev], n, nest->hdr[lev].nx,
			nest->edge_row[lev], nest->edge_rowTmp[lev]);
		for (col = 0; col < nest->hdr[lev].nx; col++) {
			if (nest->bat[lev][ij_grd(col, nest->hdr[lev].ny-1, nest->hdr[lev])] +
			    nest->etaa[lev][ij_grd(col, nest->hdr[lev].ny-1, nest->hdr[lev])] > EPS5)
				flux_L2[ij_grd(col, nest->hdr[lev].ny-1, nest->hdr[lev])] = nest->edge_rowTmp[lev][col];
			else
				flux_L2[ij_grd(col,nest->hdr[lev].ny-1,nest->hdr[lev])] = 0;
		}
	}
	else {							/* Only FLUXM uses this branch */
		n = (nest->ULrow[lev] - nest->LLrow[lev] + 1);
		/* WEST (left) boundary. */
		s = nest->hdr[lev].x_inc / nest->hdr_P[lev].x_inc;
		for (i = 0, row = nest->LLrow[lev]; row <= nest->ULrow[lev]; row++, i++) {
			t1 = flux_L1[ij_grd(nest->LLcol[lev],   row, nest->hdr_P[lev])];
			t2 = flux_L1[ij_grd(nest->LLcol[lev]+1, row, nest->hdr_P[lev])];
			nest->edge_col_Ptmp[lev][i] = (t1 + t2) * 0.5;
		}
		intp_lin (nest->edge_col_P[lev], nest->edge_col_Ptmp[lev], n, nest->hdr[lev].ny,
			nest->edge_col[lev], nest->edge_colTmp[lev]);
		for (row = 0; row < nest->hdr[lev].ny; row++) {		/* Put interp val in nested grid */
			if (nest->bat[lev][ij_grd(0, row, nest->hdr[lev])] + nest->etaa[lev][ij_grd(0, row, nest->hdr[lev])] > EPS5)
				flux_L2[ij_grd(0,row,nest->hdr[lev])] = nest->edge_colTmp[lev][row];
			else
				flux_L2[ij_grd(0,row,nest->hdr[lev])] = 0;
		}

		/* EAST (right) boundary */
		for (i = 0, row = nest->LLrow[lev]; row <= nest->ULrow[lev]; row++, i++) {
			t1 = flux_L1[ij_grd(nest->LRcol[lev]-1, row, nest->hdr_P[lev])];
			t2 = flux_L1[ij_grd(nest->LRcol[lev],   row, nest->hdr_P[lev])];
			nest->edge_col_Ptmp[lev][i] = (t1 + t2) * 0.5;
		}
		intp_lin (nest->edge_col_P[lev], nest->edge_col_Ptmp[lev], n, nest->hdr[lev].ny,
			nest->edge_col[lev], nest->edge_colTmp[lev]);
		for (row = 0; row < nest->hdr[lev].ny; row++) {
			if (nest->bat[lev][ij_grd(nest->hdr[lev].nx-1,  row, nest->hdr[lev])] +
			    nest->etaa[lev][ij_grd(nest->hdr[lev].nx-1, row, nest->hdr[lev])] > EPS5)
				flux_L2[ij_grd(nest->hdr[lev].nx-1, row, nest->hdr[lev])] = nest->edge_colTmp[lev][row];
			else
				flux_L2[ij_grd(nest->hdr[lev].nx-1, row, nest->hdr[lev])] = 0;
		}
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * INTP_LIN will interpolate from the dataset <x,y> onto a new set <u,v>
 * where <x,y> and <u> is supplied by the user. <v> is returned. 
 *
 * input:  x = x-values of input data
 *	   y = y-values "    "     "
 *	   n = number of data pairs
 *	   m = number of desired interpolated values
 *	   u = x-values of these points
 * output: v = y-values at interpolated points
 *
 * Programmer:	Paul Wessel
 * Date:	16-JAN-1987
 * Ver:		v.2 --> cripled version for linear interpolation (J.L.)
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

int intp_lin (double *x, double *y, int n, int m, double *u, double *v) {
	int i, j, err_flag = 0;
	int down = FALSE;
	double dx;
	
	/* Check to see if x-values are monotonically increasing/decreasing */

	dx = x[1] - x[0];
	if (dx > 0.0) {
		for (i = 2; i < n && err_flag == 0; i++)
			if ((x[i] - x[i-1]) < 0.0) err_flag = i;
	}
	else {
		down = TRUE;
		for (i = 2; i < n && err_flag == 0; i++)
			if ((x[i] - x[i-1]) > 0.0) err_flag = i;
	}

	if (err_flag) {
		mexPrintf ("%s: Fatal Error: x-values are not monotonically increasing/decreasing!\n", "intp_lin");
		return (err_flag);
	}
	
	if (down) {	/* Must flip directions temporarily */
		for (i = 0; i < n; i++) x[i] = -x[i];
		for (i = 0; i < m; i++) u[i] = -u[i];
	}

	/* Compute the interpolated values from the coefficients */
	
	j = 0;
	for (i = 0; i < m; i++) {
		while (x[j] > u[i] && j > 0) j--;	/* In case u is not sorted */
		while (j < n && x[j] <= u[i]) j++;
		if (j == n) j--;
		if (j > 0) j--;

		dx = u[i] - x[j];
		v[i] = (y[j+1]-y[j])*dx/(x[j+1]-x[j]) + y[j];
		if (mxIsNaN(v[i]) || mxIsInf(v[i]))
			v[i] = 0;
	}

	if (down) {	/* Must reverse directions */
		for (i = 0; i < n; i++) x[i] = -x[i];
		for (i = 0; i < m; i++) u[i] = -u[i];
	}

	return (0);
}

/* --------------------------------------------------------------------------------------------- */
/* upscale from doughter to parent level
/* --------------------------------------------------------------------------------------------- */
void upscale(struct nestContainer *nest, double *out, int lev, int i_tsr) {
	/* Computes the mean of cells inside a square window
	   lev   -> This grid level
	*/
	int	inc, k, col, row, col_P, row_P, wcol, wrow, prow, count, half, do_half = FALSE, r,c;
	unsigned int ij;
	double	*p, *pa, soma, *bat_P, t;

	inc = nest->incRatio[lev];	/* Grid spatial ratio between Parent and doughter */
	bat_P = (lev == 0) ? nest->bat_L0 : nest->bat[lev-1];	/* Parent bathymetry */

	for (ij = 0; ij < nest->hdr[lev].nx * nest->hdr[lev].ny; ij++)
		if (nest->bat[lev][ij] < 0) nest->etad[lev][ij] += nest->bat[lev][ij];

	if (i_tsr % 2 == 0) do_half = TRUE;  /* Compute eta as the mean of etad & etaa */

	half = rint(floor(nest->incRatio[lev] * nest->incRatio[lev] * 2.0 / 3.0));
	for (r = row = prow = 0, row_P = nest->LLrow[lev]+1; row < nest->hdr[lev].ny-inc*0; row_P++, prow++, row += inc,r++) {
		ij = ij_grd(nest->LLcol[lev] + 1,  nest->LLrow[lev] + 1 + prow,  nest->hdr_P[lev]);
		for (c = col = 0, col_P = nest->LLcol[lev]+1; col < nest->hdr[lev].nx-inc*0; col_P++, col += inc,c++) {
			k = col + row * nest->hdr[lev].nx;       /* Index of window's LL corner */
			soma = 0;
			count = 0;
			for (wrow = 0; wrow < inc; wrow++) {     /* Loop rows inside window */
				p = &nest->etad[lev][k + wrow * nest->hdr[lev].nx];
				if (do_half) pa = &nest->etaa[lev][k + wrow * nest->hdr[lev].nx];
				for (wcol = 0; wcol < inc; wcol++) {
					if (nest->bat[lev][k + wcol + wrow * nest->hdr[lev].nx] + *p > EPS5) {
						if (do_half)
							soma += (*p + *pa) * 0.5;
						else
							soma += *p;
						count++;
					}
					p++;
					if (do_half) pa++;
				}
			}
			/* --- case when more than 50% of daugther cells add to a mother cell */
			if (soma && count > half) {
				if (bat_P[ij_grd(col_P, row_P, nest->hdr_P[lev])] < 0)
					out[ij] = soma / count - bat_P[ij_grd(col_P, row_P, nest->hdr_P[lev])];
				else
					out[ij] = soma / count;
			}
//if (r == 2 && c == 40 && out[ij])
	//mexPrintf("MERDA_j ij = %d\t%.8f\n", ij, out[ij]);
			ij++;
		}
	}

	/* Replicate Left and Bottom boundaries */
	for (row = 0; row < nest->hdr[lev].ny; row++) {
		nest->etad[lev][ij_grd(0, row, nest->hdr[lev])] = nest->etad[lev][ij_grd(1, row, nest->hdr[lev])];
		nest->etad[lev][ij_grd(nest->hdr[lev].nx-1, row, nest->hdr[lev])] =
			nest->etad[lev][ij_grd(nest->hdr[lev].nx-2, row, nest->hdr[lev])];
	}

	for (col = 0; col < nest->hdr[lev].nx; col++) {
		nest->etad[lev][ij_grd(col, 0, nest->hdr[lev])] = nest->etad[lev][ij_grd(col, 1, nest->hdr[lev])];
		nest->etad[lev][ij_grd(col, nest->hdr[lev].ny-1, nest->hdr[lev])] =
			nest->etad[lev][ij_grd(col, nest->hdr[lev].ny-2, nest->hdr[lev])];
	}

	/* --- reputs bathymetry on etad on land --- */
	for (ij = 0; ij < nest->hdr[lev].nx * nest->hdr[lev].ny; ij++)
		if (nest->bat[lev][ij] < 0) nest->etad[lev][ij] -= nest->bat[lev][ij];
}

/* --------------------------------------------------------------------- */
/* upscale from doughter to parent level
/* --------------------------------------------------------------------- */
void upscale_(struct nestContainer *nest, double *etad, int lev, int i_tsr) {
	/* i_tst -> loop variable over the time step ration of the two grids */
	int grids_time_step_ratio, k, nhalf, count, row, col;
	int i0, j0, ii, jj, ki, kj, r,c;
	unsigned int ij;
	double sum, half, *bat_P;

	bat_P = (lev == 0) ? nest->bat_L0 : nest->bat[lev-1];	/* Parent bathymetry */
	grids_time_step_ratio = irint(nest->dt_P[lev] / nest->dt[lev]);

	for (ij = 0; ij < nest->hdr[lev].nx * nest->hdr[lev].ny; ij++)
		if (nest->bat[lev][ij] < 0) nest->etad[lev][ij] += nest->bat[lev][ij];

	//half = nest->incRatio[lev] * nest->incRatio[lev] / 2.0;
	half = rint(floor(nest->incRatio[lev] * nest->incRatio[lev] * 2.0 / 3.0));
	for (r=0,row = nest->LLrow[lev] + 1; row < nest->ULrow[lev] - 1; row++, r++) {
		i0 = (row - nest->LLrow[lev]-1) * nest->incRatio[lev];
		for (c=0,col = nest->LLcol[lev] + 1; col < nest->LRcol[lev] - 1; col++, c++) {
			j0 = (col - nest->LLcol[lev]-1) * nest->incRatio[lev];
			sum = 0;
			count = 0;
			for (ki = 0; ki < nest->incRatio[lev]; ki++) {
				ii = i0 + ki;
				for (kj = 0; kj < nest->incRatio[lev]; kj++) {
					jj = j0 + kj;
					ij = ij_grd(jj,ii, nest->hdr[lev]);
					if (nest->bat[lev][ij] + nest->etad[lev][ij] > EPS5) {
						if (i_tsr % 2 == 0)
							sum += 0.5 * (nest->etaa[lev][ij] + nest->etad[lev][ij]);
						else
							sum += nest->etad[lev][ij];
						count++;
					}
				}
			}

			/* --- case when more than 50% of daugther cells add to a mother cell */
			if (sum && count > half) {
				if (bat_P[ij_grd(col,row, nest->hdr_P[lev])] < 0)
					etad[ij_grd(col,row, nest->hdr_P[lev])] = sum / count - bat_P[ij_grd(col,row, nest->hdr_P[lev])];
				else
					etad[ij_grd(col,row, nest->hdr_P[lev])] = sum / count;
			}
//ij = ij_grd(col,row, nest->hdr_P[lev]);
//if (r == 2 && c == 40 && etad[ij])
	//mexPrintf("MERDA_m ij = %d\t%.8f\n", ij, etad[ij]);
		}
	}

	/* --- replicates the boundary rows and columns --- */
	for (row = 0; row < nest->hdr[lev].ny; row++)
		nest->etad[lev][ij_grd(0, row, nest->hdr[lev])] = nest->etad[lev][ij_grd(1, row, nest->hdr[lev])];

	for (col = 0; col < nest->hdr[lev].nx; col++)
		nest->etad[lev][ij_grd(col, 0, nest->hdr[lev])] = nest->etad[lev][ij_grd(col, 1, nest->hdr[lev])];

	/* --- reputs bathymetry on etad on land --- */
	for (ij = 0; ij < nest->hdr[lev].nx * nest->hdr[lev].ny; ij++)
		if (nest->bat[lev][ij] < 0) nest->etad[lev][ij] -= nest->bat[lev][ij];
}

/* ---------------------------------------------------------------------------------- */
void sanitize_nestContainer(struct nestContainer *nest) {
	int i;

	for (i = 0; i < 10; i++) {
		nest->manning2[i] = 0;
		nest->LLrow[i] = nest->LLcol[i] = nest->ULrow[i] = nest->ULcol[i] =
		nest->URrow[i] = nest->URcol[i] = nest->LRrow[i] = nest->LRcol[i] =
		nest->incRatio[i] = 0;
		nest->LLx[i] = nest->LLy[i] = nest->ULx[i] = nest->ULy[i] =
		nest->URx[i] = nest->URy[i] = nest->LRx[i] = nest->LRy[i] = 0;
		nest->dt_P[i] = nest->dt[i] = 0;
		nest->bat[i] = NULL;
		nest->fluxm_a[i] = nest->fluxm_d[i] = NULL;
		nest->fluxn_a[i] = nest->fluxm_d[i] = NULL;
		nest->htotal_a[i] = nest->htotal_d[i] = NULL;
		nest->etaa[i] = nest->etad[i] = NULL;
		nest->vex[i] = nest->vey[i] = NULL;
		nest->edge_col[i] = nest->edge_colTmp[i] = NULL;
		nest->edge_row[i] = nest->edge_rowTmp[i] = NULL;
		nest->edge_col_P[i] = nest->edge_col_Ptmp[i] = NULL;
		nest->edge_row_P[i] = nest->edge_row_Ptmp[i] = NULL;
		nest->edge_row_P[i] = nest->edge_row_Ptmp[i] = NULL;
	}
}

/* ------------------------------------------------------------------------------ */
void nestify(struct nestContainer *nest, int nNg, int level, int isGeog) {
	/* nNg -> number of nested grids */
	/* level contains the number of times this function was called recursively.
	   Start with 0 in first call and this counter is increased internally */
	int m, n, j, last_iter;

	for (m = 0; m < nNg; m++) {
		last_iter = (int)(nest->dt_P[level] / nest->dt[level]);  /* No truncations here */
		for (j = 0; j < last_iter; j++) {
			edge_communication(nest, level);
			mass_conservation(nest, isGeog, level);

			/* MAGIC happens here */
			if (m != nNg - 1) {
				level++;
				nestify(nest, nNg - 1, level, isGeog);
				level--;
			}

			moment_conservation(nest, isGeog, level);

			if (j == last_iter - 1) {	/* Do the upscale only at last iteration of this cycle */
				if (level == 0)
					upscale(nest, nest->etad_L0, level, last_iter);
				else
					upscale(nest, nest->etad[level-1], level, last_iter);
			}

			update(nest->hdr[level], nest->etaa[level], nest->etad[level], nest->fluxm_a[level],
				nest->fluxm_d[level], nest->fluxn_a[level], nest->fluxn_d[level],
				nest->htotal_a[level], nest->htotal_d[level]);
		}
	}
}

/* ------------------------------------------------------------------------------ */
void edge_communication(struct nestContainer *nest, int m) {

	if (m == 0) {
		interp_edges(nest, nest->fluxm_a_L0, nest->fluxm_a[m], "M", m);
		interp_edges(nest, nest->fluxn_a_L0, nest->fluxn_a[m], "N", m);
	}
	else {
		interp_edges(nest, nest->fluxm_a[m-1], nest->fluxm_a[m], "M", m);
		interp_edges(nest, nest->fluxn_a[m-1], nest->fluxn_a[m], "N", m);
	}
}

/* ------------------------------------------------------------------------------ */
void mass_conservation(struct nestContainer *nest, int isGeog, int m) {
	/* m is the level of nesting which starts counting at zero for FIRST nesting level */
	if (isGeog == 0)
		mass(nest->hdr[m], nest->dt[m], nest->bat[m], nest->etaa[m], nest->htotal_d[m],
			nest->fluxm_a[m], nest->fluxn_a[m], nest->etad[m]);
	else
		mass_sp(nest->hdr[m], nest->bat[m], nest->etaa[m], nest->htotal_d[m], nest->fluxm_a[m],
			nest->fluxn_a[m], nest->etad[m], nest->r0[m], nest->r1m[m], nest->r1n[m], nest->r2m[m], nest->r2n[m]);
}

/* ------------------------------------------------------------------------------ */
void moment_conservation(struct nestContainer *nest, int isGeog, int m) {
	/* m is the level of nesting which starts counting at zero for FIRST nesting level */
	if (isGeog == 0)
		moment(nest->hdr[m], nest->dt[m], nest->manning2[m], nest->htotal_a[m], nest->htotal_d[m],
			nest->bat[m], nest->etad[m], nest->fluxm_a[m], nest->fluxn_a[m], nest->fluxm_d[m],
			nest->fluxn_d[m], nest->vex[m], nest->vey[m], TRUE);
	else
		moment_sp(nest->hdr[m], nest->dt[m], nest->manning2[m], nest->htotal_a[m], nest->htotal_d[m],
			nest->bat[m], nest->etad[m], nest->fluxm_a[m], nest->fluxn_a[m], nest->fluxm_d[m],
			nest->fluxn_d[m], nest->vex[m], nest->vey[m], nest->r0[m], nest->r1m[m], nest->r1n[m],
			nest->r2m[m], nest->r2n[m], nest->r3m[m], nest->r3n[m], nest->r4m[m], nest->r4n[m], TRUE);
}