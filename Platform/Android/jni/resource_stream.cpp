//
//  resource_stream.h
//  ePub3
//
//  Created by Yonathan Teitelbaum (Mantano) on 2013-09-09.
//  Copyright (c) 2014 Readium Foundation and/or its licensees. All rights reserved.
//  
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY 
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
//  
//  Licensed under Gnu Affero General Public License Version 3 (provided, notwithstanding this notice, 
//  Readium Foundation reserves the right to license this material under a different separate license, 
//  and if you have done so, the terms of that separate license control and the following references 
//  to GPL do not apply).
//  
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU 
//  Affero General Public License as published by the Free Software Foundation, either version 3 of 
//  the License, or (at your option) any later version. You should have received a copy of the GNU 
//  Affero General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.


//#include <ePub3/utilities/resource_stream.h>
#include <string>
#include <memory>
#include <algorithm>    // std::min
#include <typeinfo>
#include <ePub3/utilities/byte_stream.h>
#include <ePub3/filter.h>
#include <ePub3/filter_chain_byte_stream_range.h>

#include "jni/jni.h"

#include "epub3.h"
#include "helpers.h"
#include "resource_stream.h"

ePub3::ByteStream* ResourceStream::getPtr() {
	ePub3::ByteStream* reader = _ptr.get();
	return reader;
}

std::size_t ResourceStream::getBufferSize() {
	return _bufferSize;
}

ResourceStream::~ResourceStream() {
	ePub3::ByteStream* reader = _ptr.get();
	reader->Close();
	_ptr.release();
}

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Internal constants
 **************************************************/
static const char *java_class_ResourceInputStream_name = "org/readium/sdk/android/util/ResourceInputStream";
static const char *java_class_IOException_name = "java/io/IOException";

static const char *java_method_ResourceInputStream_createResourceInputStream_name = "createResourceInputStream";
static const char *java_method_ResourceInputStream_createResourceInputStream_sign = "(J)Lorg/readium/sdk/android/util/ResourceInputStream;";

/*
 * Internal variables
 **************************************************/

/*
 * Cached classes, methods and fields IDs.
 */

static jni::Class java_class_ResourceInputStream;

static jni::Class java_class_IOException;

static jni::StaticMethod<jobject> java_method_ResourceInputStream_createResourceInputStream;

/*
 * Exported functions
 **************************************************/

/**
 * Initialize the cached java elements for ResourceInputStream class
 */
int onLoad_cacheJavaElements_ResourceInputStream(JNIEnv *env) {

	// Cache IRI class
	jclass java_class_ResourceInputStream_ = NULL;
	INIT_CLASS_RETVAL(java_class_ResourceInputStream_, java_class_ResourceInputStream_name, ONLOAD_ERROR);
	java_class_ResourceInputStream = jni::Class(env, java_class_ResourceInputStream_);

	jclass java_class_IOException_ = NULL;
	INIT_CLASS_RETVAL(java_class_IOException_, java_class_IOException_name, ONLOAD_ERROR);
	java_class_IOException = jni::Class(env, java_class_IOException_);

	// Cache IRI class methods
	java_method_ResourceInputStream_createResourceInputStream = jni::StaticMethod<jobject>(env, java_class_ResourceInputStream, java_method_ResourceInputStream_createResourceInputStream_name, java_method_ResourceInputStream_createResourceInputStream_sign);

	// Return JNI_VERSION for OK, if not one of the lines above already returned ONLOAD_ERROR
	return JNI_VERSION;
}

/**
 * Calls the java createResourceInputStream method of ResourceInputStream class
 */
jobject javaResourceInputStream_createResourceInputStream(JNIEnv *env, long streamPtr) {

	return java_method_ResourceInputStream_createResourceInputStream(env, (jlong) streamPtr);
}

