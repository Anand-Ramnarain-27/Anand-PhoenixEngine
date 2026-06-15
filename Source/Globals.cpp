#include "Globals.h"

#include <cstdio>
#include <vector>

void log(const char file[], int line, const char* format, ...){
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
