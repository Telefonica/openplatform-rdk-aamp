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
 * @file drm.cpp
 * @brief AVE DRM helper definitions
 */


#if 0
per Doug Adler, following hack(forcing true) allows initialization to complete
rdk\generic\ave\third_party\drm-public\portingkit\robust\CConstraintEnforcer.cpp

IConstraintEnforcer::Status OutputProtectionEnforcer::isConstraintSatisfiedInner(const IOutputProtectionConstraint& opConstraint, bool validateResult)
{
	ACBool result = true;
	ACUns32 error = 0;
	logprintf("%s --> %d\n", __FUNCTION__, __LINE__);
#if 0 // hack
	...
#endif
	return std::pair<ACBool, ACUns32>(result, error);
#endif

// TODO: THIS MODULE NEEDS TO BE MADE MULTI-SESSION-FRIENDLY
#include "drm.h"
#ifdef AVE_DRM
#include "media/IMedia.h" // TBR - remove dependency
#include <sys/time.h>
#include <stdio.h> // for printf
#include <stdlib.h> // for malloc
#include <pthread.h>
#include <errno.h>

using namespace media;

#define DRM_ERROR_SENTINEL_FILE "/tmp/DRM_Error"

static PrivateInstanceAAMP *mpAamp;
static MyFlashAccessAdapter *m_pDrmAdapter; // lazily allocated
static class TheDRMListener *m_pDrmListner; // lazily allocated

#endif /*!NO_AVE_DRM*/

static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef AVE_DRM

static DRMState mDrmState = eDRM_INITIALIZED, mPrevDrmState = eDRM_INITIALIZED;

static int drmSignalKeyAquired(void * arg);
static int drmSignalError(void * arg);

/*
From FlashAccessKeyFormats.pdf:
- DRM Key: RSA 1024 bit key pair generated by OEM; public key passed to Adobe
- Machine Key: RSA 1024 bit key pair generated by Adobe Individualization server
- Individualization Transport Key: RSA 1024 bit public key
- Individualization Response Wrapper Key: ARS 128 bit key
- Machine Transport Key: RSA 1024 bit key pair generated by Adobe individualization server
- Session Keys
- Content Keys: AES 128 bit keys

1. Individualization
- done first time device attempts to access protected content
- device and player/runtime identifiers sent to Adobe-hosted server
- each device is assigned machine private key
- Adobe server returns a unique digital machine certificate
- certificate can be revoked to prevent future license acquisition
- persists across reboots as a .bin file

2. Key Negotiation
- client parses manifest, reading URL of local comcast license server (from EXT-X-KEY)
- client extracts initialization vector (IV); placeholder for stronger security (from EXT-X-KEY)
- client collects additional channel-specific DRM metadata from manifest (from EXT-X-FAXS-CM)
- client transmits DRM metadata and machine certificate as parameters to an encrypted license request
- license request is encrypted using Transport public key (from DRM metadata)
- license server returns Content Encryption Key (CEK) and usage rules
- license is signed using license server private key
- license response is signed using Transport private key, and encrypted before being returned to client
- usage rules may authorize offline access
- Flash Player or Adobe AIR runtime client extracts Content Encryption Key (CEK) from license, allowing consumer to view authorized content

3. Decoding/Playback
- Advanced Encryption Standard 128-bit key (AES-128) decryption
- PKCS7 padding
- video output protection enforcement
*/

/**
 * @class TheDRMListener
 * @brief
 */
class TheDRMListener : public MyDRMListener
{
public:
	/**
	 * @brief TheDRMListener Constructor
	 */
	TheDRMListener()
	{
	}

	/**
	 * @brief TheDRMListener Constructor
	 */
	~TheDRMListener()
	{
	}

	/**
	 * @brief Callback on key acquired
	 */
	void SignalKeyAcquired()
	{
		logprintf("aamp:DRM %s drmState:%d moving to KeyAcquired\n", __FUNCTION__, mDrmState);
		pthread_mutex_lock(&mutex);
		mDrmState = eDRM_KEY_ACQUIRED;
		pthread_cond_broadcast(&cond);
		pthread_mutex_unlock(&mutex);
		mpAamp->LogDrmInitComplete();
	}

	/**
	 * @brief Callback on initialization success
	 */
	void NotifyInitSuccess()
	{ // callback from successful pDrmAdapter->Initialize
		//log_current_time("NotifyInitSuccess\n");
		PrivateInstanceAAMP::AddIdleTask(drmSignalKeyAquired, this);
	}

	/**
	 * @brief Callback on drm error
	 */
	void NotifyDRMError(uint32_t majorError, uint32_t minorError)//(ErrorCode majorError, DRMMinorError minorError, AVString* errorString, media::DRMMetadata* metadata)
	{
		AAMPTuneFailure drmFailure = AAMP_TUNE_UNTRACKED_DRM_ERROR;
		switch(majorError)
		{
		case 3329:
			if(12012 == minorError || 12013 == minorError)
			{
				drmFailure = AAMP_TUNE_AUTHORISATION_FAILURE;
			}
			break;

		case 3321:
		case 3322:
		case 3328:
				drmFailure = AAMP_TUNE_CORRUPT_DRM_DATA;
			break;

		default:
                    break;
		}

		mpAamp->DisableDownloads();
		if(AAMP_TUNE_UNTRACKED_DRM_ERROR == drmFailure)
		{
			char description[128] = {};
			sprintf(description, "AAMP: DRM Failure majorError = %d, minorError = %d",(int)majorError, (int)minorError);
			mpAamp->SendErrorEvent(drmFailure, description);
		}
		else if(AAMP_TUNE_CORRUPT_DRM_DATA == drmFailure)
		{
			char description[128] = {};

			/*
			* Creating file "/tmp/DRM_Error" will invoke self heal logic in
			* ASCPDriver.cpp and regenrate cert files in /opt/persistent/adobe
			* in the next tune attempt, this could clear tune error scenarios
			* due to corrupt drm data.
			*/
			FILE *sentinelFile;
			sentinelFile = fopen(DRM_ERROR_SENTINEL_FILE,"w");

			if(sentinelFile)
			{
				fclose(sentinelFile);
			}
			else
			{
				logprintf("%s %d : *** /tmp/DRM_Error file creation for self heal failed. Error %d -> %s\n",
							__FUNCTION__, __LINE__, errno, strerror(errno));
			}

			sprintf(description, "AAMP: DRM Failure possibly due to corrupt drm data; majorError = %d, minorError = %d",(int)majorError, (int)minorError);
			mpAamp->SendErrorEvent(drmFailure, description);
		}
		else
		{
			mpAamp->SendErrorEvent(drmFailure);
		}

		PrivateInstanceAAMP::AddIdleTask(drmSignalError, this);
		logprintf("aamp:***TheDRMListener::NotifyDRMError: majorError = %d, minorError = %d drmState:%d\n", (int)majorError, (int)minorError,mDrmState );
	}



	/**
	 * @brief Signal drm error
	 */
	void SignalDrmError()
	{
		pthread_mutex_lock(&mutex);
		mDrmState = eDRM_KEY_FAILED;
		pthread_cond_broadcast(&cond);
		pthread_mutex_unlock(&mutex);
	}



	/**
	 * @brief Callback on drm status change
	 */
	void NotifyDRMStatus()//media::DRMMetadata* metadata, const DRMLicenseInfo* licenseInfo)
	{ // license available
		logprintf("aamp:***TheDRMListener::NotifyDRMStatus drmState:%d\n",mDrmState);
	}
};

namespace FlashAccess {

	/**
	 * @brief Caches drm resources
	 */
	void CacheDRMResources();
}


/**
 * @brief Signal key acquired to listener
 * @param arg drm status listener
 * @retval 0
 */
static int drmSignalKeyAquired(void * arg)
{
	TheDRMListener * drmListener = (TheDRMListener*)arg;
	drmListener->SignalKeyAcquired();
	return 0;
}


/**
 * @brief Signal drm error to listener
 * @param arg drm status listener
 * @retval 0
 */
static int drmSignalError(void * arg)
{
	TheDRMListener * drmListener = (TheDRMListener*)arg;
	drmListener->SignalDrmError();
	return 0;
}

/**
 * @brief prepare for decryption - individualization & license acquisition
 *
 * @param[in] aamp pointer to PrivateInstanceAAMP object associated with player
 * @param[in] metadata pointed to DrmMetadata structure - unpacked binary metadata from EXT-X-FAXS-CM
 * @param[in] drmInfo DRM information required to decrypt
 */
int AveDrm::SetContext( class PrivateInstanceAAMP *aamp, void *metadata, const struct DrmInfo *drmInfo )
{
	const DrmMetadata *drmMetadata = ( DrmMetadata *)metadata;
	pthread_mutex_lock(&mutex);
	mpAamp = aamp;
	int err = -1;
	if( !m_pDrmAdapter )
	{
		m_pDrmAdapter = new MyFlashAccessAdapter();
		m_pDrmListner = new TheDRMListener();
	}

	if (m_pDrmAdapter)
	{
		mDrmState = eDRM_ACQUIRING_KEY;
		m_pDrmAdapter->Initialize( drmMetadata, m_pDrmListner );
		m_pDrmAdapter->SetupDecryptionContext();
		m_pDrmAdapter->SetDecryptInfo(drmInfo);
		err = 0;
	}
	else
	{
		mDrmState = eDRM_INITIALIZED;
	}
	mPrevDrmState = eDRM_INITIALIZED;
	pthread_mutex_unlock(&mutex);
	logprintf("aamp:drm_SetContext drmState:%d \n",mDrmState);
	return err;
}


/**
 * @brief Decrypts an encrypted buffer
 * @param bucketType Type of bucket for profiling
 * @param encryptedDataPtr pointer to encyrpted payload
 * @param encryptedDataLen length in bytes of data pointed to by encryptedDataPtr
 * @param timeInMs wait time
 */
DrmReturn AveDrm::Decrypt( ProfilerBucketType bucketType, void *encryptedDataPtr, size_t encryptedDataLen,int timeInMs)
{
	DrmReturn err = eDRM_ERROR;

	pthread_mutex_lock(&mutex);
	if (mDrmState == eDRM_ACQUIRING_KEY )
	{
		logprintf( "aamp:waiting for key acquisition to complete,wait time:%d\n",timeInMs );
		struct timespec ts;
		struct timeval tv;
		gettimeofday(&tv, NULL);
		ts.tv_sec = time(NULL) + timeInMs / 1000;
		ts.tv_nsec = (long)(tv.tv_usec * 1000 + 1000 * 1000 * (timeInMs % 1000));
		ts.tv_sec += ts.tv_nsec / (1000 * 1000 * 1000);
		ts.tv_nsec %= (1000 * 1000 * 1000);

		if(ETIMEDOUT == pthread_cond_timedwait(&cond, &mutex, &ts)) // block until drm ready
		{
			logprintf("%s:%d wait for key acquisition timed out\n", __FUNCTION__, __LINE__);
			err = eDRM_KEY_ACQUSITION_TIMEOUT;
		}
	}

	if (mDrmState == eDRM_KEY_ACQUIRED)
	{
		// create same-sized buffer for decrypted data; q: can we decrypt in-place?
		struct DRMBuffer decryptedData;
		decryptedData.buf = (uint8_t *)malloc(encryptedDataLen);
		if (decryptedData.buf)
		{
			decryptedData.len = (uint32_t)encryptedDataLen;

			struct DRMBuffer encryptedData;
			encryptedData.buf = (uint8_t *)encryptedDataPtr;
			encryptedData.len = (uint32_t)encryptedDataLen;

			mpAamp->LogDrmDecryptBegin(bucketType);
			if( 0 == m_pDrmAdapter->Decrypt(true, encryptedData, decryptedData))
			{
				err = eDRM_SUCCESS;
			}
			mpAamp->LogDrmDecryptEnd(bucketType);

			memcpy(encryptedDataPtr, decryptedData.buf, encryptedDataLen);
			free(decryptedData.buf);
		}
	}
	else
	{
		logprintf( "aamp:key acquisition failure! mDrmState = %d\n", (int)mDrmState);
	}
	pthread_mutex_unlock(&mutex);
	return err;
}


/**
 * @brief Release drm session
 */
void AveDrm::Release()
{
	pthread_mutex_lock(&mutex);
	if (m_pDrmAdapter)
	{
		// close all drm sessions
		m_pDrmAdapter->AbortOperations();
	}
	pthread_cond_broadcast(&cond);
	pthread_mutex_unlock(&mutex);
}


/**
 * @brief Cancel timed_wait operation drm_Decrypt
 */
void AveDrm::CancelKeyWait()
{
	pthread_mutex_lock(&mutex);
	//save the current state in case required to restore later.
	mPrevDrmState = mDrmState;
	//required for demuxed assets where the other track might be waiting on mutex lock.
	mDrmState = eDRM_KEY_FLUSH;
	pthread_cond_broadcast(&cond);
	pthread_mutex_unlock(&mutex);
}


/**
 * @brief Restore key state post cleanup of
 * audio/video TrackState in case DRM data is persisted
 */
void AveDrm::RestoreKeyState()
{
	pthread_mutex_lock(&mutex);
	//In case somebody overwritten mDrmState before restore operation, keep that state
	if (mDrmState == eDRM_KEY_FLUSH)
	{
		mDrmState = mPrevDrmState;
	}
	pthread_mutex_unlock(&mutex);
}
#else

int AveDrm::SetContext(class PrivateInstanceAAMP *aamp, void *drmMetadata, const struct DrmInfo *drmInfo)
{
	return -1;
}


DrmReturn AveDrm::Decrypt( ProfilerBucketType bucketType, void *encryptedDataPtr, size_t encryptedDataLen,int timeInMs)
{
	return eDRM_ERROR;
}


void AveDrm::Release()
{
}


void AveDrm::CancelKeyWait()
{
}

void AveDrm::RestoreKeyState()
{
}
#endif // !AVE_DRM

AveDrm* AveDrm::mInstance = nullptr; /// Singleton instance


/**
 * @brief Get static instance
 * @retval pointer to AveDrm object
 */
AveDrm* AveDrm::GetInstance()
{
	pthread_mutex_lock(&mutex);
	if (nullptr == mInstance)
	{
		mInstance = new AveDrm();
	}
	pthread_mutex_unlock(&mutex);
	return mInstance;
}
