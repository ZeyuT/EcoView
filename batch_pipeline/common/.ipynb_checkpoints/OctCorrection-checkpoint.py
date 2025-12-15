import numpy as np
from skimage import filters

import scipy.ndimage
from numba import jit
import time
import re
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import time

def loadImage(fileName,shape=None,dtype='u1'):
    
    if shape is None:
        m=re.match('.*\s(\d+)x(\d+)x(\d+).raw',fileName)
        if m is None:
            raise Exception('loadImage','cannot get image shape from filename')
        shape=[int(m.group(1)),int(m.group(2)),int(m.group(3))]
    
    img=np.fromfile(fileName, dtype=dtype)  
    if shape[0]*shape[1]*shape[2] != len(img):
        raise Exception('loadImage','file shape and file size not equal')
    
    return img.reshape(shape)
    
def filtRangePlane(img,x,y,fltSize):
    gridx,gridy = np.ogrid[-fltSize: fltSize+1, -fltSize: fltSize+1]
    flt = (gridx**2+gridy**2 <= fltSize**2).astype(float)
    fltOnes=flt.sum()
    output=np.zeros((len(x),len(y),img.shape[2]))
    
    if(max(x)+fltSize>=img.shape[0] or min(x)<fltSize):
        raise Exception("x vector out of img range")
    if(max(y)+fltSize>=img.shape[1] or min(y)<fltSize):
        raise Exception("y vector out of img range")
    
    @jit(nopython=True)
    def _JITfiltRangePlane(img,x,y,fltSize,flt,output,fltOnes):    
        for xindPos in range(len(x)):
            for yindPos in range(len(y)):
                pos=(x[xindPos],y[yindPos])
                for z in range(img.shape[2]):
                    outVal=0.0
                    for xfilt in range(fltSize*2+1):
                        for yfilt in range(fltSize*2+1):
                            outVal+=img[pos[0]-fltSize+xfilt,pos[1]-fltSize+yfilt,z]*flt[xfilt,yfilt]
                    output[xindPos,yindPos,z]=outVal/fltOnes

    _JITfiltRangePlane(img,x,y,fltSize,flt,output,fltOnes)
    return output

