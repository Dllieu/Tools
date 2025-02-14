//--------------------------------------------------------------------------------
// (C) Copyright 2014-2015 Stephane Molina, All rights reserved.
// See https://github.com/Dllieu for updates, documentation, and revision history.
//--------------------------------------------------------------------------------
#include <boost/test/unit_test.hpp>
#include <array>
#include <random>

#include "tools/Benchmark.h"

BOOST_AUTO_TEST_SUITE( AlignmentTestSuite )

namespace
{
    template < size_t Alignment >
    constexpr size_t  alignment_up( size_t n )
    {
        static_assert( Alignment == 1 || Alignment != 0 && ( Alignment & 1 ) == 0, "Wrong alignment" );
        return n + ( Alignment - 1 ) & ~( Alignment - 1 );
    }
}

BOOST_AUTO_TEST_CASE( AlignmentUpTest )
{
    constexpr const size_t alignment = 8;
    for ( auto i = 0; i < 100; ++i )
        BOOST_CHECK( alignment_up< alignment >( i ) % alignment == 0 );
}

namespace
{
    // All attributes must have unique address, instanciation of empty class still have size overhead, but inheritance of EmptyClass have 0 overhead (as there's no attributes)
    class EmptyClass {};
    class EmptyClass2 : public EmptyClass {};
    class NonEmptyClass : public EmptyClass { int a; };
    class EmptyVirtualClass { virtual void foo() {}; };

    static_assert( sizeof( EmptyClass ) == sizeof( char ), "wrong size" ); // usually sizeof( char ) to be aligned
    static_assert( sizeof( EmptyClass2 ) == sizeof( char ), "wrong size" ); // usually sizeof( char ) to be aligned
    static_assert( sizeof( NonEmptyClass ) == sizeof( NonEmptyClass::a ), "wrong size" );
    static_assert( sizeof( EmptyVirtualClass ) == sizeof( nullptr_t ), "wrong size" );

    struct alignas( 16 ) A
    {
        int     a;
    };
    static_assert( sizeof( A ) == 16 && alignof( A ) == 16, "not aligned" );

    struct alignas( 16 ) B
    {
        char    b[ 17 ];
    };
    static_assert( sizeof( B ) == 32 && alignof( B ) == 16, "not aligned" );

    struct C
    {
        char    a;
        double  b;
        char    c;
    };
    static_assert( sizeof( C ) == 24, "invalid size" ); // x64
}

namespace
{
    template < size_t N >
    auto    generateArray( int maxDistrib )
    {
        std::uniform_int_distribution<> rnd( 0, maxDistrib );
        std::mt19937 gen;

        std::array< uint32_t, N > result;
        std::generate( result.begin(), result.end(), [ &rnd, &gen ] { return rnd( gen ); } );

        return result;
    }

    struct Aligned
    {
        char    a;
        int     b;
        char    c;
    };
    static_assert( sizeof( Aligned ) == 12, "invalid size" );

    struct Aligned2
    {
        int     a; // 0-4
        char    b; // 4-8
        int     c; // 8-16 (i.e. sizeof(size_t) can't be aligned if using 12-16)
        char*   d; // 16-24
    };
    static_assert( sizeof( Aligned2 ) == 24, "invalid size" );

    // pragma pack 1 isn't helpful to avoid heap fragmentation.
    // pragma pack 1 is used to remove the padding bytes from structures to help with transmission of binary structures between programs (i.e. transferring a message through the NIC),
    // but will slower the program vs aligned struct when reading / writing on it (specially true for x86)
    // The CPU always reads at its word size(4 bytes/32bit || 8 bytes/64bit), so when you do an unaligned address access (on a processor that supports it)
    //   The processor is going to read multiple words. The CPU will read each word of memory that your requested address straddles.
    //   This causes an amplification of up to 2X the number of memory transactions required to access the requested data.
    // In this example, on a 32bit system (4 bytes word), b won't be aligned
    //   When you ask for 32 bits from 0x0001 (&b) you'll get a 2X amplification.
    //   The processor will read from 0x0000 (&a) into the result register and shift left 1 byte,
    //   then read again from 0x0004 (b is contained within 2 words) into a temporary register, shift right 3 bytes, then OR it with the result register.
    // Also, The CPU can operate on an aligned word of memory atomically, meaning that no other instruction can interrupt that operation.
    //   This is critical to the correct operation of many lock-free data structures and other concurrent computing paradigms
    #pragma pack(1)
    //struct alignas(1) C // Alignment specifier is less than actual alignment (4) (vc140 limitation?)
    struct Unaligned
    {
        char    a;
        int     b;
        char    c;
    };
    static_assert( sizeof( Unaligned ) == 6, "invalid size" );
    #pragma pack() // reset default alignment
}

BOOST_AUTO_TEST_CASE( AlignedVsUnalignedBenchmark )
{
    auto a = generateArray< 4096 >( std::numeric_limits<int>::max() );
    auto indexes = generateArray< 10000 >( static_cast< int >( a.size() ) );

    auto f = [] ( auto numbers, const auto& indexes ) { auto res = 0; for ( const auto& i : indexes ) res += *reinterpret_cast< uint32_t* >( numbers + i ); return res; }; // will be unaligned at some point if input is uint8_t
    auto test = [ & ] ( auto n )
    {
        double alignedT, unalignedT;
        std::tie( alignedT, unalignedT ) = tools::benchmark( n,
                                                             [ & ] { return f( a.data(), indexes ); },
                                                             [ & ] { return f( reinterpret_cast< uint8_t* >( a.data() ), indexes ); } );

        BOOST_CHECK( alignedT < unalignedT );
    };
    tools::run_test< int >( "aligned;unaligned;", test, 100'000, 1'000'000 );
}

BOOST_AUTO_TEST_SUITE_END() // AlignmentTestSuite
