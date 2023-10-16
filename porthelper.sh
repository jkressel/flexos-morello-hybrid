#!/bin/bash

# argument 1: file path
filetoport=$1

# format of the call file:
# function_name,libname
# e.g.,
# accept,liblwip
callfile="/root/.unikraft/callfile.csv"

rulefile="/tmp/porthelper.cocci"

templaterule="./rule.cocci.in"

# 1. generate call file (CSV)

# Find the symbols used by the target file
rm -rf cscope*
cscope -L -2 ".*" $filetoport | grep -v "extern" | grep -v "/usr/"  | awk -F ' ' '{print $2}' | sort | uniq > res.symb
rm -rf cscope*

# Create a the DB with all the symbols exported by internal and external libraries
find . -name '*.c' > cscope.files
cscope -b -q -k

USE_NEWLIB=True

# For each symbol we found, we search for its definition, if the definition is not in the lib that the target files is part of, then we add it to a temporary dependncy file. We add the symbol, and the path to its definition.
echo "" > res.deps
while read in; do
	if [[ $(cscope -d -L1 $in | grep ".*(.*)" | grep -v "newlib" | grep -v "define" | grep -v "lwip-2.1.2/src/apps/" | sort | uniq | wc -l) -eq 1 ]]; then
		cscope -d -L1 $in | grep ".*(.*)" | grep -v "newlib" | grep -v "define" | grep -v "lwip-2.1.2/src/apps/" | sort | uniq >> res.deps
	fi
done < res.symb

# We create a csv file from the dependency file
python3 parse_results.py

# Cleanup
rm -rf cscope*
rm res.deps
rm res.symb

# 2. generate coccinelle rules

i=0
rm -f $rulefile && touch $rulefile
while IFS=, read -r fname lname; do
  cat $templaterule >> $rulefile
  sed -i "s/{{ rule_nr }}/${i}/g" $rulefile
  sed -i "s/{{ lname }}/${lname}/g" $rulefile
  sed -i "s/{{ fname }}/${fname}/g" $rulefile
  i=$(( i + 1 ))
done < $callfile

# 3. apply coccinelle rules

spatch -sp_file $rulefile $filetoport
