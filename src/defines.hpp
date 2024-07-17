#pragma once

#include "includes.hpp"
#include "debug/Log.hpp"

// git stuff
#ifndef GIT_COMMIT_HASH
#define GIT_COMMIT_HASH "?"
#endif
#ifndef GIT_BRANCH
#define GIT_BRANCH "?"
#endif
#ifndef GIT_COMMIT_MESSAGE
#define GIT_COMMIT_MESSAGE "?"
#endif
#ifndef GIT_DIRTY
#define GIT_DIRTY "?"
#endif

#include <hyprutils/math/Vector2D.hpp>
using namespace Hyprutils::Math;

#include <hyprutils/memory/WeakPtr.hpp>
using namespace Hyprutils::Memory;
#define SP Hyprutils::Memory::CSharedPointer
#define WP Hyprutils::Memory::CWeakPointer
