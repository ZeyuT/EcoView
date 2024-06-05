
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <sys/timeb.h>
#include <windows.h>
#include <wingdi.h>
#include <winuser.h>
#include <process.h>    /* _beginthread, _endthread */
#include "resource.h"
#include "globals.h"

int		LOAD_AT_RUNTIME;							// if image double-clicked

LPCTSTR lpszTitle="EcoView";

BOOL RegisterWin95(CONST WNDCLASS* lpwc);


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
				LPTSTR lpCmdLine, int nCmdShow)

{
MSG			msg;
HWND		hWnd;
WNDCLASS	wc;
int			i;
FILE		*fpt;

wc.style=CS_HREDRAW | CS_VREDRAW;
wc.lpfnWndProc=(WNDPROC)WndProc;
wc.cbClsExtra=0;
wc.cbWndExtra=0;
wc.hInstance=hInstance;
wc.hIcon=LoadIcon(hInstance,"ID_ECOVIEW_ICON");
wc.hCursor=LoadCursor(NULL,IDC_ARROW);
wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
wc.lpszMenuName="ID_MAIN_MENU";
wc.lpszClassName="ECOVIEW";

if (!RegisterClass(&wc))
  return(FALSE);

hInst=hInstance;

hWnd=CreateWindow("ECOVIEW",lpszTitle,
		WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL,
		CW_USEDEFAULT,0,400,400,NULL,NULL,hInstance,NULL);
if (!hWnd)
  return(FALSE);

ShowScrollBar(hWnd,SB_BOTH,FALSE);
ShowWindow(hWnd,SW_MAXIMIZE);
UpdateWindow(hWnd);
MainWnd=hWnd;

SoftwareVersion=10;		/* version 1.0 */

ScaleImageToWindow=0;
MagFact=1;
TopDispRow=LeftDispCol=0;
ShowPixelCoords=0;
DisplayImageDetails=1;	  /* show filename, slice number, etc. */
DisplayView=0;			  /* 0=>B-scan (side view), 1=>eleveation map (top view) */
DisplayEnhance=0;		  /* show raw values (not normalized) */

DisplayScaleBar=1;		 /* show scalebar */

strcpy(CurrentPath,"F:\\Data\\BioFilm\\");
strcpy(image_filename,"");
LOAD_AT_RUNTIME=0;
if (strlen(lpCmdLine) > 0)
  {
  if (lpCmdLine[0] == '"')
	{
	strcpy(image_filename,&(lpCmdLine[1]));
	image_filename[strlen(image_filename)-1]='\0';
	}
  else
    strcpy(image_filename,lpCmdLine);
  LOAD_AT_RUNTIME=1;
  PostMessage(hWnd,WM_COMMAND,ID_FILE_LOADIMAGE,0);
  }
else
  {
  strcpy(image_filename,"ecocoating.jpg");
  LOAD_AT_RUNTIME=1;
  PostMessage(hWnd,WM_COMMAND,ID_FILE_LOADIMAGE,0);
  }
oct_stack=oct_proc=oct_depth=NULL;
ROWS=COLS=TotalSlices=0;

		/* check for master list; allows scrolling through multiple files using keystrokes */
TotalFilenames=0;
fpt=fopen("IMAGE_LIST.txt","r");
if (fpt == NULL)
  fpt=fopen("..\\IMAGE_LIST.txt","r");
if (fpt != NULL)
  {
  while (1)
	{
	i=fscanf(fpt,"%s",FilenameList[TotalFilenames]);
	if (i != 1)
	  break;
	TotalFilenames++;
	}
  fclose(fpt);
  }
FilenameIndex=-1;	/* start with none loaded */

strcpy(MeshLabFilename,"C:\\Program Files\\VCG\\MeshLab\\meshlab.exe");
strcpy(PlyFilename,"C:\\Data\\Biofilm\\oct_depth.ply");

MassThreshold=15;
cc_filter_on=1;
proj_filter_on=0;
median_filter_on=1;
MinCCSize=50;
ProjThreshold=10;
MedianWindowSize=3;

InvalidateRect(hWnd,NULL,TRUE);
UpdateWindow(hWnd);

while (GetMessage(&msg,NULL,0,0))
  {
  TranslateMessage(&msg);
  DispatchMessage(&msg);
  }
return(msg.wParam);
}



BOOL RegisterWin95(CONST WNDCLASS* lpwc)

