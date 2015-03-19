#ifndef __PUSH_TASK_TP_H__
#define __PUSH_TASK_TP_H__

#include "http/http_request.h"
#include "process_base/http_declaration.h"
#include "process_base/work_threadpool.h"
#include "nfl/cq_base.h"

using namespace http;

class push_task_tp : public work_threadpool< http_request >
{
public:
	virtual int svc(void);

protected:

private:

};


#endif /* __PUSH_TASK_TP_H__ */
