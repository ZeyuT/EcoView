
#define SQR(x) ((x)*(x))	/* macro for square */
#ifndef M_PI			/* in case M_PI not found in math.h */
#define M_PI 3.1415927
#endif
#ifndef M_E
#define M_E 2.718282
#endif

int		SoftwareVersion;	/* 10's digit is main version #, 1's digit is for minor updates */

#define MAX_FILENAME_CHARS	320

char	CurrentPath[MAX_FILENAME_CHARS];
char	image_filename[MAX_FILENAME_CHARS];

char	FilenameList[500][MAX_FILENAME_CHARS];
int		TotalFilenames,FilenameIndex;
int		ImageTypeLoaded;

char	MeshLabFilename[MAX_FILENAME_CHARS];
char	PlyFilename[MAX_FILENAME_CHARS];

HWND	MainWnd;
HINSTANCE	hInst;						// current instance

		// Display flags
int		ScaleImageToWindow,ShowPixelCoords,DisplayEnhance;
int		TopDispRow,LeftDispCol,MagFact;
int		DisplayImageDetails,DisplayView;
int		DisplayScaleBar, ScaleBarRow, ScaleBarCol, ScaleBarXlen, ScaleBarYlen;
		/* We store different versions of the OCT data:
		** (a) oct_stack stores raw B scan images (ROWS x COLS each) in a 1D pointer as an image stack (TotalSlices gives depth)
		** (b) oct_proc stores all the data during processing for thresholding, cleaning, and taking measurements
		** (c) oct_depth stores a single image of top-imaged depth along each OCT ray (COLS x TotalSlices)
		** (d) oct_x_proj stores a density view of the data in the x direction (ROWS x COLS)
		** (e) oct_y_proj stores a density view of the data in the y direction (ROWS x TotalSlices)
		*/
unsigned char *oct_stack,*oct_proc,*oct_depth,*oct_x_proj,*oct_y_proj, *baseline;
int				*oct_depth_cal, *baseline_cal;
int			  ROWS,COLS,TotalSlices,CurrentSlice;
double		  roughnessA,roughnessQ,coverage_ratio,min_thickness,max_thickness;
		/* processing parameters */
int		MassThreshold;
int		cc_filter_on,proj_filter_on,median_filter_on;
int		MinCCSize,ProjThreshold,MedianWindowSize;

		// Function prototypes
LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
int ReadImage(char *,unsigned char **,int *,int *);
void MakeDisplayImage(unsigned char *,int,int,int);
void PaintImage();
LRESULT CALLBACK MagFactProc(HWND,UINT,WPARAM,LPARAM);
LRESULT CALLBACK ParamsProc(HWND,UINT,WPARAM,LPARAM);
void GetImageValue(int *,int *,int,int,int *,int *,int *);
void ProcessOCT();
void FreeAllOCT();
void OutputPly();



