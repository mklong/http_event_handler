/** 
 * @file	http_declaration.h
 * @brief
 *		http协议框架
 *		1.由http_eh产生http_request,最终投递到http_socket的虚方法处理
 *		同时提供同线程和跨线程的响应方法
 *		2.整个http_request使用了一个内存池，内存管理非常方便
 *		3.框架支持pipeline模式，支持不响应
 *		
 * detail...
 * 
 * @author	mklong
 * @version	1.0
 * @date	2014/12/17
 * 
 * @see		
 * 
 * <b>History:</b><br>
 * <table>
 *  <tr> <th>Version	<th>Date		<th>Author	<th>Notes</tr>
 *  <tr> <td>1.0		<td>2014/12/17	<td>mklong	<td>Create this file</tr>
 * </table>
 * 
 */
#ifndef __HTTP_DECLARATION_H__
#define __HTTP_DECLARATION_H__

class http_eh;
class http_response_eh;
class http_acceptor;
class http_socket;


#include "http_eh.h"
#include "http_response_eh.h"
#include "http_acceptor.h"
#include "http_socket.h"


#endif /* __HTTP_DECLARATION_H__ */
