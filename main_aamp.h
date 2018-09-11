/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**
 * @file main_aamp.h
 * @brief public API for AAMP
 */

#ifndef MAINAAMP_H
#define MAINAAMP_H

#include <vector>
#include <string>

#include <stddef.h>

/*! \mainpage
 *
 * \section intro_sec Introduction
 *
 * See PlayerInstanceAAMP for libaamp public C++ API's
 *
 */

#ifndef STANDALONE_AAMP
extern "C"
{
	/**
	 *   @brief  Load aamp JS bindings.
	 *
	 *   @param[in]  context - JS Core context.
	 *   @param[in]  playerInstanceAAMP - AAMP instance. NULL creates new aamp instance.
	 */
    void aamp_LoadJS(void* context, void* playerInstanceAAMP);

	/**
	 *   @brief  Unload aamp JS bindings.
	 *
	 *   @param[in]  context - JS Core context.
	 */
    void aamp_UnloadJS(void* context);
}
#endif

/**
 * @enum AAMPEventType 
 * @brief
 */
typedef enum
{
	AAMP_EVENT_TUNED = 1, /** Tune success*/
	AAMP_EVENT_TUNE_FAILED, /** Tune failure*/
	AAMP_EVENT_SPEED_CHANGED, /** Speed changed internally*/
	AAMP_EVENT_EOS, /** End of stream*/
	AAMP_EVENT_PLAYLIST_INDEXED, /** Playlist downloaded and indexed*/
	AAMP_EVENT_PROGRESS, /** Progress event with stats. Sent every ~250ms while streaming */
	AAMP_EVENT_CC_HANDLE_RECEIVED, /** Sent when video decoder handle retrieved */
	AAMP_EVENT_JS_EVENT, /** generic event generated by JavaScript binding */
	AAMP_EVENT_VIDEO_METADATA, /**  meta-data of tuned channel*/
	AAMP_EVENT_ENTERING_LIVE, /** event when live point reached*/
	AAMP_EVENT_BITRATE_CHANGED, /** event when bitrate changes */
	AAMP_EVENT_TIMED_METADATA, /** event when subscribe tag parsed*/
	AAMP_EVENT_STATUS_CHANGED, /** event when player status changes */
	AAMP_MAX_NUM_EVENTS
} AAMPEventType;

/**
 * @enum AAMPTuneFailure
 * @brief
 */
typedef enum
{
	AAMP_TUNE_INIT_FAILED, /** Tune failure due to initialization error*/
	AAMP_TUNE_MANIFEST_REQ_FAILED, /** Tune failure caused by manifest fetch failure*/
	AAMP_TUNE_AUTHORISATION_FAILURE, /** Not authorised to view the content*/
	AAMP_TUNE_FRAGMENT_DOWNLOAD_FAILURE, /**  When fragment download fails for 5 consecutive fragments*/
	AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, /** Unable to download init fragment*/
	AAMP_TUNE_UNTRACKED_DRM_ERROR, /**  DRM error*/
	AAMP_TUNE_DRM_INIT_FAILED, /** DRM initialization failure */
	AAMP_TUNE_DRM_DATA_BIND_FAILED, /** InitData binding with DRM failed */
	AAMP_TUNE_DRM_CHALLENGE_FAILED, /** DRM key request challenge generation failed */
	AAMP_TUNE_LICENCE_TIMEOUT, /** DRM license request timeout */
	AAMP_TUNE_LICENCE_REQUEST_FAILED, /** DRM license got invalid response */
	AAMP_TUNE_INVALID_DRM_KEY, /** DRM reporting invalid license key */
	AAMP_TUNE_UNSUPPORTED_STREAM_TYPE, /** Unsupported stream type */
	AAMP_TUNE_FAILED_TO_GET_KEYID, /** Failed to parse key id from init data*/
	AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN, /** Failed to get session token from AuthService*/
	AAMP_TUNE_CORRUPT_DRM_DATA, /** DRM failure due to corrupt drm data, self heal might clear further errors*/
	AAMP_TUNE_DRM_DECRYPT_FAILED, /** DRM Decryption Failed for Fragments */
	AAMP_TUNE_GST_PIPELINE_ERROR, /** Playback failure due to error from GStreamer pipeline or associated plugins */
#ifdef AAMP_JS_PP_STALL_DETECTOR_ENABLED
	AAMP_TUNE_PLAYBACK_STALLED, /** Playback was stalled due to valid fragments not available in playlist */
#endif
	AAMP_TUNE_CONTENT_NOT_FOUND, /** The resource was not found at the URL provided (HTTP 404) */
	AAMP_TUNE_DRM_KEY_UPDATE_FAILED, /**Failed to process DRM key, see the error code returned from Update() for more info */
	AAMP_TUNE_FAILURE_UNKNOWN /**  Unknown failure */
}AAMPTuneFailure;

