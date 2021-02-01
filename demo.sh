#!/bin/bash
echo make
make
echo
echo 
echo make run NTIMES=42 MAXBYTES=420
echo "(waiting)"
sleep 4
make run NTIMES=42 MAXBYTES=420
echo
echo
echo make uninstall
echo "(waiting)"
sleep 4
make uninstall
