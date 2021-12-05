///
/// \brief \b [Internal] SHA-1 computation class
///
/// Performant RSA en/decryption with 256-bit to 16384-bit modulus
///
/// catid(cat02e@fsu.edu)
///
/// 7/30/2004 Fixed VS6 compat
/// 7/26/2004 Now internally generates private keys
/// simpleModExp() is faster for encryption than MontyModExp
/// CRT-MontyModExp is faster for decryption than CRT-SimpleModExp
/// 7/25/2004 Implemented Montgomery modular exponentation
/// Implemented CRT modular exponentation optimization
/// 7/21/2004 Did some pre-lim coding
///
/// Best performance on my 1.8 GHz P4 (mobile):
/// 1024-bit generate key : 30 seconds
/// 1024-bit set private key : 100 ms (pre-compute this step)
/// 1024-bit encryption : 200 usec
/// 1024-bit decryption : 400 ms
///
/// \todo There's a bug in MonModExp() that restricts us to k-1 bits
///
/// Tabs: 4 spaces
/// Dist: public

#ifndef RSACRYPT_H
#define RSACRYPT_H

#if !defined(_XBOX360) && !defined(_WIN32_WCE)

#define RSASUPPORTGENPRIME
#include "Export.h"
/// Can't go under 256 or you'll need to disable the USEASSEMBLY macro in bigtypes.h
/// That's because the assembly assumes at least 128-bit data to work on
/// #define RSA_BIT_SIZE big::u512
#define RSA_BIT_SIZE big::u256

#include "RakMemoryOverride.h"
#include "BigTypes.h"
#include "Rand.h" //Giblet - added missing include for randomMT()

#ifdef _MSC_VER
#pragma warning( push )
#endif

namespace big
{

	using namespace cat;

	// r = x^y Mod n (fast for small y)
	BIGONETYPE void simpleModExp( T &x0, T &y0, T &n0, T &r0 )
	{
		BIGDOUBLESIZE( T, x );
		BIGDOUBLESIZE( T, y );
		BIGDOUBLESIZE( T, n );
		BIGDOUBLESIZE( T, r );

		usetlow( x, x0 );
		usetlow( y, y0 );
		usetlow( n, n0 );
		usetw( r, 1 );

		umodulo( x, n, x );

		u32 squares = 0;

		for ( u32 ii = 0; ii < BIGWORDCOUNT( T ); ++ii )
		{
			word y_i = y[ ii ];

			u32 ctr = WORDBITS;

			while ( y_i )
			{
				if ( y_i & 1 )
				{
					if ( squares )
						do
						{
							usquare( x );
							umodulo( x, n, x );
						}

						while ( --squares );

						umultiply( r, x, r );

						umodulo( r, n, r );
				}

				y_i >>= 1;
				++squares;
				--ctr;
			}

			squares += ctr;
		}

		takelow( r0, r );
	}

	// computes Rn = 2^k (mod n), n < 2^k
	BIGONETYPE void rModn( T &n, T &Rn )
	{
		BIGDOUBLESIZE( T, dR );
		BIGDOUBLESIZE( T, dn );
		BIGDOUBLESIZE( T, dRn );
		T one;

		// dR = 2^k
		usetw( one, 1 );
		sethigh( dR, one );

		// Rn = 2^k (mod n)
		usetlow( dn, n );
		umodulo( dR, dn, dRn );
		takelow( Rn, dRn );
	}

	// computes c = GCD(a, b)
	BIGONETYPE void GCD( T &a0, T &b0, T &c )
	{
		T a;

		umodulo( a0, b0, c );

		if ( isZero( c ) )
		{
			set ( c, b0 )

				;
			return ;
		}

		umodulo( b0, c, a );

		if ( isZero( a ) )
			return ;

#ifdef _MSC_VER
#pragma warning( disable : 4127 ) // warning C4127: conditional expression is constant
#endif
		while ( true )
		{
			umodulo( c, a, c );

			if ( isZero( c ) )
			{
				set ( c, a )

					;
				return ;
			}

			umodulo( a, c, a );

			if ( isZero( a ) )
				return ;
		}
	}

