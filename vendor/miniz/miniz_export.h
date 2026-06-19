#pragma once
// Static build — no DLL export/import needed
#define MINIZ_EXPORT
#define MINIZ_NO_EXPORT
#define MINIZ_DEPRECATED __declspec(deprecated)
#define MINIZ_DEPRECATED_EXPORT  MINIZ_DEPRECATED
#define MINIZ_DEPRECATED_NO_EXPORT MINIZ_DEPRECATED
