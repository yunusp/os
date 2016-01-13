/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    softfp.c

Abstract:

    This module implements support for software based floating point operations.

    This implementation is a derivative of John R. Hauser's SoftFloat package,
    version 2b.

Author:

    Evan Green 25-Jul-2013

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "rtlp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro packs the given sign, exponent, and significand, returning the
// ULONG form of a float.
//

#define FLOAT_PACK(_Sign, _Exponent, _Significand)                  \
    (((ULONG)(_Sign)) << FLOAT_SIGN_BIT_SHIFT) +                    \
    (((ULONG)(_Exponent)) << FLOAT_EXPONENT_SHIFT) + (_Significand)

//
// This macro gets the sign out of a float parts structure.
//

#define FLOAT_GET_SIGN(_Parts) ((_Parts).Ulong >> FLOAT_SIGN_BIT_SHIFT)

//
// This macro gets the exponent out of a float parts structure.
//

#define FLOAT_GET_EXPONENT(_Parts) \
    (((_Parts).Ulong & FLOAT_EXPONENT_MASK) >> FLOAT_EXPONENT_SHIFT)

//
// This macro gets the significand out of a float parts structure.
//

#define FLOAT_GET_SIGNIFICAND(_Parts) ((_Parts).Ulong & FLOAT_VALUE_MASK)

//
// This macro returns non-zero if the given value (in float parts) is a
// signaling NaN.
//

#define FLOAT_IS_SIGNALING_NAN(_Parts)                                      \
    ((((_Parts).Ulong >> (FLOAT_EXPONENT_SHIFT - 1) & 0x1FF) == 0x1FE) &&   \
     ((_Parts).Ulong == 0x003FFFFF))

//
// This macro returns the sign bit of the given double parts.
//

#define DOUBLE_GET_SIGN(_Parts) \
    (((_Parts).Ulong.High & (DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT)) != 0)

//
// This macro packs up the given sign, exponent, and significand, returning the
// ULONGLONG form of a double.
//

#define DOUBLE_PACK(_Sign, _Exponent, _Significand)         \
    (((ULONGLONG)(_Sign) << DOUBLE_SIGN_BIT_SHIFT) +        \
     ((ULONGLONG)(_Exponent) << DOUBLE_EXPONENT_SHIFT) +    \
     (ULONGLONG)(_Significand))

//
//
// This macro extracts the exponent from the given double parts structure.
//

#define DOUBLE_GET_EXPONENT(_Parts)                         \
    (((_Parts).Ulong.High &                                 \
      (DOUBLE_EXPONENT_MASK >> DOUBLE_HIGH_WORD_SHIFT)) >>  \
     (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT))

//
// This macro extracts the significand portion of the given double parts.
//

#define DOUBLE_GET_SIGNIFICAND(_Parts) ((_Parts).Ulonglong & DOUBLE_VALUE_MASK)

//
// This macro returns non-zero if the given value (in double parts) is NaN.
//

#define DOUBLE_IS_NAN(_Parts) \
    ((ULONGLONG)((_Parts).Ulonglong << 1) > 0xFFE0000000000000ULL)

//
// This macro returns non-zero if the given value (in double parts) is a
// signaling NaN.
//

#define DOUBLE_IS_SIGNALING_NAN(_Parts)                 \
    (((((_Parts).Ulonglong >> 51) & 0xFFF) == 0xFFE) && \
     (((_Parts).Ulonglong & 0x0007FFFFFFFFFFFFULL) != 0))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define soft float exception flags.
//

#define SOFT_FLOAT_INEXACT          0x00000001
#define SOFT_FLOAT_UNDERFLOW        0x00000002
#define SOFT_FLOAT_OVERFLOW         0x00000004
#define SOFT_FLOAT_DIVIDE_BY_ZERO   0x00000008
#define SOFT_FLOAT_INVALID          0x00000010

//
// Define a default NaN value.
//

#define FLOAT_DEFAULT_NAN  0xFFC00000UL
#define DOUBLE_DEFAULT_NAN 0xFFF8000000000000ULL

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SOFT_FLOAT_ROUNDING_MODE {
    SoftFloatRoundNearestEven = 0,
    SoftFloatRoundDown        = 1,
    SoftFloatRoundUp          = 2,
    SoftFloatRoundToZero      = 3
} SOFT_FLOAT_ROUNDING_MODE, *PSOFT_FLOAT_ROUNDING_MODE;

typedef enum _SOFT_FLOAT_DETECT_TININESS {
    SoftFloatTininessAfterRounding  = 0,
    SoftFloatTininessBeforeRounding = 1,
} SOFT_FLOAT_DETECT_TININESS, *PSOFT_FLOAT_DETECT_TININESS;

typedef struct _COMMON_NAN {
    CHAR Sign;
    ULONGLONG High;
    ULONGLONG Low;
} COMMON_NAN, *PCOMMON_NAN;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
RtlpSoftFloatRaise (
    ULONG Flags
    );

double
RtlpDoubleAdd (
    DOUBLE_PARTS Value1,
    DOUBLE_PARTS Value2,
    CHAR Sign
    );

double
RtlpDoubleSubtract (
    DOUBLE_PARTS Value1,
    DOUBLE_PARTS Value2,
    CHAR Sign
    );

VOID
RtlpMultiply64To128 (
    ULONGLONG Value1,
    ULONGLONG Value2,
    PULONGLONG ResultHigh,
    PULONGLONG ResultLow
    );

ULONGLONG
RtlpEstimateDivide128To64 (
    ULONGLONG DividendHigh,
    ULONGLONG DividendLow,
    ULONGLONG Divisor
    );

ULONGLONG
RtlpEstimateSquareRoot32 (
    SHORT ValueExponent,
    ULONG Value
    );

VOID
RtlpAdd128 (
    ULONGLONG Value1High,
    ULONGLONG Value1Low,
    ULONGLONG Value2High,
    ULONGLONG Value2Low,
    PULONGLONG ResultHigh,
    PULONGLONG ResultLow
    );

VOID
RtlpSubtract128 (
    ULONGLONG Value1High,
    ULONGLONG Value1Low,
    ULONGLONG Value2High,
    ULONGLONG Value2Low,
    PULONGLONG ResultHigh,
    PULONGLONG ResultLow
    );

double
RtlpDoublePropagateNan (
    DOUBLE_PARTS Value1,
    DOUBLE_PARTS Value2
    );

LONG
RtlpRoundAndPack32 (
    CHAR SignBit,
    ULONGLONG AbsoluteValue
    );

LONGLONG
RtlpRoundAndPack64 (
    CHAR SignBit,
    ULONGLONG AbsoluteValueHigh,
    ULONGLONG AbsoluteValueLow
    );

double
RtlpRoundAndPackDouble (
    CHAR SignBit,
    SHORT Exponent,
    ULONGLONG Significand
    );

float
RtlpRoundAndPackFloat (
    CHAR SignBit,
    SHORT Exponent,
    ULONG Significand
    );

double
RtlpNormalizeRoundAndPackDouble (
    CHAR SignBit,
    SHORT Exponent,
    ULONGLONG Significand
    );

VOID
RtlpNormalizeDoubleSubnormal (
    ULONGLONG Significand,
    PSHORT Exponent,
    PULONGLONG Result
    );

VOID
RtlpNormalizeFloatSubnormal (
    ULONG Significand,
    PSHORT Exponent,
    PULONG Result
    );

CHAR
RtlpCountLeadingZeros32 (
    ULONG Value
    );

CHAR
RtlpCountLeadingZeros64 (
    ULONGLONG Value
    );

VOID
RtlpShift32RightJamming (
    ULONG Value,
    SHORT Count,
    PULONG Result
    );

VOID
RtlpShift64RightJamming (
    ULONGLONG Value,
    SHORT Count,
    PULONGLONG Result
    );

VOID
RtlpShift64ExtraRightJamming (
    ULONGLONG ValueInteger,
    ULONGLONG ValueFraction,
    SHORT Count,
    PULONGLONG ResultHigh,
    PULONGLONG ResultLow
    );

COMMON_NAN
RtlpDoubleToCommonNan (
    DOUBLE_PARTS Value
    );

COMMON_NAN
RtlpFloatToCommonNan (
    FLOAT_PARTS Value
    );

float
RtlpCommonNanToFloat (
    COMMON_NAN Nan
    );

