#!/bin/bash

MAX_HOSTS=$1
FILE_NAME=$2

if [[ $# -ne 2 ]]; then
	echo "Need to specify MAX HOSTS and file name";
	exit 0;
fi

echo "Goin to do hosts = 1 to hosts = $MAX_HOSTS";
echo "Prepending files with $FILE_NAME";

for (( i=1; i<=$MAX_HOSTS; i+=1 )); do
	/usr/local/bin/mpiexec --pernode -np $i iospeed-mpi > $FILE_NAME.$i
done


