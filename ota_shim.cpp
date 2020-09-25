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
 * @file ota_shim.cpp
 * @brief shim for dispatching UVE OTA ATSC playback
 */

#include "AampUtils.h"
#include "ota_shim.h"
#include "priv_aamp.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS

#include <core/core.h>
#include <websocket/websocket.h>

using namespace std;
using namespace WPEFramework;
#endif

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS

/*Need to be changed to org.rdk.MediaPlayer*/
//#define MEDIAPLAYER_CALLSIGN "org.rdk.MediaServicesMoc"
#define MEDIAPLAYER_CALLSIGN "org.rdk.MediaPlayer.1"
#define APP_ID "MainPlayer"

#define RDKSHELL_CALLSIGN "org.rdk.RDKShell.1"

void StreamAbstractionAAMP_OTA::onPlayerStatusHandler(const JsonObject& parameters) {
    std::string message;
    JsonObject playerData;
    parameters.ToString(message);
    PrivAAMPState state;
    std::string currState;

    /*To Do : Confirm that player name is same as APP_ID. The plugin documentation use "MainPlayer" as player name*/
    playerData = parameters[APP_ID].Object();
    AAMPLOG_TRACE( "[OTA_SHIM]%s Received event : message : %s ", __FUNCTION__, message.c_str());
    /*Detailed Event Data*/
    //AAMPLOG_INFO( "[OTA_SHIM]%s Received event : locator : %s ", __FUNCTION__, playerData["locator"].String().c_str());
    //AAMPLOG_INFO( "[OTA_SHIM]%s Received event : playerStatus : %s ", __FUNCTION__, playerData["playerStatus"].String().c_str());
    //AAMPLOG_INFO( "[OTA_SHIM]%s Received event : length : %s ", __FUNCTION__, playerData["length"].String().c_str());
    //AAMPLOG_INFO( "[OTA_SHIM]%s Received event : position : %s ", __FUNCTION__, playerData["position"].String().c_str());
    //AAMPLOG_INFO( "[OTA_SHIM]%s Received event : liveOffset : %s ", __FUNCTION__, playerData["liveOffset"].String().c_str());
    //AAMPLOG_INFO( "[OTA_SHIM]%s Received event : speed: %s ", __FUNCTION__, playerData["speed"].String().c_str());

    currState = playerData["playerStatus"].String();
    if(0 != prevState.compare(currState))
    {
        AAMPLOG_INFO( "[OTA_SHIM]%s State changed from %s to %s ", __FUNCTION__, prevState.c_str(), currState.c_str());
        prevState.clear();
        prevState = currState;
        if(0 == prevState.compare("IDLE"))
        {
            state = eSTATE_IDLE;
        }else if(0 == prevState.compare("ERROR"))
        {
            aamp->SendAnomalyEvent(ANOMALY_WARNING, "ATSC tune error");
            state = eSTATE_ERROR;
        }else if(0 == prevState.compare("PROCESSING"))
        {
            state = eSTATE_PREPARING;
        }else if(0 == prevState.compare("PLAYING"))
        {
            if(!tuned){
                aamp->SendTunedEvent();
                tuned = true;
            }
            state = eSTATE_PLAYING;
        }else if(0 == prevState.compare("DONE"))
        {
            if(tuned){
                tuned = false;
            }
            state = eSTATE_IDLE;
        }else
        {
            AAMPLOG_INFO( "[OTA_SHIM]%s Unsupported state change!", __FUNCTION__);
        }
        aamp->SetState(state);
    }
}
#endif

/**
 *   @brief  Initialize a newly created object.
 *   @note   To be implemented by sub classes
 *   @param  tuneType to set type of object.
 *   @retval true on success
 *   @retval false on failure
 */
AAMPStatusType StreamAbstractionAAMP_OTA::Init(TuneType tuneType)
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
    logprintf( "[OTA_SHIM]Inside %s CURL ACCESS", __FUNCTION__ );
    AAMPStatusType retval = eAAMPSTATUS_OK;