	// directly computes x = c - a * b (mod n) > 0, c < n
	BIGONETYPE void SubMulMod( T &a, T &b, T &c, T &n, T &x )
	{
		BIGDOUBLESIZE( T, da );
		BIGDOUBLESIZE( T, dn );
		T y;

		// y = a b (mod n)
		usetlow( da, a );
		umultiply( da, b );
		usetlow( dn, n );
		umodulo( da, dn, da );
		takelow( y, da );

		// x = (c - y) (mod n) > 0

		set ( x, c )

			;
		if ( ugreater( c, y ) )
		{
			subtract( x, y );
		}

		else
		{
			subtract( x, y );

			add ( x, n )

				;
		}
	}

	/*
	directly compute a' s.t. a' a - b' b = 1

	b = b0 = n0
	rp = a'
	a = 2^k
	a > b > 0
	GCD(a, b) = 1 (b odd)

	Trying to keep everything positive
	*/
	BIGONETYPE void computeRinverse( T &n0, T &rp )
	{
		T x0, x1, x2, a, b, q;

		//x[0] = 1
		usetw( x0, 1 );

		// a = 2^k (mod b0)
		rModn( n0, a );

		// {q, b} = b0 / a
		udivide( n0, a, q, b );

		// if b = 0, return x[0]

		if ( isZero( b ) )
		{
			set ( rp, x0 )

				;
			return ;
		}

		// x[1] = -q (mod b0) = b0 - q, q <= b0
		set ( x1, n0 )

			;
		subtract( x1, q );

		// {q, a} = a / b
		udivide( a, b, q, a );

		// if a = 0, return x[1]
		if ( isZero( a ) )
		{
			set ( rp, x1 )

				;
			return ;
		}
#ifdef _MSC_VER
#pragma warning( disable : 4127 ) // warning C4127: conditional expression is constant
#endif
		while ( true )
		{
			// x[2] = x[0] - x[1] * q (mod b0)
			SubMulMod( q, x1, x0, n0, x2 );

			// {q, b} = b / a
			udivide( b, a, q, b );

			// if b = 0, return x[2]

			if ( isZero( b ) )
			{
				set ( rp, x2 )

					;
				return ;
			}

			// x[0] = x[1] - x[2] * q (mod b0)
			SubMulMod( q, x2, x1, n0, x0 );

			// {q, a} = a / b
			udivide( a, b, q, a );

			// if a = 0, return x[0]
			if ( isZero( a ) )
			{
				set ( rp, x0 )

					;
				return ;
			}

			// x[1] = x[2] - x[0] * q (mod b0)
			SubMulMod( q, x0, x2, n0, x1 );

			// {q, b} = b / a
			udivide( b, a, q, b );

			// if b = 0, return x[1]
			if ( isZero( b ) )
			{
				set ( rp, x1 )

					;
				return ;
			}

			// x[2] = x[0] - x[1] * q (mod b0)
			SubMulMod( q, x1, x0, n0, x2 );

			// {q, a} = a / b
			udivide( a, b, q, a );

			// if a = 0, return x[2]
			if ( isZero( a ) )
			{
				set ( rp, x2 )

					;
				return ;
			}

			// x[0] = x[1] - x[2] * q (mod b0)
			SubMulMod( q, x2, x1, n0, x0 );

			// {q, b} = b / a
			udivide( b, a, q, b );

			// if b = 0, return x[0]
			if ( isZero( b ) )
			{
				set ( rp, x0 )

					;
				return ;
			}

			// x[1] = x[2] - x[0] * q (mod b0)
			SubMulMod( q, x0, x2, n0, x1 );

			// {q, a} = a / b
			udivide( a, b, q, a );

			// if a = 0, return x[1]
			if ( isZero( a ) )
			{
				set ( rp, x1 )

					;
				return ;
			}
		}
	}

