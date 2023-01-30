/** @file
  Common functionality for the library interface for the autogen XML config header to call into to fetch a config value.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>

#define CONFIG_INCLUDE_CACHE
#include <Generated/ConfigClientGenerated.h>
#include <Generated/ConfigServiceGenerated.h>

#define CONFIG_KNOB_NAME_MAX_LENGTH  64

#include "ConfigKnobShimLibCommon.h"

/**
  GetConfigKnob returns the cached configuration knob if it is already cached, otherwise the config knob
  is fetched from variable storage and cached in a policy.

  If the config knob is not found in variable storage or the policy cache, this function returns EFI_NOT_FOUND
  and NULL in ConfigKnobData. In this case, the autogen header will return the default value for the profile.

  This function is only expected to be called from the autogen header code, all consumers of config knobs are
  expected to use the getter functions in the autogen header.

  @param[in]  ConfigKnobGuid      The GUID of the requested config knob.
  @param[in]  ConfigKnobName      The name of the requested config knob.
  @param[out] ConfigKnobData      The retrieved data of the requested config knob. The caller will allocate memory
                                  for this buffer and the caller is responsible for freeing it.
  @param[in] ConfigKnobDataSize   The allocated size of ConfigKnobData. This will be set to the correct value for the
                                  size of ConfigKnobData in the success case. This should equal ProfileDefaultSize, if
                                  not, EFI_BAD_BUFFER_SIZE will be returned.
  @param[in] ProfileDefaultValue  The profile defined default value of this knob, to be filled in if knob is not
                                  overridden in a cached policy or variable storage.

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_BAD_BUFFER_SIZE     Caller passed in a value of ConfigKnobDataSize that does not equal
                                  ProfileDefaultSize.
  @retval EFI_SUCCESS             The operation succeeds.

**/
STATIC
EFI_STATUS
GetConfigKnob (
  IN EFI_GUID  *ConfigKnobGuid,
  IN CHAR16    *ConfigKnobName,
  OUT VOID     *ConfigKnobData,
  IN UINTN     ConfigKnobDataSize,
  IN VOID      *ProfileDefaultValue
  )
{
  EFI_STATUS  Status;
  UINTN       VariableSize = ConfigKnobDataSize;

  if ((ConfigKnobGuid == NULL) || (ConfigKnobName == NULL) || (ConfigKnobData == NULL) ||
      (ConfigKnobDataSize == 0) || (ProfileDefaultValue == NULL))
  {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter!\n", __FUNCTION__));
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  // Check if it is in variable storage
  Status = GetConfigKnobFromVariable (ConfigKnobGuid, ConfigKnobName, ConfigKnobData, &VariableSize);

  if (ConfigKnobDataSize != VariableSize) {
    // we will only accept this variable if it is the correct size
    Status = EFI_NOT_FOUND;
  }

  if (EFI_ERROR (Status)) {
    // we didn't find the value in variable storage, which is expected if the knob has not been overridden, or
    // the size mismatched. In either case, we default to the profile value. This is a decision the platform may
    // choose not to take if it does not want to risk having different components having different values for config
    // knobs. Instead, this function could fail if the knob is not found (but still return the profile default if
    // the size mismatched)
    DEBUG ((
      DEBUG_VERBOSE,
      "%a: failed to find config knob %a with status %r. Expected size: %u, found size: %u."
      " Defaulting to profile defined value.\n",
      __FUNCTION__,
      ConfigKnobName,
      Status,
      ConfigKnobDataSize,
      VariableSize
      ));

    CopyMem (ConfigKnobData, ProfileDefaultValue, ConfigKnobDataSize);

    // we failed to get the variable, but successfully returned the profile value, this is a success case
    Status = EFI_SUCCESS;
  }

Exit:
  return Status;
}

/**
  GetKnobValue returns the raw knob value.

  This function is called by the generated functions and uses the generated knob metadata to call into the generic
  GetConfigKnob function to get the knob value.

  @param[in out]  Knob      The Knob enum indicating which knob should be fetched. Passed in by the autogenerated getters.
  
  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_BAD_BUFFER_SIZE     Caller passed in a value of ConfigKnobDataSize that does not equal
                                  ProfileDefaultSize.
  @retval EFI_SUCCESS             The operation succeeds.

**/
// Get the raw knob value
//

VOID *
EFIAPI
GetKnobValue (
  KNOB  Knob
  )
{
  ASSERT (Knob < KNOB_MAX);
  KNOB_DATA  *KnobData = &gKnobData[(UINTN)Knob];

  // Convert the name to a CHAR16 string
  CHAR16  UnicodeName[CONFIG_KNOB_NAME_MAX_LENGTH];
  
  AsciiStrToUnicodeStrS(KnobData->Name, UnicodeName, AsciiStrSize(KnobData->Name));

  // Get the knob value
  EFI_STATUS  Result = GetConfigKnob (
                         (EFI_GUID *)&KnobData->VendorNamespace,
                         UnicodeName,
                         KnobData->CacheValueAddress,
                         KnobData->ValueSize,
                         (VOID *)KnobData->DefaultValueAddress
                         );

  // This function should not fail for us
  // The only failure cases are invalid parameters, which should not happen
  // The platform implementing this can decide whether variable services not being available is a failure case or if
  // the profile default value is returned in that instance.
  ASSERT (Result == EFI_SUCCESS);

  // Validate the value from flash meets the constraints of the knob
  if (KnobData->Validator != NULL) {
    if (!KnobData->Validator (KnobData->CacheValueAddress)) {
      // If it doesn't, we will set the value to the default value
      DEBUG ((DEBUG_ERROR, "Config knob %a failed validation!\n", KnobData->Name));
      CopyMem (KnobData->CacheValueAddress, KnobData->DefaultValueAddress, KnobData->ValueSize);
    }
  }

  // Return a pointer to the data, the generated functions will cast this to the correct type
  return KnobData->CacheValueAddress;
}