def filterGaussian(img,filtSize=55):#filtSize=55):      #vertical gaussian filter, you can adjust the filter size
    inpu=np.zeros(filtSize)
    inpu[filtSize//2]=7
    fltGauss=scipy.ndimage.filters.gaussian_filter1d(inpu,2.0)#(inpu,5.0)
    fltGauss=fltGauss-np.mean(fltGauss)
    
    output=np.zeros_like(img,dtype=float)
    for xindPos in range(img.shape[0]):
        for yindPos in range(img.shape[1]):
            line=img[xindPos,yindPos,:]
            output[xindPos,yindPos,:]=scipy.ndimage.filters.convolve1d(line,fltGauss)
            
    return output




    
def findMaximum(img,numMaximum=1,zeroPrevMaxSize=10):
    output=np.zeros((img.shape[0],img.shape[1],numMaximum),dtype=int)
    
    for xindPos in range(img.shape[0]):
        for yindPos in range(img.shape[1]):
            line=img[xindPos,yindPos,:].copy()     #set range for maxima
            for maximumInd in range(numMaximum):
                aMax=len(line) - np.argmax(line[::-1]) -1
                
                line[max(aMax-zeroPrevMaxSize,0):min(aMax+zeroPrevMaxSize,img.shape[2])]=0
                output[xindPos,yindPos,maximumInd]=aMax
    return output

def filterPoints(pts,verbose=False):          #filter points based on neighbors sets A and B
    residualsA=np.zeros_like(pts)+np.inf
    residualsB=np.zeros_like(pts)+np.inf

    A=np.array([[0,0,1],
    [1,0,1],
    [2,0,1],
    [0,1,1],
    [1,1,1],
    [2,1,1],
    [0,2,1],
    [1,2,1],
    [2,2,1]])

    selA=[1,3,4,5,7]
    selB=[0,2,4,6,8]

    for xindPos in range(1,pts.shape[0]-1):
        for yindPos in range(1,pts.shape[1]-1):
            arr=pts[xindPos-1:xindPos+2,yindPos-1:yindPos+2]
            residualsA[xindPos,yindPos]=np.linalg.lstsq(A[selA], arr.flatten()[selA])[1][0]
            residualsB[xindPos,yindPos]=np.linalg.lstsq(A[selB], arr.flatten()[selB])[1][0]
    #resA=(residualsA<10).astype(int)     #you can increase the maximum residuals (10 here) to loosen the selection
    #resB=(residualsB<10).astype(int)
    resA=(residualsA<40).astype(int)     #you can increase the maximum residuals (10 here) to loosen the selection
    resB=(residualsB<40).astype(int)

    selection=(resA+resB)>0
    if verbose:
        print(resA+resB)
        print("Ok in Res A: %d"%sum(sum(resA)))
        print("Ok in Res B: %d"%sum(sum(resB)))
        print("Ok in A or B: %d"%sum(sum((resA+resB)>0)))
        
    return selection


def linSolve(x,y,pts,selection):      #fit surface
    yy, xx = np.meshgrid(y, x,sparse=False)
    xVals=xx[selection]
    yVals=yy[selection]
    zVals=pts[selection]
    A = np.vstack((xVals,xVals**2,yVals,yVals**2, np.ones(len(xVals)))).T
    model,res=np.linalg.lstsq(A,zVals)[0:2]
    return model,res
    
def calculateZofModel(model,x,y):    #calculate height of surface in each point of the image (x,y plane)
    Z=np.zeros((len(x),len(y)))
    for xindPos in range(len(x)):
        for yindPos in range(len(y)): 
            Z[yindPos,xindPos]=np.dot(model,np.array([x[xindPos],x[xindPos]**2,y[yindPos],y[yindPos]**2,1]))
    return Z


@jit
def makeZmap(shape,model):
    Z=np.zeros(shape)
    a,b,c,d,e=model
    for xindPos in range(shape[0]):
        xCst=a*xindPos+b*xindPos**2+e;
        for yindPos in range(shape[1]): 
            Z[xindPos,yindPos]=xCst+c*yindPos+d*yindPos**2;
        
    return Z

@jit
def correctImage(img,Zmap,zHeight=400,modelHeight=350):        #shift vertically to correct
    retImage=np.zeros((img.shape[0],img.shape[1],zHeight),dtype=img.dtype)
    imgSize=img.shape[2]
    for xindPos in range(img.shape[0]):
        for yindPos in range(img.shape[1]): 
            zVal=int(modelHeight-Zmap[xindPos,yindPos])
            if zVal<0:
                sBeg=-zVal
                dBeg=0
            else:
                sBeg=0
                dBeg=zVal

            sEnd=zHeight-zVal-1
            dEnd=zHeight-1
            if(sEnd>imgSize):
                dEnd-=sEnd-imgSize
                sEnd=imgSize
            retImage[xindPos,yindPos,dBeg:dEnd]=img[xindPos,yindPos,sBeg:sEnd]
            
            if(dEnd-dBeg==sEnd-sBeg)and            (sBeg>=0) and             (sBeg<=imgSize) and             (dBeg>=0) and             (dBeg<=zHeight)and            (sEnd>=0)and            (sEnd<=imgSize)and            (dEnd>=0)and            (dEnd<imgSize):
                pass
            else:
                print(dEnd,dBeg,sEnd,sBeg,imgSize)
                return None  
    return retImage
            

 #quality check figures
def plotSurf(size,pts,selection, Z) :   #plot points and fitted surface used for correction
    xx,yy=np.meshgrid(np.linspace(20,size[0]-20,20,dtype=int),np.linspace(20,size[1]-20,20,dtype=int))
    fig = plt.figure(figsize=(4,4), dpi=200)
    ax = fig.gca(projection='3d')
    ax.plot_surface(yy,xx, Z, rstride=1, cstride=1, alpha=1)
    ax.scatter(xx,yy, pts.flatten(), c=selection.flatten(), s=10,alpha=0.6)
    plt.xlabel('X')
    plt.ylabel('Y')
    ax.tick_params(labelsize=8) 
    ax.set_zlabel('Z')
    ax.axis('auto')
    ax.axis('tight')
    return plt.show()



def tic():
    #Homemade version of matlab tic and toc functions
    import time
    global startTime_for_tictoc
    startTime_for_tictoc = time.time()

def toc():
    import time
    if 'startTime_for_tictoc' in globals():
        t=time.time();
        print("Elapsed time is " + str(time.time() - startTime_for_tictoc) + " seconds.")  
    else:
        print("Toc: start time not set")

#comprehensive function for correcting an image
def Correct(ori, bckHeight, crop, size, retFiltSize=12, GaussFiltSize=55,zHeight=900,modelHeight=10):
    b=np.mean(ori[:,:,:bckHeight]);     #calculate background
    dat=ori[:,:,crop:]
    x=np.linspace(20,dat.shape[0]-20,20,dtype=int)    #cooerdinates for grid of correction points (20x20)
    y=np.linspace(20,dat.shape[1]-20,20,dtype=int)
    retFilt=filtRangePlane(dat,x,y,retFiltSize) # zeyut: basically a normalized blurring kernel
    gausFiltered=filterGaussian(retFilt,filtSize=GaussFiltSize) # zeyut
    maximums=findMaximum(gausFiltered,numMaximum=1) # TODO add number option and do not sort results
    maximumsSort=maximums.copy()
    maximumsSort.sort()
    pts=maximumsSort[:,:,0]
    selection=filterPoints(pts,verbose=False)
    model,res=linSolve(x,y,pts,selection)      #find model and append it to the coeff dictionary
    Z=calculateZofModel(model,x,y)
    Zmap=makeZmap((dat.shape[0],dat.shape[1]),model)
    #imgC1=correctImage(dat,Zmap,zHeight=zHeight,modelHeight=modelHeight)   #modelHeight= height of plexi in corrected image
    imgC1 = dat #zeyut: skip the correctImage, need to figure out what ZHeight is.
                #zeyut: plus, no distortion on z direction observed from current data
    imgC=np.zeros(imgC1.shape, dtype=np.uint8);     #subtract background
    imgC[imgC1>int(b)]=imgC1[imgC1>int(b)]-int(b)
    
    return imgC.swapaxes(1,2),pts, selection, Z
    
   