	/* BIGONETYPE void computeRinverse2(T &_n0, T &_rp)
	{
	//T x0, x1, x2, a, b, q;
	BIGDOUBLESIZE(T, x0);
	BIGDOUBLESIZE(T, x1);
	BIGDOUBLESIZE(T, x2);
	BIGDOUBLESIZE(T, a);
	BIGDOUBLESIZE(T, b);
	BIGDOUBLESIZE(T, q);
	BIGDOUBLESIZE(T, n0);
	BIGDOUBLESIZE(T, rp);

	usetlow(n0, _n0);
	usetlow(rp, _rp);

	std::string old;
	//x[0] = 1
	usetw(x0, 1);

	T _a;
	// a = 2^k (mod b0)
	rModn(_n0, _a);
	RECORD("TEST") << "a=" << toString(a, false) << " = 2^k (mod " << toString(n0, false) << ")";
	usetlow(a, _a);

	// {q, b} = b0 / a
	udivide(n0, a, q, b);
	RECORD("TEST") << "{q=" << toString(q, false) << ", b=" << toString(b, false) << "} = n0=" << toString(n0, false) << " / a=" << toString(a, false);

	// if b = 0, return x[0]
	if (isZero(b))
	{
	RECORD("TEST") << "b == 0, Returning x[0]";
	set(rp, x0);
	takelow(_rp, rp);
	return;
	}

	// x[1] = -q (mod b0)
	negate(q);
	smodulo(q, n0, x1);
	if (BIGHIGHBIT(x1))
	add(x1, n0); // q > 0
	RECORD("TEST") << "x1=" << toString(x1, false) << " = q=" << toString(q, false) << " (mod n0=" << toString(n0, false) << ")";

	// {q, a} = a / b
	old = toString(a, false);
	udivide(a, b, q, a);
	RECORD("TEST") << "{q=" << toString(q, false) << ", a=" << toString(a, false) << "} = a=" << old << " / b=" << toString(b);

	// if a = 0, return x[1]
	if (isZero(a))
	{
	RECORD("TEST") << "a == 0, Returning x[1]";
	set(rp, x1);
	takelow(_rp, rp);
	return;
	}

	RECORD("TEST") << "Entering loop...";
	while (true)
	{
	// x[2] = x[0] - x[1] * q (mod b0)
	SubMulMod(q, x1, x0, n0, x2);
	RECORD("TEST") << "x[0] = " << toString(x0, false);
	RECORD("TEST") << "x[1] = " << toString(x1, false);
	RECORD("TEST") << "x[2] = " << toString(x2, false);

	// {q, b} = b / a
	old = toString(b);
	udivide(b, a, q, b);
	RECORD("TEST") << "{q=" << toString(q, false) << ", b=" << toString(b) << "} = b=" << old << " / a=" << toString(a, false);

	// if b = 0, return x[2]
	if (isZero(b))
	{
	RECORD("TEST") << "b == 0, Returning x[2]";
	set(rp, x2);
	takelow(_rp, rp);
	return;
	}

	// x[0] = x[1] - x[2] * q (mod b0)
	SubMulMod(q, x2, x1, n0, x0);
	RECORD("TEST") << "x[0] = " << toString(x0, false);
	RECORD("TEST") << "x[1] = " << toString(x1, false);
	RECORD("TEST") << "x[2] = " << toString(x2, false);

	// {q, a} = a / b
	old = toString(a, false);
	udivide(a, b, q, a);
	RECORD("TEST") << "{q=" << toString(q, false) << ", a=" << toString(a, false) << "} = a=" << old << " / b=" << toString(b);

	// if a = 0, return x[0]
	if (isZero(a))
	{
	RECORD("TEST") << "a == 0, Returning x[0]";
	set(rp, x0);
	takelow(_rp, rp);
	return;
	}

	// x[1] = x[2] - x[0] * q (mod b0)
	SubMulMod(q, x0, x2, n0, x1);
	RECORD("TEST") << "x[0] = " << toString(x0, false);
	RECORD("TEST") << "x[1] = " << toString(x1, false);
	RECORD("TEST") << "x[2] = " << toString(x2, false);

	// {q, b} = b / a
	old = toString(b);
	udivide(b, a, q, b);
	RECORD("TEST") << "{q=" << toString(q, false) << ", b=" << toString(b) << "} = b=" << old << " / a=" << toString(a, false);

	// if b = 0, return x[1]
	if (isZero(b))
	{
	RECORD("TEST") << "b == 0, Returning x[1]";
	set(rp, x1);
	takelow(_rp, rp);
	return;
	}

	// x[2] = x[0] - x[1] * q (mod b0)
	SubMulMod(q, x1, x0, n0, x2);
	RECORD("TEST") << "x[0] = " << toString(x0, false);
	RECORD("TEST") << "x[1] = " << toString(x1, false);
	RECORD("TEST") << "x[2] = " << toString(x2, false);

	// {q, a} = a / b
	old = toString(a, false);
	udivide(a, b, q, a);
	RECORD("TEST") << "{q=" << toString(q, false) << ", a=" << toString(a, false) << "} = a=" << old << " / b=" << toString(b);

	// if a = 0, return x[2]
	if (isZero(a))
	{
	RECORD("TEST") << "a == 0, Returning x[2]";
	set(rp, x2);
	takelow(_rp, rp);
	return;
	}

	// x[0] = x[1] - x[2] * q (mod b0)
	SubMulMod(q, x2, x1, n0, x0);
	RECORD("TEST") << "x[0] = " << toString(x0, false);
	RECORD("TEST") << "x[1] = " << toString(x1, false);
	RECORD("TEST") << "x[2] = " << toString(x2, false);

	// {q, b} = b / a
	old = toString(b);
	udivide(b, a, q, b);
	RECORD("TEST") << "{q=" << toString(q, false) << ", b=" << toString(b) << "} = b=" << old << " / a=" << toString(a, false);

	// if b = 0, return x[0]
	if (isZero(b))
	{
	RECORD("TEST") << "b == 0, Returning x[0]";
	set(rp, x0);
	takelow(_rp, rp);
	return;
	}

	// x[1] = x[2] - x[0] * q (mod b0)
	SubMulMod(q, x0, x2, n0, x1);
	RECORD("TEST") << "x[0] = " << toString(x0, false);
	RECORD("TEST") << "x[1] = " << toString(x1, false);
	RECORD("TEST") << "x[2] = " << toString(x2, false);

	// {q, a} = a / b
	old = toString(a, false);
	udivide(a, b, q, a);
	RECORD("TEST") << "{q=" << toString(q, false) << ", a=" << toString(a, false) << "} = a=" << old << " / b=" << toString(b);

	// if a = 0, return x[1]
	if (isZero(a))
	{
	RECORD("TEST") << "a == 0, Returning x[1]";
	set(rp, x1);
	takelow(_rp, rp);
	return;
	}
	}
	}
	*/
	// directly compute a^-1 s.t. a^-1 a (mod b) = 1, a < b, GCD(a, b)
	BIGONETYPE void computeModularInverse( T &a0, T &b0, T &ap )
	{
		T x0, x1, x2;
		T a, b, q;

		// x[2] = 1
		usetw( x2, 1 );

		// {q, b} = b0 / a0
		udivide( b0, a0, q, b );

		// x[0] = -q (mod b0) = b0 - q, q <= b0

		set ( x0, b0 )

			;
		subtract( x0, q );

		set ( a, a0 )

			;

#ifdef _MSC_VER
#pragma warning( disable : 4127 ) // warning C4127: conditional expression is constant
#endif
		while ( true )
		{
			// {q, a} = a / b
			udivide( a, b, q, a );

			// if a = 0, return x[0]

			if ( isZero( a ) )
			{
				set ( ap, x0 )

					;
				return ;
			}

			// x[1] = x[2] - x[0] * q (mod b0)
			SubMulMod( x0, q, x2, b0, x1 );

			// {q, b} = b / a
			udivide( b, a, q, b );

			// if b = 0, return x[1]
			if ( isZero( b ) )
			{
				set ( ap, x1 )

					;
				return ;
			}

			// x[2] = x[0] - x[1] * q (mod b0)
			SubMulMod( x1, q, x0, b0, x2 );

			// {q, a} = a / b
			udivide( a, b, q, a );

			// if a = 0, return x[2]
			if ( isZero( a ) )
			{
				set ( ap, x2 )

					;
				return ;
			}

			// x[0] = x[1] - x[2] * q (mod b0)
			SubMulMod( x2, q, x1, b0, x0 );

			// {q, b} = b / a
			udivide( b, a, q, b );

			// if b = 0, return x[0]
			if ( isZero( b ) )
			{
				set ( ap, x0 )

					;
				return ;
			}

			// x[1] = x[2] - x[0] * q (mod b0)
			SubMulMod( x0, q, x2, b0, x1 );

			// {q, a} = a / b
			udivide( a, b, q, a );

			// if a = 0, return x[1]
			if ( isZero( a ) )
			{
				set ( ap, x1 )

					;
				return ;
			}

			// x[2] = x[0] - x[1] * q (mod b0)
			SubMulMod( x1, q, x0, b0, x2 );

			// {q, b} = b / a
			udivide( b, a, q, b );

			// if b = 0, return x[2]
			if ( isZero( b ) )
			{
				set ( ap, x2 )

					;
				return ;
			}

			// x[0] = x[1] - x[2] * q (mod b0)
			SubMulMod( x2, q, x1, b0, x0 );
		}
	}

