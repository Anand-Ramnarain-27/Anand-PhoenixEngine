#include "Globals.h"

#include <cstdio>
#include <vector>

void log(const char file[], int line, const char* format, ...){
	// Size the buffer to the message: vsprintf_s into a fixed buffer asserts
	// and aborts in Debug when a log line (e.g. long animation channel dumps)
	// exceeds it. Locals instead of statics so background import threads can log.
	va_list ap;
	va_start(ap, format);
	int msgLen = _vscprintf(format, ap);
	va_end(ap);
	if (msgLen < 0)
		return;

	std::vector<char> msg(msgLen + 1);
	va_start(ap, format);
	vsnprintf(msg.data(), msg.size(), format, ap);
	va_end(ap);

	int outLen = _scprintf("\n%s(%d) : %s", file, line, msg.data());
	if (outLen < 0)
		return;

	std::vector<char> out(outLen + 1);
	snprintf(out.data(), out.size(), "\n%s(%d) : %s", file, line, msg.data());
	OutputDebugStringA(out.data());
}
