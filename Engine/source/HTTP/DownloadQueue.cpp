// Implements Micro Download system

#include <vector>
#include <direct.h>
#include <io.h>

#include "CPFInterface.h"
#include "CKLBScriptEnv.h"
#include "CKLBUtility.h"
#include "MultithreadedNetwork.h"

#include "DownloadQueue.h"

struct MDLData
{
	const char* url;
	const char* callback;
	const char* filename;
	CKLBHTTPInterface* http;

	MDLData(const char* c, const char* f, const char* u)
	{
		url = CKLBUtility::copyString(u);
		callback = CKLBUtility::copyString(c);
		filename = CKLBUtility::copyString(f);
		http = NetworkManager::createConnection();

		http->httpGET(u, false);
	};

	~MDLData()
	{
		KLBDELETEA(url);
		KLBDELETEA(callback);
		KLBDELETEA(filename);

		NetworkManager::releaseConnection(http);
	};
};

typedef std::vector<MDLData*> MicroDLQueue;
MicroDLQueue queue_list;

void MicroDownload::MainLoop(int )
{
	if (queue_list.empty()) return;

	CKLBScriptEnv& scriptenv = CKLBScriptEnv::getInstance();

	for(MicroDLQueue::iterator i = queue_list.begin(); i != queue_list.end();)
	{
		MDLData* mdl = *i;
		int status_code = mdl->http->getHttpState();

		if(mdl->http->httpRECV() || status_code != (-1))
		{
			// Downloaded, but unsure if it's okay
			if(status_code != 200)
				goto error_mdl;

			i = queue_list.erase(i);

			// Write to file.
			u8* body = mdl->http->getRecvResource();

			if(body == NULL)
				goto error_mdl2;
			
			ITmpFile *f = CPFInterface::getInstance().platform().openTmpFile(mdl->filename);
			
			if(f == NULL)
				goto error_mdl2;

			size_t bodylen = body ? mdl->http->getSize() : 0;
			f->writeTmp(body, bodylen);
			KLBDELETE(f);
			
			// callback
			scriptenv.call_eventMdlFinish(mdl->callback, mdl->filename, mdl->url, true, status_code);

			delete mdl;
		}
		else if(mdl->http->m_threadStop == 1 && mdl->http->getHttpState() == (-1))
		{
			// Failed.
			error_mdl:
			i = queue_list.erase(i);

			error_mdl2:
			DEBUG_PRINT("MDL Error 2 occured.");
			scriptenv.call_eventMdlFinish(mdl->callback, mdl->filename, mdl->url, false, 500);

			delete mdl;
		}
		else
			++i;
	}
}

void MicroDownload::Queue(const char* callback, const char* filename, const char* url)
{
	MDLData* a = new MDLData(callback, filename, url);

	DEBUG_PRINT("Micro Download: %s ; %s ; %s", callback, filename, url);

	queue_list.push_back(a);
}

void MicroDownload::DeleteAll()
{
	CKLBScriptEnv& scriptenv = CKLBScriptEnv::getInstance();

	while(queue_list.empty() == false)
	{
		MDLData* mdl = queue_list.back();

		queue_list.pop_back();
		scriptenv.call_eventMdlFinish(mdl->callback, mdl->filename, mdl->url, false, -1);
		
		delete mdl;
	}
}
