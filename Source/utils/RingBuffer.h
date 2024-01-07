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

//
// Created by whyask37 on 2023-10-04.
//

#pragma once

#ifndef TRGKASIO_RINGBUFFER_H
#define TRGKASIO_RINGBUFFER_H

#include <vector>
#include <mutex>

template<typename T>
class RingBuffer {
public:
    RingBuffer(size_t capacity)
            : _ringBuffer(capacity), _capacity(capacity), _readPos(0), _size(0) {}

    ~RingBuffer() = default;

    [[nodiscard]] size_t capacity() const { return _capacity; }

    [[nodiscard]] size_t size() const { return _size; }

    bool push(const T *input, size_t inputSize) {
        bool write_overflow = false;

        auto rp = _readPos;
        auto wp = writePos();

        if (rp <= wp) {
            // case 1: -----readPos+++++++++++writePos-------
            if (wp + inputSize <= _capacity) {
                // case 1-1 -----readPos+++++++++++wp@@@@@writePos--
                std::copy(input, input + inputSize, _ringBuffer.data() + wp);
                _size += inputSize;
            } else {
                // case 1-1 @@wp--readPos+++++++++++writePos@@@@@@@@
                auto fillToEndSize = _capacity - wp;
                auto fillFromStartSize = inputSize - fillToEndSize;
                if (fillFromStartSize > rp) {
                    write_overflow = true;
                } else {
                    std::copy(input, input + fillToEndSize, _ringBuffer.data() + wp);
                    std::copy(input + fillToEndSize, input + inputSize, _ringBuffer.data());
                    _size += inputSize;
                }
            }
        } else {
            // case 2: ++++writePos--------------readPos++++
            if (wp + inputSize > rp) {
                write_overflow = true;
            } else {
                memcpy(
                        _ringBuffer.data() + wp,
                        input,
                        inputSize * sizeof(T));
                _size += inputSize;
            }
        }

        return !write_overflow;
    }

    bool get(T *output, size_t requestedSize) {
        if (_size < requestedSize) return false; // Insufficient data

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
        _size -= requestedSize;
        return true;
    }


public:

    [[nodiscard]] size_t readPos() const { return _readPos; }

    [[nodiscard]] size_t writePos() const { return (_readPos + _size) % _capacity; }

private:
    std::vector<T> _ringBuffer;
    size_t _capacity;
    size_t _readPos = 0;
    size_t _size = 0;
};

#endif //TRGKASIO_RINGBUFFER_H
