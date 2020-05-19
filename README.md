# csteg
LSB steganography tool written in C

## Building
```
git clone https://github.com/jakergrossman/csteg
cd csteg
make
```

## Usage
To encode data:
```
csteg -w -i png_in -d data_in -o png_out
```

To decode data:
```
csteg -r -i png_in
```
