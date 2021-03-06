/***************************************************************************
    \file ADM_ffmp43
    \brief Decoders using lavcodec
    \author mean & all (c) 2002-2010
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <stddef.h>

#include "ADM_default.h"
#include "ADM_ffmp43.h"
#include "DIA_coreToolkit.h"
#include "ADM_hwAccel.h"

extern "C"
{
    static void ADM_releaseBuffer(struct AVCodecContext *avctx, AVFrame *pic);
    static int  ADM_getBuffer(AVCodecContext *avctx, AVFrame *pic);
}

#define aprintf(...) {}


//****************************
extern uint8_t DIA_lavDecoder (bool  * swapUv, bool * showU);
extern "C"
{
  int av_is_voppacked (AVCodecContext * avctx, int *vop_packed, int *gmc,
		       int *qpel);
};
/**
    \fn clonePic
    \brief Convert AvFrame to ADMImage
*/
uint8_t decoderFF::clonePic (AVFrame * src, ADMImage * out)
{
  uint32_t    u,v;
  ADM_assert(out->isRef());
  ADMImageRef *ref=out->castToRef();
  ref->_planes[0] = (uint8_t *) src->data[0];
  ref->_planeStride[0] = src->linesize[0];
  if (decoderFF_params.swapUv)
    {
      u = 1;
      v = 2;
    }
  else
    {
      u = 2;
      v = 1;
    }
  ref->_planes[1] = (uint8_t *) src->data[u];
  ref->_planeStride[1] = src->linesize[u];

  ref->_planes[2] = (uint8_t *) src->data[v];
  ref->_planeStride[2] = src->linesize[v];
  
  _lastQ = 0;			//_context->quality;
  out->_Qp = (src->quality * 32) / FF_LAMBDA_MAX;
  out->flags = frameType ();

  // Quant ?
  if (src->qstride && src->qscale_table && codecId != AV_CODEC_ID_H264)
    {
      out->quant = (uint8_t *) src->qscale_table;
      out->_qStride = src->qstride;
      out->_qSize = (_w + 15) >> 4;
      out->_qSize *= (_h + 15) >> 4;	// FixME?
    }
  else
    {
      out->_qSize = out->_qStride = 0;
      out->quant = NULL;
    }
    uint64_t pts_opaque=(uint64_t)(src->reordered_opaque);
    //printf("[LAVC] Old pts :%"PRId64" new pts :%"PRId64"\n",out->Pts, pts_opaque);
    //printf("[LAVC] pts: %"PRIu64"\n",src->pts);
    out->Pts= (uint64_t)(pts_opaque);

    return 1;
}
/**
        \fn decoderMultiThread
        \brief Enabled multitheaded decoder if possible
*/
void decoderFF::decoderMultiThread (void)
{
  uint32_t threads = 0;

 // prefs->get(FEATURE_THREADING_LAVC, &threads);
//#warning Fixme
    threads=1;
  if (threads == 0)
	  threads = ADM_cpu_num_processors();

  if (threads == 1)
	  threads = 0;

  if (threads)
  {
      printf ("[lavc] Enabling MT decoder with %u threads\n", threads);
      _threads = threads;
      _usingMT = 1;
  }
}
uint8_t decoderFF::getPARWidth (void)
{
  if(!_context->sample_aspect_ratio.num) return 1;
  return _context->sample_aspect_ratio.num;
}
uint8_t decoderFF::getPARHeight (void)
{
  if(!_context->sample_aspect_ratio.den) return 1;
  return _context->sample_aspect_ratio.den;

}

//________________________________________________
bool  decoderFF::setParam(void)
{
	DIA_lavDecoder(&decoderFF_params.swapUv, &decoderFF_params.showMv);

	return true;			// no param for ffmpeg
}

const decoderFF::decoderFF_param_t decoderFF::defaultConfig = {false, false};

const ADM_paramList decoderFF::decoderFF_param_template[] =
{
	{"swapUv", offsetof(decoderFF_param_t, swapUv), "bool", ADM_param_bool},
	{"showMv", offsetof(decoderFF_param_t, showMv), "bool", ADM_param_bool},
	{NULL, 0, NULL}
};

bool decoderFF::getConfiguration(CONFcouple **conf)
{
	return ADM_paramSave(conf, decoderFF_param_template, &decoderFF_params);
}

bool decoderFF::resetConfiguration()
{
	memcpy(&decoderFF_params, &defaultConfig, sizeof(decoderFF_param_t));

	return true;
}