double
RtlpCommonNanToDouble (
    COMMON_NAN Nan
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define global exception flags.
//

ULONG RtlSoftFloatExceptionFlags = 0;

//
// Define the soft float rounding mode.
//

SOFT_FLOAT_ROUNDING_MODE RtlRoundingMode = SoftFloatRoundNearestEven;

//
// Define the method for detecting very small values.
//

SOFT_FLOAT_DETECT_TININESS RtlTininessDetection =
                                                SoftFloatTininessAfterRounding;

//
// Define constants used for estimating the square root.
//

const USHORT RtlSquareRootOddAdjustments[] = {
    0x0004, 0x0022, 0x005D, 0x00B1, 0x011D, 0x019F, 0x0236, 0x02E0,
    0x039C, 0x0468, 0x0545, 0x0631, 0x072B, 0x0832, 0x0946, 0x0A67
};

const USHORT RtlSquareRootEvenAdjustments[] = {
    0x0A2D, 0x08AF, 0x075A, 0x0629, 0x051A, 0x0429, 0x0356, 0x029E,
    0x0200, 0x0179, 0x0109, 0x00AF, 0x0068, 0x0034, 0x0012, 0x0002
};

//
// ------------------------------------------------------------------ Functions
//

RTL_API
BOOL
RtlDoubleIsNan (
    double Value
    )

/*++

Routine Description:

    This routine determines if the given value is Not a Number.

Arguments:

    Value - Supplies the floating point value to query.

Return Value:

    TRUE if the given value is Not a Number.

    FALSE otherwise.

--*/

{

    DOUBLE_PARTS Parts;

    Parts.Double = Value;
    if (DOUBLE_GET_EXPONENT(Parts) == DOUBLE_NAN_EXPONENT) {
        return TRUE;
    }

    return FALSE;
}

RTL_API
double
RtlDoubleConvertFromInteger32 (
    LONG Integer
    )

/*++

Routine Description:

    This routine converts the given signed 32-bit integer into a double.

Arguments:

    Integer - Supplies the integer to convert to a double.

Return Value:

    Returns the double equivalent to the given integer.

--*/

{

    ULONG AbsoluteInteger;
    DOUBLE_PARTS Parts;
    CHAR ShiftCount;
    CHAR Sign;
    ULONGLONG Significand;

    if (Integer == 0) {
        Parts.Ulonglong = 0;
        return Parts.Double;
    }

    AbsoluteInteger = Integer;
    Sign = 0;
    if (Integer < 0) {
        AbsoluteInteger = -Integer;
        Sign = 1;
    }

    ShiftCount = RtlpCountLeadingZeros32(AbsoluteInteger) + 21;
    Significand = AbsoluteInteger;
    Parts.Ulonglong = DOUBLE_PACK(Sign,
                                  0x432 - ShiftCount,
                                  Significand << ShiftCount);

    return Parts.Double;
}

RTL_API
double
RtlDoubleConvertFromUnsignedInteger32 (
    ULONG Integer
    )

/*++

Routine Description:

    This routine converts the given unsigned 32-bit integer into a double.

Arguments:

    Integer - Supplies the integer to convert to a double.

Return Value:

    Returns the double equivalent to the given integer.

--*/

{

    DOUBLE_PARTS Parts;
    CHAR ShiftCount;
    ULONGLONG Significand;

    if (Integer == 0) {
        Parts.Ulonglong = 0;
        return Parts.Double;
    }

    ShiftCount = RtlpCountLeadingZeros32(Integer) + 21;
    Significand = Integer;
    Parts.Ulonglong = DOUBLE_PACK(0,
                                  0x432 - ShiftCount,
                                  Significand << ShiftCount);

    return Parts.Double;
}

RTL_API
double
RtlDoubleConvertFromInteger64 (
    LONGLONG Integer
    )

/*++

Routine Description:

    This routine converts the given signed 64-bit integer into a double.

Arguments:

    Integer - Supplies the integer to convert to a double.

Return Value:

    Returns the double equivalent to the given integer.

--*/

{

    DOUBLE_PARTS Parts;
    CHAR Sign;

    if (Integer == 0) {
        Parts.Ulonglong = 0;
        return Parts.Double;
    }

    if (Integer == (LONGLONG)MIN_LONGLONG) {
        Parts.Ulonglong = DOUBLE_PACK(1, 0x43E, 0);
        return Parts.Double;
    }

    Sign = 0;
    if (Integer < 0) {
        Sign = 1;
        Integer = -Integer;
    }

    return RtlpNormalizeRoundAndPackDouble(Sign, 0x43C, Integer);
}

RTL_API
double
RtlDoubleConvertFromUnsignedInteger64 (
    ULONGLONG Integer
    )

/*++

Routine Description:

    This routine converts the given unsigned 64-bit integer into a double.

Arguments:

    Integer - Supplies the unsigned integer to convert to a double.

Return Value:

    Returns the double equivalent to the given integer.

--*/

{

    DOUBLE_PARTS Parts;

    if (Integer == 0) {
        Parts.Ulonglong = 0;
        return Parts.Double;
    }

    return RtlpNormalizeRoundAndPackDouble(0, 0x43C, Integer);
}

RTL_API
LONG
RtlDoubleConvertToInteger32 (
    double Double
    )

/*++

Routine Description:

    This routine converts the given double into a signed 32 bit integer.

Arguments:

    Double - Supplies the double to convert.

Return Value:

    Returns the integer, rounded according to the current rounding mode.

--*/

{

    SHORT Exponent;
    DOUBLE_PARTS Parts;
    SHORT ShiftCount;
    CHAR Sign;
    ULONGLONG Significand;

    Parts.Double = Double;
    Significand = DOUBLE_GET_SIGNIFICAND(Parts);
    Exponent = DOUBLE_GET_EXPONENT(Parts);
    Sign = DOUBLE_GET_SIGN(Parts);
    if ((Exponent == DOUBLE_NAN_EXPONENT) && (Significand != 0)) {
        Sign = 0;
    }

    if (Exponent != 0) {
        Significand |= 1ULL << DOUBLE_EXPONENT_SHIFT;;
    }

    ShiftCount = 0x42C - Exponent;
    if (ShiftCount > 0) {
        RtlpShift64RightJamming(Significand, ShiftCount, &Significand);
    }

    return RtlpRoundAndPack32(Sign, Significand);
}

RTL_API
LONG
RtlDoubleConvertToInteger32RoundToZero (
    double Double
    )

/*++

Routine Description:

    This routine converts the given double into a signed 32 bit integer. It
    always rounds towards zero.

Arguments:

    Double - Supplies the double to convert.

Return Value:

    Returns the integer, rounded towards zero.

--*/

{

    SHORT Exponent;
    DOUBLE_PARTS Parts;
    LONG Result;
    ULONGLONG SavedSignificand;
    SHORT ShiftCount;
    CHAR Sign;
    ULONGLONG Significand;

    Parts.Double = Double;
    Significand = DOUBLE_GET_SIGNIFICAND(Parts);
    Exponent = DOUBLE_GET_EXPONENT(Parts);
    Sign = DOUBLE_GET_SIGN(Parts);
    if (Exponent > 0x41E) {
        if ((Exponent == DOUBLE_NAN_EXPONENT) && (Significand != 0)) {
            Sign = 0;
        }

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        if (Sign != 0) {
            return MIN_LONG;

        } else {
            return MAX_LONG;
        }

    } else if (Exponent < DOUBLE_EXPONENT_BIAS) {
        if ((Exponent != 0) || (Significand != 0)) {
            RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
        }

        return 0;
    }

    Significand |= 1ULL << DOUBLE_EXPONENT_SHIFT;
    ShiftCount = 0x433 - Exponent;
    SavedSignificand = Significand;
    Significand >>= ShiftCount;
    Result = Significand;
    if (Sign != 0) {
        Result = -Result;
    }

    //
    // Handle signed overflow, assuming it wraps (which actually it doesn't
    // have to do in C).
    //

    if ((Result < 0) ^ Sign) {
        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        if (Sign != 0) {
            return MIN_LONG;

        } else {
            return MAX_LONG;
        }
    }

    if ((Significand << ShiftCount) != SavedSignificand) {
        RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
    }

    return Result;
}

RTL_API
LONGLONG
RtlDoubleConvertToInteger64 (
    double Double
    )

/*++

Routine Description:

    This routine converts the given double into a signed 64 bit integer. If the
    value is NaN, then the largest positive integer is returned.

Arguments:

    Double - Supplies the double to convert.

Return Value:

    Returns the integer, rounded according to the current rounding mode.

--*/

{

    SHORT Exponent;
    DOUBLE_PARTS Parts;
    SHORT ShiftCount;
    CHAR Sign;
    ULONGLONG Significand;
    ULONGLONG SignificandExtra;

    Parts.Double = Double;
    Significand = DOUBLE_GET_SIGNIFICAND(Parts);
    Exponent = DOUBLE_GET_EXPONENT(Parts);
    Sign = DOUBLE_GET_SIGN(Parts);
    if (Exponent != 0) {
        Significand |= 1ULL << DOUBLE_EXPONENT_SHIFT;;
    }

    ShiftCount = 0x433 - Exponent;
    if (ShiftCount <= 0) {
        if (Exponent > 0x43E) {
            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            if ((Sign == 0) ||
                ((Exponent == DOUBLE_NAN_EXPONENT) &&
                 (Significand != (1ULL << DOUBLE_EXPONENT_SHIFT)))) {

                return MAX_LONGLONG;
            }

            return MIN_LONGLONG;
        }

        SignificandExtra = 0;
        Significand <<= -ShiftCount;

    } else {
        RtlpShift64ExtraRightJamming(Significand,
                                     0,
                                     ShiftCount,
                                     &Significand,
                                     &SignificandExtra);
    }

    return RtlpRoundAndPack64(Sign, Significand, SignificandExtra);
}

RTL_API
LONGLONG
RtlDoubleConvertToInteger64RoundToZero (
    double Double
    )

/*++

Routine Description:

    This routine converts the given double into a signed 64 bit integer. If the
    value is NaN, then the largest positive integer is returned. This routine
    always rounds towards zero.

Arguments:

    Double - Supplies the double to convert.

Return Value:

    Returns the integer, rounded towards zero.

--*/

{

    SHORT Exponent;
    DOUBLE_PARTS Parts;
    LONGLONG Result;
    SHORT ShiftCount;
    CHAR Sign;
    ULONGLONG Significand;

    Parts.Double = Double;
    Significand = DOUBLE_GET_SIGNIFICAND(Parts);
    Exponent = DOUBLE_GET_EXPONENT(Parts);
    Sign = DOUBLE_GET_SIGN(Parts);
    if (Exponent != 0) {
        Significand |= 1ULL << DOUBLE_EXPONENT_SHIFT;;
    }

    ShiftCount = Exponent - 0x433;
    if (ShiftCount >= 0) {
        if (Exponent >= 0x43E) {
            if (Parts.Ulonglong != 0xC3E0000000000000ULL) {
                RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
                if ((Sign == 0) ||
                    ((Exponent == DOUBLE_NAN_EXPONENT) &&
                     (Significand != (1ULL << DOUBLE_EXPONENT_SHIFT)))) {

                    return MAX_LONGLONG;
                }
            }

            return MIN_LONGLONG;
        }

        Result = Significand << ShiftCount;

    } else {
        if (Exponent < (DOUBLE_EXPONENT_BIAS - 1)) {
            if ((Exponent != 0) || (Significand != 0)) {
                RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
            }

            return 0;
        }

        Result = Significand >> (-ShiftCount);
        if ((LONGLONG)(Significand << (ShiftCount & 0x3F))) {
            RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
        }
    }

    if (Sign != 0) {
        Result = -Result;
    }

    return Result;
}

RTL_API
float
RtlDoubleConvertToFloat (
    double Double
    )

/*++

Routine Description:

    This routine converts the given double into a float (32 bit floating
    point number).

Arguments:

    Double - Supplies the double to convert.

Return Value:

    Returns the float equivalent.

--*/

{

    SHORT Exponent;
    DOUBLE_PARTS Parts;
    FLOAT_PARTS Result;
    ULONG ResultSignificand;
    CHAR Sign;
    ULONGLONG Significand;

    Parts.Double = Double;
    Significand = DOUBLE_GET_SIGNIFICAND(Parts);
    Exponent = DOUBLE_GET_EXPONENT(Parts);
    Sign = DOUBLE_GET_SIGN(Parts);
    if (Exponent == DOUBLE_NAN_EXPONENT) {
        if (Significand != 0) {
            return RtlpCommonNanToFloat(RtlpDoubleToCommonNan(Parts));
        }

        Result.Ulong = FLOAT_PACK(Sign, 0xFF, 0);
        return Result.Float;
    }

    RtlpShift64RightJamming(Significand, 22, &Significand);
    ResultSignificand = Significand;
    if ((Exponent != 0) || (ResultSignificand != 0)) {
        ResultSignificand |= 0x40000000;
        Exponent -= 0x381;
    }

    return RtlpRoundAndPackFloat(Sign, Exponent, ResultSignificand);
}

RTL_API
double
RtlDoubleAdd (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine adds two doubles together.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value.

Return Value:

    Returns the sum of the two values.

--*/

{

    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    Sign1 = DOUBLE_GET_SIGN(Parts1);
    Sign2 = DOUBLE_GET_SIGN(Parts2);
    if (Sign1 == Sign2) {
        return RtlpDoubleAdd(Parts1, Parts2, Sign1);
    }

    return RtlpDoubleSubtract(Parts1, Parts2, Sign1);
}

RTL_API
double
RtlDoubleSubtract (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine subtracts two doubles from each other.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value, the value to subtract.

Return Value:

    Returns the difference of the two values.

--*/

{

    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    Sign1 = DOUBLE_GET_SIGN(Parts1);
    Sign2 = DOUBLE_GET_SIGN(Parts2);
    if (Sign1 == Sign2) {
        return RtlpDoubleSubtract(Parts1, Parts2, Sign1);
    }

    return RtlpDoubleAdd(Parts1, Parts2, Sign1);
}

RTL_API
double
RtlDoubleMultiply (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine multiplies two doubles together.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value.

Return Value:

    Returns the product of the two values.

--*/

{

    SHORT Exponent1;
    SHORT Exponent2;
    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;
    DOUBLE_PARTS Result;
    SHORT ResultExponent;
    BOOL ResultSign;
    ULONGLONG ResultSignificand0;
    ULONGLONG ResultSignificand1;
    BOOL Sign1;
    BOOL Sign2;
    ULONGLONG Significand1;
    ULONGLONG Significand2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    Significand1 = DOUBLE_GET_SIGNIFICAND(Parts1);
    Exponent1 = DOUBLE_GET_EXPONENT(Parts1);
    Sign1 = DOUBLE_GET_SIGN(Parts1);
    Significand2 = DOUBLE_GET_SIGNIFICAND(Parts2);
    Exponent2 = DOUBLE_GET_EXPONENT(Parts2);
    Sign2 = DOUBLE_GET_SIGN(Parts2);
    ResultSign = Sign1 ^ Sign2;
    if (Exponent1 == DOUBLE_NAN_EXPONENT) {
        if ((Significand1 != 0) ||
            ((Exponent2 == DOUBLE_NAN_EXPONENT) && (Significand2 != 0))) {

            return RtlpDoublePropagateNan(Parts1, Parts2);
        }

        if ((Exponent2 | Significand2) == 0) {
            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            Result.Ulonglong = DOUBLE_DEFAULT_NAN;
            return Result.Double;
        }

        Result.Ulonglong = DOUBLE_PACK(ResultSign, DOUBLE_NAN_EXPONENT, 0);
        return Result.Double;
    }

    if (Exponent2 == DOUBLE_NAN_EXPONENT) {
        if (Significand2 != 0) {
            return RtlpDoublePropagateNan(Parts1, Parts2);
        }

        if ((Exponent1 | Significand1) == 0) {
            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            Result.Ulonglong = DOUBLE_DEFAULT_NAN;
            return Result.Double;
        }

        Result.Ulonglong = DOUBLE_PACK(ResultSign, DOUBLE_NAN_EXPONENT, 0);
        return Result.Double;
    }

    if (Exponent1 == 0) {
        if (Significand1 == 0) {
            Result.Ulonglong = DOUBLE_PACK(ResultSign, 0, 0);
            return Result.Double;
        }

        RtlpNormalizeDoubleSubnormal(Significand1, &Exponent1, &Significand1);
    }

    if (Exponent2 == 0) {
        if (Significand2 == 0) {
            Result.Ulonglong = DOUBLE_PACK(ResultSign, 0, 0);
            return Result.Double;
        }

        RtlpNormalizeDoubleSubnormal(Significand2, &Exponent2, &Significand2);
    }

    ResultExponent = Exponent1 + Exponent2 - DOUBLE_EXPONENT_BIAS;
    Significand1 = (Significand1 | 0x0010000000000000ULL) << 10;
    Significand2 = (Significand2 | 0x0010000000000000ULL) << 11;
    RtlpMultiply64To128(Significand1,
                        Significand2,
                        &ResultSignificand0,
                        &ResultSignificand1);

    if (ResultSignificand1 != 0) {
        ResultSignificand0 |= 0x1;
    }

    if ((LONGLONG)(ResultSignificand0 << 1) >= 0) {
        ResultSignificand0 <<= 1;
        ResultExponent -= 1;
    }

    Result.Double = RtlpRoundAndPackDouble(ResultSign,
                                           ResultExponent,
                                           ResultSignificand0);

    return Result.Double;
}

RTL_API
double
RtlDoubleDivide (
    double Dividend,
    double Divisor
    )

/*++

Routine Description:

    This routine divides one double into another.

Arguments:

    Dividend - Supplies the numerator.

    Divisor - Supplies the denominator.

Return Value:

    Returns the quotient of the two values.

--*/

{

    SHORT DividendExponent;
    DOUBLE_PARTS DividendParts;
    BOOL DividendSign;
    ULONGLONG DividendSignificand;
    SHORT DivisorExponent;
    DOUBLE_PARTS DivisorParts;
    BOOL DivisorSign;
    ULONGLONG DivisorSignificand;
    ULONGLONG Remainder0;
    ULONGLONG Remainder1;
    DOUBLE_PARTS Result;
    SHORT ResultExponent;
    BOOL ResultSign;
    ULONGLONG ResultSignificand;
    ULONGLONG Term0;
    ULONGLONG Term1;

    DividendParts.Double = Dividend;
    DivisorParts.Double = Divisor;
    DividendSignificand = DOUBLE_GET_SIGNIFICAND(DividendParts);
    DividendExponent = DOUBLE_GET_EXPONENT(DividendParts);
    DividendSign = DOUBLE_GET_SIGN(DividendParts);
    DivisorSignificand = DOUBLE_GET_SIGNIFICAND(DivisorParts);
    DivisorExponent = DOUBLE_GET_EXPONENT(DivisorParts);
    DivisorSign = DOUBLE_GET_SIGN(DivisorParts);
    ResultSign = DividendSign ^ DivisorSign;
    if (DividendExponent == DOUBLE_NAN_EXPONENT) {
        if (DividendSignificand ) {
            return RtlpDoublePropagateNan(DividendParts, DivisorParts);
        }

        if (DivisorExponent == DOUBLE_NAN_EXPONENT) {
            if (DivisorSignificand != 0) {
                return RtlpDoublePropagateNan(DividendParts, DivisorParts);
            }

            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            Result.Ulonglong = DOUBLE_DEFAULT_NAN;
            return Result.Double;
        }

        Result.Ulonglong = DOUBLE_PACK(ResultSign, DOUBLE_NAN_EXPONENT, 0);
        return Result.Double;
    }

    if (DivisorExponent == DOUBLE_NAN_EXPONENT) {
        if (DivisorSignificand != 0) {
            return RtlpDoublePropagateNan(DividendParts, DivisorParts);
        }

        Result.Ulonglong = DOUBLE_PACK(ResultSign, 0, 0);
        return Result.Double;
    }

    if (DivisorExponent == 0) {
        if (DivisorSignificand == 0) {
            if ((DividendExponent | DividendSignificand) == 0) {
                RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
                Result.Ulonglong = DOUBLE_DEFAULT_NAN;
                return Result.Double;
            }

            RtlpSoftFloatRaise(SOFT_FLOAT_DIVIDE_BY_ZERO);
            Result.Ulonglong = DOUBLE_PACK(ResultSign, DOUBLE_NAN_EXPONENT, 0);
            return Result.Double;
        }

        RtlpNormalizeDoubleSubnormal(DivisorSignificand,
                                     &DivisorExponent,
                                     &DivisorSignificand);
    }

    if (DividendExponent == 0) {
        if (DividendSignificand == 0) {
            Result.Ulonglong = DOUBLE_PACK(ResultSign, 0, 0);
            return Result.Double;
        }

        RtlpNormalizeDoubleSubnormal(DividendSignificand,
                                     &DividendExponent,
                                     &DividendSignificand);
    }

    ResultExponent = DividendExponent - DivisorExponent + 0x3FD;
    DividendSignificand = (DividendSignificand | 0x0010000000000000ULL) << 10;
    DivisorSignificand = (DivisorSignificand | 0x0010000000000000ULL) << 11;
    if (DivisorSignificand <= (DividendSignificand + DividendSignificand)) {
        DividendSignificand >>= 1;
        ResultExponent += 1;
    }

    ResultSignificand = RtlpEstimateDivide128To64(DividendSignificand,
                                                  0,
                                                  DivisorSignificand);

    if ((ResultSignificand & 0x1FF) <= 2) {
        RtlpMultiply64To128(DivisorSignificand,
                            ResultSignificand,
                            &Term0,
                            &Term1);

        RtlpSubtract128(DividendSignificand,
                        0,
                        Term0,
                        Term1,
                        &Remainder0,
                        &Remainder1);

        while ((LONGLONG)Remainder0 < 0) {
            ResultSignificand -= 1;
            RtlpAdd128(Remainder0,
                       Remainder1,
                       0,
                       DivisorSignificand,
                       &Remainder0,
                       &Remainder1);
        }

        ResultSignificand |= (Remainder1 != 0);
    }

    Result.Double = RtlpRoundAndPackDouble(ResultSign,
                                           ResultExponent,
                                           ResultSignificand);

    return Result.Double;
}

RTL_API
double
RtlDoubleModulo (
    double Dividend,
    double Divisor
    )

/*++

Routine Description:

    This routine divides one double into another, and returns the remainder.

Arguments:

    Dividend - Supplies the numerator.

    Divisor - Supplies the denominator.

Return Value:

    Returns the modulo of the two values.

--*/

{

    ULONGLONG AlternateDividendSignificand;
    SHORT DividendExponent;
    DOUBLE_PARTS DividendParts;
    CHAR DividendSign;
    ULONGLONG DividendSignificand;
    SHORT DivisorExponent;
    DOUBLE_PARTS DivisorParts;
    ULONGLONG DivisorSignificand;
    SHORT ExponentDifference;
    ULONGLONG Quotient;
    DOUBLE_PARTS Result;
    CHAR ResultSign;
    LONGLONG SignificandMean;

    DividendParts.Double = Dividend;
    DivisorParts.Double = Divisor;
    DividendSignificand = DOUBLE_GET_SIGNIFICAND(DividendParts);
    DividendExponent = DOUBLE_GET_EXPONENT(DividendParts);
    DividendSign = DOUBLE_GET_SIGN(DividendParts);
    DivisorSignificand = DOUBLE_GET_SIGNIFICAND(DivisorParts);
    DivisorExponent = DOUBLE_GET_EXPONENT(DivisorParts);
    if (DividendExponent == DOUBLE_NAN_EXPONENT) {
        if ((DividendSignificand != 0) ||
            ((DivisorExponent == DOUBLE_NAN_EXPONENT) &&
             (DivisorSignificand != 0))) {

            return RtlpDoublePropagateNan(DividendParts, DivisorParts);
        }

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        Result.Ulonglong = DOUBLE_DEFAULT_NAN;
        return Result.Double;
    }

    if (DivisorExponent == DOUBLE_NAN_EXPONENT) {
        if (DivisorSignificand != 0) {
            return RtlpDoublePropagateNan(DividendParts, DivisorParts);
        }

        return DividendParts.Double;
    }

    if (DivisorExponent == 0) {
        if (DivisorSignificand == 0) {
            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            Result.Ulonglong = DOUBLE_DEFAULT_NAN;
            return Result.Double;
        }

        RtlpNormalizeDoubleSubnormal(DivisorSignificand,
                                     &DivisorExponent,
                                     &DivisorSignificand);
    }

    if (DividendExponent == 0) {
        if (DividendSignificand == 0) {
            return DividendParts.Double;
        }

        RtlpNormalizeDoubleSubnormal(DividendSignificand,
                                     &DividendExponent,
                                     &DividendSignificand);
    }

    ExponentDifference = DividendExponent - DivisorExponent;
    DividendSignificand = (DividendSignificand | 0x0010000000000000ULL) << 11;
    DivisorSignificand = (DivisorSignificand | 0x0010000000000000ULL) << 11;
    if (ExponentDifference < 0) {
        if (ExponentDifference < -1) {
            return DividendParts.Double;
        }

        DividendSignificand >>= 1;
    }

    Quotient = 0;
    if (DivisorSignificand <= DividendSignificand) {
        Quotient = 1;
    }

    if (Quotient != 0) {
        DividendSignificand -= DivisorSignificand;
    }

    ExponentDifference -= 64;
    while (ExponentDifference > 0) {
        Quotient = RtlpEstimateDivide128To64(DividendSignificand,
                                             0,
                                             DivisorSignificand);

        if (Quotient > 2) {
            Quotient -= 2;

        } else {
            Quotient = 0;
        }

        DividendSignificand = -((DivisorSignificand >> 2) * Quotient);
        ExponentDifference -= 62;
    }

    ExponentDifference += 64;
    if (ExponentDifference > 0) {
        Quotient = RtlpEstimateDivide128To64(DividendSignificand,
                                             0,
                                             DivisorSignificand);

        if (Quotient > 2) {
            Quotient -= 2;

        } else {
            Quotient = 0;
        }

        Quotient >>= 64 - ExponentDifference;
        DivisorSignificand >>= 2;
        DividendSignificand = ((DividendSignificand >> 1) <<
                               (ExponentDifference - 1)) -
                              DivisorSignificand * Quotient;

    } else {
        DividendSignificand >>= 2;
        DivisorSignificand >>= 2;
    }

    do {
        AlternateDividendSignificand = DividendSignificand;
        Quotient += 1;
        DividendSignificand -= DivisorSignificand;

    } while ((LONGLONG)DividendSignificand >= 0);

    SignificandMean = DividendSignificand + AlternateDividendSignificand;
    if ((SignificandMean < 0 ) ||
        ((SignificandMean == 0) && ((Quotient & 0x1) != 0))) {

        DividendSignificand = AlternateDividendSignificand;
    }

    ResultSign = 0;
    if ((LONGLONG)DividendSignificand < 0) {
        ResultSign = 1;
    }

    if (ResultSign != 0) {
        DividendSignificand = -DividendSignificand;
    }

    Result.Double = RtlpNormalizeRoundAndPackDouble(DividendSign ^ ResultSign,
                                                    DivisorExponent,
                                                    DividendSignificand);

    return Result.Double;
}

RTL_API
double
RtlDoubleSquareRoot (
    double Value
    )

/*++

Routine Description:

    This routine returns the square root of the given double.

Arguments:

    Value - Supplies the value to take the square root of.

Return Value:

    Returns the square root of the given value.

--*/

{

    ULONGLONG DoubleResultSignificand;
    ULONGLONG PreDivisionSignificand;
    ULONGLONG RemainderHigh;
    ULONGLONG RemainderLow;
    DOUBLE_PARTS Result;
    SHORT ResultExponent;
    ULONGLONG ResultSignificand;
    ULONGLONG TermHigh;
    ULONGLONG TermLow;
    SHORT ValueExponent;
    DOUBLE_PARTS ValueParts;
    BOOL ValueSign;
    ULONGLONG ValueSignificand;

    ValueParts.Double = Value;
    ValueSignificand = DOUBLE_GET_SIGNIFICAND(ValueParts);
    ValueExponent = DOUBLE_GET_EXPONENT(ValueParts);
    ValueSign = DOUBLE_GET_SIGN(ValueParts);
    if (ValueExponent == DOUBLE_NAN_EXPONENT) {
        if (ValueSignificand != 0) {
            return RtlpDoublePropagateNan(ValueParts, ValueParts);
        }

        if (ValueSign == 0) {
            return ValueParts.Double;
        }

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        Result.Ulonglong = DOUBLE_DEFAULT_NAN;
        return Result.Double;
    }

    if (ValueSign != 0) {
        if ((ValueExponent | ValueSignificand) == 0) {
            return ValueParts.Double;
        }

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        Result.Ulonglong = DOUBLE_DEFAULT_NAN;
        return Result.Double;
    }

    if (ValueExponent == 0) {
        if (ValueSignificand == 0) {
            return 0;
        }

        RtlpNormalizeDoubleSubnormal(ValueSignificand,
                                     &ValueExponent,
                                     &ValueSignificand);
    }

    ResultExponent = ((ValueExponent - DOUBLE_EXPONENT_BIAS) >> 1) +
                     (DOUBLE_EXPONENT_BIAS - 1);

    ValueSignificand |= (1ULL << DOUBLE_EXPONENT_SHIFT);
    ResultSignificand = RtlpEstimateSquareRoot32(ValueExponent,
                                                 ValueSignificand >> 21);

    ValueSignificand <<= 9 - (ValueExponent & 0x1);
    PreDivisionSignificand = ResultSignificand;
    ResultSignificand = RtlpEstimateDivide128To64(ValueSignificand,
                                                  0,
                                                  ResultSignificand << 32);

    ResultSignificand += (PreDivisionSignificand << 30);
    if ((ResultSignificand & 0x1FF) <= 5) {
        DoubleResultSignificand = ResultSignificand << 1;
        RtlpMultiply64To128(ResultSignificand,
                            ResultSignificand,
                            &TermHigh,
                            &TermLow);

        RtlpSubtract128(ValueSignificand,
                        0,
                        TermHigh,
                        TermLow,
                        &RemainderHigh,
                        &RemainderLow);

        while ((LONGLONG)RemainderHigh < 0) {
            ResultSignificand -= 1;
            DoubleResultSignificand -= 2;
            RtlpAdd128(RemainderHigh,
                       RemainderLow,
                       ResultSignificand >> 63,
                       DoubleResultSignificand | 1,
                       &RemainderHigh,
                       &RemainderLow );
        }

        if (((RemainderHigh | RemainderLow ) != 0)) {
            ResultSignificand |= 0x1;
        }
    }

    return RtlpRoundAndPackDouble(0, ResultExponent, ResultSignificand);
}

RTL_API
BOOL
RtlDoubleIsEqual (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine determines if the given doubles are equal.

Arguments:

    Value1 - Supplies the first value to compare.

    Value2 - Supplies the second value to compare.

Return Value:

    TRUE if the values are equal.

    FALSE if the values are not equal. Note that NaN is not equal to anything,
    including itself.

--*/

{

    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    if (((DOUBLE_GET_EXPONENT(Parts1) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((DOUBLE_GET_EXPONENT(Parts2) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts2) != 0))) {

        if (DOUBLE_IS_SIGNALING_NAN(Parts1) ||
            DOUBLE_IS_SIGNALING_NAN(Parts2)) {

            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        }

        return FALSE;
    }

    if ((Parts1.Ulonglong == Parts2.Ulonglong) ||
        (((Parts1.Ulonglong | Parts2.Ulonglong) << 1) == 0)) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
BOOL
RtlDoubleIsLessThanOrEqual (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine determines if the given value is less than or equal to the
    second value.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is less than or equal to the first.

    FALSE if the first value is greater than the second.

--*/

{

    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    if (((DOUBLE_GET_EXPONENT(Parts1) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((DOUBLE_GET_EXPONENT(Parts2) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts2) != 0))) {

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        return FALSE;
    }

    Sign1 = DOUBLE_GET_SIGN(Parts1);
    Sign2 = DOUBLE_GET_SIGN(Parts2);
    if (Sign1 != Sign2) {
        if ((Sign1 != 0) ||
            ((ULONGLONG)((Parts1.Ulonglong | Parts2.Ulonglong) << 1) == 0)) {

            return TRUE;
        }

        return FALSE;
    }

    if ((Parts1.Ulonglong == Parts2.Ulonglong) ||
        (Sign1 ^ (Parts1.Ulonglong < Parts2.Ulonglong))) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
BOOL
RtlDoubleIsLessThan (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine determines if the given value is strictly less than the
    second value.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is strictly less than to the first.

    FALSE if the first value is greater than or equal to the second.

--*/

{

    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    if (((DOUBLE_GET_EXPONENT(Parts1) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((DOUBLE_GET_EXPONENT(Parts2) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts2) != 0))) {

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        return FALSE;
    }

    Sign1 = DOUBLE_GET_SIGN(Parts1);
    Sign2 = DOUBLE_GET_SIGN(Parts2);
    if (Sign1 != Sign2) {
        if ((Sign1 != 0) &&
            ((ULONGLONG)((Parts1.Ulonglong | Parts2.Ulonglong) << 1) != 0)) {

            return TRUE;
        }

        return FALSE;
    }

    if ((Parts1.Ulonglong != Parts2.Ulonglong) &&
        (Sign1 ^ (Parts1.Ulonglong < Parts2.Ulonglong))) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
BOOL
RtlDoubleSignalingIsEqual (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine determines if the given values are equal, generating an
    invalid floating point exception if either is NaN.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is strictly less than to the first.

    FALSE if the first value is greater than or equal to the second.

--*/

{

    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    if (((DOUBLE_GET_EXPONENT(Parts1) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((DOUBLE_GET_EXPONENT(Parts2) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts2) != 0))) {

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        return FALSE;
    }

    if ((Parts1.Ulonglong == Parts2.Ulonglong) ||
        ((ULONGLONG)((Parts1.Ulonglong | Parts2.Ulonglong) << 1) == 0)) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
BOOL
RtlDoubleIsLessThanOrEqualQuiet (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine determines if the given value is less than or equal to the
    second value. Quiet NaNs do not generate floating point exceptions.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is less than or equal to the first.

    FALSE if the first value is greater than the second.

--*/

{

    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    if (((DOUBLE_GET_EXPONENT(Parts1) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((DOUBLE_GET_EXPONENT(Parts2) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts2) != 0))) {

        if ((DOUBLE_IS_SIGNALING_NAN(Parts1)) ||
            (DOUBLE_IS_SIGNALING_NAN(Parts2))) {

            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        }

        return FALSE;
    }

    Sign1 = DOUBLE_GET_SIGN(Parts1);
    Sign2 = DOUBLE_GET_SIGN(Parts2);
    if (Sign1 != Sign2) {
        if ((Sign1 != 0) ||
            ((ULONGLONG)((Parts1.Ulonglong | Parts2.Ulonglong) << 1) == 0)) {

            return TRUE;
        }

        return FALSE;
    }

    if ((Parts1.Ulonglong == Parts2.Ulonglong) ||
        (Sign1 ^ (Parts1.Ulonglong < Parts2.Ulonglong))) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
BOOL
RtlDoubleIsLessThanQuiet (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine determines if the given value is strictly less than the
    second value. Quiet NaNs do not cause float point exceptions to be raised.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is strictly less than to the first.

    FALSE if the first value is greater than or equal to the second.

--*/

{

    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    if (((DOUBLE_GET_EXPONENT(Parts1) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((DOUBLE_GET_EXPONENT(Parts2) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts2) != 0))) {

        if ((DOUBLE_IS_SIGNALING_NAN(Parts1)) ||
            (DOUBLE_IS_SIGNALING_NAN(Parts2))) {

            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        }

        return FALSE;
    }

    Sign1 = DOUBLE_GET_SIGN(Parts1);
    Sign2 = DOUBLE_GET_SIGN(Parts2);
    if (Sign1 != Sign2) {
        if ((Sign1 != 0) &&
            ((ULONGLONG)((Parts1.Ulonglong | Parts2.Ulonglong) << 1) != 0)) {

            return TRUE;
        }

        return FALSE;
    }

    if ((Parts1.Ulonglong != Parts2.Ulonglong) &&
        (Sign1 ^ (Parts1.Ulonglong < Parts2.Ulonglong))) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
double
RtlFloatConvertToDouble (
    float Float
    )

/*++

Routine Description:

    This routine converts the given float into a double.

Arguments:

    Float - Supplies the float to convert.

Return Value:

    Returns the double equivalent.

--*/

{

    DOUBLE_PARTS DoubleParts;
    SHORT Exponent;
    FLOAT_PARTS FloatParts;
    CHAR Sign;
    ULONG Significand;

    FloatParts.Float = Float;
    Significand = FLOAT_GET_SIGNIFICAND(FloatParts);
    Exponent = FLOAT_GET_EXPONENT(FloatParts);
    Sign = FLOAT_GET_SIGN(FloatParts);
    if (Exponent == FLOAT_NAN_EXPONENT) {
        if (Significand != 0) {
            return RtlpCommonNanToDouble(RtlpFloatToCommonNan(FloatParts));
        }

        DoubleParts.Ulonglong = DOUBLE_PACK(Sign, DOUBLE_NAN_EXPONENT, 0);
        return DoubleParts.Double;
    }

    if (Exponent == 0) {
        if (Significand == 0) {
            DoubleParts.Ulonglong = DOUBLE_PACK(Sign, 0, 0);
            return DoubleParts.Double;
        }

        RtlpNormalizeFloatSubnormal(Significand,
                                    &Exponent,
                                    &Significand);

        Exponent -= 1;
    }

    DoubleParts.Ulonglong = DOUBLE_PACK(Sign,
                                        Exponent + 0x380,
                                        (ULONGLONG)Significand << 29);

    return DoubleParts.Double;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
RtlpSoftFloatRaise (
    ULONG Flags
    )

/*++

Routine Description:

    This routine raises the given conditions in the soft float implementation.

Arguments:

    Flags - Supplies the flags to raise.

Return Value:

    None.

--*/

{

    RtlSoftFloatExceptionFlags |= Flags;
    return;
}

double
RtlpDoubleAdd (
    DOUBLE_PARTS Value1,
    DOUBLE_PARTS Value2,
    CHAR Sign
    )

/*++

Routine Description:

    This routine adds the absolute values of two doubles together.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value to add.

    Sign - Supplies 1 if the sum should be negated before being returned, or
        0 if it should be left alone.

Return Value:

    Returns the sum of the two absolute values.

--*/

{

    SHORT Exponent1;
    SHORT Exponent2;
    SHORT ExponentDifference;
    DOUBLE_PARTS Result;
    SHORT ResultExponent;
    ULONGLONG ResultSignificand;
    ULONGLONG Significand1;
    ULONGLONG Significand2;

    Significand1 = DOUBLE_GET_SIGNIFICAND(Value1);
    Exponent1 = DOUBLE_GET_EXPONENT(Value1);
    Significand2 = DOUBLE_GET_SIGNIFICAND(Value2);
    Exponent2 = DOUBLE_GET_EXPONENT(Value2);
    ExponentDifference = Exponent1 - Exponent2;
    Significand1 <<= 9;
    Significand2 <<= 9;
    if (ExponentDifference > 0) {
        if (Exponent1 == DOUBLE_NAN_EXPONENT) {
            if (Significand1 != 0) {
                return RtlpDoublePropagateNan(Value1, Value2);
            }

            return Value1.Double;
        }

        if (Exponent2 == 0) {
            ExponentDifference -= 1;

        } else {
            Significand2 |= 0x2000000000000000ULL;
        }

        RtlpShift64RightJamming(Significand2,
                                ExponentDifference,
                                &Significand2);

        ResultExponent = Exponent1;

    } else if (ExponentDifference < 0) {
        if (Exponent2 == DOUBLE_NAN_EXPONENT) {
            if (Significand2 != 0) {
                return RtlpDoublePropagateNan(Value1, Value2);
            }

            Result.Ulonglong = DOUBLE_PACK(Sign, DOUBLE_NAN_EXPONENT, 0);
            return Result.Double;
        }

        if (Exponent1 == 0) {
            ExponentDifference += 1;

        } else {
            Significand1 |= 0x2000000000000000ULL;
        }

        RtlpShift64RightJamming(Significand1,
                                -ExponentDifference,
                                &Significand1);

        ResultExponent = Exponent2;

    } else {
        if (Exponent1 == DOUBLE_NAN_EXPONENT) {
            if ((Significand1 | Significand2) != 0) {
                return RtlpDoublePropagateNan( Value1, Value2 );
            }

            return Value1.Double;
        }

        if (Exponent1 == 0) {
            Result.Ulonglong = DOUBLE_PACK(Sign,
                                           0,
                                           (Significand1 + Significand2) >> 9);

            return Result.Double;
        }

        ResultSignificand = 0x4000000000000000ULL + Significand1 + Significand2;
        ResultExponent = Exponent1;
        goto DoubleAddEnd;
    }

    Significand1 |= 0x2000000000000000ULL;
    ResultSignificand = (Significand1 + Significand2) << 1;
    ResultExponent -= 1;
    if ((LONGLONG)ResultSignificand < 0 ) {
        ResultSignificand = Significand1 + Significand2;
        ResultExponent += 1;
    }

DoubleAddEnd:
    return RtlpRoundAndPackDouble(Sign, ResultExponent, ResultSignificand);
}

double
RtlpDoubleSubtract (
    DOUBLE_PARTS Value1,
    DOUBLE_PARTS Value2,
    CHAR Sign
    )

/*++

Routine Description:

    This routine subtracts the absolute values of two doubles from each other.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the value to subtract.

    Sign - Supplies 1 if the difference should be negated before being
        returned, or 0 if it should be left alone.

Return Value:

    Returns the difference of the two absolute values.

--*/

{

    SHORT Exponent1;
    SHORT Exponent2;
    SHORT ExponentDifference;
    DOUBLE_PARTS Result;
    SHORT ResultExponent;
    ULONGLONG ResultSignificand;
    ULONGLONG Significand1;
    ULONGLONG Significand2;

    Significand1 = DOUBLE_GET_SIGNIFICAND(Value1);
    Exponent1 = DOUBLE_GET_EXPONENT(Value1);
    Significand2 = DOUBLE_GET_SIGNIFICAND(Value2);
    Exponent2 = DOUBLE_GET_EXPONENT(Value2);
    ExponentDifference = Exponent1 - Exponent2;
    Significand1 <<= 10;
    Significand2 <<= 10;
    if (ExponentDifference > 0) {
        if (Exponent1 == DOUBLE_NAN_EXPONENT) {
            if (Significand1 != 0) {
                return RtlpDoublePropagateNan(Value1, Value2);
            }

            return Value1.Double;
        }

        if (Exponent2 == 0) {
            ExponentDifference -= 1;

        } else {
            Significand2 |= 0x4000000000000000ULL;
        }

        RtlpShift64RightJamming(Significand2,
                                ExponentDifference,
                                &Significand2);

        Significand1 |= 0x4000000000000000ULL;
        ResultSignificand = Significand1 - Significand2;
        ResultExponent = Exponent1;
        goto DoubleSubtractEnd;
    }

    if (ExponentDifference < 0) {
        if (Exponent2 == DOUBLE_NAN_EXPONENT) {
            if (Significand2 != 0) {
                return RtlpDoublePropagateNan(Value1, Value2);
            }

            if (Sign != 0) {
                Result.Ulonglong = DOUBLE_PACK(0, DOUBLE_NAN_EXPONENT, 0);

            } else {
                Result.Ulonglong = DOUBLE_PACK(1, DOUBLE_NAN_EXPONENT, 0);
            }

            return Result.Double;
        }

        if (Exponent1 == 0) {
            ExponentDifference += 1;

        } else {
            Significand1 |= 0x4000000000000000ULL;
        }

        RtlpShift64RightJamming(Significand1,
                                -ExponentDifference,
                                &Significand1);

        Significand2 |= 0x4000000000000000ULL;
        ResultSignificand = Significand2 - Significand1;
        ResultExponent = Exponent2;
        Sign ^= 1;
        goto DoubleSubtractEnd;
    }

    if (Exponent1 == DOUBLE_NAN_EXPONENT) {
        if ((Significand1 | Significand2) != 0) {
            return RtlpDoublePropagateNan(Value1, Value2);
        }

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        Result.Ulonglong = DOUBLE_DEFAULT_NAN;
        return Result.Double;
    }

    if (Exponent1 == 0) {
        Exponent1 = 1;
        Exponent2 = 1;
    }

    if (Significand2 < Significand1) {
        ResultSignificand = Significand1 - Significand2;
        ResultExponent = Exponent1;
        goto DoubleSubtractEnd;
    }

    if (Significand1 < Significand2) {
        ResultSignificand = Significand2 - Significand1;
        ResultExponent = Exponent2;
        Sign ^= 1;
        goto DoubleSubtractEnd;
    }

    if (RtlRoundingMode == SoftFloatRoundDown) {
        Result.Ulonglong = DOUBLE_PACK(1, 0, 0);

    } else {
        Result.Ulonglong = DOUBLE_PACK(0, 0, 0);
    }

    return Result.Double;

DoubleSubtractEnd:
    ResultExponent -= 1;
    Result.Double = RtlpNormalizeRoundAndPackDouble(Sign,
                                                    ResultExponent,
                                                    ResultSignificand);

    return Result.Double;
}

VOID
RtlpMultiply64To128 (
    ULONGLONG Value1,
    ULONGLONG Value2,
    PULONGLONG ResultHigh,
    PULONGLONG ResultLow
    )

/*++

Routine Description:

    This routine multiplies two values to obtain a 128-bit product.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value.

    ResultHigh - Supplies a pointer where the high part of the product will be
        returned.

    ResultLow - Supplies a pointer where the low part of the product will be
        returned.

Return Value:

    None.

--*/

{

    ULONGLONG ProductHigh;
    ULONGLONG ProductLow;
    ULONGLONG ProductMiddleA;
    ULONGLONG ProductMiddleB;
    ULONG UlongBitSize;
    ULONG Value1High;
    ULONG Value1Low;
    ULONG Value2High;
    ULONG Value2Low;

    UlongBitSize = sizeof(ULONG) * BITS_PER_BYTE;
    Value1Low = Value1;
    Value1High = Value1 >> UlongBitSize;
    Value2Low = Value2;
    Value2High = Value2 >> UlongBitSize;
    ProductLow = ((ULONGLONG)Value1Low) * Value2Low;
    ProductMiddleA = ((ULONGLONG)Value1Low) * Value2High;
    ProductMiddleB = ((ULONGLONG)Value1High) * Value2Low;
    ProductHigh = ((ULONGLONG) Value1High ) * Value2High;
    ProductMiddleA += ProductMiddleB;
    ProductHigh += (((ULONGLONG)(ProductMiddleA < ProductMiddleB)) <<
                   UlongBitSize) +
                  (ProductMiddleA >> UlongBitSize);

    ProductMiddleA <<= UlongBitSize;
    ProductLow += ProductMiddleA;
    ProductHigh += (ProductLow < ProductMiddleA);
    *ResultLow = ProductLow;
    *ResultHigh = ProductHigh;
    return;
}

ULONGLONG
RtlpEstimateDivide128To64 (
    ULONGLONG DividendHigh,
    ULONGLONG DividendLow,
    ULONGLONG Divisor
    )

/*++

Routine Description:

    This routine returns an approximation of the 64-bit integer quotient
    obtained by dividing the given divisor into the given 128-bit divident.
    The divisor must be at least 2^63. If q is the exact quotient truncated
    toward zero, the approximation returned lies between q and q + 2,
    inclusive. If the exact quotient q is larger than 64 bits, the maximum
    positive 64-bit unsigned integer is returned.

Arguments:

    DividendHigh - Supplies the high part of the dividend.

    DividendLow - Supplies the low part of the dividend.

    Divisor - Supplies the divisor.

Return Value:

    Returns the approximation of the quotient.

--*/

{

    ULONGLONG DivisorHigh;
    ULONGLONG DivisorLow;
    ULONGLONG RemainderHigh;
    ULONGLONG RemainderLow;
    ULONGLONG Result;
    ULONGLONG Term0;
    ULONGLONG Term1;
    ULONG UlongBits;

    if (Divisor <= DividendHigh) {
        return MAX_ULONGLONG;
    }

    UlongBits = sizeof(ULONG) * BITS_PER_BYTE;
    DivisorHigh = Divisor >> UlongBits;
    if ((DivisorHigh << UlongBits) <= DividendHigh) {
        Result = 0xFFFFFFFF00000000ULL;

    } else {
        Result = (DividendHigh / DivisorHigh) << UlongBits;
    }

    RtlpMultiply64To128(Divisor, Result, &Term0, &Term1);
    RtlpSubtract128(DividendHigh,
                    DividendLow,
                    Term0,
                    Term1,
                    &RemainderHigh,
                    &RemainderLow);

    while (((LONGLONG)RemainderHigh) < 0) {
        Result -= (ULONGLONG)MAX_ULONG + 1ULL;
        DivisorLow = Divisor << UlongBits;
        RtlpAdd128(RemainderHigh,
                   RemainderLow,
                   DivisorHigh,
                   DivisorLow,
                   &RemainderHigh,
                   &RemainderLow);
    }

    RemainderHigh = (RemainderHigh << UlongBits) | (RemainderLow >> UlongBits);
    if ((DivisorHigh << UlongBits) <= RemainderHigh) {
        Result |= MAX_ULONG;

    } else {
        Result |= RemainderHigh / DivisorHigh;
    }

    return Result;
}

ULONGLONG
RtlpEstimateSquareRoot32 (
    SHORT ValueExponent,
    ULONG Value
    )

/*++

Routine Description:

    This routine returns an approximation of the square root of the given
    32-bit significand. Considered as an integer, the value must be at least
    2^31. If bit 0 of the exponent is 1, the integer returned approximates
    2^31 * sqrt(Value / 2^31), where the value is considered an integer. If bit
    0 of the exponent is 0, the integer returned approximates
    2^31*sqrt(Value/2^30). In either case, the approximation returned lies
    strictly within +/-2 of the exact value.

Arguments:

    ValueExponent - Supplies the exponent of the value.

    Value - Supplies the significand to estimate the square root of.

Return Value:

    Returns an approximation of the square root.

--*/

{

    ULONG Index;
    ULONG Result;

    Index = (Value >> 27) & 0xF;
    if ((ValueExponent & 0x1) != 0) {
        Result = 0x4000 + (Value >> 17) - RtlSquareRootOddAdjustments[Index];
        Result = ((Value / Result) << 14) + (Result << 15);
        Value >>= 1;

    } else {
        Result = 0x8000 + (Value >> 17) - RtlSquareRootEvenAdjustments[Index];
        Result = Value / Result + Result;
        if (Result >= 0x20000) {
            Result = 0xFFFF8000;

        } else {
            Result = Result << 15;
        }

        if (Result <= Value) {
            return (ULONG)(((LONG)Value) >> 1);
        }
    }

    return ((ULONG)((((LONGLONG)Value) << 31) / Result)) + (Result >> 1);
}

VOID
RtlpAdd128 (
    ULONGLONG Value1High,
    ULONGLONG Value1Low,
    ULONGLONG Value2High,
    ULONGLONG Value2Low,
    PULONGLONG ResultHigh,
    PULONGLONG ResultLow
    )

/*++

Routine Description:

    This routine adds two 128-bit values together. Addition is modulo 2^128,
    so any carry is lost.

Arguments:

    Value1High - Supplies the high 64 bits of the first value.

    Value1Low - Supplies the low 64 bits of the first value.

    Value2High - Supplies the high 64 bits of the second value.

    Value2Low - Supplies the low 64 bits of the second value.

    ResultHigh - Supplies a pointer where the high 64 bits of the result will
        be returned.

    ResultLow - Supplies a pointer where the low 64 bits of the result will be
        returned.

Return Value:

    None.

--*/

{

    ULONGLONG SumLow;

    SumLow = Value1Low + Value2Low;
    *ResultLow = SumLow;
    *ResultHigh = Value1High + Value2High;
    if (SumLow < Value1Low) {
        *ResultHigh += 1;
    }

    return;
}

VOID
RtlpSubtract128 (
    ULONGLONG Value1High,
    ULONGLONG Value1Low,
    ULONGLONG Value2High,
    ULONGLONG Value2Low,
    PULONGLONG ResultHigh,
    PULONGLONG ResultLow
    )

/*++

Routine Description:

    This routine subtracts two 128-bit values from each other.
    Addition is modulo 2^128, so any borrow is lost.

Arguments:

    Value1High - Supplies the high 64 bits of the first value.

    Value1Low - Supplies the low 64 bits of the first value.

    Value2High - Supplies the high 64 bits of the second value.

    Value2Low - Supplies the low 64 bits of the second value.

    ResultHigh - Supplies a pointer where the high 64 bits of the result will
        be returned.

    ResultLow - Supplies a pointer where the low 64 bits of the result will be
        returned.

Return Value:

    None.

--*/

{

    *ResultLow = Value1Low - Value2Low;
    *ResultHigh = Value1High - Value2High;
    if (Value1Low < Value2Low) {
        *ResultHigh -= 1;
    }

    return;
}

double
RtlpDoublePropagateNan (
    DOUBLE_PARTS Value1,
    DOUBLE_PARTS Value2
    )

/*++

Routine Description:

    This routine takes two double values, one of which is NaN, and returns the
    appropriate NaN result. If with one is a signaling NaN, then the invalid
    exception is raised.

Arguments:

    Value1 - Supplies the first possibly NaN value.

    Value2 - Supplies the second possibly NaN value.

Return Value:

    Returns the appropriate NaN value.

--*/

{

    BOOL Value1IsNan;
    BOOL Value1IsSignalingNan;
    BOOL Value2IsNan;
    BOOL Value2IsSignalingNan;

    Value1IsNan = DOUBLE_IS_NAN(Value1);
    Value1IsSignalingNan = DOUBLE_IS_SIGNALING_NAN(Value1);
    Value2IsNan = DOUBLE_IS_NAN(Value2);
    Value2IsSignalingNan = DOUBLE_IS_SIGNALING_NAN(Value2);
    Value1.Ulonglong |= 1ULL << (DOUBLE_EXPONENT_SHIFT - 1);
    Value2.Ulonglong |= 1ULL << (DOUBLE_EXPONENT_SHIFT - 1);
    if ((Value1IsSignalingNan != FALSE) || (Value2IsSignalingNan != FALSE)) {
        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
    }

    if (Value1IsSignalingNan != FALSE) {
        if (Value2IsSignalingNan == FALSE) {
            if (Value2IsNan != FALSE) {
                return Value2.Double;
            }

            return Value1.Double;
        }

    } else if (Value1IsNan != FALSE) {
        if ((Value2IsSignalingNan != FALSE) || (Value2IsNan == FALSE)) {
            return Value1.Double;
        }

    } else {
        return Value2.Double;
    }

    if ((ULONGLONG)(Value1.Ulonglong << 1) <
        (ULONGLONG)(Value2.Ulonglong << 1)) {

        return Value2.Double;
    }

    if ((ULONGLONG)(Value2.Ulonglong << 1) <
        (ULONGLONG)(Value1.Ulonglong << 1)) {

        return Value1.Double;
    }

    if (Value1.Ulonglong < Value2.Ulonglong) {
        return Value1.Double;
    }

    return Value2.Double;
}

LONG
RtlpRoundAndPack32 (
    CHAR SignBit,
    ULONGLONG AbsoluteValue
    )

/*++

Routine Description:

    This routine takes a 64 bit fixed point value with binary point between
    bits 6 and 7 and returns the properly rounded 32 bit integer corresponding
    to the input. If the sign is one, the input is negated before being
    converted. If the fixed-point input is too large, the invalid exception is
    raised and the largest positive or negative integer is returned.

Arguments:

    SignBit - Supplies a non-zero value if the value should be negated before
        being converted.

    AbsoluteValue - Supplies the value to convert.

Return Value:

    Returns the 32 bit integer representation of the fixed point number.

--*/

{

    ULONGLONG Mask;
    LONG Result;
    CHAR RoundBits;
    CHAR RoundIncrement;
    SOFT_FLOAT_ROUNDING_MODE RoundingMode;
    BOOL RoundNearestEven;

    RoundingMode = RtlRoundingMode;
    RoundNearestEven = FALSE;
    if (RoundingMode == SoftFloatRoundNearestEven) {
        RoundNearestEven = TRUE;
    }

    RoundIncrement = sizeof(ULONGLONG) * BITS_PER_BYTE;
    if (RoundNearestEven == FALSE) {
        if (RoundingMode == SoftFloatRoundToZero) {
            RoundIncrement = 0;

        } else {
            RoundIncrement = 0x7F;
            if (SignBit != 0) {
                if (RoundingMode == SoftFloatRoundUp) {
                    RoundIncrement = 0;
                }

            } else {
                if (RoundingMode == SoftFloatRoundDown) {
                    RoundIncrement = 0;
                }
            }
        }
    }

    //
    // Add the rounding amount and remove the fixed point.
    //

    RoundBits = AbsoluteValue & 0x7F;
    AbsoluteValue = (AbsoluteValue + RoundIncrement) >> 7;
    Mask = MAX_ULONGLONG;
    if (((RoundBits ^ (sizeof(ULONGLONG) * BITS_PER_BYTE)) == 0) &&
        (RoundNearestEven != FALSE)) {

        Mask ^= 0x1;
    }

    AbsoluteValue &= Mask;
    Result = AbsoluteValue;
    if (SignBit != 0) {
        Result = -Result;
    }

    if (((AbsoluteValue >> (sizeof(ULONG) * BITS_PER_BYTE)) != 0) ||
        ((Result != 0) && ((Result < 0) ^ SignBit))) {

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        if (SignBit != 0) {
            return MIN_LONG;
        }

        return MAX_LONG;
    }

    if (RoundBits != 0) {
        RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
    }

    return Result;
}

LONGLONG
RtlpRoundAndPack64 (
    CHAR SignBit,
    ULONGLONG AbsoluteValueHigh,
    ULONGLONG AbsoluteValueLow
    )

/*++

Routine Description:

    This routine takes a 128-bit fixed point value with binary point between
    bits 63 and 64 and returns the properly rounded 64 bit integer corresponding
    to the input. If the sign is one, the input is negated before being
    converted. If the fixed-point input is too large, the invalid exception is
    raised and the largest positive or negative integer is returned.

Arguments:

    SignBit - Supplies a non-zero value if the value should be negated before
        being converted.

    AbsoluteValueHigh - Supplies the high part of the absolute value.

    AbsoluteValueLow - Supplies the low part of the absolute value.

Return Value:

    Returns the 32 bit integer representation of the fixed point number.

--*/

{

    CHAR Increment;
    LONGLONG Result;
    SOFT_FLOAT_ROUNDING_MODE RoundingMode;
    BOOL RoundNearestEven;

    RoundingMode = RtlRoundingMode;
    RoundNearestEven = FALSE;
    if (RoundingMode == SoftFloatRoundNearestEven) {
        RoundNearestEven = TRUE;
    }

    Increment = 0;
    if ((LONGLONG)AbsoluteValueLow < 0) {
        Increment = 1;
    }

    if (RoundNearestEven == FALSE) {
        if (RoundingMode == SoftFloatRoundToZero) {
            Increment = 0;

        } else {
            Increment = 0;
            if (SignBit != 0) {
                if ((RoundingMode == SoftFloatRoundDown) &&
                    (AbsoluteValueLow != 0)) {

                    Increment = 1;
                }

            } else {
                if ((RoundingMode == SoftFloatRoundUp) &&
                    (AbsoluteValueLow != 0)) {

                    Increment = 1;
                }
            }
        }
    }

    if (Increment != 0) {
        AbsoluteValueHigh += 1;
        if (AbsoluteValueHigh == 0) {
            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            if (SignBit != 0) {
                return MIN_LONGLONG;
            }

            return MAX_LONGLONG;
        }

        if (((ULONGLONG)(AbsoluteValueLow << 1) == 0) &&
            (RoundNearestEven != FALSE)) {

            AbsoluteValueHigh &= ~0x1ULL;
        }
    }

    Result = AbsoluteValueHigh;
    if (SignBit != 0) {
        Result = - Result;
    }

    if ((Result != 0) && (((Result < 0) ^ SignBit) != 0)) {
        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        if (SignBit != 0) {
            return MIN_LONGLONG;
        }

        return MAX_LONGLONG;
    }

    if (AbsoluteValueLow != 0) {
        RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
    }

    return Result;
}

double
RtlpRoundAndPackDouble (
    CHAR SignBit,
    SHORT Exponent,
    ULONGLONG Significand
    )

/*++

Routine Description:

    This routine takes a sign, exponent, and significand and creates the
    proper rounded double floating point value from that input. Overflow and
    underflow can be raised here.

Arguments:

    SignBit - Supplies a non-zero value if the value should be negated before
        being converted.

    Exponent - Supplies the exponent for the value.

    Significand - Supplies the significand with its binary point between bits
        62 and 61, which is 10 bits to the left of its usual location. The
        shifted exponent must be normalized or smaller. If the significand is
        not normalized, the exponent must be 0. In that case, the result
        returned is a subnormal number, and it must not require rounding. In
        the normal case wehre the significand is normalized, the exponent must
        be one less than the true floating point exponent.

Return Value:

    Returns the double representation of the given components.

--*/

{

    BOOL IsTiny;
    DOUBLE_PARTS Result;
    SHORT RoundBits;
    SHORT RoundIncrement;
    SOFT_FLOAT_ROUNDING_MODE RoundingMode;
    BOOL RoundNearestEven;

    RoundingMode = RtlRoundingMode;
    RoundNearestEven = FALSE;
    if (RoundingMode == SoftFloatRoundNearestEven) {
        RoundNearestEven = TRUE;
    }

    RoundIncrement = 0x200;
    if (RoundNearestEven == FALSE) {
        if (RoundingMode == SoftFloatRoundToZero) {
            RoundIncrement = 0;

        } else {
            RoundIncrement = 0x3FF;
            if (SignBit != 0) {
                if (RoundingMode == SoftFloatRoundUp) {
                    RoundIncrement = 0;
                }

            } else {
                if (RoundingMode == SoftFloatRoundDown) {
                    RoundIncrement = 0;
                }
            }
        }
    }

    RoundBits = Significand & 0x3FF;
    if ((USHORT)Exponent >= 0x7FD) {
        if ((Exponent > 0x7FD) ||
            ((Exponent == 0x7FD) &&
             ((LONGLONG)(Significand + RoundIncrement) < 0))) {

            RtlpSoftFloatRaise(SOFT_FLOAT_OVERFLOW | SOFT_FLOAT_INEXACT);
            Result.Ulonglong = DOUBLE_PACK(SignBit, 0x7FF, 0);
            if (RoundIncrement == 0) {
                Result.Ulonglong -= 1;
            }

            return Result.Double;
        }

        if (Exponent < 0) {
            IsTiny = (RtlTininessDetection ==
                      SoftFloatTininessBeforeRounding) ||
                     (Exponent < -1) ||
                     (Significand + RoundIncrement < 0x8000000000000000ULL);

            RtlpShift64RightJamming(Significand, -Exponent, &Significand);
            Exponent = 0;
            RoundBits = Significand & 0x3FF;
            if ((IsTiny != FALSE) && (RoundBits != 0)) {
                RtlpSoftFloatRaise(SOFT_FLOAT_UNDERFLOW);
            }
        }
    }

    if (RoundBits != 0) {
        RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
    }

    Significand = (Significand + RoundIncrement) >> 10;
    Significand &= ~(((RoundBits ^ 0x200) == 0) & (RoundNearestEven != FALSE));
    if (Significand == 0) {
        Exponent = 0;
    }

    Result.Ulonglong = DOUBLE_PACK(SignBit, Exponent, Significand);
    return Result.Double;
}

float
RtlpRoundAndPackFloat (
    CHAR SignBit,
    SHORT Exponent,
    ULONG Significand
    )

/*++

Routine Description:

    This routine takes a sign, exponent, and significand and creates the
    proper rounded floating point value from that input. Overflow and
    underflow can be raised here.

Arguments:

    SignBit - Supplies a non-zero value if the value should be negated before
        being converted.

    Exponent - Supplies the exponent for the value.

    Significand - Supplies the significand with its binary point between bits
        30 and 29, which is 7 bits to the left of its usual location. The
        shifted exponent must be normalized or smaller. If the significand is
        not normalized, the exponent must be 0. In that case, the result
        returned is a subnormal number, and it must not require rounding. In
        the normal case wehre the significand is normalized, the exponent must
        be one less than the true floating point exponent.

Return Value:

    Returns the float representation of the given components.

--*/

{

    BOOL IsTiny;
    FLOAT_PARTS Result;
    CHAR RoundBits;
    CHAR RoundIncrement;
    SOFT_FLOAT_ROUNDING_MODE RoundingMode;
    BOOL RoundNearestEven;

    RoundingMode = RtlRoundingMode;
    RoundNearestEven = FALSE;
    if (RoundingMode == SoftFloatRoundNearestEven) {
        RoundNearestEven = TRUE;
    }

    RoundIncrement = 0x40;
    if (RoundNearestEven == FALSE) {
        if (RoundingMode == SoftFloatRoundToZero) {
            RoundIncrement = 0;

        } else {
            RoundIncrement = 0x7F;
            if (SignBit != 0) {
                if (RoundingMode == SoftFloatRoundUp) {
                    RoundIncrement = 0;
                }

            } else {
                if (RoundingMode == SoftFloatRoundDown) {
                    RoundIncrement = 0;
                }
            }
        }
    }

    RoundBits = Significand & 0x7F;
    if ((USHORT)Exponent >= 0xFD) {
        if ((Exponent > 0xFD) ||
            ((Exponent == 0xFD) &&
             ((LONG)(Significand + RoundIncrement) < 0))) {

            RtlpSoftFloatRaise(SOFT_FLOAT_OVERFLOW | SOFT_FLOAT_INEXACT);
            Result.Ulong = FLOAT_PACK(SignBit, 0xFF, 0);
            if (RoundIncrement == 0) {
                Result.Ulong -= 1;
            }

            return Result.Float;
        }

        if (Exponent < 0) {
            IsTiny = (RtlTininessDetection ==
                      SoftFloatTininessBeforeRounding) ||
                     (Exponent < -1) ||
                     (Significand + RoundIncrement < 0x80000000);

            RtlpShift32RightJamming(Significand, -Exponent, &Significand);
            Exponent = 0;
            RoundBits = Significand & 0x7F;
            if ((IsTiny != FALSE) && (RoundBits != 0)) {
                RtlpSoftFloatRaise(SOFT_FLOAT_UNDERFLOW);
            }
        }
    }

    if (RoundBits != 0) {
        RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
    }

    Significand = (Significand + RoundIncrement ) >> 7;
    if (((RoundBits ^ 0x40) == 0) && (RoundNearestEven != FALSE)) {
        Significand &= ~0x1;
    }

    if (Significand == 0) {
        Exponent = 0;
    }

    Result.Ulong = FLOAT_PACK(SignBit, Exponent, Significand);
    return Result.Float;
}

double
RtlpNormalizeRoundAndPackDouble (
    CHAR SignBit,
    SHORT Exponent,
    ULONGLONG Significand
    )

/*++

Routine Description:

    This routine takes a sign, exponent, and significand and creates the
    proper rounded double floating point value from that input. Overflow and
    underflow can be raised here. This routine is very similar to the "round
    and pack double" routine except that the significand does not have to be
    normalized. Bit 63 of the significand must be zero, and the exponent must
    be one less than the true floating point exponent.

Arguments:

    SignBit - Supplies a non-zero value if the value should be negated before
        being converted.

    Exponent - Supplies the exponent for the value.

    Significand - Supplies the significand with its binary point between bits
        62 and 61, which is 10 bits to the left of its usual location.

Return Value:

    Returns the double representation of the given components.

--*/

{

    double Result;
    CHAR ShiftCount;

    ShiftCount = RtlpCountLeadingZeros64(Significand) - 1;
    Result = RtlpRoundAndPackDouble(SignBit,
                                    Exponent - ShiftCount,
                                    Significand << ShiftCount);

    return Result;
}

VOID
RtlpNormalizeDoubleSubnormal (
    ULONGLONG Significand,
    PSHORT Exponent,
    PULONGLONG Result
    )

/*++

Routine Description:

    This routine normalizes a subnormal double floating point value represented
    by the given denormalized significand. The normalized exponent and
    significand are returned.

Arguments:

    Significand - Supplies the significand to normalize.

    Exponent - Supplies a pointer where the exponent from the normalization
        will be returned.

    Result - Supplies a pointer where the normalized significand will be
        returned.

Return Value:

    None.

--*/

{

    CHAR ShiftCount;

    ShiftCount = RtlpCountLeadingZeros64(Significand) - 11;
    *Result = Significand << ShiftCount;
    *Exponent = 1 - ShiftCount;
    return;
}

VOID
RtlpNormalizeFloatSubnormal (
    ULONG Significand,
    PSHORT Exponent,
    PULONG Result
    )

/*++

Routine Description:

    This routine normalizes a subnormal single-precision floating point value
    represented by the given denormalized significand. The normalized exponent
    and significand are returned.

Arguments:

    Significand - Supplies the significand to normalize.

    Exponent - Supplies a pointer where the exponent from the normalization
        will be returned.

    Result - Supplies a pointer where the normalized significand will be
        returned.

Return Value:

    None.

--*/

{

    CHAR ShiftCount;

    ShiftCount = RtlpCountLeadingZeros32(Significand) - 8;
    *Result = Significand << ShiftCount;
    *Exponent = 1 - ShiftCount;
    return;
}

CHAR
RtlpCountLeadingZeros32 (
    ULONG Value
    )

/*++

Routine Description:

    This routine counts the number of leading zeros in the given 32 bit
    integer. If the value is zero, 32 will be returned.

Arguments:

    Value - Supplies the value.

Return Value:

    Returns the number of leading zero bits.

--*/

{

    CHAR Count;

    Count = 0;
    while (((Value & (1 << ((sizeof(ULONG) * BITS_PER_BYTE) - 1))) == 0) &&
           (Count < 32)) {

        Count += 1;
        Value <<= 1;
    }

    return Count;
}

CHAR
RtlpCountLeadingZeros64 (
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine counts the number of leading zeros in the given 64 bit
    integer. If the value is zero, 64 will be returned.

Arguments:

    Value - Supplies the value.

Return Value:

    Returns the number of leading zero bits.

--*/

{

    CHAR ShiftCount;

    ShiftCount = 0;
    if (Value < ((ULONGLONG)1 << (sizeof(ULONG) * BITS_PER_BYTE))) {
        ShiftCount += sizeof(ULONG) * BITS_PER_BYTE;

    } else {
        Value >>= sizeof(ULONG) * BITS_PER_BYTE;
    }

    ShiftCount += RtlpCountLeadingZeros32((ULONG)Value);
    return ShiftCount;
}

VOID
RtlpShift32RightJamming (
    ULONG Value,
    SHORT Count,
    PULONG Result
    )

/*++

Routine Description:

    This routine shifts the given value right by the requested number of bits.
    If any bits are shifted off the right, the least significant bit is set.
    The imagery is that the bits get "jammed" on the end as they try to fall
    off.

Arguments:

    Value - Supplies the value to shift.

    Count - Supplies the number of bits to shift by.

    Result - Supplies a pointer where the result will be stored.

Return Value:

    None.

--*/

{

    ULONG ShiftedValue;

    if (Count == 0) {
        ShiftedValue = Value;

    } else if (Count < 32) {
        ShiftedValue = (Value >> Count) | ((Value << ((-Count) & 31)) != 0);

    } else {
        ShiftedValue = 0;
        if (Value != 0) {
            ShiftedValue = 1;
        }
    }

    *Result = ShiftedValue;
    return;
}

VOID
RtlpShift64RightJamming (
    ULONGLONG Value,
    SHORT Count,
    PULONGLONG Result
    )

/*++

Routine Description:

    This routine shifts the given value right by the requested number of bits.
    If any bits are shifted off the right, the least significant bit is set.
    The imagery is that the bits get "jammed" on the end as they try to fall
    off.

Arguments:

    Value - Supplies the value to shift.

    Count - Supplies the number of bits to shift by.

    Result - Supplies a pointer where the result will be stored.

Return Value:

    None.

--*/

{

    ULONGLONG ShiftedValue;

    if (Count == 0) {
        ShiftedValue = Value;

    } else if (Count < (sizeof(ULONGLONG) * BITS_PER_BYTE)) {
        ShiftedValue = (Value >> Count) | ((Value << ((-Count) & 63)) != 0);

    } else {
        ShiftedValue = 0;
        if (Value != 0) {
            ShiftedValue = 1;
        }
    }

    *Result = ShiftedValue;
}

VOID
RtlpShift64ExtraRightJamming (
    ULONGLONG ValueInteger,
    ULONGLONG ValueFraction,
    SHORT Count,
    PULONGLONG ResultHigh,
    PULONGLONG ResultLow
    )

/*++

Routine Description:

    This routine shifts the given 128-bit value right by the requested number
    of bits plus 64. The shifted result is at most 64 non-zero bits. The bits
    shifted off form a second 64-bit result as follows: The last bit shifted
    off is the most significant bit of the extra result, and all other 63 bits
    of the extra result are all zero if and only if all but the last bits
    shifted off were all zero.

Arguments:

    ValueInteger - Supplies the integer portion of the fixed point 128 bit
        value. The binary point is between the integer and the fractional parts.

    ValueFraction - Supplies the lower portion of the fixed point 128 bit
        value.

    Count - Supplies the number of bits to shift by.

    ResultHigh - Supplies a pointer where the result will be returned.

    ResultLow - Supplies a pointer where the lower part of the result will be
        returned.

Return Value:

    None.

--*/

{

    CHAR NegativeCount;
    ULONGLONG ShiftedValueHigh;
    ULONGLONG ShiftedValueLow;

    NegativeCount = (-Count) & 63;
    if (Count == 0) {
        ShiftedValueLow = ValueFraction;
        ShiftedValueHigh = ValueInteger;

    } else if (Count < 64) {
        ShiftedValueLow = (ValueInteger << NegativeCount) |
                          (ValueFraction != 0);

        ShiftedValueHigh = ValueInteger >> Count;

    } else {
        if (Count == 64) {
            ShiftedValueLow = ValueInteger | (ValueFraction != 0);

        } else {
            ShiftedValueLow = ((ValueInteger | ValueFraction) != 0);
        }

        ShiftedValueHigh = 0;
    }

    *ResultLow = ShiftedValueLow;
    *ResultHigh = ShiftedValueHigh;
    return;
}

COMMON_NAN
RtlpDoubleToCommonNan (
    DOUBLE_PARTS Value
    )

/*++

Routine Description:

    This routine converts a double value to a canonical NaN type. If the value
    is signaling, the invalid exception is raised.

Arguments:

    Value - Supplies the double parts.

Return Value:

    Returns the canonical NaN.

--*/

{

    COMMON_NAN Result;

    if (DOUBLE_IS_SIGNALING_NAN(Value)) {
        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
    }

    Result.Sign = (Value.Ulong.High &
                   (DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT)) >>
                  (DOUBLE_SIGN_BIT_SHIFT - DOUBLE_HIGH_WORD_SHIFT);

    Result.Low = 0;
    Result.High = Value.Ulonglong << 12;
    return Result;
}

COMMON_NAN
RtlpFloatToCommonNan (
    FLOAT_PARTS Value
    )

/*++

Routine Description:

    This routine converts a float value to a canonical NaN type. If the value
    is signaling, the invalid exception is raised.

Arguments:

    Value - Supplies the double parts.

Return Value:

    Returns the canonical NaN.

--*/

{

    COMMON_NAN Result;

    if (FLOAT_IS_SIGNALING_NAN(Value)) {
        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
    }

    Result.Sign = FLOAT_GET_SIGN(Value);
    Result.Low = 0;
    Result.High = (ULONGLONG)Value.Ulong << 41;
    return Result;
}

float
RtlpCommonNanToFloat (
    COMMON_NAN Nan
    )

/*++

Routine Description:

    This routine converts a canonical NaN into a 32 bit floating point number.

Arguments:

    Nan - Supplies the canonical NaN to convert.

Return Value:

    Returns the float representation.

--*/

{

    FLOAT_PARTS Parts;

    Parts.Ulong = (((ULONG)Nan.Sign) << FLOAT_SIGN_BIT_SHIFT) |
                  FLOAT_NAN |
                  (1 << (FLOAT_EXPONENT_SHIFT - 1)) |
                  (Nan.High >> 41);

    return Parts.Float;
}

double
RtlpCommonNanToDouble (
    COMMON_NAN Nan
    )

/*++

Routine Description:

    This routine converts a canonical NaN into a 64 bit floating point number.

Arguments:

    Nan - Supplies the canonical NaN to convert.

Return Value:

    Returns the double representation.

--*/

{

    DOUBLE_PARTS Parts;

    Parts.Ulonglong = ((ULONGLONG)Nan.Sign << DOUBLE_SIGN_BIT_SHIFT) |
                      ((ULONGLONG)NAN_HIGH_WORD << DOUBLE_HIGH_WORD_SHIFT) |
                      (1ULL << (DOUBLE_EXPONENT_SHIFT - 1)) |
                      Nan.High >> 12;

    return Parts.Double;
}

