/** @file
  Standard structure definitions shared by core code and KnobService.py
  autogenerated code.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef CONFIG_STD_STRUCT_DEFS_LIB_H_
#define CONFIG_STD_STRUCT_DEFS_LIB_H_

/*
 * Standard structure definitons that KnobService.py autogenerated
 * headers and core code use.
 */
typedef BOOLEAN (KNOB_VALIDATION_FN)(
  CONST VOID *
  );

typedef struct {
  UINTN                 Knob;
  CONST VOID            *DefaultValueAddress;
  VOID                  *CacheValueAddress;
  UINTN                 ValueSize;
  CONST CHAR8           *Name;
  UINTN                 NameSize;
  EFI_GUID              VendorNamespace;
  INTN                  Attributes;
  KNOB_VALIDATION_FN    *Validator;
} KNOB_DATA;

#endif // CONFIG_STD_STRUCT_DEFS.h