#else
    AAMPLOG_INFO( "[OTA_SHIM]Inside %s ", __FUNCTION__ );
    prevState = "IDLE";
    tuned = false;

    thunderAccessObj.ActivatePlugin(MEDIAPLAYER_CALLSIGN);
    std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> actualMethod = std::bind(&StreamAbstractionAAMP_OTA::onPlayerStatusHandler, this, std::placeholders::_1);

    thunderAccessObj.SubscribeEvent(_T("onPlayerStatus"), actualMethod);

    AAMPStatusType retval = eAAMPSTATUS_OK;

    //activate RDK Shell
    thunderRDKShellObj.ActivatePlugin(RDKSHELL_CALLSIGN);

#endif
    return retval;
}

/**
 * @brief StreamAbstractionAAMP_MPD Constructor
 * @param aamp pointer to PrivateInstanceAAMP object associated with player
 * @param seek_pos Seek position
 * @param rate playback rate
 */
StreamAbstractionAAMP_OTA::StreamAbstractionAAMP_OTA(class PrivateInstanceAAMP *aamp,double seek_pos, float rate): StreamAbstractionAAMP(aamp)
{ // STUB
}

/**
 * @brief StreamAbstractionAAMP_OTA Destructor
 */
StreamAbstractionAAMP_OTA::~StreamAbstractionAAMP_OTA()
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
        /*
        Request : {"jsonrpc":"2.0", "id":3, "method": "org.rdk.MediaPlayer.1.release", "params":{ "id":"MainPlayer", "tag" : "MyApp"} }
        Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
        */
        std::string id = "3";
        std:: string response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.release", "{\"id\":\"MainPlayer\",\"tag\" : \"MyApp\"}");
        logprintf( "StreamAbstractionAAMP_OTA:%s:%d response '%s'\n", __FUNCTION__, __LINE__, response.c_str());
#else
        JsonObject param;
        JsonObject result;
	param["id"] = APP_ID;
	param["tag"] = "MyApp";
        thunderAccessObj.InvokeJSONRPC("release", param, result);

	thunderAccessObj.UnSubscribeEvent(_T("onPlayerStatus"));
	AAMPLOG_INFO("[OTA_SHIM]StreamAbstractionAAMP_OTA Destructor called !! ");
#endif
}

/**
 *   @brief  Starts streaming.
 */