{
WNDCLASSEX wcex;

wcex.style=lpwc->style;
wcex.lpfnWndProc=lpwc->lpfnWndProc;
wcex.cbClsExtra=lpwc->cbClsExtra;
wcex.cbWndExtra=lpwc->cbWndExtra;
wcex.hInstance=lpwc->hInstance;
wcex.hIcon=lpwc->hIcon;
wcex.hCursor=lpwc->hCursor;
wcex.hbrBackground=lpwc->hbrBackground;
wcex.lpszMenuName=lpwc->lpszMenuName;
wcex.lpszClassName=lpwc->lpszClassName;
		// Added elements for Windows 95:
wcex.cbSize=sizeof(WNDCLASSEX);
wcex.hIconSm=LoadIcon(wcex.hInstance,"ID_STARE_ICON");
return (RegisterClassEx(&wcex));
}



LRESULT CALLBACK WndProc (HWND hWnd, UINT uMsg,
		WPARAM wParam, LPARAM lParam)

{
HMENU				hMenu;
HDC					hDC;
RECT				TextRect;
HBRUSH				DrawBrush;
OPENFILENAME		ofn;
static unsigned char *ActiveImage;
static int			ActiveROWS,ActiveCOLS,ActiveBPP;
int					xPos,yPos,red,green,blue,i;
int					image_type;
char				text[200];
DWORD				dwAttrib;

switch (uMsg)
  {
  case WM_COMMAND:
    switch (LOWORD(wParam))
      {
      case ID_FILE_LOADIMAGE:
		if (LOAD_AT_RUNTIME)
		  LOAD_AT_RUNTIME=0;
		else
		  {
		  memset(&(ofn),0,sizeof(ofn));
		  ofn.lStructSize=sizeof(ofn);
		  ofn.lpstrFile=image_filename;
		  image_filename[0]=0;
		  ofn.nMaxFile=MAX_FILENAME_CHARS;
		  ofn.Flags=OFN_EXPLORER | OFN_HIDEREADONLY;
		  ofn.lpstrFilter = "TIFF files\0*.tif\0PPM files\0*.ppm\0JPG files\0*.jpg\0All files\0*.*\0\0";
		  ofn.nFilterIndex=1;
		  if (!(GetOpenFileName(&ofn))  ||  image_filename[0] == '\0')
			break;		/* user cancelled load */
		  image_type=ofn.nFilterIndex;
		  FreeAllOCT();
		  }
		ImageTypeLoaded=ReadImage(image_filename,&oct_stack,&ROWS,&COLS);
		if (ImageTypeLoaded == 0)
		  {
		  MessageBox(hWnd,image_filename,"Cannot read file:",MB_APPLMODAL | MB_OK);
		  break;
		  }
		if (ImageTypeLoaded == 1)  /* only process if a TIF image */
		  {
		  sprintf(text,"EcoView   %s",image_filename);
		  SetWindowText(hWnd,text);
		  ProcessOCT();
		  }
		PaintImage();
		break;

	  case ID_DISPLAY_BSCANS:
		DisplayView=0;
		PaintImage();
		break;
	  case ID_DISPLAY_BINARYBSCANS:
		DisplayView=1;
		PaintImage();
		break;
	  case ID_DISPLAY_XZ_DENSITIES:
		DisplayView=2;
		PaintImage();
		break;
	  case ID_DISPLAY_YZ_DENSITIES:
		DisplayView=3;
		PaintImage();
		break;
	  case ID_DISPLAY_ELEVATIONMAP:
		DisplayView=4;
		PaintImage();
		break;
	  case ID_DISPLAY_BASESURFACE:
		  DisplayView = 5;
		  PaintImage();
		  break;
	  case ID_DISPLAY_3DMODEL:
		if (ImageTypeLoaded != 1)
		  break;
		dwAttrib=GetFileAttributes(MeshLabFilename);
		if (dwAttrib == INVALID_FILE_ATTRIBUTES)
		  {
		  MessageBox(hWnd,"Please install MeshLab to be able to view 3D models.\nhttps://www.meshlab.net\nInstall in default location.",
							"Software not found",MB_APPLMODAL | MB_OK);
		  break;
		  }
		OutputPly();
		break;

	  case ID_OPTIONS_MAGFACT:
		DialogBox(hInst,"ID_MAGFACT_DIALOG",hWnd,(DLGPROC)MagFactProc);
		PaintImage();
		break;
	  case ID_OPTIONS_ENHANCE:
		DisplayEnhance=(DisplayEnhance+1)%2;
		PaintImage();
		break;
	  case ID_OPTIONS_PARAMS:
		DialogBox(hInst,"ID_PARAMS_DIALOG",hWnd,(DLGPROC)ParamsProc);
		if (oct_stack != NULL  &&  ImageTypeLoaded == 1)
		  {
		  ProcessOCT();
		  PaintImage();
		  }
		break;
	  case ID_OPTIONS_SHOWPIXELCOORDINATES:
		ShowPixelCoords=(ShowPixelCoords+1)%2;
		PaintImage();
		break;
	  case ID_OPTIONS_TEXT:
		DisplayImageDetails=(DisplayImageDetails+1)%2;
		PaintImage();
		break;
	  case ID_OPTIONS_ABOUT:
		sprintf(text,"EcoView version %d.%d\n",SoftwareVersion/10,SoftwareVersion%10);
		MessageBox(hWnd,text,"About...",MB_APPLMODAL | MB_OK);
		PaintImage();
		break;

      case ID_EXIT:case ID_FILE_QUIT:
		FreeAllOCT();
        DestroyWindow(hWnd);
        break;
      }
    break;
  case WM_SIZE:
	if (hWnd != MainWnd  ||  GetUpdateRect(hWnd,NULL,FALSE) == 0)
	  {
      return(DefWindowProc(hWnd,uMsg,wParam,lParam));
	  break;
	  }
	PaintImage();
	break;
  case WM_PAINT:
	PaintImage();
    return(DefWindowProc(hWnd,uMsg,wParam,lParam));
	break;
  case WM_CHAR:
	if ((TCHAR)wParam == '<'  ||  (TCHAR)wParam == ','  ||  (TCHAR)wParam == '>'  ||  (TCHAR)wParam == '.')
	  {
	  if (((TCHAR)wParam == '<'  ||  (TCHAR)wParam == ',')  &&  CurrentSlice > 0)
		CurrentSlice--;
	  else if (((TCHAR)wParam == '>'  ||  (TCHAR)wParam == '.')  &&  CurrentSlice < TotalSlices-1)
		CurrentSlice++;
	  else
		break;	  /* no change; do not reload */
	  PaintImage();
	  }
	if ((TCHAR)wParam == '['  ||  (TCHAR)wParam == '{'  ||  (TCHAR)wParam == ']'  ||  (TCHAR)wParam == '}')
	  {
	  if (((TCHAR)wParam == '['  ||  (TCHAR)wParam == '{')  &&  FilenameIndex > 0)
		FilenameIndex--;
	  else if (((TCHAR)wParam == ']'  ||  (TCHAR)wParam == '}')  &&  FilenameIndex < TotalFilenames-1)
		FilenameIndex++;
	  else
		break;	  /* no change; do not reload */
	  strcpy(image_filename,FilenameList[FilenameIndex]);
	  strcpy(CurrentPath,image_filename);
	  i=strlen(CurrentPath)-1;
	  while (i > 0  &&  CurrentPath[i] != '\\')
		i--;
	  CurrentPath[i]='\0';
	  SetCurrentDirectory(CurrentPath);
	  FreeAllOCT();
	  ImageTypeLoaded=ReadImage(image_filename,&oct_stack,&ROWS,&COLS);
	  if (ImageTypeLoaded == 0)
		{
		MessageBox(hWnd,image_filename,"Cannot read file:",MB_APPLMODAL | MB_OK);
		break;
		}
	  if (ImageTypeLoaded == 1)  /* only process if a TIF image */
		{
		sprintf(text,"EcoView   %s",image_filename);
		SetWindowText(hWnd,text);
		ProcessOCT();
		}
	  PaintImage();
	  }
	break;
  case WM_MOUSEMOVE:
	if (ShowPixelCoords == 0)
      return(DefWindowProc(hWnd,uMsg,wParam,lParam));
	if (ImageTypeLoaded != 1)
      return(DefWindowProc(hWnd,uMsg,wParam,lParam));
	if (oct_stack == NULL)
	  return(DefWindowProc(hWnd,uMsg,wParam,lParam));
	xPos=LOWORD(lParam);
	yPos=HIWORD(lParam);
	GetImageValue(&xPos,&yPos,ActiveROWS,ActiveCOLS,&red,&green,&blue);
	sprintf(text,"(%d,%d) => [%d,%d,%d]",xPos,yPos,red,green,blue);
	hDC=GetDC(MainWnd);
	GetClientRect(MainWnd,&TextRect);
    TextRect.top=TextRect.bottom-18;
    DrawBrush=CreateSolidBrush(RGB(255,255,255));
    FillRect(hDC,&TextRect,DrawBrush);
    DeleteObject(DrawBrush);
	TextOut(hDC,0,TextRect.top,text,strlen(text));
	ReleaseDC(MainWnd,hDC);
    return(DefWindowProc(hWnd,uMsg,wParam,lParam));
	break;
  case WM_HSCROLL:	// boundary checking done in MakeDisplayImage()
	if (LOWORD(wParam) == SB_LINELEFT) LeftDispCol--;
	if (LOWORD(wParam) == SB_LINERIGHT) LeftDispCol++;
	if (LOWORD(wParam) == SB_PAGELEFT) LeftDispCol-=COLS/6;
	if (LOWORD(wParam) == SB_PAGERIGHT) LeftDispCol+=COLS/6;
	if (LOWORD(wParam) == SB_THUMBPOSITION) LeftDispCol=HIWORD(wParam);
	PaintImage();
	break;
  case WM_VSCROLL:	// boundary checking done in MakeDisplayImage()
	if (LOWORD(wParam) == SB_LINEUP) TopDispRow--;
	if (LOWORD(wParam) == SB_LINEDOWN) TopDispRow++;
	if (LOWORD(wParam) == SB_PAGEUP) TopDispRow-=ROWS/6;
	if (LOWORD(wParam) == SB_PAGEDOWN) TopDispRow+=ROWS/6;
	if (LOWORD(wParam) == SB_THUMBPOSITION) TopDispRow=HIWORD(wParam);
	PaintImage();
	break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  default:
    return(DefWindowProc(hWnd,uMsg,wParam,lParam));
    break;
  }

hMenu=GetMenu(MainWnd);

CheckMenuItem(hMenu,ID_DISPLAY_BSCANS,MF_UNCHECKED);
CheckMenuItem(hMenu,ID_DISPLAY_BINARYBSCANS,MF_UNCHECKED);
CheckMenuItem(hMenu,ID_DISPLAY_XZ_DENSITIES,MF_UNCHECKED);
CheckMenuItem(hMenu,ID_DISPLAY_YZ_DENSITIES,MF_UNCHECKED);
CheckMenuItem(hMenu,ID_DISPLAY_ELEVATIONMAP,MF_UNCHECKED);
CheckMenuItem(hMenu,ID_DISPLAY_BASESURFACE, MF_UNCHECKED);

if (DisplayView == 0)
  CheckMenuItem(hMenu,ID_DISPLAY_BSCANS,MF_CHECKED);
else if (DisplayView == 1)
  CheckMenuItem(hMenu,ID_DISPLAY_BINARYBSCANS,MF_CHECKED);
else if (DisplayView == 2)
  CheckMenuItem(hMenu,ID_DISPLAY_XZ_DENSITIES,MF_CHECKED);
else if (DisplayView == 3)
  CheckMenuItem(hMenu,ID_DISPLAY_YZ_DENSITIES,MF_CHECKED);
else if (DisplayView == 4)
  CheckMenuItem(hMenu,ID_DISPLAY_ELEVATIONMAP,MF_CHECKED);
else if (DisplayView == 5)
  CheckMenuItem(hMenu,ID_DISPLAY_BASESURFACE,MF_CHECKED);

if (DisplayEnhance == 1)
  CheckMenuItem(hMenu,ID_OPTIONS_ENHANCE,MF_CHECKED);
else
  CheckMenuItem(hMenu,ID_OPTIONS_ENHANCE,MF_UNCHECKED);
if (ShowPixelCoords == 1)
  CheckMenuItem(hMenu,ID_OPTIONS_SHOWPIXELCOORDINATES,MF_CHECKED);
else
  CheckMenuItem(hMenu,ID_OPTIONS_SHOWPIXELCOORDINATES,MF_UNCHECKED);
if (DisplayImageDetails == 1)
  CheckMenuItem(hMenu,ID_OPTIONS_TEXT,MF_CHECKED);
else
  CheckMenuItem(hMenu,ID_OPTIONS_TEXT,MF_UNCHECKED);

DrawMenuBar(hWnd);

return(0L);
}



void FreeAllOCT()

{
if (oct_stack != NULL)
  free(oct_stack);
if (oct_depth != NULL)
  free(oct_depth);
if (oct_x_proj != NULL)
  free(oct_x_proj);
if (oct_y_proj != NULL)
  free(oct_y_proj);
if (oct_proc != NULL)
  free(oct_proc);
oct_stack=oct_depth=oct_x_proj=oct_y_proj=oct_proc=NULL;
ROWS=COLS=TotalSlices=0;
}

