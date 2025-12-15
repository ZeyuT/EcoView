#!/bin/bash

#SBATCH --job-name batch_bio
#SBATCH --nodes 1
#SBATCH --tasks-per-node 1
#SBATCH --cpus-per-task 24
#SBATCH --mem 500gb
#SBATCH --time 2:00:00
#SBATCH --output=/home/zeyut/biofilm/EcoView/batch_pipeline/job_output/%j.out

module purge

module load anaconda3/2023.09-0
source activate biofilm

cd /home/zeyut/biofilm/EcoView/batch_pipeline

python ./batch_run.py
