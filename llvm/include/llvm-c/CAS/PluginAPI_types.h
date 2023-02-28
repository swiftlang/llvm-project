/*===-- llvm-c/CAS/PluginAPI_Types.h - CAS Plugin Types Interface -*- C -*-===*\
|*                                                                            *|
|* Part of the LLVM Project, under the Apache License v2.0 with LLVM          *|
|* Exceptions.                                                                *|
|* See https://llvm.org/LICENSE.txt for license information.                  *|
|* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception                    *|
|*                                                                            *|
|*===----------------------------------------------------------------------===*|
|*                                                                            *|
|* The types for the LLVM CAS plugin API.                                     *|
|* The API is experimental and subject to change.                             *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

#ifndef LLVM_C_CAS_PLUGINAPI_TYPES_H
#define LLVM_C_CAS_PLUGINAPI_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LLCAS_VERSION_MAJOR 0
#define LLCAS_VERSION_MINOR 1

typedef struct llcas_cas_options_s *llcas_cas_options_t;
typedef struct llcas_cas_s *llcas_cas_t;

/**
 * Digest hash bytes.
 */
typedef struct {
  const uint8_t *data;
  size_t size;
} llcas_digest_t;

/**
 * Data buffer for stored CAS objects.
 */
typedef struct {
  const void *data;
  size_t size;
} llcas_data_t;

/**
 * Identifier for a CAS object.
 */
typedef struct {
  uint64_t opaque;
} llcas_objectid_t;

/**
 * A loaded CAS object.
 */
typedef struct {
  uint64_t opaque;
} llcas_loaded_object_t;

/**
 * Object references for a CAS object.
 */
typedef struct {
  uint64_t opaque_b;
  uint64_t opaque_e;
} llcas_object_refs_t;

/**
 * Return values for a load operation.
 */
typedef enum {
  /**
   * The object was found.
   */
  LLCAS_LOOKUP_RESULT_SUCCESS = 0,

  /**
   * The object was not found.
   */
  LLCAS_LOOKUP_RESULT_NOTFOUND = 1,

  /**
   * An error occurred.
   */
  LLCAS_LOOKUP_RESULT_ERROR = 2,
} llcas_lookup_result_t;

/**
 * Encompasses a set of (name -> object) pairs, essentially associating a name
 * string with an \c llcas_objectid_t that can be materialized asynchronously.
 */
typedef struct llcas_actioncache_map_s *llcas_actioncache_map_t;

/**
 * A (name -> object) association.
 */
typedef struct {
  const char *name;
  llcas_objectid_t ref;
} llcas_actioncache_map_entry;

/**
 * Callback for \c llcas_actioncache_map_get_entry_value_async.
 *
 * \param ctx pointer passed through from the
 * \c llcas_actioncache_map_get_entry_value_async call.
 * \param result one of \c llcas_lookup_result_t.
 * \param entry contains the \c name that was associated with the originating
 * \c llcas_actioncache_map_get_entry_value_async call no matter what the
 * \p result is. The \c ref field is only valid to use if \p result is
 * \c LLCAS_LOOKUP_RESULT_SUCCESS.
 */
typedef void (*llcas_actioncache_map_get_entry_value_callback)(
    void *ctx, llcas_lookup_result_t result, llcas_actioncache_map_entry entry,
    char *error);

#endif /* LLVM_C_CAS_PLUGINAPI_TYPES_H */
