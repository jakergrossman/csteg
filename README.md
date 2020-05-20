# csteg
LSB steganography tool written in C

## Building
Make sure to have a version of [libpng](http://www.libpng.org/pub/png/libpng.html) installed
```
git clone https://github.com/jakergrossman/csteg
cd csteg
make
```

## Usage
To encode files:
```
csteg -w -i png_in -d data_file_in -o png_out
```

To decode files:
```
csteg -r -i png_in
```

Flag descriptors:
```
-f             do not prompt for confirmation when 
               operation would overwrite existing files

-w             write data to file

-r             write data from file

-i <filename>  specify input PNG file

-d <filename>  specify input data file

-o <filename>  specify output PNG file
```