/**
 * @struct TuneFailureMap 
 * @brief Holds aamp tune failure code and corresponding application error code and description
 */
struct TuneFailureMap
{
        AAMPTuneFailure tuneFailure;
        int code;
        const char* description;
};

/**
 * @enum PrivAAMPState
 
 * @brief  Mapping all required status codes based on JS player requirement. These requirements may be
 * forced by psdk player.AAMP may not use all the statuses mentioned below:
 * Mainly required statuses - idle, initializing, initialized, preparing, prepared, playing, paused, seek, complete and error
 */
typedef enum
{
	eSTATE_IDLE,         /** 0  - Player is idle */

	eSTATE_INITIALIZING, /** 1  - Player is initializing a particular content */

	eSTATE_INITIALIZED,  /** 2  - Player has initialized for a content successfully */

	eSTATE_PREPARING,    /** 3  - Player is loading all associated resources */

	eSTATE_PREPARED,     /** 4  - Player has loaded all associated resources successfully */

	eSTATE_BUFFERING,    /** 5  - Player is in buffering state */

	eSTATE_PAUSED,       /** 6  - Playback is paused */

	eSTATE_SEEKING,      /** 7  - Seek is in progress */

	eSTATE_PLAYING,      /** 8  - Playback is in progress */

	eSTATE_STOPPING,     /** 9  - Player is stopping the playback */

	eSTATE_STOPPED,      /** 10 - Player has stopped playback successfully */

	eSTATE_COMPLETE,     /** 11 - Playback completed */

	eSTATE_ERROR,        /** 12 - Error encountered and playback stopped */

	eSTATE_RELEASED      /** 13 - Player has released all resources for playback */

} PrivAAMPState;

#define MAX_LANGUAGE_COUNT 4
#define MAX_LANGUAGE_TAG_LENGTH 4
#define MAX_ERROR_DESCRIPTION_LENGTH 128
#define MAX_BITRATE_COUNT 10

/**
 * @struct AAMPEvent
 * @brief Event structure
 */
struct AAMPEvent
{
	AAMPEventType type;

	union
	{
		/**
		 * @struct progress
		 * @brief holds progress state
		 */
		struct
		{
			double durationMiliseconds; // current size of time shift buffer
			double positionMiliseconds; // current play/pause position relative to tune time - starts at zero)
			float playbackSpeed; // current trick speed (1.0 for normal play rate)
			double startMiliseconds; // time shift buffer start position (relative to tune time - starts at zero)
			double endMiliseconds;  // time shift buffer end position (relative to tune time - starts at zero)
		} progress;

		/**
		 * @struct speedChanged
		 * @brief holds speed state
		 */
		struct
		{
			float rate;
		} speedChanged;

		/**
		 * @struct bitrateChanged
		 * @brief holds bitrate information
		 */
		struct
		{
			int time;
			int bitrate;
			char description[128];  
			int width;
			int height;
		} bitrateChanged;

		/**
		 * @struct metadata
		 * @brief stream metadata information
		 */
		struct
		{
			long durationMiliseconds;
			int languageCount;
			char languages[MAX_LANGUAGE_COUNT][MAX_LANGUAGE_TAG_LENGTH];
			int bitrateCount;
			long bitrates[MAX_BITRATE_COUNT];
			int width;
			int height;
			bool hasDrm;
		} metadata;

		/**
		 * @struct ccHandle
		 * @brief contains cc handle
		 */
		struct
		{
			unsigned long handle;
		} ccHandle;

		/**
		 * @struct timedMetadata
		 * @brief holds timed meta data
		 */
		struct
		{
			const char* szName;
			double timeMilliseconds; // current play/pause position relative to tune time - starts at zero)
			const char* szContent;
		} timedMetadata;

		/**
		 * @struct jsEvent
		 * @brief custom js event
		 */
		struct
		{
			const char* szEventType;
			void*  jsObject;
		} jsEvent;

