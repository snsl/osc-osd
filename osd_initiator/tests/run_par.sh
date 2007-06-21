#!/bin/bash

#PBS -l walltime=180:00:00
#PBS -l nodes=ib26:ppn=2+ib25:ppn=2+ib21:ppn=2+ib20:ppn=2+ib19:ppn=2+ib18:ppn=2+ib17:ppn=2+ib16:ppn=2+ib15:ppn=2+ib14:ppn=2

export LD_LIBRARY_PATH=/usr/local/mpich2-eth-pvfs-openib/lib:/usr/local/mpich2-eth-pvfs-openib/share

MAX_HOSTS=10
FILE_NAME=ib

cd /home/dennis/OSD/osd/osd_initiator/tests

echo "Goin to do hosts = 1 to hosts = $MAX_HOSTS";

for (( i=1; i<=$MAX_HOSTS; i+=1 )); do
	/usr/local/bin/mpiexec --pernode -np $i iospeed-mpi
done


