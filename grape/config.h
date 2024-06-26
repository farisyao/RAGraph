/** Copyright 2020 Alibaba Group Holding Limited.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef GRAPE_CONFIG_H_
#define GRAPE_CONFIG_H_

#ifdef USE_JEMALLOC
#include <jemalloc/jemalloc.h>
#endif

#include "grape/utils/default_allocator.h"
#include "grape/utils/hp_allocator.h"

namespace google {}
namespace grape {

#ifdef GFLAGS_NAMESPACE
namespace gflags = GFLAGS_NAMESPACE;
#else
namespace gflags = google;
#endif

// type alias
using fid_t = unsigned;

#ifdef USE_HUGEPAGES
template <typename T>
using Allocator = HpAllocator<T>;
#else
template <typename T>
using Allocator = DefaultAllocator<T>;
#endif

const int kCoordinatorRank = 0;

const char kSerializationVertexMapFilename[] = "vertex_map.s";
const char kSerializationFilenameFormat[] = "%s/frag_%d.s";

//massage passing tag
const int TagSendMessage = 10;
const int TagTerminateCheck = 11;
const int TagTerminated = 12;


}  // namespace grape
#endif  // GRAPE_CONFIG_H_