		/**
		 * @struct mediaError
		 * @brief Media error event
		 */
		struct
		{
			AAMPTuneFailure failure;
			int code;
			char description[MAX_ERROR_DESCRIPTION_LENGTH];
		} mediaError;

		/**
		 * @struct stateChanged
		 * @brief state changed event, holds new state
		 */
		struct
		{
			PrivAAMPState state;
		} stateChanged;

	} data;

	/**
	 * @brief AAMPEvent Constructor
	 */
	AAMPEvent()
	{
	}

	/**
	 * @brief AAMPEvent Constructor
	 */
	AAMPEvent(AAMPEventType t) : type(t)
	{
	}
};

/**
 * @class AAMPEventListener
 * @brief Listener for AAMP events
 */
class AAMPEventListener
{
public:

	/**
	 * @brief Invoked on event
	 * @param event AAMP event
	 */
	virtual void Event(const AAMPEvent& event) = 0;

	/**
	 * @brief Destructor
	 */
	virtual ~AAMPEventListener(){};
};

/**
 * @enum MediaType 
 * @brief Media types
 */
enum MediaType
{
	eMEDIATYPE_VIDEO,
	eMEDIATYPE_AUDIO,
	eMEDIATYPE_MANIFEST,
	eMEDIATYPE_LICENCE
};

/**
 * @enum StreamOutputFormat
 * @brief Output stream format
 */
enum StreamOutputFormat
{
	FORMAT_INVALID,
	FORMAT_MPEGTS,
	FORMAT_ISO_BMFF,
	FORMAT_AUDIO_ES_AAC,
	FORMAT_AUDIO_ES_AC3,
	FORMAT_AUDIO_ES_EC3,
	FORMAT_VIDEO_ES_H264,
	FORMAT_VIDEO_ES_HEVC,
	FORMAT_VIDEO_ES_MPEG2,
	FORMAT_NONE
};

/**
 * @enum VideoZoomMode 
 * @brief Modes of video zoom
 */
enum VideoZoomMode
{
	VIDEO_ZOOM_FULL, /** Zoom to full screen*/
	VIDEO_ZOOM_NONE /** Reset zoom*/
};

/**
 * @class StreamSink
 * @brief GStreamer Abstraction - with implementations in AAMPGstPlayer and gstaamp plugin
 */
class StreamSink
{
public:

	/**
	 * @brief Configure output formats
	 * @param format Main/video output format
	 * @param audioFormat Secondary/audio output format
	 */
	virtual void Configure(StreamOutputFormat format, StreamOutputFormat audioFormat)=0;

	/**
	 * @brief Gives data to sink.
	 * @param mediaType type of media
	 * @param ptr buffer - caller responsible of freeing memory
	 * @param len buffer length
	 * @param fpts pts in seconds
	 * @param fdts dts in seconds
	 * @param duration duration of buffer
	 */
	virtual void Send( MediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double duration)= 0;

	/**
	 * @brief Gives data to sink.
	 * @param mediaType type of media
	 * @param buffer - data - ownership is taken by the sink.
	 * @param fpts pts in seconds
	 * @param fdts dts in seconds
	 * @param duration duration of buffer
	 */
	virtual void Send( MediaType mediaType, struct GrowableBuffer* buffer, double fpts, double fdts, double duration)= 0;

	/**
	 * @brief Notifies EOS to sink
	 * @param mediaType Type of media
	 */
	virtual void EndOfStreamReached(MediaType mediaType){}

	/**
	 * @brief Starts streaming
	 */
	virtual void Stream(void){}

	/**
	 * @brief Stop streaming
	 * @param keepLastFrame true to keep last frame on stop
	 */
	virtual void Stop(bool keepLastFrame){}

	/**
	 * @brief Dump sink status for debugging purpose
	 */
	virtual void DumpStatus(void){}

	/**
	 * @brief Flushes sink
	 * @param position new seek position
	 * @param rate playback rate
	 */
	virtual void Flush(double position = 0, float rate = 1.0){}

	/**
	 * @brief Selects audio in case of multiple audio
	 * @param index index of audio to be selected
	 */
	virtual void SelectAudio(int index){}

	/**
	 * @brief Pause the sink
	 * @param pause true to pause, false to resume
	 */
	virtual void Pause(bool pause){}

	/**
	 * @brief Get sink position in milliseconds
	 * @retval sink position
	 */
	virtual long GetPositionMilliseconds(void){ return 0; };

