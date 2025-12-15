#include <stdio.h>
#include <windows.h>
#include <winuser.h>
#include "resource.h"
#include "globals.h"


static int				DISPLAY_ROWS,DISPLAY_COLS;	// display window
static unsigned char	*disp_image;				// display data


		/* This function supports RGB, greyscale, image->display pixel mapping, stretch pixel mapping */
		/* We might not use it all but I'm leaving it available just in case */

void MakeDisplayImage(unsigned char *Image,
					  int IMAGE_ROWS,int IMAGE_COLS,
					  int BYTES_PER_PIXEL)

{
int				r,c,r2,c2,i,rCell,cCell,slice;
static int		LastRows=0,LastCols=0;
SCROLLINFO		BarPos;
RECT			MainRect;

ScaleBarCol = IMAGE_COLS - 30; // X of scalebar origin (bottom-right)
ScaleBarRow = IMAGE_ROWS - 30;  // Y of scalebar origin (bottom-right)
ScaleBarXlen = 57;
ScaleBarYlen = 57;

GetClientRect(MainWnd,&MainRect);
DISPLAY_ROWS=MainRect.bottom-MainRect.top;
DISPLAY_COLS=MainRect.right-MainRect.left;
if (DISPLAY_ROWS < 100)
  DISPLAY_ROWS=100;
if (DISPLAY_COLS < 100)
  DISPLAY_COLS=100;
if (DISPLAY_ROWS % 4 != 0)	// Windows pads to 4-byte boundaries
  DISPLAY_ROWS=(DISPLAY_ROWS/4+1)*4;
if (DISPLAY_COLS % 4 != 0)
  DISPLAY_COLS=(DISPLAY_COLS/4+1)*4;

if (ScaleImageToWindow == 0  &&  DISPLAY_ROWS < IMAGE_ROWS*MagFact)
  {
  if (TopDispRow >= IMAGE_ROWS-DISPLAY_ROWS/MagFact)
	TopDispRow=IMAGE_ROWS-DISPLAY_ROWS/MagFact-1;
  if (TopDispRow < 0) TopDispRow=0;
  BarPos.cbSize=sizeof(SCROLLINFO);
  BarPos.fMask=SIF_ALL;
  BarPos.nMin=0; BarPos.nMax=IMAGE_ROWS;
  BarPos.nPage=DISPLAY_ROWS/MagFact;
  BarPos.nPos=TopDispRow;
  SetScrollInfo(MainWnd,SB_VERT,&BarPos,TRUE);
  ShowScrollBar(MainWnd,SB_VERT,TRUE);
  }
else
  {
  if (ScaleImageToWindow == 0)
	TopDispRow=0;	// entire height fits in window
  ShowScrollBar(MainWnd,SB_VERT,FALSE);
  }
if (ScaleImageToWindow == 0  &&  DISPLAY_COLS < IMAGE_COLS*MagFact)
  {
  if (LeftDispCol >= IMAGE_COLS-DISPLAY_COLS/MagFact)
	LeftDispCol=IMAGE_COLS-DISPLAY_COLS/MagFact-1;
  if (LeftDispCol < 0) LeftDispCol=0;
  BarPos.cbSize=sizeof(SCROLLINFO);
  BarPos.fMask=SIF_ALL;
  BarPos.nMin=0; BarPos.nMax=IMAGE_COLS;
  BarPos.nPage=DISPLAY_COLS/MagFact;
  BarPos.nPos=LeftDispCol;
  SetScrollInfo(MainWnd,SB_HORZ,&BarPos,TRUE);
  ShowScrollBar(MainWnd,SB_HORZ,TRUE);
  }
else
  {
  if (ScaleImageToWindow == 0)
	LeftDispCol=0;	// entire width fits in window
  ShowScrollBar(MainWnd,SB_HORZ,FALSE);
  }

if (disp_image != NULL  &&  (LastRows != DISPLAY_ROWS  ||  LastCols != DISPLAY_COLS))
  {
  free(disp_image);
  disp_image=NULL;
  }
if (disp_image == NULL)
  {
  disp_image=(unsigned char *)calloc(DISPLAY_ROWS*DISPLAY_COLS*3,1);
  if (disp_image == NULL)
    {
    MessageBox(MainWnd,"Out of memory","MakeDisplayImage()",MB_APPLMODAL | MB_OK);
    exit(0);
    }
  }


if (Image != NULL  &&  IMAGE_ROWS > 0  &&  IMAGE_COLS > 0)
  {			// have some data to display (not a blank image)
   if (ScaleImageToWindow == 0)
	{		// image will be displayed by mag factor
	  {
	  r2=TopDispRow;
	  c2=LeftDispCol;
	  r=0; c=0; rCell=0; cCell=0;
	  while (r < DISPLAY_ROWS)
	    {
	    while (c < DISPLAY_COLS)
	      {
	      if (r2 < 0  ||  r2 >= IMAGE_ROWS  ||
			  c2 < 0  ||  c2 >= IMAGE_COLS)
	        {
						disp_image[(r * DISPLAY_COLS + c) * 3 + 0] = 0;
						disp_image[(r * DISPLAY_COLS + c) * 3 + 1] = 0;
						disp_image[(r * DISPLAY_COLS + c) * 3 + 2] = 0;
					}
	      else
	        {
			i=(r2*IMAGE_COLS+c2)*BYTES_PER_PIXEL;
			if (DisplayView == 0  ||  DisplayView == 1)
			  slice=CurrentSlice;
			else
			  slice=0;
			if (DisplayView == 4)
				{
				if ((r2 > ScaleBarRow - 5 && r2 < ScaleBarRow &&
						c2 > ScaleBarCol - ScaleBarXlen && c2 < ScaleBarCol) ||
						(r2 > ScaleBarRow - ScaleBarYlen && r2 < ScaleBarRow &&
								c2 > ScaleBarCol - 5 && c2 < ScaleBarCol))
					{
						disp_image[(r * DISPLAY_COLS + c) * 3 + 2] = 0;
						disp_image[(r * DISPLAY_COLS + c) * 3 + 1] = 0;
						disp_image[(r * DISPLAY_COLS + c) * 3 + 0] = 0;
					}
				else
					{
						disp_image[(r * DISPLAY_COLS + c) * 3 + 2] = Image[(slice * IMAGE_ROWS * IMAGE_COLS) + i + 0];
						disp_image[(r * DISPLAY_COLS + c) * 3 + 1] = Image[(slice * IMAGE_ROWS * IMAGE_COLS) + i + BYTES_PER_PIXEL / 3];
						disp_image[(r * DISPLAY_COLS + c) * 3 + 0] = Image[(slice * IMAGE_ROWS * IMAGE_COLS) + i + BYTES_PER_PIXEL / 3 * 2];
					}
				}
			else
				{ 
				disp_image[(r*DISPLAY_COLS+c)*3+2]=Image[(slice*IMAGE_ROWS*IMAGE_COLS)+i+0];
				disp_image[(r*DISPLAY_COLS+c)*3+1]=Image[(slice*IMAGE_ROWS*IMAGE_COLS)+i+BYTES_PER_PIXEL/3];
				disp_image[(r*DISPLAY_COLS+c)*3+0]=Image[(slice*IMAGE_ROWS*IMAGE_COLS)+i+BYTES_PER_PIXEL/3*2];
				}
			}
	      c++;
	      cCell++;
	      if (cCell == MagFact)
			{ cCell=0; c2++; }
	      }
		r++; c=0;
		rCell++; cCell=0; c2=LeftDispCol;
		if (rCell == MagFact)
	      { rCell=0; r2++; }
		}
	  }
	}
  else		// fitting image data to window
		{
		for (r=0; r<DISPLAY_ROWS; r++)
			{
				r2=(int)((double)r/(double)DISPLAY_ROWS*(double)IMAGE_ROWS);
			for (c = 0; c < DISPLAY_COLS; c++)
				{
				c2 = (int)((double)c / (double)DISPLAY_COLS * (double)IMAGE_COLS);
				i = (r2 * IMAGE_COLS + c2) * BYTES_PER_PIXEL;
				if (DisplayView == 0 || DisplayView == 1)
						slice = CurrentSlice;
				else
						slice = 0;
				disp_image[(r * DISPLAY_COLS + c) * 3 + 2] = Image[(slice * IMAGE_ROWS * IMAGE_COLS) + i + 0];
				disp_image[(r * DISPLAY_COLS + c) * 3 + 1] = Image[(slice * IMAGE_ROWS * IMAGE_COLS) + i + BYTES_PER_PIXEL / 3];
				disp_image[(r * DISPLAY_COLS + c) * 3 + 0] = Image[(slice * IMAGE_ROWS * IMAGE_COLS) + i + BYTES_PER_PIXEL / 3 * 2];
				}
			}
		}
	}
else		// no image data given; create blank image
  for (r=0; r<DISPLAY_ROWS*DISPLAY_COLS*3; r++)
	disp_image[r]=0;
LastRows=DISPLAY_ROWS;
LastCols=DISPLAY_COLS;
}



