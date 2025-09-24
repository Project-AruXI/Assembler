#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdbool.h>
#include <stdint.h>


// Config struct to keep track of:
// Which warnings to show
// To include debug symbols or not
// Whether warnings are fatal or not
// Output filename
// Whether to enable entire enhanced typing features
// Which enhanced typing features to enable/disable

typedef uint8_t FLAGS8;

typedef struct Config {
	bool useDebugSymbols;
	bool warningAsFatal;
	const char* outbin;
	FLAGS8 warnings;
	FLAGS8 enhancedFeatures;
} Config;

typedef enum {
	WARN_NONE = 0x00,
	WARN_UNUSED_SYMB = 1 << 0,
	WARN_OVERFLOW = 1 << 1,
	WARN_UNREACHABLE = 1 << 2,
	WARN_ALL = 0xFF
} WarningFlags;

typedef enum {
	FEATURE_NONE = 0x00,
	FEATURE_TYPES = 1 << 1, // Any sort of defining types (.type, .def)
	FEATURE_MACROS = 1 << 2,
	FEATURE_PTR_DEREF = 1 << 3, // Pointer dereferencing in expressions
	FEATURE_FIELD_ACCESS = 1 << 4, // Accessing struct fields in expressions
	FEATURE_ALL = 0xFF
} EnhancedFeatures;

#endif