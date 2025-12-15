#!/bin/bash

#SBATCH --job-name bio-downsize
#SBATCH --nodes 1
#SBATCH --tasks-per-node 1
#SBATCH --cpus-per-task 24
#SBATCH --mem 150gb
#SBATCH --time 24:00:00
#SBATCH --constraint interconnect_hdr
#SBATCH --output=/home/zeyut/biofilm/EcoView/batch_pipeline/job_output/%j.out

module purge

module load anaconda3/2023.09-0 gcc/12.3.0 
source activate biofilm

cd /home/zeyut/biofilm/EcoView/batch_pipeline

python ./batch_downsize.py 
