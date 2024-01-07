// Copyright (C) 2023 Hyunwoo Park
//
// This file is part of trgkASIO.
//
// trgkASIO is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// trgkASIO is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with trgkASIO.  If not, see <http://www.gnu.org/licenses/>.
//


#include "catch.hpp"
#include "../Source/utils/RingBuffer.h"

TEST_CASE("Ringbuffer push", "[ring_buffer]") {
    RingBuffer<int> rb(10);
    REQUIRE(rb.size() == 0);
    REQUIRE(rb.capacity() == 10);

    {
        int arr[5] = {1, 2, 3, 4, 5};
        REQUIRE(rb.push(arr, 5));
        REQUIRE(rb.get(arr, 3));
    }

    SECTION("Overflow doesnt work") {
        int arr[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
        REQUIRE(!rb.push(arr, 9));
    };

    SECTION("Wrapping overflow works") {
        int arr[7] = {1, 2, 3, 4, 5, 6, 7};
        REQUIRE(rb.push(arr, 7));
    };

    SECTION("Wrapping overflow works (2)") {
        int arr[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        REQUIRE(rb.push(arr, 8));
    };
}


TEST_CASE("Ringbuffer works", "[ring_buffer]") {
    RingBuffer<int> rb(10);
    REQUIRE(rb.size() == 0);
    REQUIRE(rb.capacity() == 10);

    // Push works
    {
        int arr[5] = {1, 2, 3, 4, 5};
        REQUIRE(rb.push(arr, 5));
        REQUIRE(rb.size() == 5);
    }

    // push overflow doesn't work
    {
        int arr[10] = {1, 2, 3, 4, 5, 6, 7};
        REQUIRE(!rb.push(arr, 7));
    }

    // get work
    {
        int arr[3];
        REQUIRE(rb.get(arr, 3));
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 3);
        REQUIRE(rb.size() == 2);
        REQUIRE(!rb.get(arr, 3)); // get underflow doesn't work
    }

    // wrapping push works
    {
        int arr[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        REQUIRE(rb.push(arr, 8));
    }

    // wrapping get works
    {
        int arr[10];
        int expected[10] = {4, 5, 1, 2, 3, 4, 5, 6, 7, 8};
        REQUIRE(rb.get(arr, 10));
        REQUIRE(rb.size() == 0);
        REQUIRE(rb.capacity() == 10);
        REQUIRE(memcmp(arr, expected, sizeof(int) * 10) == 0);
    }
}