	// indirectly computes  n' s.t. 1 = r' r - n' n = GCD(r, n)
	BIGONETYPE void computeNRinverse( T &n0, T &np )
	{
		BIGDOUBLESIZE( T, r );
		BIGDOUBLESIZE( T, n );

		// r' = (1 + n' n) / r
		computeRinverse( n0, np );

		// n' = (r' r - 1) / n
		sethigh( r, np ); // special case of r = 2^k
		decrement( r );
		usetlow( n, n0 );
		udivide( r, n, n, r );
		takelow( np, n );
	}

	/*
	// indirectly computes n' s.t. 1 = r' r - n' n = GCD(r, n)
	BIGONETYPE void computeNRinverse2(T &n0, T &np)
	{
	BIGDOUBLESIZE(T, r);
	BIGDOUBLESIZE(T, n);

	// r' = (1 + n' n) / r
	computeRinverse2(n0, np);

	// n' = (r' r - 1) / n
	sethigh(r, np); // special case of r = 2^k
	decrement(r);
	usetlow(n, n0);
	udivide(r, n, n, r);
	takelow(np, n);
	}
	*/
	// Montgomery product u = a * b (mod n)
	BIGONETYPE void MonPro( T &ap, T &bp, T &n, T &np, T &u_out )
	{
		BIGDOUBLESIZE( T, t );
		BIGDOUBLESIZE( T, u );
		T m;

		// t = a' b'
		umultiply( ap, bp, t );

		// m = (low half of t)*np (mod r)
		takelow( m, t );
		umultiply( m, np );

		// u = (t + m*n), u_out = u / r = high half of u
		umultiply( m, n, u );

		add ( u, t )

			;
		takehigh( u_out, u );

		// if u >= n, return u - n, else u
		if ( ugreaterOrEqual( u_out, n ) )
			subtract( u_out, n );
	}

