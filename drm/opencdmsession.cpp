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

/* Playready Session management
 *
 */
#include "config.h"
#include "opencdmsession.h"
#include <gst/gst.h>
#include <assert.h>
#include <iostream>
#include <sstream>
#include <string>
#include <string.h>
#include <vector>
#include <sys/utsname.h>
#include "priv_aamp.h"

#include <sys/time.h>
#define USEC_PER_SEC   1000000
static inline uint64_t GetCurrentTimeStampInUSec()
{
	   struct timeval  timeStamp;
	   uint64_t        retVal = 0;

	   gettimeofday(&timeStamp, NULL);

	   // Convert timestamp to Micro Seconds
	   retVal = (uint64_t)(((uint64_t)timeStamp.tv_sec * USEC_PER_SEC) + timeStamp.tv_usec);

	   return retVal;
}
static inline uint64_t GetCurrentTimeStampInMSec()
{
	   return GetCurrentTimeStampInUSec() / 1000;
}

#define LOG_DECRYPT_STATS 1
#define DECRYPT_AVG_TIME_THRESHOLD 5.0 //5 milliseconds 
#ifdef LOG_DECRYPT_STATS
#define MAX_THREADS 10
#define INTERVAL 120

/**
 * @struct DecryptStats
 * @brief Holds decryption profile stats
 */
struct DecryptStats
{
    uint64_t    nBytesInterval;
    uint64_t    nTimeInterval;
    uint64_t    nBytesTotal;
    uint64_t    nTimeTotal;
    uint64_t    nCallsTotal;
    pthread_t   threadID;

};
#endif // LOG_DECRYPT_STATS
#define SEC_SIZE size_t
void LogPerformanceExt(const char* strFunc, uint64_t msStart, uint64_t msEnd, SEC_SIZE nDataSize)
{
    bool        bThreshold  = false;
    uint64_t    delta       = msEnd - msStart;
    uint32_t    nRateMin    = 1000;     // Bytes/ms
    uint32_t    nRestart    = 5;
    uint32_t    nDataMin    = 1000;
    uint32_t    nTimeMin    = 5; // Can not be < 1 to protect against divide by 0 error

#ifdef LOG_DECRYPT_STATS
    {
        static DecryptStats stats[MAX_THREADS] = { 0 };
        int idx = 0;
        while(idx < MAX_THREADS) {
            if(stats[idx].threadID == pthread_self()) {
                break;
            }
            idx++;
        }
        if(idx == MAX_THREADS) {
            // new thread
            idx = 0;
            while(idx < MAX_THREADS) {
                if(stats[idx].threadID == 0) {
                    // empty slot
                    stats[idx].threadID = pthread_self();
                    break;
                }
                idx++;
            }
        }
        if(idx == MAX_THREADS) {
            printf("%s >>>>>>>> All slots allocated!!!, idx = %d, clearing the array.\n", __FUNCTION__, idx);
            memset(stats, 0, sizeof(DecryptStats) * MAX_THREADS);
            return;
        }

        if(nDataSize > 0 ) {
            stats[idx].nBytesInterval     += (uint64_t)nDataSize;
            stats[idx].nTimeInterval      += delta;
            stats[idx].nCallsTotal++;

            if(stats[idx].nCallsTotal % INTERVAL == 0) {
               stats[idx].nBytesTotal += stats[idx].nBytesInterval;
               stats[idx].nTimeTotal += stats[idx].nTimeInterval;
               double avgTime = (double)stats[idx].nTimeTotal/(double)stats[idx].nCallsTotal;
               if(avgTime >= DECRYPT_AVG_TIME_THRESHOLD) {
                  logprintf("%s >>>>>>>> Thread ID %X (%d) Avg Time %0.2llf ms, Avg Bytes %llu  calls (%llu) Interval avg time %0.2llf, Interval avg bytes %llu",
                     strFunc, stats[idx].threadID, idx, avgTime, stats[idx].nBytesTotal/stats[idx].nCallsTotal,
                     stats[idx].nCallsTotal, (double)stats[idx].nTimeInterval/(double)INTERVAL,
                     stats[idx].nBytesInterval/INTERVAL);
               }
               stats[idx].nBytesInterval = 0;
               stats[idx].nTimeInterval = 0;

            }
        }
    }
#endif //LOG_DECRYPT_STATS
}

#ifdef USE_SAGE_SVP
#include "b_secbuf.h"

