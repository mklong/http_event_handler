#ifndef __HTTP_ACCEPTOR_H__
#define __HTTP_ACCEPTOR_H__

#include "http_declaration.h"
#include "core/nfl/nfl_accept_handler.h"

class http_acceptor : public nfl::accept_handler< http_eh >
{
public:

	http_acceptor();
	~http_acceptor();

	void max_fds(size_t size);

	//配置传递麻烦，直接传递对象引用
	void set_http_socket(http_socket* p);

protected:

	virtual int handle_input (ACE_HANDLE fd = ACE_INVALID_HANDLE);

private:
	http_socket * socket_base_;
	size_t max_fds_;
};


#endif /* __HTTP_ACCEPTOR_H__ */
