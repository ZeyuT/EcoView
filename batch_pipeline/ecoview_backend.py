# Zeyu Tang
# Clemson University
# Eco-coating project
# 01/2025
# This script is the python version of EcoView's image processing algorithm.
# Image pre-processing steps are packed into a class oct_toolbox
# morphological calculation is summarized in function measure_morphology

import sys
import random
import numpy as np
from skimage.morphology import *
from skimage.filters import gaussian
import cv2

from scipy.ndimage import median_filter

import os
import glob
import pickle
import pandas as pd
from tifffile import imsave, imread, imwrite
from skimage.transform import resize

from common.tiff_reader import read_tiff_volume
from common.oct_reader import read_oct_volume

class oct_toolbox:
    def __init__(self, **kwargs):
        ''' Each B scan is ROWS x COLS, where ROWS is the depth of the scan, and COLS is the movement of the sensor */
            TotalSlices = number of B scans */
            coord system origin in the plane of the sensor moving across the top of the biofilm and looking down at it */
            x/y is the plane of sensor motion, and z is the imaging depth */

            x-axis = which B scan (slice); positive in direction of how the set of scans acquired
            y-axis = which column of data in B scan slice; positive in direction of how scan acquired 
            z-axis = which row of data in B scan slice (depth of OCT scan); positive as depth increases (away from sensor) 
        '''
        # Portion of total mass to set as positive at each column during binarization
        self.mass_threshold = kwargs.get('mass_threshold', 0.10)
        # The max allowable biofilm height/thickness from base surface along z axes
        self.min_size = kwargs.get('min_size', 50)
        self.max_height = kwargs.get('max_height', 300)
        
        file_path = kwargs.get('file_path')

        
        # Load raw oct data
        self.oct_stack = []
        if os.path.splitext(file_path)[1] in ['.tif', '.tiff']:
            self.oct_stack = read_tiff_volume(file_path)
        else:
            self.oct_stack = read_oct_volume(file_path)
        if not len(self.oct_stack):
            raise ValueError(f"cannot load files from {file_path}.")        
        if len(self.oct_stack.shape) == 4:
            # Oct images are grayscale by default. Tiff images have 4 channels sometimes.
            # If oct_stack has rgb channel, use only one channel
            self.oct_stack = self.oct_stack[:, :, :, 0]

        self.TotalSlices, self.ROWS, self.COLS = self.oct_stack.shape
        
        TotalSlices = kwargs.get('target_slices', self.TotalSlices) 
        ROWS = kwargs.get('target_rows', self.ROWS) 
        COLS = kwargs.get('target_cols', self.COLS) 
        if TotalSlices!=self.TotalSlices or ROWS!=self.ROWS or COLS!=self.COLS:
            # Resize volume image
            self.oct_stack = resize(self.oct_stack, 
                                    [TotalSlices, ROWS, COLS], 
                                    mode='constant', 
                                    anti_aliasing=True, 
                                    preserve_range=True)
            self.TotalSlices, self.ROWS, self.COLS = self.oct_stack.shape

        # Top height of the valid space
        self.top_height = kwargs.get('top_height', 50)
        # Bottom height of the valid space. Default: bottom of the whole image
        self.bottom_height = kwargs.get('bottom_height', self.ROWS)

        # Normalization
        self.oct_stack = self.oct_stack.astype('float32')
        max_ = self.oct_stack.max()
        min_ = self.oct_stack.min()
        self.oct_stack = (self.oct_stack-min_)/(max_-min_)*255.
        self.oct_stack = self.oct_stack.astype('uint8')
                
        # create copy of original OCT data for processing
        self.oct_proc = np.copy(self.oct_stack)
        # Mute noise at the top
        self.oct_proc[:, 0:self.top_height, :] = 0
        # Mute noise at the bottom
        self.oct_proc[:, self.bottom_height:, :] = 0
        
        # Density projection maps
        self.oct_x_proj = []
        self.oct_y_proj = []
        # Base surface
        self.base_map = []
        # depth map from the top view
        self.depth_map = []
        # Biofilm layer's thickness map, the height is calcd from the base surface
        self.height_map = []        

        # The Z map that separate the biofilm layer from above bright layers (glass, reflection, etc.) 
        self.cutoff_height_map = np.ones_like(self.oct_stack[:,0,:]) * self.top_height

    def binarize(self):
        ''' Threshold each Z column independently
            this allows for local adaptation, and works a little better than global thresholding
        '''
        threshold_map = np.percentile(self.oct_proc, (1. - self.mass_threshold) * 100, axis=1, keepdims=True)
        self.oct_proc = (self.oct_proc >= threshold_map).astype(int) * 255
        return

    def remove_small_objects_3d(self):
        ''' erase small volumes that are <MinCCSize total voxels
            we experimented with 3D volume erasure, but 2D works okay and maybe even a little better
            Returns:
                None. Modify self.oct_proc in place

        '''
        self.oct_proc = np.array(list(map(lambda x: remove_small_objects(x.astype(bool), min_size=self.min_size,connectivity=2), \
                                          self.oct_proc))).astype(int) * 255
        return

    def calc_projection(self, axis=1):
        '''find density projection along one direction and normalize it
            Args:
                axis: direction of summarizing density, 0: x-axis, 1: z-axis, 2: y-axis
            Returns:
                projection: 2d array, value range: [0, 255]
        '''
        projection = np.sum(self.oct_proc>0, axis=axis)
        projection = self._norm(projection)
        return projection

    def calc_cutoff_height_map(self):
        ''' Find the cut-off height that separate the biofilm layer from other bright layers (glass, reflection, etc.) above it 
            The way is to look at projection maps along x and y direction, consider the first thin strip (if any) as noise and remove it.
            Then combine results from two directions to form a 2D height map on XY plane.
            The method is heuristic, and may need fine tuning when applied on a new dataset
            Returns:
                None. Modify self.cutoff_height in place
        '''
        
        if not len(self.oct_x_proj):
            self.oct_x_proj = self.calc_projection(axis=0)
        if not len(self.oct_y_proj):
            self.oct_y_proj = self.calc_projection(axis=2)
        # Isolate bright areas
        binary_x = self.oct_x_proj>np.percentile(self.oct_x_proj, 85, axis=0, keepdims=True) # dim: [Z, Y]
        binary_y = self.oct_y_proj>np.percentile(self.oct_y_proj, 85, axis=1, keepdims=True)# dim: [X, Z]                                  
        # calculate cutoff_height on binarized projection maps 
        cutoff_heights_x = np.array(list(map(self.calc_col_cutoff_height, [binary_x[:,c] for c in range(binary_x.shape[1])]))) # dim: [Y]
        cutoff_heights_y = np.array(list(map(self.calc_col_cutoff_height, binary_y))) # dim: [X]
        cutoff_heights_x = median_filter(cutoff_heights_x, size=31, mode='reflect') 
        cutoff_heights_y = median_filter(cutoff_heights_y, size=31, mode='reflect') 
        
        self.cutoff_height_map = np.maximum(cutoff_heights_x[None, :], cutoff_heights_y[:, None])

        return
 

    def calc_col_cutoff_height(self, col):
        ''' Find the cutoff_height on one column
            The intuition is to find the largest nonzero chunk and take it as biofilm layer,
            and all other smaller non-zero chunks above it are noise.
            Therefore, cutoff height is set to slightly below the nearest nonzero chunk above the largest one.
            Args:
                col: 1D array with length of ROWs,  a column from volume image
            Return:
                cutoff_height: calcd z value that separates the biofilm layer from other bright layers above it
        '''
        # positives = np.where(tmp_col)[0]
        # if len(positives) < 2:
        #     return cutoff_height
        # pre = positives[0]
        # count = 0
        # Median filter to close small gaps

        # Find non-zero values
        mask = col != 0
        # Mute noise at the top
        mask[0:self.top_height] = 0
        # Mute noise at the bottom
        mask[self.bottom_height:] = 0
        
        # Find where the non-zero chunks start and end
        diff = np.diff(mask.astype(int))
        start_indices = np.where(diff == 1)[0] + 1
        end_indices = np.where(diff == -1)[0]
        # Check for non-zero at the beginning of the array
        if mask[0]:
            start_indices = np.insert(start_indices, 0, 0)
        # Check for non-zero at the end of the array
        if mask[-1]:
            end_indices = np.append(end_indices, len(col) - 1)
        if len(start_indices) == 0:
            return self.top_height
        
        # Merge close chunks, distance threshold = 30
        merged_starts = [start_indices[0]]
        merged_ends = [end_indices[0]]
        for i in range(1, len(start_indices)):
            if start_indices[i] - merged_ends[-1] <= 30:
                merged_ends[-1] = end_indices[i]
            else:
                merged_starts.append(start_indices[i])
                merged_ends.append(end_indices[i])
        merged_starts = np.array(merged_starts)
        merged_ends = np.array(merged_ends)
        # Remove tiny chunks and prepare for merging, , threshold = 3
        chunk_sizes = merged_ends - merged_starts
        valid_chunks = chunk_sizes >= 3
        merged_starts = merged_starts[valid_chunks]
        merged_ends = merged_ends[valid_chunks]
        
        if len(merged_starts) == 0:
            return self.top_height  # Default cutoff if no valid chunks
        max_idx = np.argmax(merged_ends - merged_starts)    
        if max_idx != 0:
            # If there is a non-zero chunk above the largest one, take 30+end_idx of the chunk as cutoff
            cutoff_height = 30+merged_ends[max_idx-1]
        else:
            cutoff_height = self.top_height # Default cutoff height
        return cutoff_height

        

    def calc_base_map(self, sigma=5):
        ''' threshold each slice individually to get base surface 
            Args:
                sigma: std value used for gaussian smoothing
            Returns:
                base_map. 2D array with size [TotalSlices, COLS], 
                            where value at each x,y location is the z position of base surface

        '''
        # calc baseline per slice
        base_map = np.array(list(map(self.calc_slice_baseline, np.arange(self.TotalSlices))))
        # Limit minimum heights on baseline 
        base_map = np.maximum(base_map, self.top_height)
        # 2D median filter to smooth
        base_map = median_filter(base_map, size=(13, 13), mode='reflect') 
        # gaussian blurring filter to smooth
        base_map = gaussian(base_map, sigma=sigma, preserve_range=True)
        self.base_map = base_map

        return base_map
    
    def calc_slice_baseline(self, slice_idx):
        ''' threshold raw slice image to get baseline. Assume that baseline is always the brighest area.
            Returns:
                baseline. 1D array with length of COLS, stores the baseline's z value at each column
        '''
        img = np.copy(self.oct_stack[slice_idx])
        # Mute area above cutoff heights
        cutoff_heights = self.cutoff_height_map[slice_idx]
        mask = np.arange(img.shape[0]).reshape(-1, 1) < cutoff_heights 
        # Mute area below bottom heights
        mask |= np.arange(img.shape[0]).reshape(-1, 1) > self.bottom_height
        img = np.where(mask, 0, img)
        
        # Isolate the brighest area via binarization
        # High threshold makes sure that biofilm mass is muted
        threshold_mask = img>np.percentile(img, 99.5, axis=0, keepdims=True)
        threshold_mask = remove_small_objects(threshold_mask, min_size=25, connectivity=2) # min_size was 50
        
        #------------------------------Optional---------------------------------------------#
        # Remove remaining clusters above baselines by keeping only the major horizontal band 
        # This step will reduce the algorithm ability to handle highly tilt baseline surface 
        #-----------------------------------------------------------------------------------#
        # Find the dominant horizontal band (y with most foreground)
        ys, xs = np.where(threshold_mask > 0)
        hist, edges = np.histogram(ys, bins=50, range=(0, threshold_mask.shape[0]))
        main_bin = np.argmax(hist)
        band_center_y = (edges[main_bin] + edges[main_bin+1]) / 2.0
        # Keep only connected components whose centroid y is near the dominant band center
        comp_num, labels, stats, cents = cv2.connectedComponentsWithStats(threshold_mask.astype('uint8'), connectivity=8)
        clean_mask = np.zeros_like(threshold_mask)
        for label_idx in range(1, comp_num):
            cy = cents[label_idx][1]
            if abs(cy - band_center_y) <= 20:
                clean_mask[labels == label_idx] = 255
        
        # The first non-zero position at each column is the baseline
        mask = np.maximum.accumulate(clean_mask, axis=0)
        baseline = np.argmax(mask, axis=0)
        # Fill zeros (no baseline detected) with the previous (or following) nonzero values
        for i in range(1, len(baseline)):
            if baseline[i] == 0 and baseline[i-1] != 0:
                baseline[i] = baseline[i-1]
        for i in range(len(baseline)-2, -1, -1):
            if baseline[i] == 0 and baseline[i+1] != 0:
                baseline[i] = baseline[i+1]
        
        if baseline.std() > 30:
            # High std means there are lots of outliers in form of isolated plateaus and valleys.
            # This is always caused by improper focus during image acquisition or low intensity values.
            # Apply median_filter with a large kernel to remove outliers 
            baseline = median_filter(baseline, size=121, mode='reflect')             
        # Slightly elevate the baseline, to offset the effect of harsh binarization 
        # Also eliminate super tiny clumps near the baseline
        baseline -= 5

        return baseline

    def calc_depth_map(self):
        ''' find depth image:  min depth to see solid at each x,y location 
            Returns:
                depth_map: 2d array with size [TotalSlices, COLS]. 
        '''
        if not len(self.base_map):
            self.calc_base_map()
        top_surface = np.maximum(self.cutoff_height_map, self.base_map - self.max_height)
        mask = (np.arange(self.ROWS)[None, :, None] >= top_surface[:, None, :]) & (self.oct_proc != 0)

        oct_depth_cal = np.argmax(mask, axis=1)
        oct_depth_cal[oct_depth_cal == 0] = self.base_map[oct_depth_cal == 0]  # Adjust for cases where np.argmax returns 0 if no True found

        oct_depth_cal = median_filter(oct_depth_cal, size=(5, 5), mode='reflect')   
        oct_depth_cal = np.minimum(oct_depth_cal, self.base_map)
        self.depth_map = oct_depth_cal
        return self.depth_map

    def calc_height_map(self):
        ''' find height map:  biofilm layer's height/thickness at each x,y location 
            Returns:
                height_map: 2d array with size [TotalSlices, COLS]. 
        '''
        if not len(self.base_map):
            self.calc_base_map()
        if not len(self.depth_map):
            self.calc_depth_map()
        self.height_map = self.base_map -  self.depth_map
        self.height_map = median_filter(self.height_map, size=(5, 5), mode='reflect')   
        return self.height_map

    
    def visualize_depth_map(self):
        ''' Modify depth map for better visualization, include:
            min-max normalization;
            invert values:  darker = shorter/farther from sensor, brighter = taller/closer to sensor 
        '''
        if not len(self.depth_map):
            _ = self.calc_depth_map()
        visual = cv2.equalizeHist((255-self._norm(self.depth_map)*255.).astype('uint8'))
        return visual

    def visualize_base_map(self):
        ''' Modify base map for better visualization, include:
            min-max normalization
        '''
        if not len(self.base_map):
            _ = self.calc_base_map()
        visual = self._norm(self.base_map) * 255.
        return visual
    
    def equalize_height_map(self):
        ''' Modify height_map for better visualization, include:
            Histogram equalization: 0 = base surface, 255 = top of the heightest biofilm
        '''
        if not len(self.height_map):
            self.calc_height_map()
        equalized = cv2.equalizeHist((self._norm(self.height_map)*255.).astype('uint8'))
        return equalized
    
    def density_filter(self, density_threshold):
        ''' filter using x-y projections
            Args:
                density_threshold: minimum density on each vector along the axis
                                    range: [0,1]
                                    vectors with density less than minimum are set to 0
            Returns:
                None. Modify self.oct_proc in place
        '''
        if not len(self.oct_x_proj):
            self.oct_x_proj = self.calc_projection(axis=0)
        if not len(self.oct_y_proj):
            self.oct_y_proj = self.calc_projection(axis=2).T
        density_threshold *= 255.
        self.oct_proc[self.oct_x_proj<density_threshold & self.oct_y_proj<density_threshold] = 0
    
    def _norm(self, arr):
        """
        Rescales data to the range [0, 1].
        Args:
        data: A NumPy array.
        Returns:
        A normalized NumPy array.
        """
        min_ = np.min(arr)
        max_ = np.max(arr)
        arr_norm = np.copy(arr)
        if max_ - min_ != 0:
            arr_norm = (arr_norm - min_) / (max_ - min_)
        return arr_norm
    
    def reset(self):
        # Erase all processed results
        self.oct_proc = np.copy(self.oct_stack)
        # Mute noise at the top
        self.oct_proc[:, 0:self.top_height, :] = 0
        # Mute noise at the bottom
        self.oct_proc[:, self.bottom_height:, :] = 0
        self.oct_x_proj = []
        self.oct_y_proj = []

        self.base_map = []
        self.depth_map = []
        self.cutoff_height_map = np.ones_like(self.oct_stack[:,0,:]) * self.top_height

        return
    
def measure_morphology(height_map, min_thickness=3):
    ''' calc height/thickness statistics, , coverage_ratio Rq, Ra
        Args:
            height_map: height/thickness map of biofilm layers
            min_thickness: min_thickness to consider a biofilm column as positive 
        Returns:
            results: dictionary of resulted numbers
    '''    
    TotalSlices, COLS = height_map.shape
    min_h = np.min(height_map)
    max_h = np.max(height_map)
    avg_h = np.mean(height_map)
    coverage_ratio = float(np.sum(height_map>min_thickness)/(TotalSlices*COLS))
    
    Rq=np.std(height_map) #Root-mean-square of heights
    if avg_h == 0:
        Ra = 0
    else:
        Ra=np.mean(np.abs(avg_h-height_map))/avg_h #Arithmetic average of heights, normalized.

    results = {
                'average_thickness': avg_h,
                'coverage_ratio': coverage_ratio,
                'min_thickness': min_h,
                'max_thickness': max_h,
                'Rq': Rq,
                'Ra': Ra}
    return results