	// indirectly calculates x = M^e (mod n)
	BIGONETYPE void MonModExp( T &x, T &M, T &e, T &n, T &np, T &xp0 )
	{
		// x' = xp0

		set ( x, xp0 )

			;

		// find M' = M r (mod n)
		BIGDOUBLESIZE( T, dM );

		BIGDOUBLESIZE( T, dn );

		T Mp;

		sethigh( dM, M ); // dM = M r

		usetlow( dn, n );

		umodulo( dM, dn, dM ); // dM = dM (mod n)

		takelow( Mp, dM ); // M' = M r (mod n)

		/*	i may be wrong, but it seems to me that the squaring
		results in a constant until we hit the first set bit
		this could save a lot of time, but it needs to be proven
		*/

		s32 ii, bc;

		word e_i;

		// for i = k - 1 down to 0 do
		for ( ii = BIGWORDCOUNT( T ) - 1; ii >= 0; --ii )
		{
			e_i = e[ ii ];
			bc = WORDBITS;

			while ( bc-- )
			{
				// if e_i = 1, x = MonPro(M', x')

				if ( e_i & WORDHIGHBIT )
					goto start_squaring;

				e_i <<= 1;
			}
		}

		for ( ; ii >= 0; --ii )
		{
			e_i = e[ ii ];
			bc = WORDBITS;

			while ( bc-- )
			{
				// x' = MonPro(x', x')
				MonPro( x, x, n, np, x );

				// if e_i = 1, x = MonPro(M', x')

				if ( e_i & WORDHIGHBIT )
				{

start_squaring:
					MonPro( Mp, x, n, np, x );
				}

				e_i <<= 1;
			}
		}

		// x = MonPro(x', 1)
		T one;

		usetw( one, 1 );

		MonPro( x, one, n, np, x );
	}