bool decoderFF::setConfiguration(CONFcouple * conf)
{
	return ADM_paramLoad(conf, decoderFF_param_template, &decoderFF_params);
}

//-------------------------------
decoderFF::decoderFF (uint32_t w, uint32_t h,uint32_t fcc, uint32_t extraDataLen, uint8_t *extraData,uint32_t bpp)
            :decoders (w, h,fcc,extraDataLen,extraData,bpp)
{
	resetConfiguration();

  hurryUp=false;
  _setBpp=false;
  _setFcc=false;
  codecId = 0;
//                              memset(&_context,0,sizeof(_context));
  _allowNull = 0;
  _gmc = 0;
  _context = NULL;
  _frame = NULL;
  _refCopy = 0;
  _usingMT = 0;
  _bpp = bpp;
  _fcc = fcc;

  _frame=av_frame_alloc();
  if(!_frame)
  {
      ADM_assert (_frame);
  }

  printf ("[lavc] Build: %d\n", LIBAVCODEC_BUILD);
  _extraDataCopy=NULL;
  
  if(extraDataLen)
    {
            _extraDataLen=(int)extraDataLen;
            _extraDataCopy=new uint8_t[extraDataLen+FF_INPUT_BUFFER_PADDING_SIZE];
            memset(_extraDataCopy,0,extraDataLen+FF_INPUT_BUFFER_PADDING_SIZE);
            memcpy(_extraDataCopy,extraData,extraDataLen);
    }
   hwDecoder=NULL;

}

//_____________________________________________________

decoderFF::~decoderFF ()
{
  if (_usingMT)
    {
      printf ("[lavc] Killing decoding threads\n");
      _usingMT = 0;
    }
  if(_context)
  {
        avcodec_close (_context);
        av_free(_context);
        _context=NULL;
        printf ("[lavc] Destroyed\n");
    }
  
  if(_frame)
  {
      av_frame_free(&_frame);
      _frame=NULL;
  }
  
  if(_extraDataCopy)
  {
      delete [] _extraDataCopy;
      _extraDataCopy=NULL;
  }
  delete hwDecoder;
  hwDecoder=NULL;
}
/**
 * 
 * @param pic
 * @return 
 */
uint32_t decoderFF::admFrameTypeFromLav (AVFrame *pic)
{
    uint32_t outFlags=0;
#define SET(x)      {outFlags=x;}
#define SET_ADD(x)  {outFlags|=x;}
    
  switch (pic->pict_type)
    {
        case AV_PICTURE_TYPE_B:
                SET (AVI_B_FRAME);
                break;
        case AV_PICTURE_TYPE_I:
                SET (AVI_KEY_FRAME);
                if (!pic->key_frame)
                  {
                    if (codecId == AV_CODEC_ID_H264)
                        SET (AVI_P_FRAME)
                    else
                      ADM_info ("\n But keyframe is not set\n");
                  }
                break;
        case AV_PICTURE_TYPE_S:
                _gmc = 1;			// No break, just inform that gmc is there
        case AV_PICTURE_TYPE_P:
                SET (AVI_P_FRAME);
                if (pic->key_frame)
                        aprintf ("\n But keyframe is set\n");
                break;
        default:
                break;
    }
    outFlags&=~AVI_STRUCTURE_TYPE_MASK;
    if(pic->interlaced_frame)
    {
        SET_ADD(AVI_FIELD_STRUCTURE)
        if(pic->top_field_first)
            SET_ADD(AVI_TOP_FIELD)
        else
            SET_ADD(AVI_BOTTOM_FIELD)
    }
  return outFlags;
}
/**
    \fn frameType
    \return frametype of the last decoded frame
*/
uint32_t decoderFF::frameType (void)
{
    return admFrameTypeFromLav(_frame);

}
bool decoderFF::decodeHeaderOnly (void)
{
  hurryUp=true;
  _context->skip_frame=AVDISCARD_ALL;
  _context->skip_idct=AVDISCARD_ALL;
  printf ("\n[lavc] Hurry up\n");
  return 1;
}
bool decoderFF::decodeFull (void)
{
  _context->skip_frame=AVDISCARD_DEFAULT;
  _context->skip_idct=AVDISCARD_DEFAULT;
  hurryUp=false;
  printf ("\n[lavc] full decoding\n");
  return 1;
}

