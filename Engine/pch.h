#pragma once
#pragma comment(lib, "winmm.lib")
#pragma warning(disable: 4819)
#include <Windows.h>

#include <atlimage.h>

#include <wchar.h>
#include <cmath>
#include <assert.h>

#include <algorithm>
#include <format>
#include <memory>
#include <functional>

#include <list>
using std::list;

#include <sstream>     // std::wstringstream
#include <iomanip>     // std::put_time
#include <ctime>

#include <vector>
using std::vector;

#include <map>
using std::map;
using std::make_pair;

#include <string>

using std::string;
using std::wstring;
using std::to_wstring;

#pragma comment(lib, "Msimg32.lib")
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;
#pragma comment(lib, "winmm.lib")

#include "define.h"
#include "struct.h"
#include "func.h"

