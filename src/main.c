//===================== Copyright 2020, Jake Grossman =======================//
//
// Purpose: main entry point of program
//
// This file is subject to the terms and conditions defined in the
// file 'LICENSE.txt', which is part of this source code package
//
//===========================================================================//
#include <stdio.h>
#include <stdarg.h> // va_list, va_start, va_end
#include <stdlib.h> // malloc
#include <stdint.h> // uint8_t
#include <string.h> // strlen
#include <unistd.h> // getopt, access
#include <png.h> // libpng

// the number of bits used to store sizes in the signature
#define SIG_SIZE_BITS 32

// print message to stderr and abort
void abort_msg(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	fprintf(stderr,	"\n");
	va_end(args);
	abort();
}

void print_usage() {
	printf("Usage: csteg [-f] -w -i png_in -d data_file_in -o png_out\n");
	printf("       csteg [-f] -r -i png_in\n");
}

// global image variables
size_t width, height; // width and height of png
png_byte color_type; // color type of png
png_byte bit_depth; // bit depth of png
int number_of_passes;

png_bytep* row_pointers; // raw pixel info

// returns row_pointers of specified png file
void read_png_file(char* filename) {
	// open file
	FILE *file_ptr = fopen(filename, "rb");

	// check that file_ptr exists
	if (!file_ptr) {
		abort_msg("read_png_file() : File %s could not be opened for reading", filename);
	}

	// test that file is png
	unsigned char header[8];

	// copy first 8 bytes of file
	fread(header, 1 , 8, file_ptr);

	// validate
	int is_png = !png_sig_cmp(header, 0, 8);
	if (!is_png) {
		abort_msg("read_png_file() : File %s is not recognized as a PNG file", filename);
	}

	// png and info pointers
	png_structp png_ptr; // png struct
	png_infop info_ptr; // png info struct

	// initialize variables
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	// check that png_ptr was created successfully
	if (!png_ptr) {
		abort_msg("read_png_file() : png_create_read_struct failed");
	}

	info_ptr = png_create_info_struct(png_ptr);

	// check that info_ptr was created successfully
	if (!info_ptr) {
		abort_msg("read_png_file() : png_create_info_struct failed");
	}

	// set jump buffer for png_init_io to fail back to
	if (setjmp(png_jmpbuf(png_ptr))) {
		abort_msg("read_png_file() : png_init_io failed");
	}

	png_init_io(png_ptr, file_ptr);
	png_set_sig_bytes(png_ptr, 8);

	// read information from png
	png_read_info(png_ptr, info_ptr);

	// set global image variables
	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);
	bit_depth = png_get_bit_depth(png_ptr, info_ptr);

	number_of_passes = png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	// set jump buffer for png_read_image to fail back to
	if (setjmp(png_jmpbuf(png_ptr))) {
		abort_msg("read_png_file() : error during png_read_image");
	}

	// allocate memory for row_pointers
	row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
	for (size_t y = 0; y < height; y++) {
		row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(png_ptr, info_ptr));
	}

	// read pixel data
	png_read_image(png_ptr, row_pointers);

	// abort if color type is not RGB or RGBA
	if (color_type != PNG_COLOR_TYPE_RGB && color_type != PNG_COLOR_TYPE_RGBA) {
		abort_msg("read_png_file() : File %s is not RGB or RGBA", filename);
	}

	// close file
	fclose(file_ptr);
}

