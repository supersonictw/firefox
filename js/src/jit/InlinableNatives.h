/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_InlinableNatives_h
#define jit_InlinableNatives_h

#define INLINABLE_NATIVE_LIST(_)                   \
  _(Array)                                         \
  _(ArrayIsArray)                                  \
  _(ArrayJoin)                                     \
  _(ArrayPop)                                      \
  _(ArrayShift)                                    \
  _(ArrayPush)                                     \
  _(ArraySlice)                                    \
                                                   \
  _(AtomicsCompareExchange)                        \
  _(AtomicsExchange)                               \
  _(AtomicsLoad)                                   \
  _(AtomicsStore)                                  \
  _(AtomicsAdd)                                    \
  _(AtomicsSub)                                    \
  _(AtomicsAnd)                                    \
  _(AtomicsOr)                                     \
  _(AtomicsXor)                                    \
  _(AtomicsIsLockFree)                             \
                                                   \
  _(Boolean)                                       \
                                                   \
  _(IntlGuardToCollator)                           \
  _(IntlGuardToDateTimeFormat)                     \
  _(IntlGuardToLocale)                             \
  _(IntlGuardToNumberFormat)                       \
  _(IntlGuardToPluralRules)                        \
  _(IntlGuardToRelativeTimeFormat)                 \
                                                   \
  _(MathAbs)                                       \
  _(MathFloor)                                     \
  _(MathCeil)                                      \
  _(MathRound)                                     \
  _(MathClz32)                                     \
  _(MathSqrt)                                      \
  _(MathATan2)                                     \
  _(MathHypot)                                     \
  _(MathMax)                                       \
  _(MathMin)                                       \
  _(MathPow)                                       \
  _(MathRandom)                                    \
  _(MathImul)                                      \
  _(MathFRound)                                    \
  _(MathSin)                                       \
  _(MathTan)                                       \
  _(MathCos)                                       \
  _(MathExp)                                       \
  _(MathLog)                                       \
  _(MathASin)                                      \
  _(MathATan)                                      \
  _(MathACos)                                      \
  _(MathLog10)                                     \
  _(MathLog2)                                      \
  _(MathLog1P)                                     \
  _(MathExpM1)                                     \
  _(MathSinH)                                      \
  _(MathTanH)                                      \
  _(MathCosH)                                      \
  _(MathASinH)                                     \
  _(MathATanH)                                     \
  _(MathACosH)                                     \
  _(MathSign)                                      \
  _(MathTrunc)                                     \
  _(MathCbrt)                                      \
                                                   \
  _(ReflectGetPrototypeOf)                         \
                                                   \
  _(RegExpMatcher)                                 \
  _(RegExpSearcher)                                \
  _(RegExpTester)                                  \
  _(IsRegExpObject)                                \
  _(RegExpPrototypeOptimizable)                    \
  _(RegExpInstanceOptimizable)                     \
  _(GetFirstDollarIndex)                           \
                                                   \
  _(String)                                        \
  _(StringCharCodeAt)                              \
  _(StringFromCharCode)                            \
  _(StringFromCodePoint)                           \
  _(StringCharAt)                                  \
  _(StringToLowerCase)                             \
  _(StringToUpperCase)                             \
                                                   \
  _(IntrinsicStringReplaceString)                  \
  _(IntrinsicStringSplitString)                    \
                                                   \
  _(Object)                                        \
  _(ObjectCreate)                                  \
  _(ObjectIs)                                      \
  _(ObjectToString)                                \
                                                   \
  _(TestBailout)                                   \
  _(TestAssertFloat32)                             \
  _(TestAssertRecoveredOnBailout)                  \
                                                   \
  _(IntrinsicUnsafeSetReservedSlot)                \
  _(IntrinsicUnsafeGetReservedSlot)                \
  _(IntrinsicUnsafeGetObjectFromReservedSlot)      \
  _(IntrinsicUnsafeGetInt32FromReservedSlot)       \
  _(IntrinsicUnsafeGetStringFromReservedSlot)      \
  _(IntrinsicUnsafeGetBooleanFromReservedSlot)     \
                                                   \
  _(IntrinsicIsCallable)                           \
  _(IntrinsicIsConstructor)                        \
  _(IntrinsicToObject)                             \
  _(IntrinsicIsObject)                             \
  _(IntrinsicIsCrossRealmArrayConstructor)         \
  _(IntrinsicToInteger)                            \
  _(IntrinsicToString)                             \
  _(IntrinsicIsConstructing)                       \
  _(IntrinsicSubstringKernel)                      \
  _(IntrinsicObjectHasPrototype)                   \
  _(IntrinsicFinishBoundFunctionInit)              \
  _(IntrinsicIsPackedArray)                        \
                                                   \
  _(IntrinsicGuardToArrayIterator)                 \
  _(IntrinsicGuardToMapIterator)                   \
  _(IntrinsicGuardToSetIterator)                   \
  _(IntrinsicGuardToStringIterator)                \
  _(IntrinsicGuardToRegExpStringIterator)          \
                                                   \
  _(IntrinsicGuardToMapObject)                     \
  _(IntrinsicGetNextMapEntryForIterator)           \
                                                   \
  _(IntrinsicGuardToSetObject)                     \
  _(IntrinsicGetNextSetEntryForIterator)           \
                                                   \
  _(IntrinsicNewArrayIterator)                     \
  _(IntrinsicNewStringIterator)                    \
  _(IntrinsicNewRegExpStringIterator)              \
  _(IntrinsicArrayIteratorPrototypeOptimizable)    \
                                                   \
  _(IntrinsicGuardToArrayBuffer)                   \
  _(IntrinsicArrayBufferByteLength)                \
  _(IntrinsicPossiblyWrappedArrayBufferByteLength) \
                                                   \
  _(IntrinsicGuardToSharedArrayBuffer)             \
                                                   \
  _(TypedArrayConstructor)                         \
  _(IntrinsicIsTypedArrayConstructor)              \
  _(IntrinsicIsTypedArray)                         \
  _(IntrinsicIsPossiblyWrappedTypedArray)          \
  _(IntrinsicTypedArrayLength)                     \
  _(IntrinsicPossiblyWrappedTypedArrayLength)      \
  _(IntrinsicTypedArrayByteOffset)                 \
  _(IntrinsicTypedArrayElementShift)               \
                                                   \
  _(IntrinsicObjectIsTypedObject)                  \
  _(IntrinsicObjectIsTransparentTypedObject)       \
  _(IntrinsicObjectIsOpaqueTypedObject)            \
  _(IntrinsicObjectIsTypeDescr)                    \
  _(IntrinsicTypeDescrIsSimpleType)                \
  _(IntrinsicTypeDescrIsArrayType)                 \
  _(IntrinsicSetTypedObjectOffset)

struct JSJitInfo;

namespace js {
namespace jit {

enum class InlinableNative : uint16_t {
#define ADD_NATIVE(native) native,
  INLINABLE_NATIVE_LIST(ADD_NATIVE)
#undef ADD_NATIVE
      Limit
};

#define ADD_NATIVE(native) extern const JSJitInfo JitInfo_##native;
INLINABLE_NATIVE_LIST(ADD_NATIVE)
#undef ADD_NATIVE

}  // namespace jit
}  // namespace js

#endif /* jit_InlinableNatives_h */
