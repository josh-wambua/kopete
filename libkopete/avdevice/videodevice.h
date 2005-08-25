//
// C++ Interface: videodevicelistitem
//
// Description: 
//
//
// Author: Kopete Developers <kopete-devel@kde.org>, (C) 2005
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef KOPETE_AVVIDEODEVICELISTITEM_H
#define KOPETE_AVVIDEODEVICELISTITEM_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#if defined(__linux__) && defined(ENABLE_AV)

#include <asm/types.h>
#undef __STRICT_ANSI__
#ifndef __u64 //required by videodev.h
#define __u64 unsigned long long
#endif // __u64

#ifndef pgoff_t
#define pgoff_t unsigned long
#endif

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/videodev.h>
#define __STRICT_ANSI__

#endif // __linux__

#include <qstring.h>
#include <qfile.h>
#include <qimage.h>
#include <q3valuevector.h>
#include <kcombobox.h>

#include "videoinput.h"

namespace Kopete {

namespace AV {

/**
@author Kopete Developers
*/
typedef enum
{
	VIDEODEV_DRIVER_NONE
#if defined( __linux__) && defined(ENABLE_AV)
        ,
	VIDEODEV_DRIVER_V4L
#ifdef HAVE_V4L2
        ,
	VIDEODEV_DRIVER_V4L2
#endif
#endif
} videodev_driver;

typedef enum
{
	PIXELFORMAT_NONE,
	PIXELFORMAT_GREY,
	PIXELFORMAT_RGB332,
	PIXELFORMAT_RGB555,
	PIXELFORMAT_RGB555X,
	PIXELFORMAT_RGB565,
	PIXELFORMAT_RGB565X,
	PIXELFORMAT_RGB24,
	PIXELFORMAT_BGR24,
	PIXELFORMAT_RGB32,
	PIXELFORMAT_BGR32
} pixel_format;

typedef enum
{
	IO_METHOD_NONE,
	IO_METHOD_READ,
	IO_METHOD_MMAP,
	IO_METHOD_USERPTR
} io_method;

struct imagebuffer
{
	int height;
	int width;
	pixel_format pixelformat;
	Q3ValueVector <uchar> data; // maybe it should be a rawbuffer instead of it? It could make us avoid a memory copy
};
struct rawbuffer // raw buffer
{
	uchar * start;
	size_t length;
};


class VideoDevice{
public:
	VideoDevice();
	~VideoDevice();
	int setFileName(QString filename);
	int open();
	bool isOpen();
	int checkDevice();
	int showDeviceCapabilities();
	int initDevice();
	unsigned int inputs();
	int width();
	int minWidth();
	int maxWidth();
	int height();
	int minHeight();
	int maxHeight();
	int setSize( int newwidth, int newheight);
	pixel_format setPixelFormat(pixel_format newformat);
	int pixelFormatCode(pixel_format pixelformat);
	int pixelFormatDepth(pixel_format pixelformat);
	QString pixelFormatName(pixel_format pixelformat);
	QString pixelFormatName(int pixelformat);
	unsigned int currentInput();
	int selectInput(int input);
	int startCapturing();
	int getFrame();
	int getFrame(imagebuffer *imgbuffer);
	int getImage(QImage *qimage);
	int stopCapturing();
	int close();

	bool getAutoColorCorrection();
	bool setAutoColorCorrection(bool colorcorrection);

	bool canCapture();
	bool canChromakey();
	bool canScale();
	bool canOverlay();
	bool canRead();
	bool canAsyncIO();
	bool canStream();

	QString name;
	QString full_filename;
	videodev_driver m_driver;
	int descriptor;

//protected:
#if defined(__linux__) && defined(ENABLE_AV)
#ifdef HAVE_V4L2
	struct v4l2_capability V4L2_capabilities;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
#endif
	struct video_capability V4L_capabilities;
	struct video_buffer V4L_videobuffer;
#endif	
	Q3ValueVector<Kopete::AV::VideoInput> input;
//	QFile file;
protected:
	int currentwidth, minwidth, maxwidth, currentheight, minheight, maxheight;

	Q3ValueVector<rawbuffer> m_rawbuffers;
	unsigned int m_streambuffers;
	imagebuffer m_currentbuffer;
	int m_buffer_size;

	int m_current_input;
	pixel_format m_pixelformat;

	io_method m_io_method;
	bool m_videocapture;
	bool m_videochromakey;
	bool m_videoscale;
	bool m_videooverlay;
	bool m_videoread;
	bool m_videoasyncio;
	bool m_videostream;

	bool m_autobrightcontrast;
	bool m_autocolorcorrection;

	int xioctl(int request, void *arg);
	int errnoReturn(const char* s);
	int initRead();
	int initMmap();
	int initUserptr();

};

}

}

#endif
