/*
	Copyright (C) 2012 Alexander Overvoorde

	Permission is hereby granted, free of charge, to any person obtaining a copy of
	this software and associated documentation files (the "Software"), to deal in
	the Software without restriction, including without limitation the rights to
	use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
	the Software, and to permit persons to whom the Software is furnished to do so,
	subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
	FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
	COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
	IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
	CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE
*/

// libpng
#ifdef _MSC_VER
#pragma warning( disable : 4611 )
#endif

#include <GL/Util/Image.hpp>
#include <GL/Util/libpng/png.h>
#include <fstream>
#include <cstring>
#include <cstdlib>

namespace GL
{
	Image::Image()
	{
		image = 0;
		width = 0;
		height = 0;
	}

	Image::Image( ushort width, ushort height, const Color& background )
	{
		image = new Color[ width * height ];
		for ( uint i = 0; i < (uint)( width * height ); i++ )
			image[i] = background;

		this->width = width;
		this->height = height;
	}

	Image::Image( ushort width, ushort height, uchar* pixels )
	{
		image = new Color[ width * height ];

		for ( uint x = 0; x < width; x++ )
		{
			for ( uint y = 0; y < height; y++ )
			{
				uint o = ( x + y * width ) * sizeof( Color );
				image[ x + y * width ] = Color( pixels[o+0], pixels[o+1], pixels[o+2], pixels[o+3] );
			}
		}

		this->width = width;
		this->height = height;
	}

	Image::Image( uchar* pixels, uint size )
	{
		image = 0;
		Load( pixels, size );
	}

	Image::Image( const std::string& filename )
	{
		image = 0;
		Load( filename );
	}

	Image::~Image()
	{
		if ( image ) delete [] image;
	}

	void Image::Load( uchar* pixels, uint size )
	{
		// Unload image
		if ( image ) delete[] image;
		image = 0;
		width = 0;
		height = 0;

		// Load data from memory
		ByteReader data( size, true );
		std::memcpy( data.Data(), pixels, size );

		// Determine format and process
		if ( data.PeekByte( 0 ) == 'B' && data.PeekByte( 1 ) == 'M' )
			LoadBMP( data );
		else if ( data.Compare( data.Length() - 18, 18, (const uchar*)"TRUEVISION-XFILE." ) )
			LoadTGA( data );
		else if ( data.Compare( 0, 4, (const uchar*)"\x89PNG" ) )
			LoadPNG( data );
		else
			throw FormatException();
	}

	void Image::Load( const std::string& filename )
	{
		// Unload image
		if ( image ) delete [] image;
		image = 0;
		width = 0;
		height = 0;

		// Load data from file
		std::ifstream file( filename.c_str(), std::ios::binary | std::ios::ate );
		if ( !file.is_open() ) throw FileException();

		uint fileSize = (uint)file.tellg();
		file.seekg( 0, std::ios::beg );

		ByteReader data( fileSize, true );
		file.read( (char*)data.Data(), fileSize );
		
		file.close();

		// Determine format and process
		if ( data.PeekByte( 0 ) == 'B' && data.PeekByte( 1 ) == 'M' )
			LoadBMP( data );
		else if ( data.Compare( data.Length() - 18, 18, (const uchar*)"TRUEVISION-XFILE." ) )
			LoadTGA( data );
		else if ( data.Compare( 0, 4, (const uchar*)"\x89PNG" ) )
			LoadPNG( data );
		else
			throw FormatException();
	}

	void Image::Save( const std::string& filename, ImageFileFormat::image_file_format_t format )
	{
		if ( image == 0 || width == 0 || height == 0 ) return;

		if ( format == ImageFileFormat::BMP )
			SaveBMP( filename );
		else if ( format == ImageFileFormat::TGA )
			SaveTGA( filename );
		else if ( format == ImageFileFormat::PNG )
			SavePNG( filename );
		else
			throw FormatException();
	}

	ushort Image::GetWidth() const
	{
		return width;
	}

	ushort Image::GetHeight() const
	{
		return height;
	}

	const Color* Image::GetPixels() const
	{
		return image;
	}

	Color Image::GetPixel( uint x, uint y ) const
	{
		if ( x >= width || y >= height ) return Color();
		return image[ x + y * width ];
	}