void PaintImage()

{
PAINTSTRUCT			Painter;
HDC					hDC;
BITMAPINFOHEADER	bm_info_header;
BITMAPINFO			bm_info;
char				display_message[320];
int					text_row,i,hist[256];
double				norm_hist[256],cum_dist_func[256];

if (oct_stack == NULL)
  return;

if (ImageTypeLoaded == 2)
  MakeDisplayImage(oct_stack,ROWS,COLS,3);
else if (DisplayView == 0)
  MakeDisplayImage(oct_stack,ROWS,COLS,1);
else if (DisplayView == 1)
  MakeDisplayImage(oct_proc,ROWS,COLS,1);
else if (DisplayView == 2)
  MakeDisplayImage(oct_x_proj,ROWS,COLS,1);
else if (DisplayView == 3)
  MakeDisplayImage(oct_y_proj,ROWS,TotalSlices,1);
else if (DisplayView == 4)
  MakeDisplayImage(oct_depth,TotalSlices,COLS,1);
else if (DisplayView == 5)
	MakeDisplayImage(baseline, TotalSlices, COLS, 1);

if (DisplayEnhance == 1  &&  ImageTypeLoaded == 1)
  {
  for (i=0; i<256; i++)
	hist[i]=0;
  for (i=0; i<DISPLAY_COLS*DISPLAY_ROWS*3; i+=3)
	hist[disp_image[i]]++;
  for (i=0; i<256; i++)		/* exclude hist[0] and hist[255] during equalization */
	norm_hist[i]=(double)(hist[i])/((double)(DISPLAY_ROWS*DISPLAY_COLS-hist[0]-hist[255]));
  cum_dist_func[1]=norm_hist[1];
  for (i=2; i<255; i++)
	cum_dist_func[i]=cum_dist_func[i-1]+norm_hist[i];
  for (i=0; i<DISPLAY_COLS*DISPLAY_ROWS*3; i+=3)
	{
	if (disp_image[i] == 0  ||  disp_image[i] == 255)
	  continue;
	disp_image[i]=disp_image[i+1]=disp_image[i+2]=(unsigned char)(255.0*cum_dist_func[disp_image[i]]);
	}
  }

BeginPaint(MainWnd,&Painter);
hDC=GetDC(MainWnd);
bm_info_header.biSize=sizeof(BITMAPINFOHEADER); 
bm_info_header.biWidth=DISPLAY_COLS;
bm_info_header.biHeight=-DISPLAY_ROWS; 
bm_info_header.biPlanes=1;
bm_info_header.biBitCount=24; 
bm_info_header.biCompression=BI_RGB; 
bm_info_header.biSizeImage=0; 
bm_info_header.biXPelsPerMeter=0; 
bm_info_header.biYPelsPerMeter=0;
bm_info_header.biClrUsed=0; 
bm_info_header.biClrImportant=0;
// bm_info.bmiColors=NULL;
bm_info.bmiHeader=bm_info_header;
SetDIBitsToDevice(hDC,0,0,DISPLAY_COLS,DISPLAY_ROWS,0,0,
			  0, /* first scan line */
			  DISPLAY_ROWS, /* number of scan lines */
			  disp_image,&bm_info,DIB_RGB_COLORS);
SetBkColor(hDC,RGB(0,0,0));
if (DisplayImageDetails == 1)
  {
  text_row=10;
  for (i=0; i<TotalFilenames; i++)
	{
	if (i == FilenameIndex)
	  SetTextColor(hDC,RGB(255,255,255));
	else
	  SetTextColor(hDC,RGB(128,128,128));
	sprintf(display_message,"%s",FilenameList[i]);
	TextOut(hDC,1100,text_row,display_message,strlen(display_message));
	text_row+=20;
	}
  SetTextColor(hDC,RGB(255,255,255));
  sprintf(display_message,"use [] keys to scroll file list");
  TextOut(hDC,1110,text_row,display_message,strlen(display_message));
  }

if (DisplayImageDetails == 1  &&  ImageTypeLoaded == 1)
  {
  text_row=400;
  sprintf(display_message,"%s",image_filename);
  TextOut(hDC,30,text_row,display_message,strlen(display_message));
  if (DisplayView == 0  ||  DisplayView == 1)
	{
	sprintf(display_message,"side view, slice %d of %d    (press <> keys to change)",CurrentSlice+1,TotalSlices);
	TextOut(hDC,30,text_row+20,display_message,strlen(display_message));
	}
  else if (DisplayView == 4)
	{
	// min_thickness,max_thickness are not ready yet
	//sprintf(display_message,"top view of depth:  Ra=%lf Rq=%lf \n coverage_ratio=%f min_thickness=%f max_thickness=%f",
	//		roughnessA,roughnessQ,coverage_ratio,min_thickness,max_thickness);
	sprintf(display_message, "top view of depth:  Ra=%lf Rq=%lf \n coverage_ratio=%f",
		roughnessA, roughnessQ, coverage_ratio);
	TextOut(hDC,30,text_row+20,display_message,strlen(display_message));
	}
  }
if (DisplayView == 4 && DisplayScaleBar == 1)
{
		SetTextColor(hDC, RGB(255, 255, 255));
		sprintf(display_message, "500 um");
		TextOut(hDC, ScaleBarCol - 50, ScaleBarRow + 5, display_message, strlen(display_message));
}
ReleaseDC(MainWnd,hDC);
EndPaint(MainWnd,&Painter);
}



		/* returns the value (RGB) of the given pixel under display,
		** and also returns the row,col coordinate in the original image */