void write_png_file(char* filename) {
	// create file
	FILE *file_ptr = fopen(filename, "wb");

	// check that file_ptr exists
	if (!file_ptr) {
		abort_msg("write_png_file() : File %s could not be opened for writing", filename);
	}

	// png and info pointers
	png_structp png_ptr; // png struct
	png_infop info_ptr; // png info struct

	// initialize variables
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	// check that png_ptr was created successfully
	if (!png_ptr) {
		abort_msg("write_png_file() : png_create_write_struct failed");
	}

	info_ptr = png_create_info_struct(png_ptr);

	// check that info_ptr was created successfully
	if (!info_ptr) {
		abort_msg("write_png_file() : png_create_info_struct failed");
	}

	// set jump buffer for png_init_io to fail back to
	if (setjmp(png_jmpbuf(png_ptr))) {
		abort_msg("write_png_file() : png_init_io failed");
	}

	png_init_io(png_ptr, file_ptr);

	// set jump buffer for png_set_IHDR to fail back to
	if (setjmp(png_jmpbuf(png_ptr))) {
		abort_msg("write_png_file() : png_set_IHDR failed");
	}

	// write header
	png_set_IHDR(png_ptr, info_ptr, width, height, bit_depth, color_type,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	// write info to info_ptr
	png_write_info(png_ptr, info_ptr);

	// set jump buffer for png_write_image to fail back to
	if (setjmp(png_jmpbuf(png_ptr))) {
		abort_msg("write_png_file() : error writing bytes");
	}

	// write bytes
	png_write_image(png_ptr, row_pointers);

	// set jump buffer for png_write_end to fail back to
	if (setjmp(png_jmpbuf(png_ptr))) {
		abort_msg("write_png_file() : error during end of write");
	}
	
	// end write
	png_write_end(png_ptr, NULL);

	// cleanup allocated memory
	for (size_t y = 0; y < height; y++) {
		free(row_pointers[y]);
	}
	free(row_pointers);

	fclose(file_ptr);
}

size_t generate_signature(uint8_t** signature, char* filename, uint32_t file_size) {
	// calculate size of signature in bytes 
	size_t filename_length = strlen(filename);	
	size_t sig_length = (SIG_SIZE_BITS / 8) * 2 + filename_length;

	// allocate memory for signature
	*signature = (uint8_t*) malloc(sig_length);
	size_t signature_pos = 0;

	// initialize variables for loops
	size_t i;
	uint8_t byte_buffer;

	// write filename_length
	for (i = (SIG_SIZE_BITS / 8) - 1; i < (SIG_SIZE_BITS / 8); i--) {
		// get correct byte
		byte_buffer = (filename_length & (0xFF << (i * 8))) >> (i * 8);

		// assign byte in signature
		(*signature)[signature_pos] = byte_buffer;

		// increment position
		signature_pos++;
	}

	// write file_length
	for (i = (SIG_SIZE_BITS / 8) - 1; i < (SIG_SIZE_BITS / 8); i--) {
		// get correct byte
		byte_buffer = (file_size & (0xFF << (i * 8))) >> (i * 8);

		// assign byte in signature
		(*signature)[signature_pos] = byte_buffer;

		//increment position
		signature_pos++;
	}

	// write file name
	for (i = 0; i < filename_length; i++) {
		// write byte to signature
		(*signature)[signature_pos] = filename[i];	

		// increment position
		signature_pos++;
	}

	return sig_length;
}

void confirm_file_overwrite(char* filename) {
	char response;
	do {
		printf("File %s already exists. Would you like to overwrite it (y/N)? ", filename);
		scanf("%c", &response);
		printf("\n");

		// flush rest of input to avoid multiple responses
		fflush(stdin);
	} while (response != 'y' && response != 'Y' && response != 'n' && response != 'N' && response != EOF);

	if (response == 'N' || response == 'n') {
		abort_msg("user exit");
	}
}

void write_data(char* png_filename_in, char* png_filename_out, char* data_filename, int force_flag) {
	// check if output file exists
	if (access(png_filename_out, F_OK) != -1) {
		// if exists and force flag isn't set, check that the user wants to override it
		if (!force_flag) {
			confirm_file_overwrite(png_filename_out);
		}
	}

	// read png
	read_png_file(png_filename_in);

	// open data file
	FILE* file_ptr = fopen(data_filename, "rb");

	// check that file_ptr exists
	if (!file_ptr) {
		abort_msg("write_data() : File %s could not be opened for reading");
	}

	// get size of file
	fseek(file_ptr, 0L, SEEK_END);
	uint32_t size = ftell(file_ptr);

	// go back to beginning of file
	rewind(file_ptr);

	// generate signature
	uint8_t* signature;
	size_t sig_size = generate_signature(&signature, data_filename, size);

	// check that data can fit in file
	size_t max_data_bits = width * height * 6;
	size_t required_data_bits = sig_size * 8 + size * 8;

	// if there is too much information
	if ( required_data_bits > max_data_bits) {
		abort_msg("write_data() : PNG is too small to fit %s (%zu bytes required / %zu bytes free)", data_filename, required_data_bits / 8, max_data_bits / 8);
	}

	// declare variables for writing data
	size_t byte_offset; // current byte of PNG
	size_t color_offset; // current color channel of PNG
	size_t bit_offset; // current bit of data byte
	size_t x, y; // x, y coordinates of current pixel
	png_byte* pixel; // current pixel

	uint8_t data_byte; // current byte of data
	uint8_t data_chunk; // current chunk of data to write

	size_t i = 0;

	// write signature
	for (i = 0; i < sig_size * 8; i += 2) {

		byte_offset = i / 6;
		bit_offset = 6 - (i % 8);
		color_offset = (i / 2) % 3;
		x = byte_offset % width;
		y = byte_offset / width;
		data_byte = signature[i/8];

		data_chunk = (data_byte & (0x3 << bit_offset)) >> bit_offset;

		// 3 values per pixel for RGB, 4 values for RGBA
		if (color_type == PNG_COLOR_TYPE_RGBA) {
			pixel =	&(row_pointers[y][x*4]);
		} else if (color_type == PNG_COLOR_TYPE_RGB) {
			pixel =	&(row_pointers[y][x*3]);
		}


		// clear two least significant bits of color channel
		pixel[color_offset] &= 0xFC;
		
		// set two least significant bits of color channel
		pixel[color_offset] |= data_chunk;
	}

	// write data
	fread(&data_byte, 1, 1, file_ptr);
	while (i < required_data_bits) {
		byte_offset = i / 6;
		bit_offset = 6 - (i % 8);
		color_offset = (i / 2) % 3;
		x = byte_offset % width;
		y = byte_offset / width;

		data_chunk = (data_byte & (0x3 << bit_offset)) >> bit_offset;

		// 3 values per pixel for RGB, 4 values for RGBA
		if (color_type == PNG_COLOR_TYPE_RGBA) {
			pixel =	&(row_pointers[y][x*4]);
		} else if (color_type == PNG_COLOR_TYPE_RGB) {
			pixel =	&(row_pointers[y][x*3]);
		}

		// clear two least significant bits of color channel
		pixel[color_offset] &= 0xFC;
		
		// set two least significant bits of color channel
		pixel[color_offset] |= data_chunk;

		// update data if current data has been consumed
		if (bit_offset == 0) {
			fread(&data_byte, 1, 1, file_ptr);
		}

		i += 2;
	}

	// write png
	write_png_file(png_filename_out);

	// cleanup allocated memory
	free(signature);
}

void read_data(char* filename, int force_flag) {
	// read png
	read_png_file(filename);

	uint32_t data_filename_length = 0, data_file_size = 0;
	char* data_filename;
	uint8_t* data;

	// current bit index
	size_t i = 0;
	size_t start_pos, end_pos; // start and stop positions for reading

	// declare variables for reading data
	size_t byte_offset; // current byte of PNG
	size_t color_offset; // current color channel of PNG
	size_t bit_offset; // current bit of data
	size_t x, y; // x, y coordinates of current pixel
	png_byte* pixel; // current pixel

	size_t filename_pos = 0; // position of current character in filename
	size_t data_pos = 0; // position of current byte in data

	// read in length of filename
	start_pos = i;
	end_pos = SIG_SIZE_BITS;
	for (i = start_pos; i < end_pos; i += 2) {
		byte_offset = i / 6;
		bit_offset = (SIG_SIZE_BITS-2) - i;
		color_offset = (i / 2) % 3;
		x = byte_offset % width;
		y = byte_offset / width;

		// 3 values per pixel for RGB, 4 values for RGBA
		if (color_type == PNG_COLOR_TYPE_RGBA) {
			pixel =	&(row_pointers[y][x*4]);
		} else if (color_type == PNG_COLOR_TYPE_RGB) {
			pixel =	&(row_pointers[y][x*3]);
		}

		data_filename_length |= ((pixel[color_offset] & 0x3) << bit_offset);
	}

	// read in length of file
	start_pos = end_pos;
	end_pos = SIG_SIZE_BITS * 2;
	for (i = start_pos; i < end_pos; i += 2) {
		byte_offset = i / 6;
		bit_offset = (SIG_SIZE_BITS*2-2) - i;
		color_offset = (i / 2) % 3;
		x = byte_offset % width;
		y = byte_offset / width;

		// 3 values per pixel for RGB, 4 values for RGBA
		if (color_type == PNG_COLOR_TYPE_RGBA) {
			pixel =	&(row_pointers[y][x*4]);
		} else if (color_type == PNG_COLOR_TYPE_RGB) {
			pixel =	&(row_pointers[y][x*3]);
		}

		data_file_size |= ((pixel[color_offset] & 0x3) << bit_offset);
	}

	// allocate memory for filename
	data_filename = (char*) malloc(sizeof(char) * data_filename_length);

	// read in file name
	start_pos = end_pos;
	end_pos = SIG_SIZE_BITS * 2 + data_filename_length * 8;
	for (i = start_pos; i < end_pos; i += 2) {
		byte_offset = i / 6;
		bit_offset = 6 - (i % 8);
		color_offset = (i / 2) % 3;
		x = byte_offset % width;
		y = byte_offset / width;

		// 3 values per pixel for RGB, 4 values for RGBA
		if (color_type == PNG_COLOR_TYPE_RGBA) {
			pixel =	&(row_pointers[y][x*4]);
		} else if (color_type == PNG_COLOR_TYPE_RGB) {
			pixel =	&(row_pointers[y][x*3]);
		}

		data_filename[filename_pos] |= ((pixel[color_offset] & 0x3) << bit_offset);

		// if bit_offset is 0, we have finished writing this byte, update filename_pos
		if (bit_offset == 0) {
			filename_pos++;
		}
	}

	// check if output file exists
	if (access(data_filename, F_OK) != -1) {
		// if exists and force flag isn't set, check that the user wants to override it
		if (!force_flag) {
			confirm_file_overwrite(data_filename);
		}
	}

	// allocate space for data
	data = (uint8_t*) malloc(sizeof(uint8_t) * data_file_size);

	// read in data
	start_pos = SIG_SIZE_BITS * 2 + data_filename_length * 8;
	end_pos = SIG_SIZE_BITS * 2 + data_filename_length * 8 + data_file_size * 8;
	for (i = start_pos; i < end_pos; i+= 2) {
		byte_offset = i / 6;
		bit_offset = 6 - (i % 8);
		color_offset = (i / 2) % 3;
		x = byte_offset % width;
		y = byte_offset / width;

		// 3 values per pixel for RGB, 4 values for RGBA
		if (color_type == PNG_COLOR_TYPE_RGBA) {
			pixel =	&(row_pointers[y][x*4]);
		} else if (color_type == PNG_COLOR_TYPE_RGB) {
			pixel =	&(row_pointers[y][x*3]);
		}

		data[data_pos] |= ((pixel[color_offset] & 0x3) << bit_offset);

		// if bit_offset is 0, we have finished writing this byte, update data_pos
		if (bit_offset == 0) {
			data_pos++;
		}

	}

	// create file
	FILE *file_ptr = fopen(data_filename, "wb");
	
	// check that file_ptr exists
	if (!file_ptr) {
		abort_msg("read_data() : could not open %s for writing", data_filename);
	}

	// write data
	fwrite(data, 1, data_file_size, file_ptr);

	// close file
	fclose(file_ptr);
}

int main(int argc, char** argv) {
	int read_flag = 0;
	int write_flag = 0;
	int force_flag = 0;
	char* png_filename_in = NULL;
	char* png_filename_out = NULL;
	char* data_filename = NULL;
	int arg;

	// handle flags
	while ((arg = getopt(argc, argv, "rwfi:d:o:h?")) != -1) {
		switch (arg) {
			case 'r':
				read_flag = 1;
				break;
			case 'w':
				write_flag = 1;
				break;
			case 'f':
				force_flag = 1;
				break;
			case 'i':
				png_filename_in = optarg;
				break;
			case 'd':
				data_filename = optarg;
				break;
			case 'o':
				png_filename_out = optarg;
				break;
			case 'h': // fall through intentional
			case '?':
				print_usage();
				exit(1);
				break;
		}
	}

	// validate input and perform operations
	if (read_flag) {
		// only input png should be specified
		if (!png_filename_in || data_filename || png_filename_out || write_flag) {
			print_usage();
			exit(1);
		}
		read_data(png_filename_in, force_flag);
	} else if (write_flag) {
		// all fields should be specified
		if (!png_filename_in || !data_filename || !png_filename_out || read_flag) {
			print_usage();
			exit(1);
		}
		write_data(png_filename_in, png_filename_out, data_filename, force_flag);
	} else {
		// did not specify read or write you silly goose
		print_usage();
		exit(1);
	}

	return 0;
}
