import glob, os
import csv
import re

def write_csv(outfile, lines):
    f = open(outfile, 'w+', newline='\n')
    writer = csv.writer(f, delimiter=',',lineterminator='\n',
                            quotechar='|', quoting=csv.QUOTE_MINIMAL)

    for line in lines:
        # definition is in unikraft/lib
        if "unikraft/lib" in line:
            lib_name = line.split("/")[2]
            if lib_name == "nolibc" or lib_name == "newlib":
                lib_name = "libc"
            else:
                lib_name = "lib" + lib_name
            function_name = line.split(" ")[1]
            writer.writerow([function_name, lib_name])

        # definition is in external library libs/
        if "libs/" in line:
            lib_name = line.split("/")[1]
            if lib_name == "nolibc" or lib_name == "newlib":
                lib_name = "libc"
            else:
                lib_name = "lib" + lib_name
            function_name = line.split(" ")[1]
            writer.writerow([function_name, lib_name])
        
f = open("res.deps", "r")
lines = [line.rstrip() for line in f]
write_csv("callfile.csv", lines)
