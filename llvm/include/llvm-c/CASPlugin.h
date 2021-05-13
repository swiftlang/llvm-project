/*===- llvm-c/CASPlugin.h ---------------------------------------*- C++ -*-===*|
|*                                                                            *|
|* Part of the LLVM Project, under the Apache License v2.0 with LLVM          *|
|* Exceptions.                                                                *|
|* See https://llvm.org/LICENSE.txt for license information.                  *|
|* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception                    *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

#ifndef LLVM_C_CASPLUGIN_H
#define LLVM_C_CASPLUGIN_H

#include "llvm-c/ExternC.h"
#include <stddef.h>

LLVM_C_EXTERN_C_BEGIN

struct llvm_cas_db;
typedef struct llvm_cas_db *llvm_cas_db_handle;

/** Kind of a CAS object. */
enum llvm_cas_object_kind {
  LLVM_CAS_OBJECT_INVALID = 0,
  LLVM_CAS_OBJECT_BLOB = 1,
  LLVM_CAS_OBJECT_TREE = 2,
  LLVM_CAS_OBJECT_NODE = 3,
};

typedef struct {
  const unsigned char *hash;
  size_t size;
} llvm_cas_id_t;

typedef struct {
  const char *bytes;
  size_t size;
} llvm_cas_buffer_t;

typedef struct {
  llvm_cas_id_t id;
  llvm_cas_buffer_t buffer;
} llvm_cas_blob_t;

struct llvm_cas_tree;
typedef struct llvm_cas_tree *llvm_cas_tree_handle;
typedef struct {
  llvm_cas_id_t id;
  llvm_cas_tree_handle handle;
} llvm_cas_tree_t;

typedef enum {
  LLVM_CAS_VISITOR_OK,
  LLVM_CAS_VISITOR_SKIP,
  LLVM_CAS_VISITOR_ABORT,
} llvm_cas_visitor_status;

typedef enum {
  LLVM_CAS_TREE_ENTRY_REGULAR = 0,
  LLVM_CAS_TREE_ENTRY_EXEC = 1,
  LLVM_CAS_TREE_ENTRY_SYMLINK = 2,
  LLVM_CAS_TREE_ENTRY_TREE = 3,
} llvm_cas_tree_entry_kind;

typedef struct {
  llvm_cas_id_t id;
  llvm_cas_tree_entry_kind kind;
  const char *name;
} llvm_cas_tree_entry_t;

/**
 * Table of functions to fill in by the plugin.
 */
typedef struct {
  /** Open a connection to a CAS. */
  llvm_cas_db_handle (*open_db)(int NumArgs, const char *Args);

  /** Close a connection to a CAS. */
  void (*close_db)(llvm_cas_db_handle db);

  /**
   * Get a valid CAS object id from a human-readable reference.
   */
  int (*get_object_id)(llvm_cas_db_handle db, const char *reference,
                       llvm_cas_id_t *id);

  /**
   * Get the object kind of an existing CAS object.
   */
  llvm_cas_object_kind (*get_object_kind)(llvm_cas_db_handle db,
                                          const llvm_cas_id_t *id);

  /**
   * Create a blob object with the content of \c buffer. Write into \p blob the
   * ID for the object and a copy of \c buffer. Both should be valid as long as
   * the CAS is alive.
   */
  int (*create_blob)(llvm_cas_db_handle db, const llvm_cas_buffer_t *buffer,
                     llvm_cas_blob_t *blob);

  /**
   * Get an existing blob object. Write into \p blob the ID for the object and a
   * copy of \c buffer. Both should be valid as long as the CAS is alive.
   */
  int (*get_blob)(llvm_cas_db_handle db, const llvm_cas_id_t *id,
                  llvm_cas_blob_t *blob);

  /**
   * Get a handle to an existing tree object.
   */
  int (*get_tree)(llvm_cas_db_handle db, const llvm_cas_id_t *id,
                  llvm_cas_tree *tree);

  /**
   * Visit tree entries. \a llvm_cas_tree_entry_t::id and \a
   * llvm_cas_tree_entry_t::name should both be valid as long as the CAS is
   * alive.
   */
  int (*tree_get_entries)(llvm_cas_db_handle db, llvm_cas_tree_handle tree,
                          void *context,
                          llvm_cas_visitor_status (*visit)(
                              void *context, llvm_cas_tree_entry_t *entry));

  size_t (*tree_get_num_entries)(llvm_cas_db_handle db,
                                 llvm_cas_tree_handle tree);

  int (*tree_get_entry)(llvm_cas_db_handle db, llvm_cas_tree_handle tree,
                        size_t index, llvm_cas_tree_entry_t *entry);

  int (*tree_get_entry_by_name)(llvm_cas_db_handle db,
                                llvm_cas_tree_handle tree, size_t index,
                                const char *entry);

  /** TODO: add create_tree */
  /** TODO: add functions for the 'node' type */

  /**
   * Get a result from the cache.
   */
  int (*result_cache_get)(llvm_cas_db_handle db, const llvm_cas_id_t *key,
                          llvm_cas_id_t *value);

  /**
   * Put a result in the cache.
   */
  int (*result_cache_put)(llvm_cas_db_handle db, const llvm_cas_id_t *key,
                          const llvm_cas_id_t *value);
} llvm_cas_plugin_api;

/**
 * Entry point into the plugin, which should fill in the \p api parameter with
 * pointers to all the functions.
 */
extern int llvm_cas_plugin_initialize_api(llvm_cas_plugin_api *api);

LLVM_C_EXTERN_C_END

#endif // LLVM_C_CASPLUGIN_H
