#pragma once

#include <vector>
#include <deque>
#include <iostream>
#include <fstream>
#include <string.h>
#include <string>

#include <pthread.h>
#include <cmath>
#include <math.h>

#define class _class
#define namespace _namespace
#define static

extern "C" {
#include "wlr-layer-shell-unstable-v1-protocol.h"
#include "xdg-shell-protocol.h"
#include <wayland-client.h>
}

#undef class
#undef namespace
#undef static

#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>
#include <assert.h>
#include <cairo.h>
#include <cairo/cairo.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include <algorithm>
#include <filesystem>
#include <thread>
#include <unordered_map>