struct Rpc_Secbuf_Info {
	uint8_t *ptr;
	uint32_t type;
	size_t   size;
	void    *token;
};
#endif

//The following flag is used to use old or new(wpeframework) OpenCDM implementation
#define USE_NEW_OPENCDM 1

AAMPOCDMSession::AAMPOCDMSession(const string& keySystem) :
		AampDrmSession(keySystem),
		m_eKeyState(KEY_INIT), 
		m_pOutputProtection(NULL),
		m_pOpencdm(NULL),
		m_pOpencdmDecrypt(NULL),
		decryptMutex(),
		m_sessionID()
{

	pthread_mutex_init(&decryptMutex,NULL);

	initAampDRMSession();

	// Get output protection pointer
	m_pOutputProtection = AampOutputProtection::GetAampOutputProcectionInstance();

}

void AAMPOCDMSession::initAampDRMSession()
{

	if (m_pOpencdm == NULL) {
		m_pOpencdm = new media::OpenCdm();
	}

	m_pOpencdm->SelectKeySystem(m_keySystem);

}

void AAMPOCDMSession::generateAampDRMSession(const uint8_t *f_pbInitData,
		uint32_t f_cbInitData)
{

	pthread_mutex_lock(&decryptMutex);
#if USE_NEW_OPENCDM
	m_sessionID = m_pOpencdm->CreateSession("video/mp4", const_cast<unsigned char*>(f_pbInitData), f_cbInitData, media::OpenCdm::Temporary);
	logprintf("generateAampDRMSession :: sessionId : %s ", m_sessionID.c_str());
	if(m_sessionID.empty())	{
		m_eKeyState = KEY_ERROR_EMPTY_SESSION_ID;
	}
#else
	std::string sessionId;
	m_pOpencdm->CreateSession("video/mp4", const_cast<unsigned char*>(f_pbInitData), f_cbInitData, sessionId);
	logprintf("generateAampDRMSession :: sessionId : %s ", sessionId.c_str());
	if(sessionId.empty())	{
		m_eKeyState = KEY_ERROR_EMPTY_SESSION_ID;
	}
#endif
	pthread_mutex_unlock(&decryptMutex);
}

AAMPOCDMSession::~AAMPOCDMSession()
{
	logprintf("[HHH]OCDMSession destructor called! keySystem %s", m_keySystem.c_str());
	clearDecryptContext();

	pthread_mutex_destroy(&decryptMutex);
#if USE_NEW_OPENCDM
	if(m_pOpencdmDecrypt)
	{
		m_pOpencdmDecrypt->Close();
		delete m_pOpencdmDecrypt;
		m_pOpencdmDecrypt = nullptr;
	}
#endif

	if(m_pOpencdm) 
	{
#if USE_NEW_OPENCDM<1
		m_pOpencdm->ReleaseMem();
#endif
		m_pOpencdm->Close();
		delete m_pOpencdm;
		m_pOpencdm = nullptr;
	}
	m_eKeyState = KEY_CLOSED;

	if(m_pOutputProtection)
	{
		m_pOutputProtection->Release();
	}
}


DrmData * AAMPOCDMSession::aampGenerateKeyRequest(string& destinationURL, uint32_t timeout)
{
	DrmData * result = NULL;

	std::string challenge;
	int challengeLength = 0;
	pthread_mutex_lock(&decryptMutex);

#if USE_NEW_OPENCDM
	unsigned char temporaryUrl[1024] = {'\0'};
	uint16_t  destinationUrlLength = sizeof(temporaryUrl);

	m_pOpencdm->GetKeyMessage(challenge, temporaryUrl, destinationUrlLength);
	if (challenge.empty() || !destinationUrlLength) {
		m_eKeyState = KEY_ERROR;
		logprintf("aampGenerateKeyRequest :: challenge or URL is empty. ");
		pthread_mutex_unlock(&decryptMutex);
		return result;
	}


	std::string delimiter (":Type:");
	std::string requestType (challenge.substr(0, challenge.find(delimiter)));
	//logprintf("keymessage is %s", challenge.c_str());
	//logprintf("delimiter is %s", delimiter.c_str());
	//logprintf("requestType is %s", requestType.c_str());
	 
	if ( (requestType.size() != 0) && (requestType.size() !=  challenge.size()) ) {
		challenge.erase(0, challenge.find(delimiter) + delimiter.length());
	}

	result = new DrmData(reinterpret_cast<unsigned char*>(const_cast<char*>(challenge.c_str())), challenge.length());
	destinationURL.assign(const_cast<char*>(reinterpret_cast<char*>(temporaryUrl)));
	logprintf("destination url is %s", destinationURL.c_str());
#else
	unsigned char temporaryUrl[1024] = {'\0'};
	int destinationUrlLength = 0;

	m_pOpencdm->GetKeyMessage(challenge, &challengeLength, 
		temporaryUrl, &destinationUrlLength);

	if (!challengeLength || !destinationUrlLength) {
		m_eKeyState = KEY_ERROR;
		logprintf("aampGenerateKeyRequest :: challenge or URL is empty. ");
		pthread_mutex_unlock(&decryptMutex);
		return result;
	}

	result = new DrmData(reinterpret_cast<unsigned char*>(const_cast<char*>(challenge.c_str())), challengeLength);
	destinationURL.assign(const_cast<char*>(reinterpret_cast<char*>(temporaryUrl)));
	logprintf("destin url is %s", destinationURL.c_str());
#endif	
	m_eKeyState = KEY_PENDING;

	pthread_mutex_unlock(&decryptMutex);
	return result;
}


