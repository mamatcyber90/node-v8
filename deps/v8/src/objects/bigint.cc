// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Parts of the implementation below:

// Copyright (c) 2014 the Dart project authors.  Please see the AUTHORS file [1]
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file [2].
//
// [1] https://github.com/dart-lang/sdk/blob/master/AUTHORS
// [2] https://github.com/dart-lang/sdk/blob/master/LICENSE

// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file [3].
//
// [3] https://golang.org/LICENSE

#include "src/objects/bigint.h"

#include "src/objects-inl.h"

namespace v8 {
namespace internal {

Handle<BigInt> BigInt::UnaryMinus(Handle<BigInt> x) {
  // Special case: There is no -0n.
  if (x->is_zero()) {
    return x;
  }
  Handle<BigInt> result = BigInt::Copy(x);
  result->set_sign(!x->sign());
  return result;
}

Handle<BigInt> BigInt::BitwiseNot(Handle<BigInt> x) {
  UNIMPLEMENTED();  // TODO(jkummerow): Implement.
}

MaybeHandle<BigInt> BigInt::Exponentiate(Handle<BigInt> base,
                                         Handle<BigInt> exponent) {
  UNIMPLEMENTED();  // TODO(jkummerow): Implement.
}

Handle<BigInt> BigInt::Multiply(Handle<BigInt> x, Handle<BigInt> y) {
  if (x->is_zero()) return x;
  if (y->is_zero()) return y;
  Handle<BigInt> result =
      x->GetIsolate()->factory()->NewBigInt(x->length() + y->length());
  for (int i = 0; i < x->length(); i++) {
    MultiplyAccumulate(y, x->digit(i), result, i);
  }
  result->set_sign(x->sign() != y->sign());
  result->RightTrim();
  return result;
}

MaybeHandle<BigInt> BigInt::Divide(Handle<BigInt> x, Handle<BigInt> y) {
  UNIMPLEMENTED();  // TODO(jkummerow): Implement.
}

MaybeHandle<BigInt> BigInt::Remainder(Handle<BigInt> x, Handle<BigInt> y) {
  UNIMPLEMENTED();  // TODO(jkummerow): Implement.
}

Handle<BigInt> BigInt::Add(Handle<BigInt> x, Handle<BigInt> y) {
  bool xsign = x->sign();
  if (xsign == y->sign()) {
    // x + y == x + y
    // -x + -y == -(x + y)
    return AbsoluteAdd(x, y, xsign);
  }
  // x + -y == x - y == -(y - x)
  // -x + y == y - x == -(x - y)
  if (AbsoluteCompare(x, y) >= 0) {
    return AbsoluteSub(x, y, xsign);
  }
  return AbsoluteSub(y, x, !xsign);
}

Handle<BigInt> BigInt::Subtract(Handle<BigInt> x, Handle<BigInt> y) {
  bool xsign = x->sign();
  if (xsign != y->sign()) {
    // x - (-y) == x + y
    // (-x) - y == -(x + y)
    return AbsoluteAdd(x, y, xsign);
  }
  // x - y == -(y - x)
  // (-x) - (-y) == y - x == -(x - y)
  if (AbsoluteCompare(x, y) >= 0) {
    return AbsoluteSub(x, y, xsign);
  }
  return AbsoluteSub(y, x, !xsign);
}

Handle<BigInt> BigInt::LeftShift(Handle<BigInt> x, Handle<BigInt> y) {
  UNIMPLEMENTED();  // TODO(jkummerow): Implement.
}

Handle<BigInt> BigInt::SignedRightShift(Handle<BigInt> x, Handle<BigInt> y) {
  UNIMPLEMENTED();  // TODO(jkummerow): Implement.
}

MaybeHandle<BigInt> BigInt::UnsignedRightShift(Handle<BigInt> x,
                                               Handle<BigInt> y) {
  UNIMPLEMENTED();  // TODO(jkummerow): Implement.
}

bool BigInt::LessThan(Handle<BigInt> x, Handle<BigInt> y) {
  UNIMPLEMENTED();  // TODO(jkummerow): Implement.
}

bool BigInt::Equal(BigInt* x, BigInt* y) {
  if (x->sign() != y->sign()) return false;
  if (x->length() != y->length()) return false;
  for (int i = 0; i < x->length(); i++) {
    if (x->digit(i) != y->digit(i)) return false;
  }
  return true;
}

Handle<BigInt> BigInt::BitwiseAnd(Handle<BigInt> x, Handle<BigInt> y) {
  UNIMPLEMENTED();  // TODO(jkummerow): Implement.
}

Handle<BigInt> BigInt::BitwiseXor(Handle<BigInt> x, Handle<BigInt> y) {
  UNIMPLEMENTED();  // TODO(jkummerow): Implement.
}

Handle<BigInt> BigInt::BitwiseOr(Handle<BigInt> x, Handle<BigInt> y) {
  UNIMPLEMENTED();  // TODO(jkummerow): Implement.
}

MaybeHandle<String> BigInt::ToString(Handle<BigInt> bigint, int radix) {
  // TODO(jkummerow): Support non-power-of-two radixes.
  if (!base::bits::IsPowerOfTwo(radix)) radix = 16;
  return ToStringBasePowerOfTwo(bigint, radix);
}

void BigInt::Initialize(int length, bool zero_initialize) {
  set_length(length);
  set_sign(false);
  if (zero_initialize) {
    memset(reinterpret_cast<void*>(reinterpret_cast<Address>(this) +
                                   kDigitsOffset - kHeapObjectTag),
           0, length * kDigitSize);
#if DEBUG
  } else {
    memset(reinterpret_cast<void*>(reinterpret_cast<Address>(this) +
                                   kDigitsOffset - kHeapObjectTag),
           0xbf, length * kDigitSize);
#endif
  }
}

// Private helpers for public methods.

Handle<BigInt> BigInt::AbsoluteAdd(Handle<BigInt> x, Handle<BigInt> y,
                                   bool result_sign) {
  if (x->length() < y->length()) return AbsoluteAdd(y, x, result_sign);
  if (x->is_zero()) {
    DCHECK(y->is_zero());
    return x;
  }
  if (y->is_zero()) {
    return result_sign == x->sign() ? x : UnaryMinus(x);
  }
  Handle<BigInt> result =
      x->GetIsolate()->factory()->NewBigIntRaw(x->length() + 1);
  digit_t carry = 0;
  int i = 0;
  for (; i < y->length(); i++) {
    digit_t new_carry = 0;
    digit_t sum = digit_add(x->digit(i), y->digit(i), &new_carry);
    sum = digit_add(sum, carry, &new_carry);
    result->set_digit(i, sum);
    carry = new_carry;
  }
  for (; i < x->length(); i++) {
    digit_t new_carry = 0;
    digit_t sum = digit_add(x->digit(i), carry, &new_carry);
    result->set_digit(i, sum);
    carry = new_carry;
  }
  result->set_digit(i, carry);
  result->set_sign(result_sign);
  result->RightTrim();
  return result;
}

Handle<BigInt> BigInt::AbsoluteSub(Handle<BigInt> x, Handle<BigInt> y,
                                   bool result_sign) {
  DCHECK(x->length() >= y->length());
  SLOW_DCHECK(AbsoluteCompare(x, y) >= 0);
  if (x->is_zero()) {
    DCHECK(y->is_zero());
    return x;
  }
  if (y->is_zero()) {
    return result_sign == x->sign() ? x : UnaryMinus(x);
  }
  Handle<BigInt> result = x->GetIsolate()->factory()->NewBigIntRaw(x->length());
  digit_t borrow = 0;
  int i = 0;
  for (; i < y->length(); i++) {
    digit_t new_borrow = 0;
    digit_t difference = digit_sub(x->digit(i), y->digit(i), &new_borrow);
    difference = digit_sub(difference, borrow, &new_borrow);
    result->set_digit(i, difference);
    borrow = new_borrow;
  }
  for (; i < x->length(); i++) {
    digit_t new_borrow = 0;
    digit_t difference = digit_sub(x->digit(i), borrow, &new_borrow);
    result->set_digit(i, difference);
    borrow = new_borrow;
  }
  DCHECK_EQ(0, borrow);
  result->set_sign(result_sign);
  result->RightTrim();
  return result;
}

int BigInt::AbsoluteCompare(Handle<BigInt> x, Handle<BigInt> y) {
  int diff = x->length() - y->length();
  if (diff != 0) return diff;
  int i = x->length() - 1;
  while (i >= 0 && x->digit(i) == y->digit(i)) i--;
  if (i < 0) return 0;
  return x->digit(i) > y->digit(i) ? 1 : -1;
}

// Multiplies {multiplicand} with {multiplier} and adds the result to
// {accumulator}, starting at {accumulator_index} for the least-significant
// digit.
// Callers must ensure that {accumulator} is big enough to hold the result.
void BigInt::MultiplyAccumulate(Handle<BigInt> multiplicand, digit_t multiplier,
                                Handle<BigInt> accumulator,
                                int accumulator_index) {
  // This is a minimum requirement; the DCHECK in the second loop below
  // will enforce more as needed.
  DCHECK(accumulator->length() > multiplicand->length() + accumulator_index);
  if (multiplier == 0L) return;
  digit_t carry = 0;
  digit_t high = 0;
  for (int i = 0; i < multiplicand->length(); i++, accumulator_index++) {
    digit_t acc = accumulator->digit(accumulator_index);
    digit_t new_carry = 0;
    // Add last round's carryovers.
    acc = digit_add(acc, high, &new_carry);
    acc = digit_add(acc, carry, &new_carry);
    // Compute this round's multiplication.
    digit_t m_digit = multiplicand->digit(i);
    digit_t low = digit_mul(multiplier, m_digit, &high);
    acc = digit_add(acc, low, &new_carry);
    // Store result and prepare for next round.
    accumulator->set_digit(accumulator_index, acc);
    carry = new_carry;
  }
  for (; carry != 0 || high != 0; accumulator_index++) {
    DCHECK(accumulator_index < accumulator->length());
    digit_t acc = accumulator->digit(accumulator_index);
    digit_t new_carry = 0;
    acc = digit_add(acc, high, &new_carry);
    high = 0;
    acc = digit_add(acc, carry, &new_carry);
    accumulator->set_digit(accumulator_index, acc);
    carry = new_carry;
  }
}

Handle<BigInt> BigInt::Copy(Handle<BigInt> source) {
  int length = source->length();
  Handle<BigInt> result = source->GetIsolate()->factory()->NewBigIntRaw(length);
  memcpy(result->address() + HeapObject::kHeaderSize,
         source->address() + HeapObject::kHeaderSize,
         SizeFor(length) - HeapObject::kHeaderSize);
  return result;
}

void BigInt::RightTrim() {
  int old_length = length();
  int new_length = old_length;
  while (digit(new_length - 1) == 0) new_length--;
  int to_trim = old_length - new_length;
  if (to_trim == 0) return;
  int size_delta = to_trim * kDigitSize;
  Address new_end = this->address() + SizeFor(new_length);
  Heap* heap = GetHeap();
  heap->CreateFillerObjectAt(new_end, size_delta, ClearRecordedSlots::kNo);
  // Canonicalize -0n.
  if (new_length == 0) set_sign(false);
  set_length(new_length);
}

static const char kConversionChars[] = "0123456789abcdefghijklmnopqrstuvwxyz";

// TODO(jkummerow): Add more tests for this when we have a way to construct
// multi-digit BigInts.
MaybeHandle<String> BigInt::ToStringBasePowerOfTwo(Handle<BigInt> x,
                                                   int radix) {
  STATIC_ASSERT(base::bits::IsPowerOfTwo(kDigitBits));
  DCHECK(base::bits::IsPowerOfTwo(radix));
  DCHECK(radix >= 2 && radix <= 32);
  Factory* factory = x->GetIsolate()->factory();
  // TODO(jkummerow): check in caller?
  if (x->is_zero()) return factory->NewStringFromStaticChars("0");

  const int len = x->length();
  const bool sign = x->sign();
  const int bits_per_char = base::bits::CountTrailingZeros32(radix);
  const int char_mask = radix - 1;
  const int chars_per_digit = kDigitBits / bits_per_char;
  // Compute the number of chars needed to represent the most significant
  // bigint digit.
  int chars_for_msd = 0;
  for (digit_t msd = x->digit(len - 1); msd != 0; msd >>= bits_per_char) {
    chars_for_msd++;
  }
  // All other digits need chars_per_digit characters; a leading "-" needs one.
  if ((String::kMaxLength - chars_for_msd - sign) / chars_per_digit < len - 1) {
    CHECK(false);  // TODO(jkummerow): Throw instead of crashing.
  }
  const int chars = chars_for_msd + (len - 1) * chars_per_digit + sign;

  Handle<SeqOneByteString> result =
      factory->NewRawOneByteString(chars).ToHandleChecked();
  uint8_t* buffer = result->GetChars();
  // Print the number into the string, starting from the last position.
  int pos = chars - 1;
  for (int i = 0; i < len - 1; i++) {
    digit_t digit = x->digit(i);
    for (int j = 0; j < chars_per_digit; j++) {
      buffer[pos--] = kConversionChars[digit & char_mask];
      digit >>= bits_per_char;
    }
  }
  // Print the most significant digit.
  for (digit_t msd = x->digit(len - 1); msd != 0; msd >>= bits_per_char) {
    buffer[pos--] = kConversionChars[msd & char_mask];
  }
  if (sign) buffer[pos--] = '-';
  DCHECK(pos == -1);
  return result;
}

// Digit arithmetic helpers.

#if V8_TARGET_ARCH_32_BIT
#define HAVE_TWODIGIT_T 1
typedef uint64_t twodigit_t;
#elif defined(__SIZEOF_INT128__)
// Both Clang and GCC support this on x64.
#define HAVE_TWODIGIT_T 1
typedef __uint128_t twodigit_t;
#endif

// {carry} must point to an initialized digit_t and will either be incremented
// by one or left alone.
inline BigInt::digit_t BigInt::digit_add(digit_t a, digit_t b, digit_t* carry) {
#if HAVE_TWODIGIT_T
  twodigit_t result = static_cast<twodigit_t>(a) + static_cast<twodigit_t>(b);
  *carry += result >> kDigitBits;
  return static_cast<digit_t>(result);
#else
  digit_t result = a + b;
  if (result < a) *carry += 1;
  return result;
#endif
}

// {borrow} must point to an initialized digit_t and will either be incremented
// by one or left alone.
inline BigInt::digit_t BigInt::digit_sub(digit_t a, digit_t b,
                                         digit_t* borrow) {
#if HAVE_TWODIGIT_T
  twodigit_t result = static_cast<twodigit_t>(a) - static_cast<twodigit_t>(b);
  *borrow += (result >> kDigitBits) & 1;
  return static_cast<digit_t>(result);
#else
  digit_t result = a - b;
  if (result > a) *borrow += 1;
  return static_cast<digit_t>(result);
#endif
}

// Returns the low half of the result. High half is in {high}.
inline BigInt::digit_t BigInt::digit_mul(digit_t a, digit_t b, digit_t* high) {
#if HAVE_TWODIGIT_T
  twodigit_t result = static_cast<twodigit_t>(a) * static_cast<twodigit_t>(b);
  *high = result >> kDigitBits;
  return static_cast<digit_t>(result);
#else
  // Multiply in half-pointer-sized chunks.
  // For inputs [AH AL]*[BH BL], the result is:
  //
  //            [AL*BL]  // r_low
  //    +    [AL*BH]     // r_mid1
  //    +    [AH*BL]     // r_mid2
  //    + [AH*BH]        // r_high
  //    = [R4 R3 R2 R1]  // high = [R4 R3], low = [R2 R1]
  //
  // Where of course we must be careful with carries between the columns.
  digit_t a_low = a & kHalfDigitMask;
  digit_t a_high = a >> kHalfDigitBits;
  digit_t b_low = b & kHalfDigitMask;
  digit_t b_high = b >> kHalfDigitBits;

  digit_t r_low = a_low * b_low;
  digit_t r_mid1 = a_low * b_high;
  digit_t r_mid2 = a_high * b_low;
  digit_t r_high = a_high * b_high;

  digit_t carry = 0;
  digit_t low = digit_add(r_low, r_mid1 & kHalfDigitMask, &carry);
  low = digit_add(low, r_mid2 & kHalfDigitMask, &carry);
  *high =
      (r_mid1 >> kHalfDigitBits) + (r_mid2 >> kHalfDigitBits) + r_high + carry;
  return low;
#endif
}

#undef HAVE_TWODIGIT_T

#ifdef OBJECT_PRINT
void BigInt::BigIntPrint(std::ostream& os) {
  DisallowHeapAllocation no_gc;
  HeapObject::PrintHeader(os, "BigInt");
  int len = length();
  os << "- length: " << len << "\n";
  os << "- sign: " << sign() << "\n";
  if (len > 0) {
    os << "- digits:";
    for (int i = 0; i < len; i++) {
      os << "\n    0x" << std::hex << digit(i);
    }
    os << std::dec << "\n";
  }
}
#endif  // OBJECT_PRINT

}  // namespace internal
}  // namespace v8
