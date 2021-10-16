#ifndef FABRIC_ENTER_EXIT_TRACE
#define FABRIC_ENTER_EXIT_TRACE

#include <common/string_view.h>
#include <common/logging.h>
#include <string>
#include <iostream>

/*
 * Fabric enter/exit trace
 */
struct enter_exit_trace
{
private:
	std::string _func;
	std::string _file;
	unsigned _line;
	void write(common::string_view id_) const
	{
		ELOG("{} {} {}:{}", id_, _func, _file, _line);
	}
public:
	enter_exit_trace(common::string_view func_, common::string_view file_, unsigned line_)
		: _func(func_)
		, _file(file_)
		, _line(line_)
	{
		write(">");
	}
	~enter_exit_trace()
	{
		write("<");
	}
};

#define ENTER_EXIT_TRACE_N /* suppressed because too frequest */
#define ENTER_EXIT_TRACE0 /* suppressed because duplicative */
#if 0
#define ENTER_EXIT_TRACE1 enter_exit_trace x0(__func__, __FILE__, __LINE__);
#else
#define ENTER_EXIT_TRACE1 /* suppressed because not currently needed */
#endif
#endif