/**
    \fn flush
    \brief empty internal buffer
*/
bool    decoderFF::flush(void)
{
    if(_context)
        avcodec_flush_buffers(_context);
    return true;
}
/**
    \fn uncompress
    \brief Actually decode an image
*/
bool   decoderFF::uncompress (ADMCompressedImage * in, ADMImage * out)
{
  int got_picture = 0;
  uint8_t *oBuff[3];
  int ret = 0;
  out->_noPicture = 0;
  if(hwDecoder)
        return hwDecoder->uncompress(in,out);
 
  
  if (decoderFF_params.showMv)
    {
      _context->debug_mv |= FF_SHOW;
      _context->debug |= 0;	//FF_DEBUG_VIS_MB_TYPE;
    }
  else
    {
      _context->debug_mv &= ~FF_SHOW;
      _context->debug &= ~(FF_DEBUG_VIS_MB_TYPE + FF_DEBUG_VIS_QP);
    }

  //printf("Frame size : %d\n",in->dataLength);

  if (in->dataLength == 0 && !_allowNull)	// Null frame, silently skipped
    {

      printf ("[Codec] null frame\n");
        // search the last image
        if (_context->coded_frame &&
            _context->coded_frame->data &&
            _context->coded_frame->data[0]
            )
          {
            printf("[Codec] Cloning older pic\n");
            clonePic (_context->coded_frame, out);
            out->Pts=ADM_COMPRESSED_NO_PTS;
          }
        else
            {
                out->_noPicture = 1;
                out->Pts=ADM_COMPRESSED_NO_PTS;
                printf("[Codec] No Picture\n");
            }
          return 1;
    }
   // Put a safe value....
   out->Pts=in->demuxerPts;
    _context->reordered_opaque=in->demuxerPts;
  //_frame.opaque=(void *)out->Pts;
  //printf("Incoming Pts :%"PRId64"\n",out->Pts);
  AVPacket pkt;
  av_init_packet(&pkt);
  pkt.data=in->data;
  pkt.size=in->dataLength;
  if(in->flags&AVI_KEY_FRAME)
    pkt.flags=AV_PKT_FLAG_KEY;
  else
    pkt.flags=0;

  ret = avcodec_decode_video2 (_context, _frame, &got_picture, &pkt);
  if(!bFramePossible())
  {
    // No delay, the value is sure, no need to hide it in opaque
    _context->reordered_opaque=(int64_t)in->demuxerPts;
  }
  out->_qStride = 0;		//Default = no quant
  if (0 > ret && !hurryUp)
    {
      printf ("\n[lavc] error in lavcodec decoder!\n");
      printf ("[lavc] Err: %d, size :%d\n", ret, in->dataLength);
      return 0;
    }
  if (!got_picture && !hurryUp)
    {
      // Some encoder code a vop header with the
      // vop flag set to 0
      // it is meant to mean frame skipped but very dubious
      if (in->dataLength <= 8)
        if(codecId == AV_CODEC_ID_MPEG4||codecId==AV_CODEC_ID_FRAPS)
	{
	  printf ("[lavc] Probably pseudo black frame...\n");
	  out->_Qp = 2;
	  out->flags = 0;	// assume P ?
          if(_context->coded_frame)
            clonePic (_context->coded_frame, out);
          else
             out->_noPicture = 1;
          out->Pts=ADM_NO_PTS; // not sure
	  return 1;
	}
      // allow null means we allow null frame in and so potentially
      // have no frame out for a time
      // in that case silently fill with black and returns it as KF
      if (_allowNull)
	{
	  out->flags = AVI_KEY_FRAME;
	  if (!_refCopy)
	    {
            out->blacken();
	    }
	  else
	    {
	      out->_noPicture = 1;
	    }
	  printf ("\n[lavc] ignoring got pict ==0\n");
	  return 1;

	}
#if 0
      printf ("[lavc] Err: %d, size: %d\n", ret, in->dataLength);
      printf ("\n[lavc] error in FFMP43/mpeg4!: got picture\n");
#endif
      //GUI_Alert("Please retry with misc->Turbo off");
      //return 1;
      return 0;
    }
  if (hurryUp)
    {
      out->flags = frameType ();
      return 1;
    }
  // We have an image....
  switch (_context->pix_fmt)
    {
    case AV_PIX_FMT_YUV411P:
      out->_colorspace = ADM_COLOR_YUV411;
      break;
    case AV_PIX_FMT_YUYV422:
      out->_colorspace = ADM_COLOR_YUV422;
      break;
    case AV_PIX_FMT_YUV422P:
    case AV_PIX_FMT_YUVJ422P:
      out->_colorspace = ADM_COLOR_YUV422P;
      break;
    case AV_PIX_FMT_GRAY8:
       out->_colorspace = ADM_COLOR_Y8;
       break;
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUVJ444P:
      out->_colorspace = ADM_COLOR_YUV444;
      break;
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
    case AV_PIX_FMT_YUVA420P:
      // Default is YV12 or I420
      // In that case depending on swap u/v
      // we do it or not
      out->_colorspace = ADM_COLOR_YV12;
      break;
   case AV_PIX_FMT_BGR24:
   case AV_PIX_FMT_RGB24:
	  out->_colorspace = ADM_COLOR_BGR24;
	  break;
    case AV_PIX_FMT_BGR0:
    case AV_PIX_FMT_BGRA:
      out->_colorspace = ADM_COLOR_BGR32A;
      break;
    case AV_PIX_FMT_RGBA: // ???PIX_FMT_RGBA32:
      out->_colorspace = ADM_COLOR_RGB32A;
      break;
    case AV_PIX_FMT_RGB555:
      out->_colorspace = ADM_COLOR_RGB555;
      break;
#if 0
    case AV_PIX_FMT_VDPAU_MPEG1:
    case AV_PIX_FMT_VDPAU_MPEG2:
    case AV_PIX_FMT_VDPAU_WMV3:
    case AV_PIX_FMT_VDPAU_VC1:
    case AV_PIX_FMT_VDPAU_H264:
    case AV_PIX_FMT_VDPAU:
        out->_colorspace=ADM_COLOR_VDPAU;
        break;        
    case AV_PIX_FMT_VAAPI:
        out->_colorspace=ADM_COLOR_LIBVA;
        break;
        
#ifdef USE_XVBA        
    case AV_PIX_FMT_XVBA_VLD:
        out->_colorspace=ADM_COLOR_XVBA;
        break;
#endif        
#endif
  case AV_PIX_FMT_YUV444P10LE:
        out->_colorspace=ADM_COLOR_YUV444_10BITS;
        break;      
  case AV_PIX_FMT_YUV422P10LE:
        out->_colorspace=ADM_COLOR_YUV422_10BITS;
        break;
  case   AV_PIX_FMT_P010LE:      
        out->_colorspace= ADM_COLOR_NV12_10BITS;
        break;
  case  AV_PIX_FMT_YUV420P10LE      :  
        out->_colorspace=ADM_COLOR_YV12_10BITS;
        break;
    default:
      printf ("[lavc] Unhandled colorspace: %d (AV_PIX_FMT_YUV444P10BE=%d)\n", _context->pix_fmt,AV_PIX_FMT_YUV444P10BE);
      return 0;
    }
    clonePic (_frame, out);
    //printf("[AvCodec] Pts : %"PRIu64" Out Pts:%"PRIu64" \n",_frame.pts,out->Pts);

  return 1;
}
// *******************************************************************
// *******************************************************************
// *******************************************************************

