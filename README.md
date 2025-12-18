# EcoView

EcoView is an OCT (Optical Coherence Tomography) 3D image processing tool developed for the DARPA *eco-coating* project led by Clemson University. 
The goal of EcoView is to provide visualization and morphological analysis of underwater biofilm growth.

It provides:

- A Windows desktop interface (C / Visual Studio) for interactive inspection and processing of OCT images
- A Python batch pipeline for scalable processing on HPC systems (e.g., Slurm)

The system produces depth maps, height (thickness) maps, equalized visualizations, base surface estimation, morphology statistics, and optional 3D surface exports (`.ply`) and time-lapse height map animation.

---

## What EcoView Processes

EcoView operates on 3D OCT volumes stored as multi-page .tiff/.tif/.oct files.

- Volume shape:  
  `TotalSlices × Rows × Columns`
- The core output is a height map representing biofilm thickness relative to a baseline surface.
- **Data size considerations:**  
  EcoView is optimized for input volumes **smaller than ~300 MB**. In our project, raw OCT data can range from 1.4 GB to 4 GB depending on channel number and spatial resolution.  
  For large raw volumes, it is strongly recommended to **downsize the data before processing**, which can be done via the provided utility (`batch_pipeline/batch_downsize.py`). Processing very large volumes directly may result in instability in the GUI and significantly increased runtime in the Python batch pipeline.
---

## Repository Structure

```
EcoView/
├── Interface/
│   ├── EcoView_src/              # Windows (Visual Studio) C implementation + GUI
│   ├── library-jpeg/             # bundled JPEG library
│   └── library-tiff/             # bundled TIFF library
└── batch_pipeline/
    ├── ecoview_backend.py        # Core EcoView algorithm (Python)
    ├── batch_run.py              # Batch processing entry point
    ├── batch_downsize.py         # Volume downsampling / cropping
    ├── configs.yaml              # Processing parameters
    ├── common/                   # OCT/TIFF readers and helpers
    ├── bash/                     # Slurm scripts (sbatch, salloc)
    └── jupyter/                  # Analysis and visualization notebooks
```

---

## Core Algorithm (High Level)

Both the C (GUI) and Python (batch) implementations follow the same processing logic:
1. Load OCT volume
2. Crop valid height range
3. Per-column adaptive thresholding
4. 3D cleanup
5. Base surface estimation
6. Depth map computation
7. Height map generation
8. Morphological measurements
9. (python) Smooth top-view animation
---

## Python Batch Pipeline

### Dependencies
  
    numpy>=1.23.5 \
    scipy>=1.10.1 \
    pandas>=1.5.3 \
    scikit-image>=0.19.3 \
    opencv-python>=4.7.0.72 \
    tifffile>=2023.2.28 \
    Pillow>=9.4.0 \
    matplotlib>=3.7.1 \
    PyYAML>=6.0 \
    xmltodict>=0.13.0 \
    numba>=0.56.4 \
    h5py>=3.8.0 \
    pystackreg>=0.2.7\
    ipywidgets>=8.0.4 \
    matplotlib-scalebar>=0.8.1 \
    tqdm>=4.67.1 \
    jupyter>=1.0.0 \
    ipykernel>=6.21.2