static jbyteArray GetAllBytes(JNIEnv* env, jobject thiz, jlong nativePtr) {

	LOGD("JNI --- GetAllBytes ...\n");

	ResourceStream* stream = (ResourceStream*) nativePtr;

	auto bufferSize = stream->getBufferSize();
	jbyte tmpBuffer[bufferSize]; // stack

	std::size_t MAX = 1 * 1024 * 1024; // MB
	jbyte * fullBuffer = new jbyte[MAX]; // heap

	auto byteStream = stream->getPtr();

	std::size_t totalRead = 0;
	while (totalRead < MAX) {
		std::size_t bytesRead = byteStream->ReadBytes(&tmpBuffer, (std::size_t)bufferSize);

		if (bytesRead == 0) {
			break;
		}

		if ((totalRead + bytesRead) > MAX) {
			bytesRead = MAX - totalRead;
		}

		::memcpy(fullBuffer + totalRead, &tmpBuffer, bytesRead);

		totalRead += bytesRead;
	}

	LOGD("JNI --- GetAllBytes: %d\n", totalRead);

	jbyteArray jfullBuffer = env->NewByteArray((jsize)totalRead);
	env->SetByteArrayRegion(jfullBuffer, 0, (jsize)totalRead, fullBuffer);

	 //(*env)->GetArrayLength(jtmpBuffer)
	//env->DeleteLocalRef(jtmpBuffer); nope, JVM / Dalvik garbage collector

    delete [] fullBuffer;

	return jfullBuffer;
}

static jbyteArray GetBytes(JNIEnv* env, jobject thiz, jlong nativePtr, jlong dataLength) {

	LOGD("JNI --- GetBytes 1: %ld\n", (long)dataLength);

	ResourceStream* stream = (ResourceStream*) nativePtr;

	jbyte * tmpBuffer = new jbyte[dataLength]; // heap

	auto byteStream = stream->getPtr();
	std::size_t bytesRead = byteStream->ReadBytes(tmpBuffer, dataLength);

	LOGD("JNI --- GetBytes 2: %d\n", bytesRead);

	jbyteArray jtmpBuffer = env->NewByteArray((jsize)bytesRead);
	env->SetByteArrayRegion(jtmpBuffer, 0, (jsize)bytesRead, (jbyte *)tmpBuffer);

    delete [] tmpBuffer;

	return jtmpBuffer;
}

static jbyteArray GetBytesRange(JNIEnv* env, jobject thiz, jlong nativePtr, jlong offset, jlong length) {

	LOGD("JNI --- GetBytesRange 1: %ld\n", (long)length);

	ResourceStream* stream = (ResourceStream*) nativePtr;
	auto byteStream = stream->getPtr();
	ePub3::FilterChainByteStreamRange *rangeByteStream = dynamic_cast<ePub3::FilterChainByteStreamRange *>(byteStream);

	jbyte * tmpBuffer = new jbyte[length];

	std::size_t readBytes;

	if (rangeByteStream != nullptr) {
		LOGD("JNI --- GetBytesRange FilterChainByteStreamRange\n");
		ePub3::ByteRange range;
		range.Location(offset);
		range.Length(length);
		readBytes = rangeByteStream->ReadBytes(tmpBuffer, length, range);
	} else {
		ePub3::SeekableByteStream *seekableStream = dynamic_cast<ePub3::SeekableByteStream *>(byteStream);
		if (seekableStream != nullptr) {
			LOGD("JNI --- GetBytesRange SeekableByteStream\n");
			seekableStream->Seek(offset, std::ios::beg);
			readBytes = seekableStream->ReadBytes(tmpBuffer, length);
		} else {
			env->ThrowNew(java_class_IOException, "Seek operation not supported for this byte stream.");
			return NULL;
		}
	}

	LOGD("JNI --- GetBytesRange 2: %d\n", readBytes);

	jbyteArray jtmpBuffer = env->NewByteArray(readBytes);
	env->SetByteArrayRegion(jtmpBuffer, 0, readBytes, tmpBuffer);

    delete [] tmpBuffer;

	return jtmpBuffer;
}

/*
 * JNI functions
 **************************************************/

/*
 * Package: org.readium.sdk.android
 * Class: ResourceInputStream
 */

