from tifffile import TiffFile, imsave, imread, imwrite
import glob
import numpy as np
import multiprocessing as mp


def readImageSequence(filelist,dtype):
    filelist=glob.glob(path+'/*.tif')
    filelist.sort()
    stack = []
    for file in filelist:
        stack.append(imread(file).astype(dtype))
    stack = np.transpose(stack, (0,2,1))
    return np.array(stack)

def read_tiff_volume(file_path):    
    try:
        vol = imread(file_path)
    except:
        print(f'Unable to load {file_path}')
        vol = None
    return vol


def read_tif_stack(args):
    i, file_path = args
    page = TiffFile(file_path).pages[i].asarray()
    # In rare cases, the last one or two pages are empty
    if page is not None:
        return i, page[:,:,:]
    else:
        return i, None
    
def read_tiff_parallel(file_path):    
    # Read pages in tiff file using multiprocessing
    # file_path = file_list[file_idx]
    pages = TiffFile(file_path).pages
    n_threads = min(16, mp.cpu_count()-2)
    print(f'# threads: {n_threads}')

    pool = mp.Pool(n_threads)
    results = pool.map(read_tif_stack, [[i, file_path] for i in range(len(pages))])
    pool.close()
    pool.join()
    results.sort(key=lambda x:x[0])
    vol = []
    for _, frame in results:
        if frame is not None:
            vol.append(frame)
    return np.array(vol)