	void Image::SetPixel( uint x, uint y, const Color& color )
	{
		if ( x >= width || y >= height ) return;
		image[ x + y * width ] = color;
	}

	void Image::LoadBMP( ByteReader& data )
	{
		// BMP header
		data.Advance( 2 + 4 + 4 ); // Skip magic number, file size and application specific data
		uint pixelOffset = data.ReadUint();

		// DIB header
		if ( data.ReadUint() != 40 ) throw FormatException(); // Only version 1 is currently supported
		uint width = data.ReadUint();
		int rawHeight = data.ReadInt();
		uint height = abs( rawHeight );
		if ( width == 0 || height == 0 ) throw FormatException();
		if ( width > USHRT_MAX || height > USHRT_MAX ) throw FormatException();
		if ( data.ReadUshort() != 1 ) throw FormatException(); // Color planes
		if ( data.ReadUshort() != 24 ) throw FormatException(); // Bits per pixel
		if ( data.ReadUint() != 0 ) throw FormatException(); // Compression
		data.Advance( 4 ); // Skip pixel array size (very unreliable, a value of 0 is not uncommon)
		data.Advance( 4 + 4 ); // Skip X/Y resolution
		if ( data.ReadUint() != 0 ) throw FormatException(); // Palette colors
		if ( data.ReadUint() != 0 ) throw FormatException(); // Important colors

		// Pixel data
		data.Move( pixelOffset );
		uint padding = ( width * 3 ) % 4;

		image = new Color[ width * height ];
		
		for ( ushort y = 0; y < height; y++ )
		{
			for ( ushort x = 0; x < width; x++ )
			{
				uint o = rawHeight > 0 ? x + ( height - y - 1 ) * width : x + y * width; // Flip image vertically if height is negative
				image[ o ] = Color( data.PeekByte( 2 ), data.PeekByte( 1 ), data.PeekByte( 0 ) ); // BGR byte order
				data.Advance( 3 );
				if ( x == width - 1 ) data.Advance( padding );
			}
		}

		this->width = (ushort)width;
		this->height = (ushort)height;
	}

	void Image::SaveBMP( const std::string& filename )
	{
		ByteWriter data( true );
		uint padding = ( width * 3 ) % 4;

		// BMP header
		data.WriteUbyte( 'B' );
		data.WriteUbyte( 'M' );
		data.WriteUint( width * height * 3 + padding * height + 54 ); // File size
		data.WriteUint( 0 ); // Two application specific shorts
		data.WriteUint( 54 ); // Offset to pixel array

		// DIB header
		data.WriteUint( 40 ); // Header size
		data.WriteUint( width );
		data.WriteUint( height );
		data.WriteUshort( 1 ); // Color planes
		data.WriteUshort( 24 ); // Bits per pixel
		data.WriteUint( 0 ); // Compression
		data.WriteUint( width * height * 3 + padding * height );
		data.WriteUint( 0 ); // X resolution, ignored
		data.WriteUint( 0 ); // Y resolution, ignored
		data.WriteUint( 0 ); // Palette colors
		data.WriteUint( 0 ); // Important colors

		// Pixel data
		for ( short y = height - 1; y >= 0; y-- )
		{
			for ( ushort x = 0; x < width; x++ )
			{
				Color& col = image[ x + y * width ];
				data.WriteUbyte( col.B );
				data.WriteUbyte( col.G );
				data.WriteUbyte( col.R );

				if ( x == width - 1 ) data.Pad( padding );
			}
		}

		std::ofstream file( filename.c_str(), std::ios::binary );
		if ( !file.is_open() ) throw FileException();

		file.write( (char*)data.Data(), data.Length() );

		file.close();
	}