void StreamAbstractionAAMP_OTA::Start(void)
{
	std::string id = "3";
        std::string response;
	const char *display = getenv("WAYLAND_DISPLAY");
	std::string waylandDisplay;
	if( display )
	{
		waylandDisplay = display;
		logprintf( "WAYLAND_DISPLAY: '%s'\n", display );
	}
	else
	{
		logprintf( "WAYLAND_DISPLAY: NULL!\n" );
	}
	std::string url = aamp->GetManifestUrl();
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
        logprintf( "[OTA_SHIM]Inside %s CURL ACCESS\n", __FUNCTION__ );
	/*
	Request : {"jsonrpc": "2.0","id": 4,"method": "Controller.1.activate", "params": { "callsign": "org.rdk.MediaPlayer" }}
	Response : {"jsonrpc": "2.0","id": 4,"result": null}
	*/
	response = aamp_PostJsonRPC(id, "Controller.1.activate", "{\"callsign\":\"org.rdk.MediaPlayer\"}" );
        logprintf( "StreamAbstractionAAMP_OTA:%s:%d response '%s'\n", __FUNCTION__, __LINE__, response.c_str());
        response.clear();
	/*
	Request : {"jsonrpc":"2.0", "id":3, "method":"org.rdk.MediaPlayer.1.create", "params":{ "id" : "MainPlayer", "tag" : "MyApp"} }
	Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
	*/
	response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.create", "{\"id\":\"MainPlayer\",\"tag\":\"MyApp\"}");
        logprintf( "StreamAbstractionAAMP_OTA:%s:%d response '%s'\n", __FUNCTION__, __LINE__, response.c_str());
        response.clear();
	// inform (MediaRite) player instance on which wayland display it should draw the video. This MUST be set before load/play is called.
	/*
	Request : {"jsonrpc":"2.0", "id":3, "method":"org.rdk.MediaPlayer.1.setWaylandDisplay", "params":{"id" : "MainPlayer","display" : "westeros-123"} }
	Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true} }
	*/
	response = aamp_PostJsonRPC( id, "org.rdk.MediaPlayer.1.setWaylandDisplay", "{\"id\":\"MainPlayer\",\"display\":\"" + waylandDisplay + "\"}" );
        logprintf( "StreamAbstractionAAMP_OTA:%s:%d response '%s'\n", __FUNCTION__, __LINE__, response.c_str());
        response.clear();
	/*
	Request : {"jsonrpc":"2.0", "id":3, "method": "org.rdk.MediaPlayer.1.load", "params":{ "id":"MainPlayer", "url":"live://...", "autoplay": true} }
	Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
	*/
	response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.load","{\"id\":\"MainPlayer\",\"url\":\""+url+"\",\"autoplay\":true}" );
        logprintf( "StreamAbstractionAAMP_OTA:%s:%d response '%s'\n", __FUNCTION__, __LINE__, response.c_str());
        response.clear();
	/*
	Request : {"jsonrpc":"2.0", "id":3, "method": "org.rdk.MediaPlayer.1.play", "params":{ "id":"MainPlayer"} }
	Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
	*/
  
	// below play request harmless, but not needed, given use of autoplay above
	// response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.play", "{\"id\":\"MainPlayer\"}");
        // logprintf( "StreamAbstractionAAMP_OTA:%s:%d response '%s'\n", __FUNCTION__, __LINE__, response.c_str());

#else
	AAMPLOG_INFO( "[OTA_SHIM]Inside %s : url : %s ", __FUNCTION__ , url.c_str());
	JsonObject result;

	JsonObject createParam;
	createParam["id"] = APP_ID;
	createParam["tag"] = "MyApp";
        thunderAccessObj.InvokeJSONRPC("create", createParam, result);

	JsonObject displayParam;
	displayParam["id"] = APP_ID;
	displayParam["display"] = waylandDisplay;
        thunderAccessObj.InvokeJSONRPC("setWaylandDisplay", displayParam, result);

	JsonObject loadParam;
	loadParam["id"] = APP_ID;
	loadParam["url"] = url;
	loadParam["autoplay"] = true;
	thunderAccessObj.InvokeJSONRPC("load", loadParam, result);

	// below play request harmless, but not needed, given use of autoplay above
	//JsonObject playParam;
	//playParam["id"] = APP_ID;
        //thunderAccessObj.InvokeJSONRPC("play", playParam, result);
#endif
}

/**
*   @brief  Stops streaming.
*/
void StreamAbstractionAAMP_OTA::Stop(bool clearChannelData)
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
        /*
        Request : {"jsonrpc":"2.0", "id":3, "method": "org.rdk.MediaPlayer.1.stop", "params":{ "id":"MainPlayer"} }
        Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
        */
        std::string id = "3";
        std::string response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.stop", "{\"id\":\"MainPlayer\"}");
        logprintf( "StreamAbstractionAAMP_OTA:%s:%d response '%s'\n", __FUNCTION__, __LINE__, response.c_str());
#else
        JsonObject param;
        JsonObject result;
        param["id"] = APP_ID;
        thunderAccessObj.InvokeJSONRPC("stop", param, result);
#endif
}

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
bool StreamAbstractionAAMP_OTA::GetScreenResolution(int & screenWidth, int & screenHeight)
{
	 JsonObject param;
	 JsonObject result;
	 bool bRetVal = false;

	 if( thunderRDKShellObj.InvokeJSONRPC("getScreenResolution", param, result) )
	 {
		 screenWidth = result["w"].Number();
		 screenHeight = result["h"].Number();
		 AAMPLOG_INFO( "StreamAbstractionAAMP_OTA:%s:%d screenWidth:%d screenHeight:%d  ",__FUNCTION__, __LINE__,screenWidth, screenHeight);
		 bRetVal = true;
	 }
	 return bRetVal;
}
#endif

/**
 * @brief setVideoRectangle sets the position coordinates (x,y) & size (w,h)
 *
 * @param[in] x,y - position coordinates of video rectangle
 * @param[in] wxh - width & height of video rectangle
 */