void GetImageValue(int *xPos,int *yPos,int nrows,int ncols,
				   int *red,int *green,int *blue)

{
int		x,y;

if (*xPos >= 0  &&  *xPos < DISPLAY_COLS  &&  *yPos >= 0  &&  *yPos < DISPLAY_ROWS)
  {
  if (ScaleImageToWindow == 0)
    {
	y=(*yPos)-((*yPos)%MagFact);
	x=(*xPos)-((*xPos)%MagFact);
    *red=(int)disp_image[(y*DISPLAY_COLS+x)*3+2];
    *green=(int)disp_image[(y*DISPLAY_COLS+x)*3+1];
    *blue=(int)disp_image[(y*DISPLAY_COLS+x)*3+0];
    }
  else
    {
    *red=(int)disp_image[((*yPos)*DISPLAY_COLS+(*xPos))*3+2];
    *green=(int)disp_image[((*yPos)*DISPLAY_COLS+(*xPos))*3+1];
    *blue=(int)disp_image[((*yPos)*DISPLAY_COLS+(*xPos))*3+0];
    }
  }
if (ScaleImageToWindow == 1)
  {
  *xPos=(int)((double)(*xPos)/(double)DISPLAY_COLS*(double)ncols);
  *yPos=(int)((double)(*yPos)/(double)DISPLAY_ROWS*(double)nrows);
  }
else
  {
  *xPos=LeftDispCol+((*xPos)/MagFact);
  *yPos=TopDispRow+((*yPos)/MagFact);
  }
}