	void Image::LoadTGA( ByteReader& data )
	{
		// TGA header
		data.Advance( 1 ); // Image ID field length, ignored
		if ( data.ReadUbyte() != 0 ) throw FormatException(); // Color map
		uchar type = data.ReadUbyte();
		if ( type != 2 && type != 10 ) throw FormatException(); // Image type not true-color
		data.Advance( 5 ); // Color map info, ignored
		data.Advance( 4 ); // XY offset, ignored
		ushort width = data.ReadUshort();
		ushort height = data.ReadUshort();
		uchar depth = data.ReadUbyte();
		if ( depth != 24 && depth != 32 ) throw FormatException(); // Not RGB(A)
		uchar bytesPerPixel = depth / 8;
		uchar descriptor = data.ReadUbyte();
		if ( ( depth == 24 && descriptor != 0 ) || ( depth == 32 && descriptor != 8 ) ) throw FormatException(); // Check for alpha channel if bit depth is 32

		// If pixels are RLE encoded, they need to be unpacked first
		if ( type == 10 )
			DecodeRLE( data, width * height * bytesPerPixel, bytesPerPixel );
		
		// Pixel data
		image = new Color[ width * height ];

		for ( short y = height - 1; y >= 0; y-- )
		{
			for ( ushort x = 0; x < width; x++ )
			{
				if ( bytesPerPixel == 3 )
					image[ x + y * width ] = Color( data.PeekByte( 2 ), data.PeekByte( 1 ), data.PeekByte( 0 ) ); // BGR byte order
				else
					image[ x + y * width ] = Color( data.PeekByte( 2 ), data.PeekByte( 1 ), data.PeekByte( 0 ), data.PeekByte( 3 ) ); // BGRA byte order

				data.Advance( bytesPerPixel );
			}
		}

		this->width = width;
		this->height = height;
	}

	void Image::DecodeRLE( ByteReader& data, uint decodedLength, uchar bytesPerPixel )
	{
		std::vector<uchar> buffer;

		while ( buffer.size() < decodedLength )
		{
			uchar rle = data.ReadUbyte();
			uint count = ( rle & 0x7F ) + 1;

			if ( rle & 0x80 ) // RLE packet
			{
				Color value( data.PeekByte( 2 ), data.PeekByte( 1 ), data.PeekByte( 0 ) );
				if ( bytesPerPixel == 4 ) value.A = data.PeekByte( 3 );
				data.Advance( bytesPerPixel );

				for ( uint i = 0; i < count; i++ )
				{
					buffer.push_back( value.B );
					buffer.push_back( value.G );
					buffer.push_back( value.R );
					if ( bytesPerPixel == 4 ) buffer.push_back( value.A );
				}
			} else { // Non-RLE packet
				for ( uint i = 0; i < count * bytesPerPixel; i++ )
					buffer.push_back( data.ReadUbyte() );
			}
		}

		data.Reuse( decodedLength );
		memcpy( data.Data(), &buffer[0], decodedLength );
	}

	void Image::SaveTGA( const std::string& filename )
	{
		ByteWriter data( true );

		// TGA header
		data.WriteUbyte( 0 ); // Image ID field length
		data.WriteUbyte( 0 ); // Color map
		data.WriteUbyte( 10 ); // Image type (true-color, RLE)
		data.Pad( 5 + 4 ); // No color map or XY offset
		data.WriteUshort( width );
		data.WriteUshort( height );
		data.WriteUbyte( 24 ); // Bits per pixel
		data.WriteUbyte( 0 ); // Image descriptor (No alpha depth or direction)

		// Pixel data
		std::vector<uchar> pixelData;
		pixelData.reserve( width * height * 3 );

		for ( short y = height - 1; y >= 0; y-- )
		{
			for ( ushort x = 0; x < width; x++ )
			{
				Color& col = image[ x + y * width ];
				pixelData.push_back( col.B );
				pixelData.push_back( col.G );
				pixelData.push_back( col.R );
			}
		}

		// Compress using RLE
		EncodeRLE( data, pixelData, width );

		// Footer
		data.WriteUint( 0 );
		data.WriteUint( 0 );
		data.WriteString( "TRUEVISION-XFILE." );
		
		std::ofstream file( filename.c_str(), std::ios::binary );
		if ( !file.is_open() ) throw FileException();

		file.write( (char*)data.Data(), data.Length() );

		file.close();
	}

	inline void flushRLE( ByteWriter& data, std::vector<Color>& backlog )
	{
		if ( backlog.size() > 0 )
		{
			data.WriteUbyte( 0x80 + (uchar)backlog.size() - 1 );

			data.WriteUbyte( backlog[0].B );
			data.WriteUbyte( backlog[0].G );
			data.WriteUbyte( backlog[0].R );

			backlog.clear();
		}
	}

