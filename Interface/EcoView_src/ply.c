#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>
#include <process.h>
#include "globals.h"

#define MAX_LENGTH      1500.0

		/* creates a .ply 3D file from the OCT depth map */

void OutputPly()

{
double          *vx,*vy,*vz;
int             *faces,vertices_per_face;
int             TotalFaces,TotalVertices;
int				*map;
double          dist1,dist2,dist3;
int				x,y,i,j;
FILE			*fpt;
char			text[320];
STARTUPINFO		si;
PROCESS_INFORMATION pi;

TotalVertices=0;
TotalFaces=0;
vertices_per_face=3;    /* triangles */
vx=(double *)calloc(COLS*TotalSlices,sizeof(double));
vy=(double *)calloc(COLS*TotalSlices,sizeof(double));
vz=(double *)calloc(COLS*TotalSlices,sizeof(double));
faces=(int *)calloc(COLS*TotalSlices*2*vertices_per_face,sizeof(int));
        /* map holds the vertex index for the given oct column */
        /* values of -1 means no vertex for that column */
map=(int *)calloc(COLS*TotalSlices,sizeof(int));
for (i=0; i<COLS*TotalSlices; i++)
  map[i]=-1;

for (x=0; x<TotalSlices; x++)
  {
  for (y=0; y<COLS; y++)
    {
	vx[TotalVertices]=(double)x;
	vy[TotalVertices]=(double)y;
    vz[TotalVertices]=(double)oct_depth[x*COLS+y];
	map[x*COLS+y]=TotalVertices;
	TotalVertices++;
	}
  }

for (x=0; x<TotalSlices-1; x++)
  {
  for (y=0; y<COLS-1; y++)
    {
    if (map[x*COLS+y] != -1  &&  map[x*COLS+y+1] != -1  &&  map[(x+1)*COLS+y] != -1)
      {
      dist1=sqrt(SQR(vx[map[x*COLS+y]]-vx[map[x*COLS+y+1]])+
                SQR(vy[map[x*COLS+y]]-vy[map[x*COLS+y+1]])+
                SQR(vz[map[x*COLS+y]]-vz[map[x*COLS+y+1]]));
      dist2=sqrt(SQR(vx[map[x*COLS+y]]-vx[map[(x+1)*COLS+y]])+
                SQR(vy[map[x*COLS+y]]-vy[map[(x+1)*COLS+y]])+
                SQR(vz[map[x*COLS+y]]-vz[map[(x+1)*COLS+y]]));
      dist3=sqrt(SQR(vx[map[x*COLS+y+1]]-vx[map[(x+1)*COLS+y]])+
                SQR(vy[map[x*COLS+y+1]]-vy[map[(x+1)*COLS+y]])+
                SQR(vz[map[x*COLS+y+1]]-vz[map[(x+1)*COLS+y]]));
      if (dist1 < MAX_LENGTH  &&  dist2 < MAX_LENGTH  &&  dist3 < MAX_LENGTH)
        {
        faces[TotalFaces*vertices_per_face+0]=map[x*COLS+y];
        faces[TotalFaces*vertices_per_face+1]=map[x*COLS+y+1];
        faces[TotalFaces*vertices_per_face+2]=map[(x+1)*COLS+y];
        TotalFaces++;
        }
      }
    if (map[x*COLS+y+1] != -1  &&  map[(x+1)*COLS+y] != -1  &&  map[(x+1)*COLS+y+1] != -1)
      {
	  dist1=sqrt(SQR(vx[map[x*COLS+y+1]]-vx[map[(x+1)*COLS+y]])+
                SQR(vy[map[x*COLS+y+1]]-vy[map[(x+1)*COLS+y]])+
                SQR(vz[map[x*COLS+y+1]]-vz[map[(x+1)*COLS+y]]));
      dist2=sqrt(SQR(vx[map[x*COLS+y+1]]-vx[map[(x+1)*COLS+y+1]])+
                SQR(vy[map[x*COLS+y+1]]-vy[map[(x+1)*COLS+y+1]])+
                SQR(vz[map[x*COLS+y+1]]-vz[map[(x+1)*COLS+y+1]]));
      dist3=sqrt(SQR(vx[map[(x+1)*COLS+y]]-vx[map[(x+1)*COLS+y+1]])+
                SQR(vy[map[(x+1)*COLS+y]]-vy[map[(x+1)*COLS+y+1]])+
                SQR(vz[map[(x+1)*COLS+y]]-vz[map[(x+1)*COLS+y+1]]));
      if (dist1 < MAX_LENGTH  &&  dist2 < MAX_LENGTH  &&  dist3 < MAX_LENGTH)
        {
        faces[TotalFaces*vertices_per_face+0]=map[x*COLS+y+1];
        faces[TotalFaces*vertices_per_face+1]=map[(x+1)*COLS+y];
        faces[TotalFaces*vertices_per_face+2]=map[(x+1)*COLS+y+1];
        TotalFaces++;
        }
      }
    }
  }

        /* write out ply file */
fpt=fopen(PlyFilename,"wb");
fprintf(fpt,"ply\n");
fprintf(fpt,"format ascii 1.0\n");
fprintf(fpt,"element vertex %d\n",TotalVertices);
fprintf(fpt,"property float32 x\n");
fprintf(fpt,"property float32 y\n");
fprintf(fpt,"property float32 z\n");
fprintf(fpt,"element face %d\n",TotalFaces);
fprintf(fpt,"property list uint8 uint32 vertex_indices\n");
fprintf(fpt,"end_header\n");
for (i=0; i<TotalVertices; i++)
  fprintf(fpt,"%.3lf %.3lf %.3lf\n",vx[i],vy[i],vz[i]);
for (i=0; i<TotalFaces; i++)
  {
  fprintf(fpt,"%d",vertices_per_face);
  for (j=0; j<vertices_per_face; j++)
    fprintf(fpt," %d",faces[i*vertices_per_face+j]);
  fprintf(fpt,"\n");
  }
fclose(fpt);

free(vx);
free(vy);
free(vz);
free(faces);
free(map);

sprintf(text,"\"%s\" \"%s\"",MeshLabFilename,PlyFilename);
ZeroMemory(&si, sizeof(si));
si.cb = sizeof(si);
ZeroMemory(&pi, sizeof(pi));
CreateProcess(NULL,text,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi);

}