	// indirectly calculates x = C ^ d (mod n) using the Chinese Remainder Thm
	BIGTWOTYPES void CRTModExp( Bigger &x, Bigger &C, Bigger &d, T &p, T &q, T &pInverse, T &pnp, T &pxp, T &qnp, T &qxp )
	{
		(void) qxp;
		(void) pxp;
		(void) pnp;
		(void) qnp;

		// d1 = d mod (p - 1)
		Bigger dd1;
		T d1;
		usetlow( dd1, p );
		decrement( dd1 );
		umodulo( d, dd1, dd1 );
		takelow( d1, dd1 );

		// M1 = C1^d1 (mod p)
		Bigger dp, dC1;
		T M1, C1;
		usetlow( dp, p );
		umodulo( C, dp, dC1 );
		takelow( C1, dC1 );
		simpleModExp( C1, d1, p, M1 );
		//MonModExp(M1, C1, d1, p, pnp, pxp);

		// d2 = d mod (q - 1)
		Bigger dd2;
		T d2;
		usetlow( dd2, q );
		decrement( dd2 );
		umodulo( d, dd2, dd2 );
		takelow( d2, dd2 );

		// M2 = C2^d2 (mod q)
		Bigger dq, dC2;
		T M2, C2;
		usetlow( dq, q );
		umodulo( C, dq, dC2 );
		takelow( C2, dC2 );
		simpleModExp( C2, d2, q, M2 );
		//MonModExp(M2, C2, d2, q, qnp, qxp);

		// x = M1 + p * ((M2 - M1)(p^-1 mod q) mod q)

		if ( ugreater( M2, M1 ) )
		{
			subtract( M2, M1 );
		}

		else
		{
			subtract( M2, M1 );

			add ( M2, q )

				;
		}

		// x = M1 + p * (( M2 )(p^-1 mod q) mod q)

		umultiply( M2, pInverse, x );

		// x = M1 + p * (( x ) mod q)

		umodulo( x, dq, x );

		// x = M1 + p * ( x )

		umultiply( x, dp );

		// x = M1 + ( x )

		Bigger dM1;

		usetlow( dM1, M1 );

		// x = ( dM1 ) + ( x )

		add ( x, dM1 )

			;
	}

	// generates a suitable public exponent s.t. 4 < e << phi, GCD(e, phi) = 1
	BIGONETYPE void computePublicExponent( T &phi, T &e )
	{
		T r, one, two;
		usetw( one, 1 );
		usetw( two, 2 );
		usetw( e, 65537 - 2 );

		if ( ugreater( e, phi ) )
			usetw( e, 5 - 2 );

		do
		{
			add ( e, two )

				;

			GCD( phi, e, r );
		}

		while ( !equal( r, one ) );
	}

	// directly computes private exponent
	BIGONETYPE void computePrivateExponent( T &e, T &phi, T &d )
	{
		// d = e^-1 (mod phi), 1 < e << phi
		computeModularInverse( e, phi, d );
	}

#ifdef RSASUPPORTGENPRIME