	inline void flushNonRLE( ByteWriter& data, std::vector<Color>& backlog, Color& lastColor )
	{
		if ( backlog.size() > 1 )
		{
			data.WriteUbyte( (uchar)backlog.size() - 2 );

			for ( uint i = 0; i < backlog.size() - 1; i++ )
			{
				data.WriteUbyte( backlog[i].B );
				data.WriteUbyte( backlog[i].G );
				data.WriteUbyte( backlog[i].R );
			}

			backlog.clear();

			backlog.push_back( lastColor );
		}
	}

	void Image::EncodeRLE( ByteWriter& data, std::vector<uchar>& pixels, ushort width )
	{
		std::vector<Color> backlog;
		Color lastColor;
		bool rleMode = false;

		for ( uint i = 0; i < pixels.size(); i += 3 )
		{
			Color col( pixels[i+2], pixels[i+1], pixels[i+0] );

			if ( !rleMode && lastColor.R == col.R && lastColor.G == col.G && lastColor.B == col.B )
			{
				flushNonRLE( data, backlog, lastColor );
				rleMode = true;
			}
			else if ( rleMode && ( lastColor.R != col.R || lastColor.G != col.G || lastColor.B != col.B ) )
			{
				flushRLE( data, backlog );
				rleMode = false;
			}
			else if ( backlog.size() == 127 || ( i % width == 0 ) )
			{
				if ( rleMode )
					flushRLE( data, backlog );
				else
					flushNonRLE( data, backlog, lastColor );
			}

			backlog.push_back( col );
			lastColor = col;
		}

		if ( backlog.size() > 0 )
		{
			if ( rleMode )
				flushRLE( data, backlog );
			else
				flushNonRLE( data, backlog, lastColor );
		}
	}

	void readPNG( png_structp png_ptr, png_bytep dest, png_size_t length )
	{
		ByteReader& data = *(ByteReader*)png_ptr->io_ptr;
		data.Read( dest, length );
	}

	void Image::LoadPNG( ByteReader& data )
	{		
		// Initialize structures
		png_structp png = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
		png_infop info = png_create_info_struct( png );

		setjmp( png_jmpbuf( png ) );

		png_set_read_fn( png, (void*)&data, &readPNG );

		// Header
		png_read_info( png, info );
		if ( info->width > USHRT_MAX ) throw FormatException();
		if ( info->height > USHRT_MAX ) throw FormatException();

		// Pixel data
		image = new Color[ info->width * info->height ];

		png_uint_32 rowLength = png_get_rowbytes( png, info );
		std::vector<uchar> row( rowLength );

		for ( ushort y = 0; y < info->height; y++ )
		{
			png_read_row( png, &row[0], NULL );

			if ( info->color_type == PNG_COLOR_TYPE_RGB )
				for ( ushort x = 0; x < info->width; x++ )
					image[ x + y * info->width ] = Color( row[x*3+0], row[x*3+1], row[x*3+2] );
			else if ( info->color_type == PNG_COLOR_TYPE_RGBA )
				for ( ushort x = 0; x < info->width; x++ )
					image[ x + y * info->width ] = Color( row[x*4+0], row[x*4+1], row[x*4+2], row[x*4+3] );
			else
				throw FormatException();
		}

		width = (ushort)info->width;
		height = (ushort)info->height;

		png_destroy_read_struct( &png, &info, NULL );
	}

	void Image::SavePNG( const std::string& filename )
	{
		FILE* file = fopen( filename.c_str(), "wb" );
		if ( !file ) throw FileException();

		// Initialize structures
		png_structp png = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
		png_infop info = png_create_info_struct( png );

		setjmp( png_jmpbuf( png ) );

		// Configure image
		png_init_io( png, file );
		png_set_IHDR( png, info, width, height, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE );
		png_write_info( png, info );

		// Prepare pixel data
		std::vector<uchar> pixelData;
		pixelData.reserve( width * height * 4 );

		for ( ushort y = 0; y < height; y++ )
		{
			for ( ushort x = 0; x < width; x++ )
			{
				Color& col = image[ x + y * width ];
				pixelData.push_back( col.R );
				pixelData.push_back( col.G );
				pixelData.push_back( col.B );
				pixelData.push_back( col.A );
			}
		}

		// Write rows
		for ( ushort y = 0; y < height; y++ )
			png_write_row( png, &pixelData[ y * width * 4 ] );

		png_write_end( png, info );

		png_destroy_write_struct( &png, &info );

		fclose( file );
	}
}