	/**
	 * @brief Get decoder handle of the sink
	 * @retval decoder handle
	 */
	virtual unsigned long getCCDecoderHandle(void) { return 0; };

	/**
	 * @brief Set video display rectangle co-ordinates
	 * @param[in] x x co-ordinate of display rectangle
	 * @param[in] y y co-ordinate of display rectangle
	 * @param[in] w width of display rectangle
	 * @param[in] h height of display rectangle
	 */
	virtual void SetVideoRectangle(int x, int y, int w, int h){};

	/**
	 * @brief Set video zoom
	 * @param[in] zoom zoom setting to be set
	 */
	virtual void SetVideoZoom(VideoZoomMode zoom){};

	/**
	 * @brief Set video mute
	 * @param[in] muted true to mute video otherwise false
	 */
	virtual void SetVideoMute(bool muted){};

	/**
	 * @brief Set audio volume
	 * @param[in] volume audio volume value (0-100)
	 */
	virtual void SetAudioVolume(int volume){};

	/**
	 * @brief Destructor
	 */
	virtual ~StreamSink(){};

	/**
	 * @brief Process discontinuity for a stream type
	 * @param mediaType stream type
	 * @retval true if discontinuity processed
	 */
	virtual bool Discontinuity( MediaType mediaType) = 0;


	/**
	 * @brief Check if cache empty for a media type
	 * @param[in] mediaType stream type
	 * @retval true if cache empty
	 */
	virtual bool IsCacheEmpty(MediaType mediaType){ return true; };

	/**
	 * @brief Notify sink on completing fragment caching
	 */
	virtual void NotifyFragmentCachingComplete(){};

	/**
	 * @brief Get video display's width and height
	 * @param[out] w video width
	 * @param[out] h video height
	 */
	virtual void GetVideoSize(int &w, int &h){};

	/**
	 * @brief Queue protection event in sink
	 * @param[in] protSystemId Protection system ID
	 * @param[in] ptr protection data
	 * @param[in] len length of protection data
	 */
	virtual void QueueProtectionEvent(const char *protSystemId, const void *ptr, size_t len) {};

	/**
	 * @brief Clear protection event in sink
	 */
	virtual void ClearProtectionEvent() {};
};


/**
 * @class PlayerInstanceAAMP
 * @brief Public interface of AAMP library
 */
class PlayerInstanceAAMP
{
public:
	/**
	 *   @brief Constructor.
	 *
	 *   @param  streamSink - custom stream sink, NULL for default.
	 */
	PlayerInstanceAAMP(StreamSink* streamSink = NULL);

	/**
	 *   @brief Destructor.
	 */
	~PlayerInstanceAAMP();

	/**
	 *   @brief Tune to a URL.
	 *
	 *   @param  mainManifestUrl - HTTP/HTTPS url to be played.
	 */
	void Tune(const char *mainManifestUrl, const char *contentType=NULL);

	/**
	 *   @brief Stop playback and release resources.
	 *
	 */
	void Stop(void);

	/**
	 *   @brief Set playback rate.
	 *
	 *   @param  rate - Rate of playback.
	 *   @param  overshootcorrection - overshoot correction in milliseconds.
	 */
	void SetRate(float rate, int overshootcorrection=0);

	/**
	 *   @brief Seek to a time.
	 *
	 *   @param  secondsRelativeToTuneTime - Seek position for VOD,
	 *           relative position from first tune command.
	 */
	void Seek(double secondsRelativeToTuneTime);

	/**
	 *   @brief Seek to live point.
	 *
	 */
	void SeekToLive(void);

	/**
	 *   @brief Seek to a time and playback with a new rate.
	 *
	 *   @param  rate - Rate of playback.
	 *   @param  secondsRelativeToTuneTime - Seek position for VOD,
	 *           relative position from first tune command.
	 */
	void SetRateAndSeek(float rate, double secondsRelativeToTuneTime);

	/**
	 *   @brief Register event handler.
	 *
	 *   @param  eventListener - pointer to implementation of AAMPEventListener to receive events.
	 */
	void RegisterEvents(AAMPEventListener* eventListener);

	/**
	 *   @brief Set video rectangle.
	 *
	 *   @param  x - horizontal start position.
	 *   @param  y - vertical start position.
	 *   @param  w - width.
	 *   @param  h - height.
	 */
	void SetVideoRectangle(int x, int y, int w, int h);

