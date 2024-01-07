/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AudioProcessorQt.h"

#include "AudioDevice.h"
#include "LogController.h"

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#include <QAudio>
#include <QAudioFormat>
#include <QAudioSink>
#include <QMediaDevices>
#else
#include <QAudioOutput>
#endif

#include <mgba/core/core.h>
#include <mgba/core/thread.h>

using namespace QGBA;

AudioProcessorQt::AudioProcessorQt(QObject* parent)
	: AudioProcessor(parent), m_device(new AudioDevice(this))
{
		QAudioFormat format;
		format.setSampleRate(m_sampleRate);
		format.setChannelCount(2);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
		format.setSampleSize(16);
		format.setCodec("audio/pcm");
		format.setByteOrder(QAudioFormat::Endian(QSysInfo::ByteOrder));
		format.setSampleType(QAudioFormat::SignedInt);

		m_audioOutput = new QAudioOutput(format, this);
		m_audioOutput->setCategory("game");
#else
		format.setSampleFormat(QAudioFormat::Int16);

		QAudioDevice device(QMediaDevices::defaultAudioOutput());
		m_audioOutput = new QAudioSink(device, format, this);
		LOG(QT, INFO) << "Audio outputting to " << device.description();
#endif
}

void AudioProcessorQt::setInput(std::shared_ptr<CoreController> controller) {
	AudioProcessor::setInput(std::move(controller));
	m_device->setInput(input());
	m_device->setFormat(m_audioOutput->format());
}

void AudioProcessorQt::stop() {
	m_audioOutput->stop();
	m_device->close();
	AudioProcessor::stop();
}

bool AudioProcessorQt::start() {
	if (!input()) {
		LOG(QT, WARN) << tr("Can't start an audio processor without input");
		return false;
	}

	if (m_audioOutput->state() == QAudio::SuspendedState) {
		m_audioOutput->resume();
	} else {
		m_device->setBufferSamples(m_samples);
		m_device->setInput(input());
		m_device->setFormat(m_audioOutput->format());
		m_audioOutput->start(m_device);
	}
	return m_audioOutput->state() == QAudio::ActiveState && m_audioOutput->error() == QAudio::NoError;
}

void AudioProcessorQt::pause() {
	m_audioOutput->suspend();
}

void AudioProcessorQt::setBufferSamples(int samples) {
	m_samples = samples;
	if (m_device) {
		m_device->setBufferSamples(samples);
	}
}

void AudioProcessorQt::inputParametersChanged() {
	m_device->setFormat(m_audioOutput->format());
}

void AudioProcessorQt::requestSampleRate(unsigned rate) {
	m_sampleRate = rate;
	QAudioFormat format(m_audioOutput->format());
	format.setSampleRate(rate);
	m_device->setFormat(format);
}

unsigned AudioProcessorQt::sampleRate() const {
	return m_audioOutput->format().sampleRate();
}
