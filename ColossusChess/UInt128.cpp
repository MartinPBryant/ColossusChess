#include "UInt128.h"
#include <cstdint>
#include <intrin.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <stdexcept>

class UInt128
{
public:

	uint64_t hi;
	uint64_t lo;

	// Constructors
	UInt128() : hi(0), lo(0) {}

	UInt128(uint64_t value) : hi(0), lo(value) {}

	UInt128(uint64_t high, uint64_t low)
		: hi(high), lo(low) {}


	// ------------------------------------------------------------
	// Comparison
	// ------------------------------------------------------------

	bool operator==(const UInt128& b) const
	{
		return hi == b.hi && lo == b.lo;
	}

	bool operator!=(const UInt128& b) const
	{
		return !(*this == b);
	}

	bool operator<(const UInt128& b) const
	{
		if (hi != b.hi)
			return hi < b.hi;

		return lo < b.lo;
	}

	bool operator>(const UInt128& b) const
	{
		return b < *this;
	}

	bool operator<=(const UInt128& b) const
	{
		return !(b < *this);
	}

	bool operator>=(const UInt128& b) const
	{
		return !(*this < b);
	}


	// ------------------------------------------------------------
	// Addition
	// ------------------------------------------------------------

	UInt128 operator+(const UInt128& b) const
	{
		UInt128 result;

		result.lo = lo + b.lo;

		// Carry from low 64 bits
		uint64_t carry = (result.lo < lo);

		result.hi = hi + b.hi + carry;

		return result;
	}


	UInt128& operator+=(const UInt128& b)
	{
		*this = *this + b;
		return *this;
	}


	// ------------------------------------------------------------
	// Subtraction
	// ------------------------------------------------------------

	UInt128 operator-(const UInt128& b) const
	{
		UInt128 result;

		result.lo = lo - b.lo;

		// Borrow from high 64 bits
		uint64_t borrow = (lo < b.lo);

		result.hi = hi - b.hi - borrow;

		return result;
	}


	UInt128& operator-=(const UInt128& b)
	{
		*this = *this - b;
		return *this;
	}


	// ------------------------------------------------------------
	// Multiplication
	//
	// Returns the LOW 128 bits of the 128 x 128 product.
	// ------------------------------------------------------------

	UInt128 operator*(const UInt128& b) const
	{
		UInt128 result;

		uint64_t dummy;

		// Low × low
		result.lo = _umul128(lo, b.lo, &result.hi);

		// Cross products.
		//
		// Only their LOW 64 bits contribute to the
		// high 64 bits of our 128-bit result.
		result.hi += _umul128(lo, b.hi, &dummy);
		result.hi += _umul128(hi, b.lo, &dummy);

		return result;
	}


	UInt128& operator*=(const UInt128& b)
	{
		*this = *this * b;
		return *this;
	}


	// ------------------------------------------------------------
	// Shift left
	// ------------------------------------------------------------

	UInt128 operator<<(unsigned int shift) const
	{
		if (shift == 0)
			return *this;

		if (shift >= 128)
			return UInt128(0);

		UInt128 result;

		if (shift >= 64)
		{
			result.hi = lo << (shift - 64);
			result.lo = 0;
		}
		else
		{
			result.hi = (hi << shift) |
				(lo >> (64 - shift));

			result.lo = lo << shift;
		}

		return result;
	}


	// ------------------------------------------------------------
	// Shift right
	// ------------------------------------------------------------

	UInt128 operator>>(unsigned int shift) const
	{
		if (shift == 0)
			return *this;

		if (shift >= 128)
			return UInt128(0);

		UInt128 result;

		if (shift >= 64)
		{
			result.lo = hi >> (shift - 64);
			result.hi = 0;
		}
		else
		{
			result.lo = (lo >> shift) |
				(hi << (64 - shift));

			result.hi = hi >> shift;
		}

		return result;
	}


	// ------------------------------------------------------------
	// Bit access
	// ------------------------------------------------------------

	bool getBit(unsigned int bit) const
	{
		if (bit < 64)
			return (lo >> bit) & 1ULL;

		return (hi >> (bit - 64)) & 1ULL;
	}


	void setBit(unsigned int bit)
	{
		if (bit < 64)
			lo |= (1ULL << bit);
		else
			hi |= (1ULL << (bit - 64));
	}


	// ------------------------------------------------------------
	// Division
	//
	// Returns quotient.
	//
	// If remainder is non-null, the remainder is stored there.
	// ------------------------------------------------------------

	UInt128 divide(const UInt128& divisor,
		UInt128* remainder = nullptr) const
	{
		if (divisor == UInt128(0))
			throw std::runtime_error("Division by zero");

		UInt128 quotient(0);
		UInt128 rem(0);

		// Binary long division.
		//
		// Process the most significant bit first.
		for (int bit = 127; bit >= 0; --bit)
		{
			// Shift remainder left by one
			rem = rem << 1;

			// Bring down next bit
			if (getBit(bit))
				rem.lo |= 1;

			// If remainder >= divisor,
			// subtract divisor and set quotient bit.
			if (rem >= divisor)
			{
				rem -= divisor;
				quotient.setBit(bit);
			}
		}

		if (remainder)
			*remainder = rem;

		return quotient;
	}


	// ------------------------------------------------------------
	// Division operator
	// ------------------------------------------------------------

	UInt128 operator/(const UInt128& divisor) const
	{
		return divide(divisor);
	}


	// ------------------------------------------------------------
	// Modulo
	// ------------------------------------------------------------

	UInt128 operator%(const UInt128& divisor) const
	{
		UInt128 remainder;
		divide(divisor, &remainder);
		return remainder;
	}


	// ------------------------------------------------------------
	// Decimal conversion
	// ------------------------------------------------------------

	std::string toString() const
	{
		if (hi == 0)
			return std::to_string(lo);

		UInt128 value = *this;

		std::string result;

		// Repeatedly divide by 10.
		while (value != UInt128(0))
		{
			UInt128 remainder;

			value.divide(UInt128(10), &remainder);

			result.push_back(
				char('0' + static_cast<int>(remainder.lo))
			);

			value = value / UInt128(10);
		}

		// Reverse result
		std::reverse(result.begin(), result.end());

		return result;
	}
};


// ------------------------------------------------------------
// Stream output
// ------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const UInt128& value)
{
	os << value.toString();
	return os;
}