LRESULT CALLBACK MagFactProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)

{
SCROLLINFO	BarInfo;
BOOL		Success;
static HWND	ScrollHwnd,EditHwnd,ButtonHwnd;

switch(uMsg)
  {
  case WM_INITDIALOG:
	SetDlgItemInt(hDlg,ID_EDIT_THRESH,MagFact,FALSE);
	BarInfo.cbSize=sizeof(SCROLLINFO);
	BarInfo.nMin=1;
	BarInfo.nMax=64;
	BarInfo.nPos=MagFact;
	BarInfo.fMask=SIF_POS | SIF_RANGE;
	ScrollHwnd=GetDlgItem(hDlg,ID_SCROLL_THRESH);
	EditHwnd=GetDlgItem(hDlg,ID_EDIT_THRESH);
	ButtonHwnd=GetDlgItem(hDlg,IDD_FIT);
	SetScrollInfo(ScrollHwnd,SB_CTL,&BarInfo,TRUE);
	if (ScaleImageToWindow)
	  {
	  EnableWindow(ScrollHwnd,FALSE);
	  EnableWindow(EditHwnd,FALSE);
	  CheckDlgButton(hDlg,IDD_FIT,BST_CHECKED);
	  }
	else
	  {
	  EnableWindow(ScrollHwnd,TRUE);
	  EnableWindow(EditHwnd,TRUE);
	  CheckDlgButton(hDlg,IDD_FIT,BST_UNCHECKED);
	  }
	break;
  case WM_COMMAND:
	switch(LOWORD(wParam))
	  {
	  case ID_EDIT_THRESH:
		MagFact=GetDlgItemInt(hDlg,ID_EDIT_THRESH,&Success,FALSE);
		if (MagFact < 1) MagFact=1;
		if (MagFact > 64) MagFact=64;
		break;
	  case IDD_FIT:
		if (ScaleImageToWindow)
	      {
	      EnableWindow(ScrollHwnd,TRUE);
	      EnableWindow(EditHwnd,TRUE);
	      CheckDlgButton(hDlg,IDD_FIT,BST_UNCHECKED);
		  ScaleImageToWindow=0;
	      }
	    else
	      {
	      EnableWindow(ScrollHwnd,FALSE);
	      EnableWindow(EditHwnd,FALSE);
	      CheckDlgButton(hDlg,IDD_FIT,BST_CHECKED);
		  ScaleImageToWindow=1;
	      }
		break;
	  case IDOK:
		MagFact=GetDlgItemInt(hDlg,ID_EDIT_THRESH,&Success,FALSE);
		EndDialog(hDlg,IDOK);
		break;
	  }
	break;
  case WM_HSCROLL:
	if (LOWORD(wParam) == SB_LINELEFT) MagFact--;
	if (LOWORD(wParam) == SB_LINERIGHT) MagFact++;
	if (LOWORD(wParam) == SB_PAGELEFT) MagFact/=2;
	if (LOWORD(wParam) == SB_PAGERIGHT) MagFact*=2;
	if (LOWORD(wParam) == SB_THUMBPOSITION) MagFact=HIWORD(wParam);
	if (MagFact < 1) MagFact=1;
	if (MagFact > 64) MagFact=64;
	BarInfo.cbSize=sizeof(SCROLLINFO);
	BarInfo.fMask=SIF_POS;
	BarInfo.nPos=MagFact;
	ScrollHwnd=GetDlgItem(hDlg,ID_SCROLL_THRESH);
	SetScrollInfo(ScrollHwnd,SB_CTL,&BarInfo,TRUE);
	SetDlgItemInt(hDlg,ID_EDIT_THRESH,MagFact,FALSE);
	break;
  default:
	return(FALSE);
  }
return(TRUE);
}





