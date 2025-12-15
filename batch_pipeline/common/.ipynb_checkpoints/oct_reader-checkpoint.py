# Zeyu Tang
# Clemson University
# Eco-coating project
# 12/2023
# Re-organized from jupyter notebook provided by the group of Ussex Unversity 

# 01/2025
# Zeyu: optimized tempfile usage for parallel reading.
#       Added a function to convert intensity data to 'uint8'


# oct_reader.py
# Purpose: a function library for loading in 3D volume images with .oct format.
# Example usage: raw = read_oct_volume(file_dir)

from scipy.io import loadmat
import glob
import numpy as np
import matplotlib.pyplot as plt
import cv2
import tempfile
import os
import zipfile
import xmltodict
import warnings
from warnings import warn
import gc

# Create a python_types dictionary for required data types
# I.e. the Thorlabs concept can mean a "Raw - signed - 2 bytes" --> np.int16
python_dtypes = {'Colored': {'4': np.int32, '2': np.int16},
                 'Real': {'4': np.float32},
                 'Raw': {'signed': {'1': np.int8, '2': np.int16},
                         'unsigned': {'1': np.uint8, '2': np.uint16}}}


def read_oct_volume(filename):
    # Example usage
    tempdir = tempfile.TemporaryDirectory(suffix=f"_{os.path.basename(filename).split('.oct')[0]}")
    handle = unzip_OCTFile(filename, tempdir.name)
    handle.update({'python_dtypes': python_dtypes})
    data_intensity = get_OCTIntensityImage(handle)
    del handle
    tempdir.cleanup()
    gc.collect()
    return data_intensity

def unzip_OCTFile(filename, output_dir):
    """
    Unzip the OCT file into a temp folder.
    """
    handle = dict()
    handle['filename'] = filename
    handle['path'] = output_dir

    temp_oct_data_folder = os.path.join(handle['path'], 'data')
    handle['temp_oct_data_folder'] = temp_oct_data_folder
    if os.path.exists(temp_oct_data_folder) and os.path.exists(os.path.join(temp_oct_data_folder, 'Header.xml')):
        warn('Reuse data in {}\n'.format(temp_oct_data_folder))
    else:
        # print('\nTry to extract {} into {}. Please wait.\n'.format(filename,temp_oct_data_folder))
        if not os.path.exists(handle['path']):
            os.mkdir(handle['path'])
        if not os.path.exists(temp_oct_data_folder):
            os.mkdir(temp_oct_data_folder)

        with zipfile.ZipFile(file=handle['filename']) as zf:
            zf.extractall(path=temp_oct_data_folder)

    # read Header.xml        
    with open(os.path.join(temp_oct_data_folder, 'Header.xml'),'rb') as fid:
        up_to_EOF = -1
        xmldoc = fid.read(up_to_EOF)
        
    # convert Header.xml to dict
    handle_xml = xmltodict.parse(xmldoc)
    handle.update(handle_xml)
    

    gc.collect()
    return handle

def get_OCTFileMetaData(handle, data_name):
    """
    The metadata for files are store in a list.
    The artifact 'data\\' stems from windows path separators and may need fixing.
    On mac and linux the file names will have 'data\\' as a name prefix.
    """
    # Check if data_name is available
    data_names_available = [d['#text'] for d in handle['Ocity']['DataFiles']['DataFile']]
    data_name = 'data\\'+data_name+'.data' # check this on windows
    assert data_name in data_names_available, 'Did not find {}.\nAvailable names are: {}'.format(data_name,data_names_available)

    metadatas = handle['Ocity']['DataFiles']['DataFile'] # get list of all data files
    # select the data file matching data_name
    metadata = metadatas[np.argwhere([data_name in h['#text'] for h in handle['Ocity']['DataFiles']['DataFile']]).squeeze()]
    return handle, metadata

def get_OCTIntensityImage(handle):
    """
    Example how to extract Intensity data
    """
    handle, metadata = get_OCTFileMetaData(handle, data_name='Intensity')
    data_filename = os.path.join(handle['temp_oct_data_folder'], metadata['#text'])
    img_type = metadata['@Type']
    dtype = handle['python_dtypes'][img_type][metadata['@BytesPerPixel']] # This is not consistent! unsigned and signed not distinguished!
    sizeX = int(metadata['@SizeX'])
    sizeZ = int(metadata['@SizeZ'])
    data = (np.fromfile(data_filename, dtype=(dtype, [sizeX, sizeZ])))
    data = np.moveaxis(data, -1, 1)
    return data

def scale_and_convert_to_uint8(data, lower_percentile=1, upper_percentile=99):
    """
    Scales the input data based on percentiles and converts it to uint8.

    :param data: Input image data in any scale.
    :param lower_percentile: Lower percentile to clip the data.
    :param upper_percentile: Upper percentile to clip the data.
    :return: Scaled data as uint8.
    """
    # Clipping data to remove outliers
    vmin = np.percentile(data, lower_percentile)
    vmax = np.percentile(data, upper_percentile)
    data_clipped = np.clip(data, vmin, vmax)

    # Normalize the data to 0-1
    data_normalized = (data_clipped - vmin) / (vmax - vmin)

    # Scale to 0-255 and convert to uint8
    data_uint8 = (data_normalized * 255).astype(np.uint8)
    return data_uint8
