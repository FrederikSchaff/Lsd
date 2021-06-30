#!/usr/bin/env bash

# Gather all test models:
NumberTests=0
Failed=0
for test in Test/*/description.txt
do    
    temp=$(dirname "${test}")    
    TestDir="${temp[0]}"
    ((NumberTests++))
    echo -e "\e[1;34m ---- Running ${test} ---- \e[0m"    
    # to preserve colour output, we do not use a subshell
    cd "${TestDir}" || { 
        echo "Could not cd to '${TestDir}'. Ensure there are no spaces in the path."
        exit 1
    }
    tush-check description.txt
    status=$?
    cd ../..
    if [ ${status} = 0 ]
    then         
        echo -e "\e[1;32m ---- ${NumberTests} : ${TestDir} - Ok! ---- \e[0m"
    else {         
        echo -e "\e[1;31m ---- ${NumberTests} : ${TestDir} - failed! ---- \e[0m"
        ((Failed++))
    }
    fi
done
 
echo
if [ "${Failed}" = 0 ]
then
    echo -e "\e[1;42m\e[1;30m SUCCESS: ${NumberTests} succeeded. \e[0m"
    exit 0
else
    echo -e "\e[1;41m\e[1;37m FAILED: ${Failed}/${NumberTests} tests failed. \e[0m"
    exit 1
fi
