#ifndef _DIAGNOSTICS_H
#define _DIAGNOSTICS_H

#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"

typedef enum {
	ERR_INTERNAL,
	ERR_MEM,
	ERR_IO,
	ERR_REDEFINED,
	ERR_UNDEFINED,
	ERR_INVALID_SYNTAX,
	ERR_INVALID_EXPRESSION,
	ERR_INVALID_LABEL,
	ERR_INVALID_REGISTER,
	ERR_INVALID_DIRECTIVE,
	ERR_INVALID_INSTRUCTION,
	ERR_INVALID_SIZE,
	ERR_INVALID_CONDITION,
	ERR_INVALID_TYPE,
	ERR_DIRECTIVE_NOT_ALLOWED,
	ERR_INSTR_NOT_IN_TEXT,
	ERR_NOT_ALLOWED,
	ERR_UNSUPPORTED,
	ERR_MISALIGNMENT
} errType;

typedef enum {
	WARN_UNREACHABLE,
	WARN_UNIMPLEMENTED
} warnType;

typedef struct LineData {
	const char* source;
	int linenum;
	// int colnum;
} linedata_ctx;


void emitError(errType err, linedata_ctx* linedata, const char* fmsg, ...);
void emitWarning(warnType warn, linedata_ctx* linedata, const char* fmsg, ...);

typedef enum {
	DEBUG_BASIC,
	DEBUG_DETAIL,
	DEBUG_TRACE
} debugLvl;

void initScope(const char* fxnName);

void debug(debugLvl lvl, const char* fmsg, ...);
void rdebug(debugLvl lvl, const char* fmsg, ...);

#define log(fmt, ...) debug(DEBUG_BASIC, fmt, ##__VA_ARGS__)
#define detail(fmt, ...) debug(DEBUG_DETAIL, fmt, ##__VA_ARGS__)
#define trace(fmt, ...) debug(DEBUG_TRACE, fmt, ##__VA_ARGS__)

#define rlog(fmt, ...) rdebug(DEBUG_BASIC, fmt, ##__VA_ARGS__)
#define rdetail(fmt, ...) rdebug(DEBUG_DETAIL, fmt, ##__VA_ARGS__)
#define rtrace(fmt, ...) rdebug(DEBUG_TRACE, fmt, ##__VA_ARGS__)

#endif