// Copyright (C) 2023 Hyunwoo Park
//
// This file is part of ASIO2WASAPI2.
//
// ASIO2WASAPI2 is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// ASIO2WASAPI2 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ASIO2WASAPI2.  If not, see <http://www.gnu.org/licenses/>.
//

//
// Created by whyask37 on 2023-10-04.
//

#pragma once

#ifndef ASIO2WASAPI2_RINGBUFFER_H
#define ASIO2WASAPI2_RINGBUFFER_H

#include <vector>
#include <mutex>

template<typename T>
class RingBuffer {
public:
    RingBuffer(size_t capacity)
            : _ringBuffer(capacity), _capacity(capacity), _readPos(0), _writePos(0) {}

    ~RingBuffer() = default;

    [[nodiscard]] size_t capacity() const { return _capacity; }

    [[nodiscard]] size_t size() const {
        return (_writePos + _capacity - _readPos) % _capacity;
    }

    bool push(const T *input, size_t inputSize) {
        bool write_overflow = false;

        auto rp = _readPos;
        auto &wp = _writePos;

        if (rp <= wp) {
            // case 1: -----rp+++++++++++wp-------
            if (wp + inputSize < _capacity) {
                // case 1-1 -----rp+++++++++++wp@@@@@wp--
                std::copy(input, input + inputSize, _ringBuffer.data() + wp);
                wp += inputSize;
            } else {
                // case 1-1 @@wp--rp+++++++++++wp@@@@@@@@
                auto fillToEndSize = _capacity - wp;
                auto fillFromStartSize = inputSize - fillToEndSize;
                if (fillFromStartSize >= rp) {
                    write_overflow = true;
                } else {
                    std::copy(input, input + fillToEndSize, _ringBuffer.data() + wp);
                    std::copy(input + fillToEndSize, input + inputSize, _ringBuffer.data());
                    wp = fillFromStartSize;
                }
            }
        } else {
            // case 2: ++++wp--------------rp++++
            if (wp + inputSize >= rp) {
                write_overflow = true;
            } else {
                memcpy(
                        _ringBuffer.data() + wp,
                        input,
                        inputSize * sizeof(T));
                wp += inputSize;
            }
        }

        return !write_overflow;
    }

    bool get(T *output, size_t requestedSize) {
        auto size = (_writePos + _capacity - _readPos) % _capacity;
        if (size < requestedSize) return false; // Insufficient data

        auto bufferData = _ringBuffer.data();
        const T *pIn = bufferData + _readPos;
        const T *pInWrap = bufferData + _capacity;
        T *pOut = output;
        T *pOutEnd = output + requestedSize;

        while (pOut < pOutEnd) {
            *(pOut++) = *(pIn++);
            if (pIn == pInWrap) pIn = bufferData;
        }
        _readPos = pIn - bufferData;
        return true;
    }


public:;

    size_t rp() const { return _readPos; }

    size_t wp() const { return _writePos; }

private:
    std::vector<T> _ringBuffer;
    size_t _capacity;
    size_t _readPos = 0;
    size_t _writePos = 0;
};

#endif //ASIO2WASAPI2_RINGBUFFER_H
