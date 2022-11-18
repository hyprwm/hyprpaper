#pragma once

#include <vector>
#include <deque>
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>

#include <pthread.h>
#include <cmath>
#include <cmath>

#define class _class
#define namespace _namespace
#define static

extern "C" {
#include "wlr-layer-shell-unstable-v1-protocol.h"
#include "xdg-shell-protocol.h"
#include <wayland-client.h>
#include <wayland-cursor.h>
}

#undef class
#undef namespace
#undef static

#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>
#include <cassert>
#include <cairo.h>
#include <cairo/cairo.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include <algorithm>
#include <filesystem>
#include <thread>
#include <unordered_map>