	/**
	 *   @brief Set video zoom.
	 *
	 *   @param  zoom - zoom mode.
	 */
	void SetVideoZoom(VideoZoomMode zoom);

	/**
	 *   @brief Enable/ Disable Video.
	 *
	 *   @param  muted - true to disable video, false to enable video.
	 */
	void SetVideoMute(bool muted);

	/**
	 *   @brief Set Audio Volume.
	 *
	 *   @param  volume - Minimum 0, maximum 100.
	 */
	void SetAudioVolume(int volume);

	/**
	 *   @brief Set Audio language.
	 *
	 *   @param  language - Language of audio track.
	 */
	void SetLanguage(const char* language);

	/**
	 *   @brief Set array of subscribed tags.
	 *
	 *   @param  subscribedTags - Array of subscribed tags.
	 */
	void SetSubscribedTags(std::vector<std::string> subscribedTags);


	/**
	 *   @brief Load AAMP JS object in the specified JS context.
	 *
	 *   @param  context - JS context.
	 */
	void LoadJS(void* context);

	/**
	 *   @brief Load AAMP JS object in the specified JS context.
	 *
	 *   @param  context - JS context.
	 */
	void UnloadJS(void* context);

	// Support JS event target interface

	/**
	 *   @brief Support multiple listeners for multiple event type
	 *
	 *   @param  eventType - type of event.
	 *   @param  eventListener - listener for the eventType.
	 */
	void AddEventListener(AAMPEventType eventType, AAMPEventListener* eventListener);

	/**
	 *   @brief Remove event listener for eventType.
	 *
	 *   @param  eventType - type of event.
	 *   @param  eventListener - listener to be removed for the eventType.
	 */
	void RemoveEventListener(AAMPEventType eventType, AAMPEventListener* eventListener);

	/**
	 *   @brief To check playlist type.
	 *
	 *   @return bool - True if live content, false otherwise
	 */
	bool IsLive();

	// Ad insertion support

	/**
	 *   @brief Schedule insertion of ad at given position.
	 *
	 *   @param  url - HTTP/HTTPS url of the ad
	 *   @param  positionSeconds - position at which ad shall be inserted
	 */
	void InsertAd(const char *url, double positionSeconds);

	/**
	 *   @brief Get current audio language.
	 *
	 *   @return char* - current audio language
	 */
	char* GetCurrentAudioLanguage();

	/**
	 *   @brief Add/Remove a custom HTTP header and value.
	 *
	 *   @param  headerName - Name of custom HTTP header
	 *   @param  headerValue - Value to be pased along with HTTP header.
	 */
	void AddCustomHTTPHeader(std::string headerName, std::vector<std::string> headerValue);

	/**
	 *   @brief Set License Server URL.
	 *
	 *   @param  url - URL of the server to be used for license requests
	 */
	void SetLicenseServerURL(char *url);

	/**
	 *   @brief Indicates if session token has to be used with license request or not.
	 *
	 *   @param  isAnonymous - True if session token should be blank and false otherwise.
	 */
	void SetAnonymousRequest(bool isAnonymous);

	/**
	 *   @brief Set VOD Trickplay FPS.
	 *
	 *   @param  vodTrickplayFPS - FPS to be used for VOD Trickplay
	 */
	void SetVODTrickplayFPS(int vodTrickplayFPS);

	/**
	 *   @brief Set Linear Trickplay FPS.
	 *
	 *   @param  linearTrickplayFPS - FPS to be used for Linear Trickplay
	 */
	void SetLinearTrickplayFPS(int linearTrickplayFPS);

	/**
	 *   @brief To set the error code to be used for playback stalled error.
	 *
	 *   @param  errorCode - error code for playback stall errors.
	 */
	void SetStallErrorCode(int errorCode);

	/**
	 *   @brief To set the timeout value to be used for playback stall detection.
	 *
	 *   @param  timeoutMS - timeout in milliseconds for playback stall detection.
	 */
	void SetStallTimeout(int timeoutMS);

	/**
	 *   @brief To set the Playback Position reporting interval.
	 *
	 *   @param  reportIntervalMS - playback reporting interval in milliseconds.
	 */
	void SetReportInterval(int reportIntervalMS);

	class PrivateInstanceAAMP *aamp; ///private aamp instance associated with this
private:
	StreamSink* mInternalStreamSink;
	void* mJSBinding_DL;
};

#endif // MAINAAMP_H
