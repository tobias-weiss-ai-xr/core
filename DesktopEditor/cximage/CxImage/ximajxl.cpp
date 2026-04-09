/*
 * File:	ximajxl.cpp
 * Purpose:	JPEG XL Image Class Loader
 *
 * JPEG XL decode support for CxImage, using libjxl (https://jpeg.org/jpegxl/).
 */

#include "ximajxl.h"

#if CXIMAGE_SUPPORT_JXL

#if CXIMAGE_SUPPORT_DECODE

#ifdef SUPPORT_LIB_JXL_SOURCES
#include <jxl/decode.h>
#include <jxl/thread.h>
#include "ximaiter.h"

////////////////////////////////////////////////////////////////////////////////
CxImageJXL::CxImageJXL() : CxImage(CXIMAGE_FORMAT_JXL)
{
}
////////////////////////////////////////////////////////////////////////////////
CxImageJXL::~CxImageJXL()
{
}
////////////////////////////////////////////////////////////////////////////////
bool CxImageJXL::Decode(CxFile *hFile)
{
	if (!hFile)
		return false;

	// Read the entire file into memory — libjxl requires the full input
	long fileSize = hFile->Size();
	if (fileSize <= 0)
	{
		strcpy(info.szLastError, "JXL: unable to determine file size");
		return false;
	}

	uint8_t *jxlData = new uint8_t[fileSize];
	if (!jxlData)
	{
		strcpy(info.szLastError, "JXL: out of memory");
		return false;
	}

	long bytesRead = hFile->Read(jxlData, 1, fileSize);
	if (bytesRead != fileSize)
	{
		delete[] jxlData;
		strcpy(info.szLastError, "JXL: failed to read file");
		return false;
	}

	// Create the decoder
	JxlDecoder *decoder = JxlDecoderCreate(NULL);
	if (!decoder)
	{
		delete[] jxlData;
		strcpy(info.szLastError, "JXL: failed to create decoder");
		return false;
	}

	// Subscribe to events: basic info + full image
	JxlDecoderSubscribeEvents(decoder,
		JXL_DEC_BASIC_INFO | JXL_DEC_NEED_IMAGE_OUT_BUFFER | JXL_DEC_FULL_IMAGE);

	// Set input
	JxlDecoderSetInput(decoder, jxlData, fileSize);
	JxlDecoderCloseInput(decoder);

	bool success = false;
	JxlBasicInfo basicInfo;
	JxlPixelFormat pixelFormat;
	uint8_t *outputPixels = NULL;

	while (true)
	{
		JxlDecoderStatus status = JxlDecoderProcessInput(decoder);

		switch (status)
		{
		case JXL_DEC_NEED_MORE_INPUT:
			// We provided the full file, this should not happen
			strcpy(info.szLastError, "JXL: unexpected need for more input");
			goto cleanup;

		case JXL_DEC_BASIC_INFO:
			if (JxlDecoderGetBasicInfo(decoder, &basicInfo) != JXL_DEC_SUCCESS)
			{
				strcpy(info.szLastError, "JXL: failed to get basic info");
				goto cleanup;
			}

			// Handle dimensions-only query (info.nEscape == -1)
			if (info.nEscape == -1)
			{
				head.biWidth = basicInfo.xsize;
				head.biHeight = basicInfo.ysize;
				info.dwType = CXIMAGE_FORMAT_JXL;
				success = true;
				goto cleanup;
			}
			break;

		case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
		{
			// Set up pixel format: native RGBA, 8 bits per channel
			pixelFormat.num_channels = 4;
			pixelFormat.data_type = JXL_TYPE_UINT8;
			pixelFormat.endianness = JXL_NATIVE_ENDIAN;
			pixelFormat.align = 0;

			size_t pixelBufferSize = 0;
			if (JxlDecoderImageOutBufferSize(decoder, &pixelFormat, &pixelBufferSize)
				!= JXL_DEC_SUCCESS)
			{
				strcpy(info.szLastError, "JXL: failed to get output buffer size");
				goto cleanup;
			}

			outputPixels = new uint8_t[pixelBufferSize];
			if (!outputPixels)
			{
				strcpy(info.szLastError, "JXL: out of memory for output buffer");
				goto cleanup;
			}

			if (JxlDecoderSetImageOutBuffer(decoder, &pixelFormat,
				outputPixels, pixelBufferSize) != JXL_DEC_SUCCESS)
			{
				strcpy(info.szLastError, "JXL: failed to set output buffer");
				goto cleanup;
			}
			break;
		}

		case JXL_DEC_FULL_IMAGE:
			// Decoding complete — copy pixels into CxImage DIB
		{
			uint32_t width = basicInfo.xsize;
			uint32_t height = basicInfo.ysize;
			uint32_t bpp = 32; // RGBA

			Create(width, height, bpp, CXIMAGE_FORMAT_JXL);
			if (!pDib)
			{
				strcpy(info.szLastError, "JXL: failed to create image");
				goto cleanup;
			}

			// Copy RGBA data from libjxl output to CxImage DIB
			// libjxl outputs top-down RGBA, CxImage stores bottom-up BGRA
			CImageIterator iter(this);
			iter.Upset();

			for (uint32_t y = 0; y < height; y++)
			{
				uint8_t *dstRow = iter.GetRow();
				uint8_t *srcRow = outputPixels + y * width * 4;

				for (uint32_t x = 0; x < width; x++)
				{
					// Convert RGBA -> BGRA
					dstRow[x * 4 + 0] = srcRow[x * 4 + 2]; // B
					dstRow[x * 4 + 1] = srcRow[x * 4 + 1]; // G
					dstRow[x * 4 + 2] = srcRow[x * 4 + 0]; // R
					dstRow[x * 4 + 3] = srcRow[x * 4 + 3]; // A
				}
				iter.PrevRow();
			}

			// Set alpha channel if the image has alpha
			if (basicInfo.alpha_bits > 0)
			{
				AlphaCreate();
				if (pAlpha)
				{
					for (uint32_t y = 0; y < height; y++)
					{
						uint8_t *srcRow = outputPixels + y * width * 4;
						for (uint32_t x = 0; x < width; x++)
						{
							AlphaSet(x, y, srcRow[x * 4 + 3]);
						}
					}
				}
			}

			success = true;
			break;
		}

		case JXL_DEC_SUCCESS:
			// All done
			goto cleanup;

		case JXL_DEC_ERROR:
			strcpy(info.szLastError, "JXL: decoder error");
			goto cleanup;

		default:
			strcpy(info.szLastError, "JXL: unknown decoder status");
			goto cleanup;
		}
	}

cleanup:
	if (outputPixels)
		delete[] outputPixels;
	delete[] jxlData;
	JxlDecoderDestroy(decoder);

	return success;
}

#else // !SUPPORT_LIB_JXL_SOURCES
////////////////////////////////////////////////////////////////////////////////
CxImageJXL::CxImageJXL() : CxImage(CXIMAGE_FORMAT_JXL)
{
}
////////////////////////////////////////////////////////////////////////////////
CxImageJXL::~CxImageJXL()
{
}
////////////////////////////////////////////////////////////////////////////////
bool CxImageJXL::Decode(CxFile * /*hFile*/)
{
	// libjxl sources are not available — JXL decoding is not supported
	strcpy(info.szLastError, "JXL: libjxl not available (SUPPORT_LIB_JXL_SOURCES not defined)");
	return false;
}
#endif // SUPPORT_LIB_JXL_SOURCES

#endif // CXIMAGE_SUPPORT_DECODE

#endif // CXIMAGE_SUPPORT_JXL