	static const u16 PRIME_TABLE[ 256 ] =
	{
		3, 5, 7, 11, 13, 17, 19, 23,
			29, 31, 37, 41, 43, 47, 53, 59,
			61, 67, 71, 73, 79, 83, 89, 97,
			101, 103, 107, 109, 113, 127, 131, 137,
			139, 149, 151, 157, 163, 167, 173, 179,
			181, 191, 193, 197, 199, 211, 223, 227,
			229, 233, 239, 241, 251, 257, 263, 269,
			271, 277, 281, 283, 293, 307, 311, 313,
			317, 331, 337, 347, 349, 353, 359, 367,
			373, 379, 383, 389, 397, 401, 409, 419,
			421, 431, 433, 439, 443, 449, 457, 461,
			463, 467, 479, 487, 491, 499, 503, 509,
			521, 523, 541, 547, 557, 563, 569, 571,
			577, 587, 593, 599, 601, 607, 613, 617,
			619, 631, 641, 643, 647, 653, 659, 661,
			673, 677, 683, 691, 701, 709, 719, 727,
			733, 739, 743, 751, 757, 761, 769, 773,
			787, 797, 809, 811, 821, 823, 827, 829,
			839, 853, 857, 859, 863, 877, 881, 883,
			887, 907, 911, 919, 929, 937, 941, 947,
			953, 967, 971, 977, 983, 991, 997, 1009,
			1013, 1019, 1021, 1031, 1033, 1039, 1049, 1051,
			1061, 1063, 1069, 1087, 1091, 1093, 1097, 1103,
			1109, 1117, 1123, 1129, 1151, 1153, 1163, 1171,
			1181, 1187, 1193, 1201, 1213, 1217, 1223, 1229,
			1231, 1237, 1249, 1259, 1277, 1279, 1283, 1289,
			1291, 1297, 1301, 1303, 1307, 1319, 1321, 1327,
			1361, 1367, 1373, 1381, 1399, 1409, 1423, 1427,
			1429, 1433, 1439, 1447, 1451, 1453, 1459, 1471,
			1481, 1483, 1487, 1489, 1493, 1499, 1511, 1523,
			1531, 1543, 1549, 1553, 1559, 1567, 1571, 1579,
			1583, 1597, 1601, 1607, 1609, 1613, 1619, 1621
	};

	/*modified Rabin-Miller primality test (added small primes)

	When picking a value for insurance, note that the probability of failure
	of the test to detect a composite number is at most 4^(-insurance), so:
	insurance max. probability of failure
	3   1.56%
	4   0.39%
	5   0.098% <-- default
	6   0.024%
	...
	*/
	BIGONETYPE bool RabinMillerPrimalityTest( T &n, u32 insurance )
	{
		// check divisibility by small primes <= 1621 (speeds up computation)
		T temp;

		for ( u32 ii = 0; ii < 256; ++ii )
		{
			usetw( temp, PRIME_TABLE[ ii++ ] );

			umodulo( n, temp, temp );

			if ( isZero( temp ) )
				return false;
		}

		// n1 = n - 1
		T n1;

		set ( n1, n )

			;
		decrement( n1 );

		// write r 2^s = n - 1, r is odd
		T r;

		u32 s = 0;

		set ( r, n1 )

			;
		while ( !( r[ 0 ] & 1 ) )
		{
			ushiftRight1( r );
			++s;
		}

		// one = 1
		T one;

		usetw( one, 1 );

		// cache n -> dn
		BIGDOUBLESIZE( T, dy );

		BIGDOUBLESIZE( T, dn );

		usetlow( dn, n );

		while ( insurance-- )
		{
			// choose random integer a s.t. 1 < a < n - 1
			T a;
			int index;

			for ( index = 0; index < (int) sizeof( a ) / (int) sizeof( a[ 0 ] ); index++ )
				a[ index ] = randomMT();

			umodulo( a, n1, a );

			// compute y = a ^ r (mod n)
			T y;

			simpleModExp( a, r, n, y );

			if ( !equal( y, one ) && !equal( y, n1 ) )
			{
				u32 j = s;

				while ( ( j-- > 1 ) && !equal( y, n1 ) )
				{
					umultiply( y, y, dy );
					umodulo( dy, dn, dy );
					takelow( y, dy );

					if ( equal( y, one ) )
						return false;
				}

				if ( !equal( y, n1 ) )
					return false;
			}
		}

		return true;
	}

	// generates a strong pseudo-prime
	BIGONETYPE void generateStrongPseudoPrime( T &n )
	{
		do
		{
			int index;

			for ( index = 0; index < (int) sizeof( n ) / (int) sizeof( n[ 0 ] ); index++ )
				n[ index ] = randomMT();

			n[ BIGWORDCOUNT( T ) - 1 ] |= WORDHIGHBIT;

			//n[BIGWORDCOUNT(T) - 1] &= ~WORDHIGHBIT; n[BIGWORDCOUNT(T) - 1] |= WORDHIGHBIT >> 1;
			n[ 0 ] |= 1;
		}

		while ( !RabinMillerPrimalityTest( n, 5 ) );
	}

#endif // RSASUPPORTGENPRIME


