#include <stdio.h>
#include <string.h>
#include <math.h>
#include <windows.h>
#include "globals.h"

		/*
		** This function copies the original raw OCT data for processing.
		** It then thresholds the data, cleans it, and calculates morpohology measures (e.g. roughness).
		*/

void ProcessOCT()

{
int		  x,y,z,i,t;
int		  hist[256],chist[256];
void	  RegionGrow();
int		  *indices,RegionSize;
int		  *queue;
int		  min,max,density_threshold;
int		  *hist_proj;
double	  average,avg_diff,stddev;
int		  dx,dy,w,list[1000],list_total,j,smallest,temp;
int		  target_base_z, shifted_z, thickness;
if (oct_stack == NULL)
  return;	/* data not loaded */

		/* using a 1D index to access 3D-indexed array as follows: 1D = (x*ROWS*COLS)+(z*COLS+y) */
		/* each B scan is ROWS x COLS, where ROWS is the depth of the scan, and COLS is the movement of the sensor */
		/* TotalSlices = number of B scans */
		/* coord system origin in the plane of the sensor moving across the top of the biofilm and looking down at it */
		/* x/y is the plane of sensor motion, and z is the imaging depth */
		/* x-axis = which B scan (slice); positive in direction of how the set of scans acquired */
		/* y-axis = which column of data in B scan slice; positive in direction of how scan acquired */
		/* z-axis = which row of data in B scan slice (depth of OCT scan); positive as depth increases (away from sensor) */


		/* create copy of original OCT data for processing */
oct_proc=(unsigned char *)calloc(ROWS*COLS*TotalSlices,1);
for (x=0; x<TotalSlices; x++)
  for (y=0; y<COLS; y++)
	for (z=0; z<ROWS; z++)
	  {
	  oct_proc[(x*ROWS*COLS)+(z*COLS+y)]=oct_stack[(x*ROWS*COLS)+(z*COLS+y)];
	  }


		/* build cumulative histogram and threshold each Z column independently */
		/* this allows for local adaptation, and works a little better than global thresholding */
for (x=0; x<TotalSlices; x++)
  for (y=0; y<COLS; y++)
	{
	for (i=0; i<256; i++)
	  hist[i]=chist[i]=0;
	for (z=0; z<ROWS; z++)
	  hist[oct_proc[(x*ROWS*COLS)+(z*COLS+y)]]++;
	chist[0]=hist[0];
	for (i=1; i<256; i++)
	  chist[i]=chist[i-1]+hist[i];
		/* find threshold that passes 15% of voxels; apply threshold */
	for (t=0; t<256; t++)
	  if ((double)(chist[t]) >= (double)(100-MassThreshold)/100.0*(double)ROWS)
		break;
	t--;
	for (z=0; z<ROWS; z++)
	  {
	  if (oct_proc[(x*ROWS*COLS)+(z*COLS+y)] >= t)
		{
		oct_proc[(x*ROWS*COLS)+(z*COLS+y)]=255;
		}
	  else
		{
		oct_proc[(x*ROWS*COLS)+(z*COLS+y)]=0;
		}
	  }
	}




		/* erase small volumes that are <50 total voxels */
		/* we experimented with 3D volume erasure, but 2D works okay and maybe even a little better */
if (cc_filter_on == 1)
  {
  indices=(int *)calloc(ROWS*COLS/* *TotalSlices */,sizeof(int));
  queue=(int *)calloc(ROWS*COLS/* *TotalSlices */,sizeof(int));
  for (x=0; x<TotalSlices; x++)
	for (y=0; y<COLS; y++)
	  for (z=0; z<ROWS; z++)
		{
		if (oct_proc[(x*ROWS*COLS)+(z*COLS+y)] != 255)
		  continue;
		RegionGrow(queue,ROWS*COLS*TotalSlices,oct_proc,TotalSlices,COLS,ROWS,x,y,z,255,180,indices,&RegionSize);
		if (RegionSize < MinCCSize)	  /* erase region (relabel voxels back to 0) */
		  {
		  for (i=0; i<RegionSize; i++)
			oct_proc[indices[i]]=0;
		  }
		}
  free(queue);
  free(indices);
  }




		/* find density projection in x direction (amount of solid in each y,z vector) */
hist_proj=(int *)calloc(ROWS*COLS,sizeof(int));
		/* first pass sums, and finds min/max */
min=TotalSlices;
max=-1;
for (z=0; z<ROWS; z++)
  for (y=0; y<COLS; y++)
	{
	hist_proj[z*COLS+y]=0;
	for (x=0; x<TotalSlices; x++)
	  {
	  if (oct_proc[(x*ROWS*COLS)+(z*COLS+y)] > 0)
		hist_proj[z*COLS+y]++;
	  }
	if (hist_proj[z*COLS+y] < min)
	  min=hist_proj[z*COLS+y];
	if (hist_proj[z*COLS+y] > max)
	  max=hist_proj[z*COLS+y];
	}
		/* second pass averages */
oct_x_proj=(unsigned char *)calloc(ROWS*COLS,1);
for (z=0; z<ROWS; z++)
  for (y=0; y<COLS; y++)
	oct_x_proj[z*COLS+y]=(unsigned char)((double)(hist_proj[z*COLS+y]-min)/(double)(max-min)*255.0);
free(hist_proj);

		/* find density projection in y direction (avg intensity in each x,z vector) */
hist_proj=(int *)calloc(ROWS*TotalSlices,sizeof(int));
		/* first pass finds counts, min and max */
min=COLS;
max=-1;
for (z=0; z<ROWS; z++)
  for (x=0; x<TotalSlices; x++)
	{
	hist_proj[z*TotalSlices+x]=0;
	for (y=0; y<COLS; y++)
	  {
	  if (oct_proc[(x*ROWS*COLS)+(z*COLS+y)] > 0)
		hist_proj[z*TotalSlices+x]++;
	  }
	if (hist_proj[z*TotalSlices+x] < min)
	  min=hist_proj[z*TotalSlices+x];
	if (hist_proj[z*TotalSlices+x] > max)
	  max=hist_proj[z*TotalSlices+x];
	}
oct_y_proj=(unsigned char *)calloc(ROWS*TotalSlices,1);
for (z=0; z<ROWS; z++)
  for (x=0; x<TotalSlices; x++)
	oct_y_proj[z*TotalSlices+x]=(unsigned char)((double)(hist_proj[z*TotalSlices+x]-min)/(double)(max-min)*255.0);
free(hist_proj);


if (proj_filter_on == 1)
  {
		/* filter using x-y projections */
  density_threshold=(int)((double)ProjThreshold/100.0*255.0);
  for (x=0; x<TotalSlices; x++)
	for (y=0; y<COLS; y++)
	  for (z=0; z<ROWS; z++)
		{
		if (oct_x_proj[z*COLS+y] < density_threshold  ||  oct_y_proj[z*TotalSlices+x] < density_threshold)
		  oct_proc[(x*ROWS*COLS)+(z*COLS+y)]=0;
		}
  }

	/* zeyut added on 01/23/2024 for correcting distortion */
	/* threshold the projection map to extract baseline surface */
baseline_cal=(int *)calloc(TotalSlices*COLS, sizeof(int));
target_base_z = 1000;
for (x = 0; x < TotalSlices; x++)
	for (y = 0; y < COLS; y++)
	{
		z = 0;
		while (oct_x_proj[z*COLS + y] < 180 && oct_y_proj[z*TotalSlices + x] < 180)
			z++;
		baseline_cal[x*COLS + y] = z;
		if (z < target_base_z) target_base_z = z;
	}

	/* correction, add offsets to each column, not ready yet */
/*
for (x = 0; x < TotalSlices; x++)
	for (y = 0; y < COLS; y++)
	{
		z = 0;
		shifted_z = z - ((int)baseline_cal[x * COLS + y] - target_base_z);
		while (z < ROWS && shifted_z < ROWS)
		{
			shifted_z = z - ((int)baseline_cal[x * COLS + y] - target_base_z);
			if (shifted_z < 0) {
				z++;
				continue;
			}
			oct_proc[(x*ROWS*COLS) + (shifted_z *COLS + y)] = oct_proc[(x*ROWS*COLS) + (z*COLS + y)];
			z++;
		}
	}
	*/
	/* zeyut edition on 01/23/2024 ends here */

		/* find depth image:  min depth to see solid at each x,y location */
/* zeyut changed oct_depth data type on 02/12/2024 */
oct_depth_cal=(int*)calloc(TotalSlices*COLS,sizeof(int));
for (x=0; x<TotalSlices; x++)
  for (y=0; y<COLS; y++)
	{
	z=80;
	while (z < ROWS  &&  oct_proc[(x*ROWS*COLS)+(z*COLS+y)] == 0)
	  z++;
	oct_depth_cal[x * COLS + y] = z;
	}

		/* calculate depth map statistics */
		/* first pass finds the min, max, and average depth, and  */
min=ROWS;
max=-1;
average=0.0;
coverage_ratio=0.0;	// count the total area covered by biofilm
min_thickness=0;
max_thickness=0;

for (x=0; x<TotalSlices; x++)
  for (y=0; y<COLS; y++)
	{
	if (oct_depth_cal[x*COLS+y] < min) min= oct_depth_cal[x*COLS+y];
	if (oct_depth_cal[x*COLS+y] > max) max= oct_depth_cal[x*COLS+y];
	average+=(double)oct_depth_cal[x*COLS+y];
	// base_z-15 is the approximate top surface height
	thickness = (baseline_cal[x * COLS + y] - 15)- oct_depth_cal[x * COLS + y];
	if (thickness>0) {
		coverage_ratio += 1;
		if (thickness < min_thickness) min_thickness = thickness;
		if (thickness > max_thickness) max_thickness = thickness;
		}
	}
if (max <= min)
  max=min+1;
average/=(double)(TotalSlices*COLS);
coverage_ratio/=(double)(TotalSlices*COLS);

		/* second pass calculates the roughness measures */
stddev=0.0;
avg_diff=0.0;
for (x=0; x<TotalSlices; x++)
  for (y=0; y<COLS; y++)
	{
	stddev+=(double)SQR(average- oct_depth_cal[x*COLS+y]);
	avg_diff+=fabs((double)(average- oct_depth_cal[x*COLS+y]));
	}
stddev=sqrt(stddev/(double)(TotalSlices*COLS));
avg_diff=(avg_diff/(double)(TotalSlices*COLS))/(double)average;
roughnessQ=stddev;
roughnessA=avg_diff;
		
		/* median filter the depth map */
		/* not this is currently for visualization only; we could do it before calculations (above) */
if (median_filter_on == 1)
  {
  w=MedianWindowSize;  /* w=1 => 3x3 window; w=2> 5x5 window, etc. */
  for (x=0; x<TotalSlices; x++)
	for (y=0; y<COLS; y++)
	  {
	  list_total=0;
	  for (dx=-w; dx<=w; dx++)
		for (dy=-w; dy<=w; dy++)
		  {
		  if (x+dx < 0  ||  x+dx >= TotalSlices  ||  y+dy < 0  ||  y+dy >= COLS)
			continue;
		  list[list_total]= oct_depth_cal[(x+dx)*COLS+y+dy];
		  list_total++;
		  }
	  for (i=0; i<=list_total/2; i++)
		{
		smallest=i;
		for (j=i+1; j<list_total; j++)
		  if (list[j] < list[smallest])
			smallest=j;
		temp=list[i];
		list[i]=list[smallest];
		list[smallest]=temp;
		}
	  oct_depth_cal[x*COLS+y]=list[i-1];	/* median value in window */
	  }
	/*
	memset(list, 0, 1000);;
	for (x = 0; x < TotalSlices; x++)
			for (y = 0; y < COLS; y++)
			{
					list_total = 0;
					for (dx = -w; dx <= w; dx++)
							for (dy = -w; dy <= w; dy++)
							{
									if (x + dx < 0 || x + dx >= TotalSlices || y + dy < 0 || y + dy >= COLS)
											continue;
									list[list_total] = baseline_cal[(x + dx) * COLS + y + dy];
									list_total++;
							}
					for (i = 0; i <= list_total / 2; i++)
					{
							smallest = i;
							for (j = i + 1; j < list_total; j++)
									if (list[j] < list[smallest])
											smallest = j;
							temp = list[i];
							list[i] = list[smallest];
							list[smallest] = temp;
					}
					baseline_cal[x * COLS + y] = list[i - 1];
			}
	*/
  }


		/* invert for visualization:  darker = shorter/farther from sensor, brighter = taller/closer to sensor */
oct_depth = (unsigned char*)calloc(TotalSlices * COLS, 1);
for (x=0; x<TotalSlices; x++)
  for (y=0; y<COLS; y++)
	{
	  oct_depth[x*COLS+y]=255-(unsigned char)(oct_depth_cal[x*COLS+y]/2);
	}
baseline = (unsigned char*)calloc(TotalSlices * COLS, 1);
for (x = 0; x < TotalSlices; x++)
		for (y = 0; y < COLS; y++)
		{
				baseline[x*COLS+y]=255-(unsigned char)(baseline_cal[x*COLS+y]/2);
		}


}



        /*
        ** Given an image, a starting point, and a label, this routine
        ** paint-fills (8-connected) the volume with the given new label.
		** Code for a 3D (23-connected) version is available below, commented out (wasn't needed).
		** Code that indexes a 3D array is also commented out (went with 1D index to save space/time).
        */

