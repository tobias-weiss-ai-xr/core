/*
 * File:	ximajxl.h
 * Purpose:	JPEG XL Image Class Loader
 *
 * JPEG XL support for CxImage, using libjxl (https://jpeg.org/jpegxl/).
 */
#if !defined(__ximaJXL_h)
#define __ximaJXL_h

#include "ximage.h"

#if CXIMAGE_SUPPORT_JXL

class DLL_EXP CxImageJXL : public CxImage
{
public:
	CxImageJXL();
	~CxImageJXL();

	bool Decode(CxFile * hFile);
};

#endif // CXIMAGE_SUPPORT_JXL
#endif // __ximaJXL_h