	//////// RSACrypt class ////////

	BIGONETYPE class RAK_DLL_EXPORT RSACrypt : public RakNet::RakMemoryOverride
	{
		// public key
		T e, n;
		T np, xp;

		// private key
		bool factorsAvailable;
		T d, phi;
		BIGHALFSIZE( T, p );
		BIGHALFSIZE( T, pnp );
		BIGHALFSIZE( T, pxp );
		BIGHALFSIZE( T, q );
		BIGHALFSIZE( T, qnp );
		BIGHALFSIZE( T, qxp );
		BIGHALFSIZE( T, pInverse );

	public:
		RSACrypt()
		{
			reset();
		}

		~RSACrypt()
		{
			reset();
		}

	public:
		void reset()
		{
			zero( d );
			zero( p );
			zero( q );
			zero( pInverse );
			factorsAvailable = false;
		}

#ifdef RSASUPPORTGENPRIME

		void generateKeys()
		{
			BIGHALFSIZE( T, p0 );
			BIGHALFSIZE( T, q0 );

			generateStrongPseudoPrime( p0 );
			generateStrongPseudoPrime( q0 );

			setPrivateKey( p0, q0 );
		}

#endif // RSASUPPORTGENPRIME

		BIGSMALLTYPE void setPrivateKey( Smaller &c_p, Smaller &c_q )
		{
			factorsAvailable = true;

			// re-order factors s.t. q > p

			if ( ugreater( c_p, c_q ) )
			{
				set ( q, c_p )

					;
				set ( p, c_q )

					;
			}

			else
			{
				set ( p, c_p )

					;
				set ( q, c_q )

					;
			}

			// phi = (p - 1)(q - 1)
			BIGHALFSIZE( T, p1 );

			BIGHALFSIZE( T, q1 );

			set ( p1, p )

				;
			decrement( p1 );

			set ( q1, q )

				;
			decrement( q1 );

			umultiply( p1, q1, phi );

			// compute e
			computePublicExponent( phi, e );

			// compute d
			computePrivateExponent( e, phi, d );

			// compute p^-1 mod q
			computeModularInverse( p, q, pInverse );

			// compute n = pq
			umultiply( p, q, n );

			// find n'
			computeNRinverse( n, np );

			// x' = 1*r (mod n)
			rModn( n, xp );

			// find pn'
			computeNRinverse( p, pnp );

			//   computeNRinverse2(p, pnp);

			// px' = 1*r (mod p)
			rModn( p, pxp );

			// find qn'
			computeNRinverse( q, qnp );

			// qx' = 1*r (mod q)
			rModn( q, qxp );
		}

		void setPublicKey( u32 c_e, T &c_n )
		{
			reset(); // in case we knew a private key

			usetw( e, c_e );

			set ( n, c_n )

				;

			// find n'
			computeNRinverse( n, np );

			// x' = 1*r (mod n)
			rModn( n, xp );
		}

	public:
		void getPublicKey( u32 &c_e, T &c_n )
		{
			c_e = e[ 0 ];

			set ( c_n, n )

				;
		}

		BIGSMALLTYPE void getPrivateKey( Smaller &c_p, Smaller &c_q )
		{
			set ( c_p, p )

				;
			set ( c_q, q )

				;
		}

	public:
		void encrypt( T &M, T &x )
		{
			if ( factorsAvailable )
				CRTModExp( x, M, e, p, q, pInverse, pnp, pxp, qnp, qxp );
			else
				simpleModExp( M, e, n, x );
		}

		void decrypt( T &C, T &x )
		{
			if ( factorsAvailable )
				CRTModExp( x, C, d, p, q, pInverse, pnp, pxp, qnp, qxp );
		}
	};
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif

#endif // #if !defined(_XBOX360) && !defined(_WIN32_WCE)

#endif // RSACRYPT_H

