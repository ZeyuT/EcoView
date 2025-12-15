# qsub -I -l select=1:ncpus=20:mem=120gb:ngpus=1:interconnect=any,walltime=5:00:00
salloc --nodes 1 --tasks-per-node 20 --cpus-per-task 1 --mem 150gb --time 4:00:00
