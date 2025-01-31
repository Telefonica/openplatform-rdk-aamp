/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#include "AampIonMemorySystem.h"

// For Ion memory
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "GlobalConfigAAMP.h"

AampIonMemorySystem::AampIonMemorySystem() : context_() {
}

AampIonMemorySystem::~AampIonMemorySystem() {
}

AampIonMemorySystem::AampIonMemoryContext::AampIonMemoryContext() : fd_(0), handle_(0) {};

bool AampIonMemorySystem::AampIonMemoryContext::createBuffer(size_t len) {
	fd_ = ion_open();
	if (fd_ < 0) {
		AAMPLOG_WARN("%s:%d: Calling ion_open(): %d", __FUNCTION__, __LINE__, fd_);
		return false;
	}
	AAMPLOG_INFO("%s:%d: Got %d from ion_open()", __FUNCTION__, __LINE__, fd_);

	int ret = ion_alloc(fd_, len,
		AAMP_ION_MEMORY_ALIGN,
		AAMP_ION_MEMORY_REGION,
		AAMP_ION_MEMORY_FLAGS, &handle_);

	if (ret != 0) {
		AAMPLOG_WARN("%s:%d: Calling ion_alloc(): %d", __FUNCTION__, __LINE__, fd_);
		ion_close(fd_);
		fd_ = 0;
	}
	return (ret == 0);
}

class AampIonMemorySystemCloser {
private:
	AampIonMemorySystem::AampIonMemoryContext& context_;
	bool closeOnDestruct_;
public:
	AampIonMemorySystemCloser(AampIonMemorySystem::AampIonMemoryContext& context)
		: context_(context)
		, closeOnDestruct_(true) {}
	~AampIonMemorySystemCloser() { 
		if (closeOnDestruct_) {
			context_.close();
		}
	}
	void setNoCloseOnDestruct() {
		closeOnDestruct_ = false;
	}
};

AampIonMemorySystem::AampIonMemoryContext::~AampIonMemoryContext() { 
	close();
}
void AampIonMemorySystem::AampIonMemoryContext::close() { 
	if (handle_ != 0) {
		int ret = ion_free(fd_, handle_);
		handle_ = 0;
		if (ret) {
			AAMPLOG_ERR("%s:%d: ion_free_buffer failed", __FUNCTION__, __LINE__);
		}
	}
	if (fd_ != 0) {
		ion_close(fd_);
		fd_ = 0;
	}
	AAMPLOG_INFO("%s:%d: Closed ION memory ok", __FUNCTION__, __LINE__);
}

bool AampIonMemorySystem::AampIonMemoryContext::share(int& shareFd) {
	int status = ion_share(fd_, handle_, &shareFd);
	return status == 0;
}

bool AampIonMemorySystem::AampIonMemoryContext::phyAddr(unsigned long *phyAddr) {
	unsigned int size;
	int status = ion_phys(fd_, handle_, phyAddr, &size);
	return status == 0;
}


bool AampIonMemorySystem::encode(const uint8_t *dataIn, uint32_t dataInSz, std::vector<uint8_t>& dataOut) 
{
	AampIonMemorySystemCloser closer(context_);
	
	if (!context_.createBuffer(dataInSz))
	{
		AAMPLOG_WARN("%s:%d: Failed to create Ion memory object: %d", __FUNCTION__, __LINE__, errno);
		return false;
	}

	int share_fd;
	if (!context_.share(share_fd))
	{
		AAMPLOG_WARN("%s:%d: Failed to share Ion memory object: %d", __FUNCTION__, __LINE__, errno);
		return false;
	}

	AampMemoryHandleCloser mc(share_fd);
	
	void *dataWr = mmap(NULL, dataInSz, PROT_WRITE | PROT_READ, MAP_SHARED, share_fd, 0);
	if (dataWr == 0) 
	{
		AAMPLOG_WARN("%s:%d: Failed to map the Ion memory object %d", __FUNCTION__, __LINE__, errno);
		return false;
	}
	
	memmove(dataWr, dataIn, dataInSz);
	
	int status = munmap(dataWr, dataInSz);
	if (status < 0)
	{
		AAMPLOG_WARN("%s:%d: Failed to unmap the Ion memory object %d", __FUNCTION__, __LINE__, errno);
		return false;
	}
	// Only send the size of the Ion memory, nothing else
	AampIonMemoryInterchangeBuffer ib { };
	if (!context_.phyAddr(&ib.phyAddr)) {
		AAMPLOG_WARN("%s:%d: Failed to find the physical address of the Ion memory object %d", __FUNCTION__, __LINE__, errno);
		return false;
	}
	ib.size = sizeof(ib);
	ib.dataSize = dataInSz;
	
	// Look away now, this is horrid
	dataOut.resize(sizeof(ib));
	memcpy(dataOut.data(), &ib, sizeof(ib));
	closer.setNoCloseOnDestruct();
	
	return true;
}

bool AampIonMemorySystem::decode(const uint8_t * dataIn, uint32_t dataInSz, uint8_t* dataOut, uint32_t dataOutSz) 
{
	AampIonMemorySystemCloser closer(context_);

	const AampIonMemoryInterchangeBuffer *pib = reinterpret_cast<const AampIonMemoryInterchangeBuffer *>(dataIn);
	if (dataInSz != sizeof(AampIonMemoryInterchangeBuffer))
	{
		AAMPLOG_WARN("%s:%d: Wrong data packet size, expected %d, got %d", __FUNCTION__, __LINE__, sizeof(AampIonMemoryInterchangeBuffer), dataInSz);
		return false;
	}
	unsigned long phyAddr;
	if (!context_.phyAddr(&phyAddr)) {
		AAMPLOG_WARN("%s:%d: Failed to get physical address of ION memory: %d", __FUNCTION__, __LINE__, errno);
		return false;
	}
	if (pib->phyAddr != phyAddr) {
		AAMPLOG_WARN("%s:%d: Physical address of ION memory not what was sent... %d", __FUNCTION__, __LINE__, errno);
		return false;
	}

	int share_fd;
	if (!context_.share(share_fd))
	{
		AAMPLOG_WARN("%s:%d: Failed to share Ion memory object: %d", __FUNCTION__, __LINE__, errno);
		return false;
	}

	AampMemoryHandleCloser mc(share_fd);
	
	uint32_t packetSize = pib->dataSize;
	void *dataRd = mmap(NULL, packetSize, PROT_READ, MAP_SHARED, share_fd, 0);
	if (dataRd == 0) 
	{
		AAMPLOG_WARN("%s:%d: Failed to map the Ion memory object %d", __FUNCTION__, __LINE__, errno);
		return false;
	}
	
	if (packetSize > dataOutSz)
	{
		AAMPLOG_WARN("%s:%d: Received data is bigger than provided buffer. %d > %d", __FUNCTION__, __LINE__, packetSize, dataOutSz);
	}
	memmove(dataOut, dataRd, std::min(packetSize, dataOutSz));
	
	int status = munmap(dataRd, packetSize);
	if (status < 0)
	{
		AAMPLOG_WARN("%s:%d: Failed to unmap the shared memory object %d", __FUNCTION__, __LINE__, errno);
		return false;
	}
	
	// This time the closer can close the ION memory buffer
	return true;
}

void AampIonMemorySystem::terminateEarly() {
	AAMPLOG_WARN("%s:%d: closing transfer early", __FUNCTION__, __LINE__);
	context_.close();
}