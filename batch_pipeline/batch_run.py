import sys
import random
import numpy as np
from skimage import morphology
from time import time
from scipy.ndimage import median_filter

import os
import glob
import re
import pickle
from PIL import Image
import pandas as pd
import cv2
from tifffile import imsave, imread, imwrite
from numba import jit
import multiprocessing as mp
import gc 
import yaml

from ecoview_backend import oct_toolbox, measure_morphology

def process_single_image(args):
    file_path, save_path, configs = args
    file_name = os.path.basename(file_path).split('.oct')[0].split('.tif')[0]
    print(file_name)

    cur_configs = configs.copy()
    cur_configs['file_path'] = file_path
    oct_ = oct_toolbox( **cur_configs)
    oct_.binarize()
    oct_.remove_small_objects_3d()
    oct_.calc_cutoff_height_map()
    oct_.calc_base_map()
    base_map = oct_.visualize_base_map()
    height_map = oct_.calc_height_map()
    equalized = oct_.equalize_height_map()
    
    os.makedirs(os.path.join(save_path, 'samples_256th'), exist_ok=True)
    os.makedirs(os.path.join(save_path, 'samples'), exist_ok=True)

    os.makedirs(os.path.join(save_path, 'height_maps'), exist_ok=True)
    os.makedirs(os.path.join(save_path, 'equalized'), exist_ok=True)
    os.makedirs(os.path.join(save_path, 'base_surfaces'), exist_ok=True)
    cv2.imwrite(os.path.join(save_path, f'samples_256th/{file_name}.jpg'), oct_.oct_stack[256].astype('uint8'))
    cv2.imwrite(os.path.join(save_path, f'height_maps/{file_name}.jpg'), height_map.astype('int32'))
    cv2.imwrite(os.path.join(save_path, f'equalized/{file_name}.jpg'), equalized.astype('uint8'))
    cv2.imwrite(os.path.join(save_path, f'base_surfaces/{file_name}.jpg'), base_map.astype('uint8'))
    results = {}
    results['filename'] = file_name
    results.update(measure_morphology(height_map,min_thickness=3))
    
    gc.collect()
    return results
                
def main():   
    with open('configs.yaml', 'r') as file:
        configs = yaml.safe_load(file)
    
    n_threads = mp.cpu_count()-4
    
    root_path = '/project/ahoover/mhealth/zeyut/biofilm/2025_10/data/'
    save_path = '/project/ahoover/mhealth/zeyut/biofilm/2025_10/results/'
    file_list = glob.glob(os.path.join(root_path, '*.tif*'))
    
    # root_path = '/project/ahoover/mhealth/zeyut/biofilm/2024-12/Multi-days/'
    # save_path = './results/Multi-days/'
    # file_list = glob.glob(os.path.join(root_path, '*.tif*'))
    
    file_list.sort()

    os.makedirs(save_path, exist_ok=True)

    pool = mp.Pool(n_threads)
    results = pool.map(process_single_image, [[file_path, save_path, configs] for file_path in file_list])
    pool.close()
    pool.join()
    
    df = pd.DataFrame(results)
    df.to_csv(os.path.join(save_path, 'results.csv'), index=False)

    

if __name__ == "__main__":  
    main()