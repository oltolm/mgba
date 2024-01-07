/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VideoDumper6.h"

#include <QImage>
#include <QMediaCaptureSession>
#include <QObject>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QVideoSink>

using namespace QGBA;

VideoDumper::VideoDumper(QObject* parent)
	: QObject(parent),
	m_videoSink(new QVideoSink(this))
{
	connect(m_videoSink, &QVideoSink::videoFrameChanged, this, &VideoDumper::present);
}

QVideoSink* VideoDumper::videoSink() const {
	return m_videoSink;
}

bool VideoDumper::present(const QVideoFrame& frame) {
	QVideoFrame mappedFrame(frame);
	if (!mappedFrame.map(QVideoFrame::ReadOnly)) {
		return false;
	}
	QVideoFrameFormat::PixelFormat vFormat = mappedFrame.pixelFormat();
	QImage::Format format = QVideoFrameFormat::imageFormatFromPixelFormat(vFormat);
	bool swap = false;
#ifdef USE_FFMPEG
	bool useScaler = false;
#endif
	if (format == QImage::Format_Invalid) {
		if (vFormat < QVideoFrameFormat::Format_RGBX8888) {
			format = QVideoFrameFormat::imageFormatFromPixelFormat(vFormat);
			if (format == QImage::Format_ARGB32) {
				format = QImage::Format_RGBA8888;
			} else if (format == QImage::Format_ARGB32_Premultiplied) {
				format = QImage::Format_RGBA8888_Premultiplied;
			}
			swap = true;
		} else {
#ifdef USE_FFMPEG
			enum AVPixelFormat pixelFormat;
			switch (vFormat) {
			case QVideoFrameFormat::Format_YUV420P:
				pixelFormat = AV_PIX_FMT_YUV420P;
				break;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
			case QVideoFrameFormat::Format_YUV422P:
				pixelFormat = AV_PIX_FMT_YUV422P;
				break;
#endif
			case QVideoFrameFormat::Format_YUYV:
				pixelFormat = AV_PIX_FMT_YUYV422;
				break;
			case QVideoFrameFormat::Format_UYVY:
				pixelFormat = AV_PIX_FMT_UYVY422;
				break;
			case QVideoFrameFormat::Format_NV12:
				pixelFormat = AV_PIX_FMT_NV12;
				break;
			case QVideoFrameFormat::Format_NV21:
				pixelFormat = AV_PIX_FMT_NV12;
				break;
			default:
				return false;
			}
			format = QImage::Format_RGB888;
			useScaler = true;
			if (pixelFormat != m_pixfmt || m_scalerSize != mappedFrame.size()) {
				if (m_scaler) {
					sws_freeContext(m_scaler);
				}
				m_scaler = sws_getContext(mappedFrame.width(), mappedFrame.height(), pixelFormat,
				                          mappedFrame.width(), mappedFrame.height(), AV_PIX_FMT_RGB24,
				                          SWS_POINT, nullptr, nullptr, nullptr);
				m_scalerSize = mappedFrame.size();
				m_pixfmt = pixelFormat;
			}
#else
			return false;
#endif
		}
	}
	uchar* bits = mappedFrame.bits(0);
#ifdef USE_FFMPEG
	QImage image;
	if (!useScaler) {
		image = QImage(bits, mappedFrame.width(), mappedFrame.height(), mappedFrame.bytesPerLine(0), format);
	}
	if (useScaler) {
		image = QImage(mappedFrame.width(), mappedFrame.height(), format);
		const uint8_t* planes[8] = {0};
		int strides[8] = {0};
		for (int plane = 0; plane < mappedFrame.planeCount(); ++plane) {
			planes[plane] = mappedFrame.bits(plane);
			strides[plane] = mappedFrame.bytesPerLine(plane);
		}
		uint8_t* outBits = image.bits();
		int outStride = image.bytesPerLine();
		sws_scale(m_scaler, planes, strides, 0, mappedFrame.height(), &outBits, &outStride);
	} else
#else
	QImage image(bits, mappedFrame.width(), mappedFrame.height(), mappedFrame.bytesPerLine(), format);
#endif
	if (swap) {
		image = image.rgbSwapped();
	} else if (frame.surfaceFormat().scanLineDirection() != QVideoFrameFormat::BottomToTop) {
		image = image.copy(); // Create a deep copy of the bits
	}
	if (frame.surfaceFormat().scanLineDirection() == QVideoFrameFormat::BottomToTop) {
		image = image.mirrored();
	}
	mappedFrame.unmap();
	emit imageAvailable(image);
	return true;
}