decoderFFDiv3::decoderFFDiv3 (uint32_t w, uint32_t h,uint32_t fcc, uint32_t extraDataLen, uint8_t *extraData,uint32_t bpp):
decoderFF (w, h,fcc,extraDataLen,extraData,bpp)
{
  _refCopy = 1;			// YUV420 only
  WRAP_Open (AV_CODEC_ID_MSMPEG4V3);
}

decoderFFMpeg4::decoderFFMpeg4 (uint32_t w, uint32_t h,uint32_t fcc, uint32_t extraDataLen, uint8_t *extraData,uint32_t bpp):
decoderFF (w, h,fcc,extraDataLen,extraData,bpp)
{
// force low delay as avidemux don't handle B-frames
  ADM_info ("[lavc] Using %d bytes of extradata for MPEG4 decoder\n", (int)extraDataLen);

  _refCopy = 1;			// YUV420 only
  _setFcc=true;
  decoderMultiThread ();
  WRAP_Open (AV_CODEC_ID_MPEG4);
  
}
bool decoderFFMpeg4::uncompress (ADMCompressedImage * in, ADMImage * out)
{
    // For pseudo startcode
    if(in->dataLength)
    {
        in->data[in->dataLength]=0;
        in->data[in->dataLength+1]=0;
    }
    return decoderFF::uncompress(in,out);

}
//************************************
decoderFFDV::decoderFFDV (uint32_t w, uint32_t h,uint32_t fcc, uint32_t extraDataLen, uint8_t *extraData,uint32_t bpp):
decoderFF (w, h,fcc,extraDataLen,extraData,bpp)
{
    
    WRAP_Open (AV_CODEC_ID_DVVIDEO);
  

}
decoderFFMpeg12::decoderFFMpeg12 (uint32_t w, uint32_t h,uint32_t fcc, uint32_t extraDataLen, uint8_t *extraData,uint32_t bpp):
                decoderFF (w, h,fcc,extraDataLen,extraData,bpp)
{
  _refCopy = 1;			// YUV420 only
  decoderMultiThread ();
  WRAP_Open (AV_CODEC_ID_MPEG2VIDEO);
}

