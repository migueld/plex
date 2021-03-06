#pragma once
#include "DynamicDll.h"

extern "C" {
#ifndef HAVE_MMX
#define HAVE_MMX
#endif

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#ifndef __GNUC__
#pragma warning(disable:4244)
#endif

#ifdef __APPLE__
#include "libffmpeg-OSX/avutil.h"
#include "libffmpeg-OSX/swscale.h"
#include "libffmpeg-OSX/rgb2rgb.h"
#else
#include "avutil.h"
#include "swscale.h"
#include "rgb2rgb.h"
#endif
}

class DllSwScaleInterface
{
public:
   virtual ~DllSwScaleInterface() {}
   virtual struct SwsContext *sws_getContext(int srcW, int srcH, int srcFormat, int dstW, int dstH, int dstFormat, int flags,
                                  SwsFilter *srcFilter, SwsFilter *dstFilter, double *param)=0;

   virtual int sws_scale(struct SwsContext *context, uint8_t* src[], int srcStride[], int srcSliceY,
                         int srcSliceH, uint8_t* dst[], int dstStride[])=0;

   virtual void sws_rgb2rgb_init(int flags)=0;

   virtual void sws_freeContext(struct SwsContext *context)=0;
};

#ifdef __APPLE__

// We call into this library directly.
class DllSwScale : public DllDynamic, public DllSwScaleInterface
{
public:
  virtual ~DllSwScale() {}
  virtual struct SwsContext *sws_getContext(int srcW, int srcH, int srcFormat, int dstW, int dstH, int dstFormat, int flags,
                               SwsFilter *srcFilter, SwsFilter *dstFilter, double *param) 
    { return ::sws_getContext(srcW, srcH, srcFormat, dstW, dstH, dstFormat, flags, srcFilter, dstFilter, param); }

  virtual int sws_scale(struct SwsContext *context, uint8_t* src[], int srcStride[], int srcSliceY,
                int srcSliceH, uint8_t* dst[], int dstStride[])  
    { return ::sws_scale(context, src, srcStride, srcSliceY, srcSliceH, dst, dstStride); }

  virtual void sws_rgb2rgb_init(int flags) { ::sws_rgb2rgb_init(flags); }

  virtual void sws_freeContext(struct SwsContext *context) { ::sws_freeContext(context); }
  
  // DLL faking.
  virtual bool ResolveExports() { return true; }
  virtual bool Load() { return true; }
  virtual void Unload() {}
};

#else

class DllSwScale : public DllDynamic, public DllSwScaleInterface
{
#ifdef __APPLE__
  DECLARE_DLL_WRAPPER(DllSwScale, Q:\\system\\players\\dvdplayer\\swscale-51-osx.so)
#elif !defined(_LINUX)
  DECLARE_DLL_WRAPPER(DllSwScale, Q:\\system\\players\\dvdplayer\\swscale-0.5.0.dll)
#elif defined(__x86_64__)
  DECLARE_DLL_WRAPPER(DllSwScale, Q:\\system\\players\\dvdplayer\\swscale-51-x86_64-linux.so)
#else
  DECLARE_DLL_WRAPPER(DllSwScale, Q:\\system\\players\\dvdplayer\\swscale-51-i486-linux.so)
#endif

  DEFINE_METHOD10(struct SwsContext *, sws_getContext, ( int p1, int p2, int p3, int p4, int p5, int p6, int p7, 
							 SwsFilter * p8, SwsFilter * p9, double * p10))
  DEFINE_METHOD7(int, sws_scale, (struct SwsContext *p1, uint8_t** p2, int *p3, int p4, int p5, uint8_t **p6, int *p7))
  DEFINE_METHOD1(void, sws_rgb2rgb_init, (int p1))
  DEFINE_METHOD1(void, sws_freeContext, (struct SwsContext *p1))

  BEGIN_METHOD_RESOLVE()
    RESOLVE_METHOD(sws_getContext)
    RESOLVE_METHOD(sws_scale)
    RESOLVE_METHOD(sws_rgb2rgb_init)
    RESOLVE_METHOD(sws_freeContext)
  END_METHOD_RESOLVE()
};

#endif
