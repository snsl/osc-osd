#!/bin/bash
out=./latency.dat
bin=./latency
#bin="echo ran"
node=ib29
tcp=10.100.2.69
ipoib=10.100.5.29
iser=$ipoib
updatetcp="--op update -n node.transport_name -v tcp"
updateiser="--op update -n node.transport_name -v iser"

iscsiadm -m discovery -t sendtargets -p $tcp:3260
iscsiadm -m discovery -t sendtargets -p $ipoib:3260

iscsiadm -m node -T $node -p $tcp:3260 $updatetcp
iscsiadm -m node -T $node -p $tcp:3260 --login
echo "# TCP" > $out
$bin >> $out
echo >> $out
iscsiadm -m node -T $node -p $tcp:3260 --logout


iscsiadm -m node -T $node -p $ipoib:3260 $updatetcp
iscsiadm -m node -T $node -p $ipoib:3260 --login
echo "# IPoIB" >> $out
$bin >> $out
echo >> $out
iscsiadm -m node -T $node -p $ipoib:3260 --logout

iscsiadm -m node -T $node -p $iser:3260 $updateiser
iscsiadm -m node -T $node -p $iser:3260 --login

if [ "$?" -eq "0" ];
then
    echo "# iSER" >> $out
else
    echo "Cannot login" && exit
fi

$bin >> $out
echo >> $out
iscsiadm -m node -T $node -p $iser:3260 --logout
