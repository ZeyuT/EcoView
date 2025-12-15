import sys
import time
import numpy as np
import random
import glob
from skimage import morphology
from skimage.morphology import closing, square, reconstruction
import subprocess 

import os
import glob
import re
import pickle
import pandas
from PIL import Image
import pandas as pd
import cv2
from numba import jit
from tifffile import TiffFile, imsave, imread, imwrite

import multiprocessing as mp
import gc 

from common.oct_reader import read_oct_volume
from common.tiff_reader import read_tiff_parallel, read_tiff_volume


def process_img_parallel(args):
    save_path, file_path, height_first, downsample_rate = args
    print(os.path.basename(file_path))
    if file_path[-3:] == 'oct':
        # OCT format
        raw = read_oct_volume(file_path)
    else:
        # Tiff format. 
        # Skip 2D and empty tiff files
        if len(TiffFile(file_path).pages) <= 1:
            print(f'WARNING: {file_path} is a 2D image. It is not re-saved')
            return
        raw = read_tiff_volume(file_path)
    if len(raw.shape) == 4:
        # Oct images are grayscale by default. Tiff images have 4 channels sometimes.
        # If oct_stack has rgb channel, use only one channel
        raw = raw[:, :, :, 0]
    if len(raw) < 10:
        print(f'WARNING: {file_path} has less than 10 slices. It is not re-saved')
        return
    raw = (raw-raw.min())/(raw.max()-raw.min())*255.
    raw = raw.astype('uint8')
    if not height_first:
        raw = np.moveaxis(raw, 2, 1) # [X,Y,Z] -> [X,Z,Y]
        
    # downsized = crop_downsize(raw, downsample_rate=downsample_rate)

    # For a special condition where cols are much larger than slices
    # This happens on some tiff files.
    slices = raw.shape[0]
    cols = raw.shape[2]
    downsized = crop_downsize(raw, downsample_rate=[1, 1, cols//slices])
    
    imwrite(save_path, downsized.astype('uint8'))
    gc.collect()
    return save_path


def crop_downsize(vol, downsample_rate=[1,1,1]):
    return vol[::downsample_rate[0],::downsample_rate[1],::downsample_rate[2]]

def main():   
    n_threads = mp.cpu_count() - 2
    suffix = '.tif'
    # suffix = '.oct'
    # volume image axis = [X,Z,Y] if height_first=True, [X,Y,Z] otherwise
    height_first = True
    
    downsample_rate = [1,1,1] # [X,Z,Y]
    
    root_path = '/project/ahoover/mhealth/zeyut/biofilm/2025_10/raw/'
    root_save_path = '/project/ahoover/mhealth/zeyut/biofilm/2025_10/data/'
    
    os.makedirs(root_save_path, exist_ok=True)
    args = []
    folder_list = ['9_26 3D Images']
    for folder in folder_list:
        file_list = glob.glob(os.path.join(root_path, folder)+f'/*/*/*{suffix}*')+\
                         glob.glob(os.path.join(root_path, folder)+f'/*/*{suffix}*')+\
                         glob.glob(os.path.join(root_path, folder)+f'/*{suffix}*')
        if len(file_list) == 0:
               continue
        file_list.sort()
        
        for file_path in file_list:
            relative_path = file_path.split(root_path)[1]
            fname = os.path.basename(file_path).split(suffix)[0]
            save_name = relative_path.split(suffix)[0].replace('/', '__') + '.tif'
            save_path = os.path.join(root_save_path, save_name)
            if not os.path.exists(save_path):
                args.append([save_path, file_path, height_first, downsample_rate])
            
    print(f'Total {len(args)} images')
    pool = mp.Pool(n_threads)        
    results = pool.map(process_img_parallel, args)
    pool.close()
    pool.join()
    f = open(os.path.join(root_save_path,"IMAGE_LIST.txt"), "w")
    for out_path in results:
        if out_path and os.path.exists(out_path):
            f.write(os.path.basename(out_path)+'\n')
    f.close()

if __name__ == "__main__":  
    main()