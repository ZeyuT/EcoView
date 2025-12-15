
	/*******************************************************************
	** Some routines which deal with files (reading, writing, etc.)
	** Precompiled libtiff (v3.8.2) taken from at https://gnuwin32.sourceforge.net/packages.html
	*******************************************************************/

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <jpeglib.h>
#include <tiffio.h>
#include "globals.h"


	/*******************************************************************
	** Read a tif, jpg or ppm image from a file.  Returns 1 if TIF image read,
	** 2 if JPG, 0 otherwise.
	*******************************************************************/

int ReadImage(char			*filename,			/* complete name of image to read */
			  unsigned char	**raw,				/* image data, returned */
			  int			*ROWS,int *COLS)	/* size of image, returned */

{
FILE							*fpt;
int								i,r;
unsigned char					header[320];
struct jpeg_decompress_struct	cinfo;
struct jpeg_error_mgr			jerr;
JSAMPARRAY						buffer;
HDC								hDC;
char							LoadText[320];
TIFF							*tif;
int								w,h;
unsigned char					*raster;


if ((fpt=fopen(filename,"rb")) == NULL)
  {	  // let caller display warning message; here just return 0 
  // MessageBox(NULL,filename,"Unable to open file:",MB_APPLMODAL | MB_OK);
  return(0);
  }

		/* see if image is TIFF format */
fread(&(header[0]),1,4,fpt);
if (header[0] == 0x49  &&  header[1] == 0x49  &&  header[2] == 0x2A  &&  header[3] == 0x00)
  {		/* is in TIFF format -- close and start over to read it */
  fclose(fpt);
  tif=TIFFOpen(filename,"rb");
  TIFFGetField(tif,TIFFTAG_IMAGEWIDTH,&w);
  TIFFGetField(tif,TIFFTAG_IMAGELENGTH,&h);
		/* determine #images in this file, to allocate space for 3D cube */
  TotalSlices=1;
  while (TIFFReadDirectory(tif))
	TotalSlices++;
  (*raw)=(unsigned char *)calloc(h*w*TotalSlices,1);
  TIFFSetDirectory(tif,0);	/* go back to start of file */
  CurrentSlice=0;
  raster=(unsigned char *)calloc(h*w*4,1);
  while (CurrentSlice < TotalSlices)
	{
	i=TIFFReadRGBAImageOriented(tif,w,h,(unsigned int *)raster,ORIENTATION_TOPLEFT,0);
	for (i=0; i<h*w; i++)
	  {
	  (*raw)[(CurrentSlice*h*w)+i]=raster[i*4+0];	/* assumes all 3 RGB bytes are the same, alpha byte is always 255 */
	  }
	TIFFReadDirectory(tif);	  /* advance to next image's directory */
	CurrentSlice++;
	}
  TIFFClose(tif);
  free(raster);
  (*ROWS)=h;
  (*COLS)=w;
  CurrentSlice=0;
  return(1);
  }
		/* see if in jpeg format */
if (header[0] == 0xFF  &&  header[1] == 0xD8  &&  header[2] == 0xFF  &&  header[3] == 0xE0)
  {		/* is in jpeg format -- start over and read in */
  fclose(fpt);
  fpt=fopen(filename,"rb");
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, fpt);
  jpeg_read_header(&cinfo, TRUE);
  jpeg_start_decompress(&cinfo);
  *COLS=cinfo.output_width;
  *ROWS=cinfo.output_height;
  if ((*raw=(unsigned char *)calloc((*ROWS)*(*COLS)*3,1)) == NULL)
    {
    MessageBox(MainWnd,"Unable to allocate memory","ReadImage()",MB_APPLMODAL | MB_OK);
	return(0);
    }
  r=0;
  buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, (*COLS)*3, 1);
  hDC=GetDC(MainWnd);
  sprintf(LoadText,"Loading %s ...",filename);
  while (r < *ROWS)
    {
	if (r%50 == 0)
	  {
	  strcat(LoadText,".");
	  TextOut(hDC,0,0,LoadText,strlen(LoadText));
	  }
    jpeg_read_scanlines(&cinfo, buffer, 1);
    for (i=0; i<(*COLS)*3; i++)
      (*raw)[r*(*COLS)*3+i]=(unsigned char)buffer[0][i];
    r++;
    }
  ReleaseDC(MainWnd,hDC);
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  fclose(fpt);
  return(2);
  }
fclose(fpt);
MessageBox(MainWnd,filename,"Unknown image format",MB_APPLMODAL | MB_OK);
return(0);
}



	/*******************************************************************
	** Saves an image to a file in either jpeg (FileType=1) or PPM/PGM
	** (FileType=2) format.  Returns a 1 after a successful save, 0 otherwise.
	*******************************************************************/

int WriteImage(char			 *filename,		/* complete name of image to save */
			   unsigned char *image,		/* image data, RGB L->R T->D format */
			   int			 ROWS,int COLS,	/* size of image */
			   int			 BPP,			/* bytes per pixel of image */
			   int			 FileType)		/* 2 jpeg, 1 PPM/PGM */

{
FILE							*fpt;
struct jpeg_compress_struct		cinfo;
struct jpeg_error_mgr			jerr;
JSAMPROW						row_pointer[1];
HDC								hDC;
char							SaveText[250];

if ((fpt=fopen(filename,"wb")) == NULL)
  {
  MessageBox(MainWnd,filename,"Unable to open for writing:",MB_APPLMODAL | MB_OK);
  return(0);
  }
if (FileType == 2)
  {
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, fpt);
  cinfo.image_width = COLS;
  cinfo.image_height = ROWS;
  cinfo.input_components = BPP;
  if (BPP == 1)
	cinfo.in_color_space=JCS_GRAYSCALE;
  else
    cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, 100, TRUE );
  jpeg_start_compress(&cinfo, TRUE);
  hDC=GetDC(MainWnd);
  sprintf(SaveText,"Writing %s ...",filename);
  while (cinfo.next_scanline < cinfo.image_height)
    {
    if ((cinfo.next_scanline)%50 == 0)
      {
      strcat(SaveText,".");
      TextOut(hDC,0,0,SaveText,strlen(SaveText));
      }
    row_pointer[0] = & image[cinfo.next_scanline*COLS*BPP];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
  jpeg_finish_compress(&cinfo);
  fclose(fpt);
  jpeg_destroy_compress(&cinfo);

  sprintf(SaveText, "Finished Writing %s", filename);
  TextOut(hDC, 0, 0, SaveText, strlen(SaveText));
  ReleaseDC(MainWnd,hDC);


  return(1);
  }
else if (FileType == 1)
  {
  fprintf(fpt,"P%d %d %d 255\n",(BPP == 1 ? 5 : 6),COLS,ROWS);
  fwrite(image,1,ROWS*COLS*BPP,fpt);
  fclose(fpt);
  return(1);
  }
else
  {
  MessageBox(NULL,"Unknown File Type","WriteImage() Error",MB_OK | MB_APPLMODAL);
  fclose(fpt);
  return(0);
  }
}