int AAMPOCDMSession::aampDRMProcessKey(DrmData* key, uint32_t timeout)
{
	int retvalue = -1;

#ifdef TRACE_LOG
	cout << "aampDRMProcessKey :: Playready Update" << endl;
#endif
	pthread_mutex_lock(&decryptMutex);
	std::string responseMessage;

	media::OpenCdm::KeyStatus keyStatus = media::OpenCdm::KeyStatus::InternalError;
	const uint8_t* keyMessage = key ? key->getData() : nullptr;
	const uint16_t keyMessageLength = key ? key->getDataLength() : 0;

	if (keyMessage)
	{
		keyStatus = m_pOpencdm->Update(keyMessage, keyMessageLength, responseMessage);
	}
	else
	{
		keyStatus = m_pOpencdm->Status();
	}

	retvalue = (int)keyStatus;
	if (keyStatus == media::OpenCdm::KeyStatus::Usable) 
	{
		logprintf("processKey: Key Usable!");
	}
	else if(keyStatus == media::OpenCdm::KeyStatus::HWError)
	{
		// BCOM-3537 - SAGE Hang .. Need to restart the wpecdmi process and then self kill player to recover
		AAMPLOG_WARN("processKey: Update() returned HWError.Restarting process...");
		int systemResult = -1;
		// In Release another process handles opencdm which needs to be restarts .In Sprint this process is not available.
		// So check if process exists before killing it .
		systemResult = system("pgrep WPEcdmi");
		if(systemResult == 0)
		{
			systemResult = system("pkill -9 WPEcdmi");
			if(systemResult != 0)
			{
				AAMPLOG_WARN("Unable to shutdown WPEcdmi process.%d", systemResult);
			}
		}
		else
		{
			// check for WPEFramework process
			systemResult = system("pgrep WPEFramework");
			if(systemResult == 0)
			{
				systemResult = system("pkill -9 WPEFramework");
				if(systemResult != 0)
				{
					AAMPLOG_WARN("Unable to shutdown WPEFramework process.%d", systemResult);
				}
			}
		}

		// wait for 5sec for all the logs to be flushed
		sleep(5);
		// Now kill self
		pid_t pid = getpid();
		syscall(__NR_tgkill, pid, pid, SIGKILL);
	}
	else 
	{
		if(keyStatus == media::OpenCdm::KeyStatus::OutputRestricted)
		{	
			AAMPLOG_WARN("processKey: Update() Output restricted keystatus: %d\n", (int) keyStatus);
			retvalue = HDCP_OUTPUT_PROTECTION_FAILURE;
		}
		else if(keyStatus == media::OpenCdm::KeyStatus::OutputRestrictedHDCP22)
		{
			AAMPLOG_WARN("processKey: Update() Output Compliance error keystatus: %d\n", (int) keyStatus);
			retvalue = HDCP_COMPLIANCE_CHECK_FAILURE;
		}
		else
		{
			AAMPLOG_WARN("processKey: Update() returned keystatus: %d\n", (int) keyStatus);
		}
		m_eKeyState = KEY_ERROR;
		pthread_mutex_unlock(&decryptMutex);
		return retvalue;
	}

	m_eKeyState = KEY_READY;
#if USE_NEW_OPENCDM
	m_pOpencdmDecrypt = new media::OpenCdm(m_sessionID);
#endif
	pthread_mutex_unlock(&decryptMutex);
	return retvalue;
}

