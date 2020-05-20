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