void StreamAbstractionAAMP_OTA::SetVideoRectangle(int x, int y, int w, int h)
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
        /*
        Request : {"jsonrpc":"2.0", "id":3, "method": "org.rdk.MediaPlayer.1.setVideoRectangle", "params":{ "id":"MainPlayer", "x":0, "y":0, "w":1280, "h":720} }
        Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
        */
        std::string id = "3";
        std::string response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.setVideoRectangle", "{\"id\":\"MainPlayer\", \"x\":" + to_string(x) + ", \"y\":" + to_string(y) + ", \"w\":" + to_string(w) + ", \"h\":" + std::to_string(h) + "}");
        logprintf( "StreamAbstractionAAMP_OTA:%s:%d response '%s'\n", __FUNCTION__, __LINE__, response.c_str());
#else
        JsonObject param;
        JsonObject result;
        param["id"] = APP_ID;
        param["x"] = x;
        param["y"] = y;
        param["w"] = w;
        param["h"] = h;
        int screenWidth = 0;
        int screenHeight = 0;
        if(GetScreenResolution(screenWidth,screenHeight))
        {
		JsonObject meta;
		meta["resWidth"] = screenWidth;
		meta["resHeight"] = screenHeight;
		param["meta"] = meta;
        }

        thunderAccessObj.InvokeJSONRPC("setVideoRectangle", param, result);
#endif
}

/**
 * @brief Stub implementation
 */
void StreamAbstractionAAMP_OTA::DumpProfiles(void)
{ // STUB
}

/**
 * @brief Get output format of stream.
 *
 * @param[out]  primaryOutputFormat - format of primary track
 * @param[out]  audioOutputFormat - format of audio track
 */
void StreamAbstractionAAMP_OTA::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat)
{
    primaryOutputFormat = FORMAT_ISO_BMFF;
    audioOutputFormat = FORMAT_NONE;
}

/**
 *   @brief Return MediaTrack of requested type
 *
 *   @param[in]  type - track type
 *   @retval MediaTrack pointer.
 */
MediaTrack* StreamAbstractionAAMP_OTA::GetMediaTrack(TrackType type)
{ // STUB
    return NULL;
}

/**
 * @brief Get current stream position.
 *
 * @retval current position of stream.
 */
double StreamAbstractionAAMP_OTA::GetStreamPosition()
{ // STUB
    return 0.0;
}

/**
 *   @brief Get stream information of a profile from subclass.
 *
 *   @param[in]  idx - profile index.
 *   @retval stream information corresponding to index.
 */
StreamInfo* StreamAbstractionAAMP_OTA::GetStreamInfo(int idx)
{ // STUB
    return NULL;
}

/**
 *   @brief  Get PTS of first sample.
 *
 *   @retval PTS of first sample
 */
double StreamAbstractionAAMP_OTA::GetFirstPTS()
{ // STUB
    return 0.0;
}

double StreamAbstractionAAMP_OTA::GetBufferedDuration()
{ // STUB
	return -1.0;
}

bool StreamAbstractionAAMP_OTA::IsInitialCachingSupported()
{ // STUB
	return false;
}

/**
 * @brief Get index of profile corresponds to bandwidth
 * @param[in] bitrate Bitrate to lookup profile
 * @retval profile index
 */
int StreamAbstractionAAMP_OTA::GetBWIndex(long bitrate)
{ // STUB
    return 0;
}

/**
 * @brief To get the available video bitrates.
 * @ret available video bitrates
 */
std::vector<long> StreamAbstractionAAMP_OTA::GetVideoBitrates(void)
{ // STUB
    return std::vector<long>();
}

/*
* @brief Gets Max Bitrate avialable for current playback.
* @ret long MAX video bitrates
*/
long StreamAbstractionAAMP_OTA::GetMaxBitrate()
{ // STUB
    return 0;
}

/**
 * @brief To get the available audio bitrates.
 * @ret available audio bitrates
 */
std::vector<long> StreamAbstractionAAMP_OTA::GetAudioBitrates(void)
{ // STUB
    return std::vector<long>();
}

/**
*   @brief  Stops injecting fragments to StreamSink.
*/
void StreamAbstractionAAMP_OTA::StopInjection(void)
{ // STUB - discontinuity related
}

/**
*   @brief  Start injecting fragments to StreamSink.
*/
void StreamAbstractionAAMP_OTA::StartInjection(void)
{ // STUB - discontinuity related
}