LRESULT CALLBACK ParamsProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)

{
BOOL		Success;
static HWND	EditHwnd;

switch(uMsg)
  {
  case WM_INITDIALOG:
	SetDlgItemInt(hDlg,IDC_EDIT1,MassThreshold,FALSE);
	SetDlgItemInt(hDlg, IDC_EDIT5, CutoffHeight, FALSE);
	SetDlgItemInt(hDlg, IDC_EDIT6, baseline_threshold_x, FALSE);
	SetDlgItemInt(hDlg, IDC_EDIT7, baseline_threshold_y, FALSE);

	SetDlgItemInt(hDlg,IDC_EDIT2,MinCCSize,FALSE);
	SetDlgItemInt(hDlg,IDC_EDIT3,ProjThreshold,FALSE);
	SetDlgItemInt(hDlg,IDC_EDIT4,MedianWindowSize,FALSE);
	EditHwnd=GetDlgItem(hDlg,IDC_EDIT2);
	if (cc_filter_on == 1)
	  {
	  EnableWindow(EditHwnd,TRUE);
	  CheckDlgButton(hDlg,IDC_CHECK1,BST_CHECKED);
	  }
	else
	  {
	  EnableWindow(EditHwnd,FALSE);
	  CheckDlgButton(hDlg,IDC_CHECK1,BST_UNCHECKED);
	  }
	EditHwnd=GetDlgItem(hDlg,IDC_EDIT3);
	if (proj_filter_on == 1)
	  {
	  EnableWindow(EditHwnd,TRUE);
	  CheckDlgButton(hDlg,IDC_CHECK2,BST_CHECKED);
	  }
	else
	  {
	  EnableWindow(EditHwnd,FALSE);
	  CheckDlgButton(hDlg,IDC_CHECK2,BST_UNCHECKED);
	  }
	EditHwnd=GetDlgItem(hDlg,IDC_EDIT4);
	if (median_filter_on == 1)
	  {
	  EnableWindow(EditHwnd,TRUE);
	  CheckDlgButton(hDlg,IDC_CHECK3,BST_CHECKED);
	  }
	else
	  {
	  EnableWindow(EditHwnd,FALSE);
	  CheckDlgButton(hDlg,IDC_CHECK3,BST_UNCHECKED);
	  }
	break;
  case WM_COMMAND:
	switch(LOWORD(wParam))
	  {
	  case IDC_EDIT1:
		MassThreshold=GetDlgItemInt(hDlg,IDC_EDIT1,&Success,FALSE);
		if (MassThreshold < 1) MassThreshold=1;
		if (MassThreshold > 99) MassThreshold=99;
		break;
	  case IDC_EDIT2:
		MinCCSize=GetDlgItemInt(hDlg,IDC_EDIT2,&Success,FALSE);
		if (MinCCSize < 10) MinCCSize=10;
		if (MinCCSize > 99999) MinCCSize=99999;
		break;
	  case IDC_EDIT3:
		ProjThreshold=GetDlgItemInt(hDlg,IDC_EDIT3,&Success,FALSE);
		if (ProjThreshold < 1) ProjThreshold=1;
		if (ProjThreshold > 99) ProjThreshold=99;
		break;
	  case IDC_EDIT4:
		MedianWindowSize=GetDlgItemInt(hDlg,IDC_EDIT4,&Success,FALSE);
		if (MedianWindowSize < 1) MedianWindowSize=1;
		if (MedianWindowSize > 15) MedianWindowSize=15;
		break;
		case IDC_EDIT5:
		CutoffHeight = GetDlgItemInt(hDlg, IDC_EDIT5, &Success, FALSE);
		if (CutoffHeight < 0) CutoffHeight = 0;
		break;
		case IDC_EDIT6:
		baseline_threshold_x = GetDlgItemInt(hDlg, IDC_EDIT6, &Success, FALSE);
		if (baseline_threshold_x < 0) baseline_threshold_x = 0;
		break;
		case IDC_EDIT7:
		baseline_threshold_y = GetDlgItemInt(hDlg, IDC_EDIT7, &Success, FALSE);
		if (baseline_threshold_y < 0) baseline_threshold_y = 0;
		break;
	  case IDC_CHECK1:
		cc_filter_on=(cc_filter_on+1)%2;
		EditHwnd=GetDlgItem(hDlg,IDC_EDIT2);
		if (cc_filter_on == 1)
		  {
		  EnableWindow(EditHwnd,TRUE);
		  CheckDlgButton(hDlg,IDC_CHECK1,BST_CHECKED);
		  }
		else
		  {
		  EnableWindow(EditHwnd,FALSE);
		  CheckDlgButton(hDlg,IDC_CHECK1,BST_UNCHECKED);
		  }
		break;
	  case IDC_CHECK2:
		proj_filter_on=(proj_filter_on+1)%2;
		EditHwnd=GetDlgItem(hDlg,IDC_EDIT3);
		if (proj_filter_on == 1)
		  {
		  EnableWindow(EditHwnd,TRUE);
		  CheckDlgButton(hDlg,IDC_CHECK2,BST_CHECKED);
		  }
		else
		  {
		  EnableWindow(EditHwnd,FALSE);
		  CheckDlgButton(hDlg,IDC_CHECK2,BST_UNCHECKED);
		  }
		break;
	  case IDC_CHECK3:
		median_filter_on=(median_filter_on+1)%2;
		EditHwnd=GetDlgItem(hDlg,IDC_EDIT4);
		if (median_filter_on == 1)
		  {
		  EnableWindow(EditHwnd,TRUE);
		  CheckDlgButton(hDlg,IDC_CHECK3,BST_CHECKED);
		  }
		else
		  {
		  EnableWindow(EditHwnd,FALSE);
		  CheckDlgButton(hDlg,IDC_CHECK3,BST_UNCHECKED);
		  }
		break;
	  case IDOK:
		EndDialog(hDlg,IDOK);
		break;
	  }
	break;
  default:
	return(FALSE);
  }
return(TRUE);
}