static void Skip(JNIEnv* env, jobject thiz, jlong nativePtr, jlong byteCount) {
	ResourceStream* stream = (ResourceStream*) nativePtr;
	auto byteStream = stream->getPtr();
	ePub3::SeekableByteStream *seekableStream = dynamic_cast<ePub3::SeekableByteStream*>(byteStream);

	if (seekableStream == nullptr) {
		// When the ByteStream is most likely a FilterChainByteStream:
		env->ThrowNew(java_class_IOException, "Skip operation is not supported for this byte stream. (it is most likely not a raw stream)");
		return;
	}

	seekableStream->Seek(byteCount, std::ios::cur);
}

JNIEXPORT void JNICALL Java_org_readium_sdk_android_util_ResourceInputStream_nativeSkip
		(JNIEnv* env, jobject thiz, jlong nativePtr, jlong byteCount) {

	Skip(env, thiz, nativePtr, byteCount);
}

JNIEXPORT void JNICALL Java_org_readium_sdk_android_util_ResourceInputStream_nativeReset
		(JNIEnv* env, jobject thiz, jlong nativePtr, jboolean ignoreMark) {

	ResourceStream* stream = (ResourceStream*) nativePtr;
	auto byteStream = stream->getPtr();
	ePub3::SeekableByteStream *seekableStream = dynamic_cast<ePub3::SeekableByteStream*>(byteStream);

	if (seekableStream == nullptr) {
		// When the ByteStream is most likely a FilterChainByteStream:
		env->ThrowNew(java_class_IOException, "Reset operation is not supported for this byte stream. (it is most likely not a raw stream)");
		return;
	}

	if (ignoreMark == JNI_TRUE) {
		seekableStream->Seek(0, std::ios::beg);
	} else {
		seekableStream->Seek(stream->markPosition, std::ios::beg);
	}
}

JNIEXPORT void JNICALL Java_org_readium_sdk_android_util_ResourceInputStream_nativeMark
		(JNIEnv* env, jobject thiz, jlong nativePtr) {

	ResourceStream* stream = (ResourceStream*) nativePtr;
	auto byteStream = stream->getPtr();
	ePub3::SeekableByteStream *seekableStream = dynamic_cast<ePub3::SeekableByteStream*>(byteStream);

	if (seekableStream == nullptr) {
		// When the ByteStream is most likely a FilterChainByteStream:
		env->ThrowNew(java_class_IOException, "Mark operation is not supported for this byte stream. (it is most likely not a raw stream)");
		return;
	}

	stream->markPosition = seekableStream->Position();
}

JNIEXPORT jlong JNICALL Java_org_readium_sdk_android_util_ResourceInputStream_nativeAvailable
		(JNIEnv* env, jobject thiz, jlong nativePtr) {

	ResourceStream* stream = (ResourceStream*) nativePtr;
	auto byteStream = stream->getPtr();
	return (jlong) byteStream->BytesAvailable();
}


JNIEXPORT jbyteArray JNICALL Java_org_readium_sdk_android_util_ResourceInputStream_nativeGetBytes
		(JNIEnv* env, jobject thiz, jlong nativePtr, jlong dataLength) {

	return GetBytes(env, thiz, nativePtr, dataLength);
}

JNIEXPORT jbyteArray JNICALL Java_org_readium_sdk_android_util_ResourceInputStream_nativeGetAllBytes
		(JNIEnv* env, jobject thiz, jlong nativePtr) {

	return GetAllBytes(env, thiz, nativePtr);
}

JNIEXPORT jbyteArray JNICALL Java_org_readium_sdk_android_util_ResourceInputStream_nativeGetRangeBytes
		(JNIEnv* env, jobject thiz, jlong nativePtr, jlong offset, jlong length) {

	return GetBytesRange(env, thiz, nativePtr, offset, length);
}

JNIEXPORT void JNICALL Java_org_readium_sdk_android_util_ResourceInputStream_nativeClose
		(JNIEnv* env, jobject thiz, jlong nativePtr) {
	ResourceStream* stream = (ResourceStream*) nativePtr;
	delete stream;
}

#ifdef __cplusplus
}
#endif