void RegionGrow(int *queue,					  /* queue used for region grow */
				int MaxQueue,				  /* size of the queue */
				unsigned char *image,		  /* image data */	/* 3D indexed array could be used here, but this is simpler */
                int DEPTH,int COLS,int ROWS,  /* size of image */
                int d,int c,int r,			  /* voxel to paint from */
                int paint_over_label,		  /* image label to paint over */
                int new_label,				  /* image label for painting */
                int *indices,				  /* output:  indices of pixels painted [(d*ROWS*COLS)+(r*COLS+c)] */
                int *count)					  /* output:  count of pixels painted */
{
int     d2,c2,r2,plane,plane_location;
int     qh,qt;

*count=0;
//if (image[d][c][r] != paint_over_label)	/* for 3D indexed array */
if (image[(d*ROWS*COLS)+(r*COLS+c)] != paint_over_label)
  return;
//image[d][c][r]=new_label;					/* for 3D indexed array */
image[(d*ROWS*COLS)+(r*COLS+c)]=new_label;
if (indices != NULL)
  indices[0]=(d*ROWS*COLS)+(r*COLS+c);
queue[0]=(d*ROWS*COLS)+r*COLS+c;
qh=1;   /* queue head */
qt=0;   /* queue tail */
(*count)=1;
while (qt != qh)
  {
  plane=queue[qt]/(ROWS*COLS);
  plane_location=queue[qt]%(ROWS*COLS);
  for (r2=-1; r2<=1; r2++)
    for (c2=-1; c2<=1; c2++)
	  {
	  d2=0;						  /* using 2D area search for connected component analysis */
//	  for (d2=-1; d2<=1; d2++)	  /* can convert to 3D volume search if this loop is included */
		{
		if (r2 == 0  &&  c2 == 0  &&  d2 == 0)
		  continue;
		if (plane+d2 < 0  ||  plane+d2 >= DEPTH)
		  continue;
		if ((plane_location/COLS+r2) < 0  ||  (plane_location/COLS+r2) >= ROWS  ||
			(plane_location%COLS+c2) < 0  ||  (plane_location%COLS+c2) >= COLS)
		  continue;
//		if (image[plane+d2][plane_location%COLS+c2][plane_location/COLS+r2] != paint_over_label)	/* for 3D indexed array */
		if (image[((plane+d2)*ROWS*COLS)+((plane_location/COLS+r2)*COLS+(plane_location%COLS+c2))] != paint_over_label)
		  continue;
//		image[plane+d2][plane_location%COLS+c2][plane_location/COLS+r2]=new_label;	/* for 3D indexed array */
		image[((plane+d2)*ROWS*COLS)+((plane_location/COLS+r2)*COLS+(plane_location%COLS+c2))]=new_label;
		if (indices != NULL)
		  indices[*count]=((plane+d2)*ROWS*COLS)+((plane_location/COLS+r2)*COLS+(plane_location%COLS+c2));
		(*count)++;
		queue[qh]=((plane+d2)*ROWS*COLS)+((plane_location/COLS+r2)*COLS+(plane_location%COLS+c2));
		qh=(qh+1)%MaxQueue;
		if (qh == qt)
		  {		/* should never happen, but just in case... */
		  MessageBox(NULL,"Max queue size exceeded.","Error",MB_OK | MB_APPLMODAL);
		  exit(0);
		  }
		}
	  }
  qt=(qt+1)%MaxQueue;
  }
}