int AAMPOCDMSession::decrypt(const uint8_t *f_pbIV, uint32_t f_cbIV,
		const uint8_t *payloadData, uint32_t payloadDataSize, uint8_t **ppOpaqueData)
{
#ifdef USE_SAGE_SVP
	struct Rpc_Secbuf_Info sb_info;
#endif
	int retvalue = -1;
	uint64_t start_decrypt_time;
	uint64_t end_decrypt_time;

	*ppOpaqueData = NULL;

#if USE_NEW_OPENCDM
	if(!m_pOpencdmDecrypt)
	{
		logprintf("m_pOpencdmDecrypt is NULL, can't decrypt yet!");
		return -1;
	}
#endif

	// Verify output protection parameters
	// -----------------------------------
	// Widevine output protection is currently supported without any external configuration.
	// But the Playready output protection will be enabled based on 'enablePROutputProtection' flag which can be configured through RFC/aamp.cfg..
	if((m_keySystem == PLAYREADY_KEY_SYSTEM_STRING && gpGlobalConfig->enablePROutputProtection) && m_pOutputProtection->IsSourceUHD()) {
		// Source material is UHD
		if(!m_pOutputProtection->isHDCPConnection2_2()) {
			// UHD and not HDCP 2.2
			logprintf("%s : UHD source but not HDCP 2.2. FAILING decrypt\n", __FUNCTION__);
			return HDCP_COMPLIANCE_CHECK_FAILURE;
		}
	}

	pthread_mutex_lock(&decryptMutex);
	start_decrypt_time = GetCurrentTimeStampInMSec();
#if USE_NEW_OPENCDM
	retvalue = m_pOpencdmDecrypt->Decrypt(const_cast<unsigned char*>(payloadData), payloadDataSize, f_pbIV, f_cbIV);
#else
	retvalue = m_pOpencdm->Decrypt(const_cast<unsigned char*>(payloadData), payloadDataSize, const_cast<unsigned char*>(f_pbIV), f_cbIV);
#endif
	end_decrypt_time = GetCurrentTimeStampInMSec();
	if(retvalue != 0)
	{
		media::OpenCdm::KeyStatus keyStatus = m_pOpencdm->Status();
		AAMPLOG_INFO("%s : decrypt returned : %d key status is : %d", __FUNCTION__, retvalue,keyStatus);
		if(keyStatus == media::OpenCdm::KeyStatus::OutputRestricted){
			retvalue =  HDCP_OUTPUT_PROTECTION_FAILURE;
		}
		else if(keyStatus == media::OpenCdm::KeyStatus::OutputRestrictedHDCP22){
			retvalue =  HDCP_COMPLIANCE_CHECK_FAILURE;
        	}
    	}
	if(payloadDataSize > 0) {
		LogPerformanceExt(__FUNCTION__, start_decrypt_time, end_decrypt_time, payloadDataSize);
	}

	pthread_mutex_unlock(&decryptMutex);
#ifdef USE_SAGE_SVP
	if( 0 == retvalue )
	{
		memcpy(&sb_info, payloadData, sizeof(Rpc_Secbuf_Info));
		if (B_Secbuf_AllocWithToken(sb_info.size, (B_Secbuf_Type)sb_info.type, sb_info.token, (void **)ppOpaqueData))
		{
			logprintf("[HHH] B_Secbuf_AllocWithToken() failed!");
		}
	}
#endif
	return retvalue;

}

KeyState AAMPOCDMSession::getState()
{
	return m_eKeyState;
}

void AAMPOCDMSession:: clearDecryptContext()
{
	logprintf("[HHH] clearDecryptContext.");
	pthread_mutex_lock(&decryptMutex);

#if USE_NEW_OPENCDM < 1
	if(m_pOpencdm) m_pOpencdm->ReleaseMem();
#else
	if(m_pOpencdmDecrypt) {
		delete m_pOpencdmDecrypt;
		m_pOpencdmDecrypt = NULL;
	}
#endif
	if(m_pOpencdm) m_pOpencdm->Close();
	m_eKeyState = KEY_INIT;
	pthread_mutex_unlock(&decryptMutex);
}