decoderFFPng::decoderFFPng(uint32_t w, uint32_t h, uint32_t fcc, uint32_t extraDataLen, uint8_t *extraData, uint32_t bpp) : decoderFF(w, h, fcc, extraDataLen, extraData, bpp)
{
	WRAP_Open (AV_CODEC_ID_PNG);
}

decoderFF_ffhuff::decoderFF_ffhuff (uint32_t w, uint32_t h,uint32_t fcc, uint32_t extraDataLen, uint8_t *extraData,uint32_t bpp)
:decoderFF (w, h,fcc,extraDataLen,extraData,bpp)
{
    
  _setBpp=true;
  ADM_info ("[lavc] FFhuff: We have %d bytes of extra data\n", (int)extraDataLen);
  WRAP_Open (AV_CODEC_ID_FFVHUFF);  

}
decoderFFH264::decoderFFH264 (uint32_t w, uint32_t h,uint32_t fcc, uint32_t extraDataLen, uint8_t *extraData,uint32_t bpp)
        :decoderFF (w, h,fcc,extraDataLen,extraData,bpp)
{
  _refCopy = 1;			// YUV420 only
  decoderMultiThread ();
  ADM_info ("[lavc] Initializing H264 decoder with %d extradata\n", (int)extraDataLen);
  WRAP_Open(AV_CODEC_ID_H264);
  
}
//*********************
extern "C" {int av_getAVCStreamInfo(AVCodecContext *avctx, uint32_t  *nalSize, uint32_t *isAvc);}
/**
    \fn uncompress
*/
bool   decoderFFH264::uncompress (ADMCompressedImage * in, ADMImage * out)
{
  if(!hurryUp) return decoderFF::uncompress (in, out);
    ADM_assert(0);
#if 0
  uint32_t nalSize, isAvc;
  av_getAVCStreamInfo(_context,&nalSize,&isAvc);
  if(isAvc)
  {
      return extractH264FrameType(nalSize, in->data,in->dataLength,&(out->flags));
  }else
  {
    return extractH264FrameType_startCode(nalSize, in->data,in->dataLength,&(out->flags));
  }
#endif
  return true;
}
decoderFFH265::decoderFFH265 (uint32_t w, uint32_t h,uint32_t fcc, uint32_t extraDataLen, uint8_t *extraData,uint32_t bpp)
        :decoderFF (w, h,fcc,extraDataLen,extraData,bpp)
{
  _refCopy = 1;			// YUV420 only
  decoderMultiThread ();
  ADM_info ("[lavc] Initializing H265 decoder with %d extradata\n", (int)extraDataLen);
  WRAP_Open(AV_CODEC_ID_HEVC);
  
}
//*********************
decoderFFhuff::decoderFFhuff (uint32_t w, uint32_t h,uint32_t fcc, uint32_t extraDataLen, uint8_t *extraData,uint32_t bpp):
decoderFF (w, h,fcc,extraDataLen,extraData,bpp)
{
  _setBpp=true;
  WRAP_Open (AV_CODEC_ID_HUFFYUV);
}

//***************
extern void     avcodec_init(void );
extern  void    avcodec_register_all(void );
extern "C"
{
  void adm_lavLogCallback(void  *instance, int level, const char* fmt, va_list list);
}


extern "C" 
{
static  void ffFatalError(const char *what,int lineno, const char *filez)
{
        ADM_backTrack(what,lineno,filez);
}

}
/**
    \fn ADM_lavInit
    \brief Init both lavcodec and lavformat
*/
void ADM_lavInit(void)
{
    avcodec_register_all();
    av_log_set_callback(adm_lavLogCallback);
    av_setFatalHandler(ffFatalError);
#ifdef ADM_DEBUG
    av_log_set_level(AV_LOG_DEBUG);
#endif

}
void adm_lavLogCallback(void  *instance, int level, const char* fmt, va_list list)
{
   // if(level>1) return;
    char buf[256];

    vsnprintf(buf, sizeof(buf), fmt, list);
    if(buf[0] != '\0' && level<=AV_LOG_INFO)
        ADM_info("[lavc] %s",buf);
}

void ADM_lavDestroy(void)
{
	//av_free_static();
}

// EOF
