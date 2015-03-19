#ifndef __HTTP_RESPONSE_EH_H__
#define __HTTP_RESPONSE_EH_H__


#include "http_declaration.h"

#include "core/nfl/libevent_handler.h"
#include "core/http/http_request.h"

using namespace http;

class http_response_eh:public nfl::event_handler
{
public:

	void reset();

	virtual int handle_exception (ACE_HANDLE fd = ACE_INVALID_HANDLE);
	
	int response(http_eh *eh,http_request *r,bool close_eh);

private:

	bool close_eh_;
	http_eh * eh_;
	http_request * request_;
};



#endif /* __HTTP_RESPONSE_EH_H__ */
